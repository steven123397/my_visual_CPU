#include "machine.h"

#include <array>
#include <cctype>
#include <cstdlib>
#include <memory>
#include <stdexcept>

#include "../exec/functional_backend.h"
#include "../exec/pipeline_backend.h"

namespace {

struct GprAlias {
    const char* name;
    uint32_t index;
};

constexpr std::array<GprAlias, 33> kGprAliases = {{
    {"zero", 0}, {"ra", 1},  {"sp", 2},  {"gp", 3},  {"tp", 4},  {"t0", 5},  {"t1", 6},
    {"t2", 7},   {"s0", 8},  {"fp", 8},  {"s1", 9},  {"a0", 10}, {"a1", 11}, {"a2", 12},
    {"a3", 13},  {"a4", 14}, {"a5", 15}, {"a6", 16}, {"a7", 17}, {"s2", 18}, {"s3", 19},
    {"s4", 20},  {"s5", 21}, {"s6", 22}, {"s7", 23}, {"s8", 24}, {"s9", 25}, {"s10", 26},
    {"s11", 27}, {"t3", 28}, {"t4", 29}, {"t5", 30}, {"t6", 31},
}};

std::string normalize_gpr_name(const std::string& name) {
    std::string normalized = name;
    for (char& ch : normalized) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return normalized;
}

uint32_t parse_gpr_index(const std::string& reg_name) {
    const std::string normalized = normalize_gpr_name(reg_name);
    if (normalized.size() >= 2 && normalized[0] == 'x') {
        char* end = nullptr;
        const unsigned long index = std::strtoul(normalized.c_str() + 1, &end, 10);
        if (end != nullptr && *end == '\0' && index < 32UL) {
            return static_cast<uint32_t>(index);
        }
    }

    for (const GprAlias& alias : kGprAliases) {
        if (normalized == alias.name) {
            return alias.index;
        }
    }

    throw std::runtime_error("unknown GPR name: " + reg_name);
}

}  // namespace

const char* block_transport_name(BlockTransport transport) {
    switch (transport) {
    case BlockTransport::SimpleStorage:
        return "simple_storage";
    case BlockTransport::VirtioBlk:
        return "virtio-blk";
    }
    return "unknown";
}

BlockTransport parse_block_transport(const std::string& name) {
    if (name == "simple_storage") {
        return BlockTransport::SimpleStorage;
    }
    if (name == "virtio-blk" || name == "virtio_blk") {
        return BlockTransport::VirtioBlk;
    }
    throw std::runtime_error("unknown block transport: " + name);
}

Machine::Machine()
    : uart_(plic_),
      virtio_mmio_(plic_, VIRTIO_MMIO_PLIC_SOURCE, virtio_blk_),
      ai_accelerator_(plic_, AI_ACCEL_PLIC_SOURCE),
      bus_(ram_) {
    cpu_.csr().bind_clint(&clint_);
    ai_accelerator_.bind_bus(bus_);
    bus_.attach(uart_);
    bus_.attach(clint_);
    bus_.attach(plic_);
    bus_.attach(ai_accelerator_);
    rebuild_backend();
}

void Machine::set_backend_kind(BackendKind kind) {
    backend_kind_ = kind;
    rebuild_backend();
}

void Machine::set_block_transport(BlockTransport transport) {
    if (block_transport_bound_ && block_transport_ != transport) {
        throw std::runtime_error("block transport already bound");
    }
    block_transport_ = transport;
}

void Machine::bind_block_transport() {
    if (block_transport_bound_) {
        return;
    }

    switch (block_transport_) {
    case BlockTransport::SimpleStorage:
        bus_.attach(storage_);
        break;
    case BlockTransport::VirtioBlk:
        virtio_mmio_.bind_bus(bus_);
        bus_.attach(virtio_mmio_);
        break;
    }

    block_transport_bound_ = true;
}

void Machine::rebuild_backend() {
    switch (backend_kind_) {
    case BackendKind::Functional:
        backend_ = std::make_unique<FunctionalBackend>(cpu_, bus_);
        break;
    case BackendKind::Pipeline:
        backend_ = std::make_unique<PipelineBackend>(cpu_, bus_);
        break;
    }
}

void Machine::finish_image_load(uint64_t entry, Ram& staged_ram) {
    // Image reload swaps in a freshly loaded RAM image; device state is still
    // intentionally preserved, but reload should not carry over stale storage
    // command errors into the next guest image.
    ram_.swap(staged_ram);
    if (block_transport_ == BlockTransport::SimpleStorage) {
        bus_.try_store(STORAGE_BASE + STORAGE_REG_COMMAND, STORAGE_CMD_NONE, 8);
    }
    cpu_init(cpu_, entry);
    rebuild_backend();
    loaded_ = true;
}

void Machine::load_elf(const std::string& path) {
    bind_block_transport();
    Ram staged_ram;
    const uint64_t entry = elf_loader_.load(staged_ram, path.c_str());
    finish_image_load(entry, staged_ram);
}

void Machine::load_binary(const std::string& path, uint64_t addr) {
    bind_block_transport();
    Ram staged_ram;
    binary_loader_.load(staged_ram, path.c_str(), addr);
    finish_image_load(addr, staged_ram);
}

void Machine::load_binary_payload(const std::string& path, uint64_t addr) {
    if (!loaded_) {
        throw std::runtime_error("machine image not loaded");
    }
    binary_loader_.load(ram_, path.c_str(), addr);
}

void Machine::set_gpr(const std::string& reg_name, uint64_t value) {
    if (!loaded_) {
        throw std::runtime_error("machine image not loaded");
    }
    cpu_.core().write_gpr(parse_gpr_index(reg_name), value);
}

void Machine::attach_storage_image(const std::string& path,
                                   bool ready,
                                   bool valid_magic) {
    bind_block_transport();
    switch (block_transport_) {
    case BlockTransport::SimpleStorage:
        storage_.load_image(path.c_str());
        storage_.set_ready(ready);
        storage_.set_magic_valid(valid_magic);
        break;
    case BlockTransport::VirtioBlk:
        if (!ready || !valid_magic) {
            throw std::runtime_error("virtio-blk transport does not support simple_storage readiness or magic overrides");
        }
        virtio_blk_.load_image(path.c_str());
        break;
    }
}

void Machine::step() {
    if (!loaded_) {
        throw std::runtime_error("machine image not loaded");
    }
    if (!backend_) {
        throw std::runtime_error("execution backend not initialized");
    }
    if (!cpu_.core().halted()) {
        backend_->step();
    }
}

void Machine::run() {
    if (!loaded_) {
        throw std::runtime_error("machine image not loaded");
    }
    if (!backend_) {
        throw std::runtime_error("execution backend not initialized");
    }

    while (!cpu_.core().halted()) {
        step();
    }
}

CPU& Machine::cpu() {
    return cpu_;
}

const CPU& Machine::cpu() const {
    return cpu_;
}

Bus& Machine::bus() {
    return bus_;
}

const Bus& Machine::bus() const {
    return bus_;
}

Uart16550& Machine::uart() {
    return uart_;
}

const Uart16550& Machine::uart() const {
    return uart_;
}

Clint& Machine::clint() {
    return clint_;
}

const Clint& Machine::clint() const {
    return clint_;
}

Plic& Machine::plic() {
    return plic_;
}

const Plic& Machine::plic() const {
    return plic_;
}

BlockTransport Machine::block_transport() const {
    return block_transport_;
}

SimpleStorage& Machine::storage() {
    return storage_;
}

const SimpleStorage& Machine::storage() const {
    return storage_;
}

VirtioBlk& Machine::virtio_blk() {
    return virtio_blk_;
}

const VirtioBlk& Machine::virtio_blk() const {
    return virtio_blk_;
}

AiAccelerator& Machine::ai_accelerator() {
    return ai_accelerator_;
}

const AiAccelerator& Machine::ai_accelerator() const {
    return ai_accelerator_;
}

ExecutionBackend& Machine::backend() {
    if (!backend_) {
        throw std::runtime_error("execution backend not initialized");
    }
    return *backend_;
}

const ExecutionBackend& Machine::backend() const {
    if (!backend_) {
        throw std::runtime_error("execution backend not initialized");
    }
    return *backend_;
}

bool Machine::loaded() const {
    return loaded_;
}
