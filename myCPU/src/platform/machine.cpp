#include "machine.h"

#include <memory>
#include <stdexcept>

#include "../exec/functional_backend.h"

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
        throw std::runtime_error("pipeline backend not integrated yet");
    }
}

void Machine::load_elf(const std::string& path) {
    const uint64_t entry = elf_loader_.load(ram_, path.c_str());
    cpu_init(cpu_, entry);
    rebuild_backend();
    loaded_ = true;
}

void Machine::load_binary(const std::string& path, uint64_t addr) {
    binary_loader_.load(ram_, path.c_str(), addr);
    cpu_init(cpu_, addr);
    rebuild_backend();
    loaded_ = true;
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
