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
    std::unique_ptr<ExecutionBackend> backend;

    switch (backend_kind_) {
    case BackendKind::Functional:
        backend = std::make_unique<FunctionalBackend>(cpu_, bus_);
        break;
    case BackendKind::Pipeline:
        backend = std::make_unique<PipelineBackend>(cpu_, bus_);
        break;
    }

    backend_ = std::move(backend);
}

void Machine::prepare_for_load() {
    ram_.reset();
    plic_.reset();
    uart_.reset();
    storage_.reset();
    clint_.reset();
    bus_.clear_last_access();
    if (!storage_image_path_.empty()) {
        storage_.load_image(storage_image_path_.c_str());
    }
}

void Machine::load_elf(const std::string& path) {
    prepare_for_load();
    const uint64_t entry = elf_loader_.load(ram_, path.c_str());
    cpu_init(cpu_, entry);
    rebuild_backend();
    loaded_image_path_ = path;
    loaded_flat_binary_ = false;
    loaded_binary_addr_ = 0;
    loaded_ = true;
}

void Machine::load_binary(const std::string& path, uint64_t addr) {
    prepare_for_load();
    binary_loader_.load(ram_, path.c_str(), addr);
    cpu_init(cpu_, addr);
    rebuild_backend();
    loaded_image_path_ = path;
    loaded_flat_binary_ = true;
    loaded_binary_addr_ = addr;
    loaded_ = true;
}

void Machine::attach_storage_image(const std::string& path) {
    storage_image_path_ = path;
    storage_.load_image(path.c_str());
}

void Machine::clear_storage_image() {
    storage_image_path_.clear();
    storage_.reset();
}

void Machine::reset_loaded_image() {
    if (!loaded_) {
        throw std::runtime_error("machine image not loaded");
    }
    if (loaded_flat_binary_) {
        load_binary(loaded_image_path_, loaded_binary_addr_);
        return;
    }
    load_elf(loaded_image_path_);
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

bool Machine::loaded() const {
    return loaded_;
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
