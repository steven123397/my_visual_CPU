#include "machine.h"

#include <stdexcept>

Machine::Machine() : bus_(ram_) {
    bus_.attach(uart_);
    bus_.attach(clint_);
}

void Machine::load_elf(const std::string& path) {
    const uint64_t entry = elf_loader_.load(ram_, path.c_str());
    cpu_init(cpu_, entry);
    loaded_ = true;
}

void Machine::load_binary(const std::string& path, uint64_t addr) {
    binary_loader_.load(ram_, path.c_str(), addr);
    cpu_init(cpu_, addr);
    loaded_ = true;
}

void Machine::run() {
    if (!loaded_) {
        throw std::runtime_error("machine image not loaded");
    }

    while (!cpu_.core().halted()) {
        cpu_step(cpu_, bus_);
    }
}
