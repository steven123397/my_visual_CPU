#include "machine.h"

#include <memory>
#include <stdexcept>

#include "../exec/functional_backend.h"
#include "../exec/pipeline_backend.h"

Machine::Machine() : uart_(plic_), bus_(ram_) {
    cpu_.csr().bind_clint(&clint_);
    bus_.attach(uart_);
    bus_.attach(storage_);
    bus_.attach(clint_);
    bus_.attach(plic_);
    rebuild_backend();
}

void Machine::set_backend_kind(BackendKind kind) {
    backend_kind_ = kind;
    rebuild_backend();
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
    bus_.try_store(STORAGE_BASE + STORAGE_REG_COMMAND, STORAGE_CMD_NONE, 8);
    cpu_init(cpu_, entry);
    rebuild_backend();
    loaded_ = true;
}

void Machine::load_elf(const std::string& path) {
    Ram staged_ram;
    const uint64_t entry = elf_loader_.load(staged_ram, path.c_str());
    finish_image_load(entry, staged_ram);
}

void Machine::load_binary(const std::string& path, uint64_t addr) {
    Ram staged_ram;
    binary_loader_.load(staged_ram, path.c_str(), addr);
    finish_image_load(addr, staged_ram);
}

void Machine::attach_storage_image(const std::string& path,
                                   bool ready,
                                   bool valid_magic) {
    storage_.load_image(path.c_str());
    storage_.set_ready(ready);
    storage_.set_magic_valid(valid_magic);
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

SimpleStorage& Machine::storage() {
    return storage_;
}

const SimpleStorage& Machine::storage() const {
    return storage_;
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
