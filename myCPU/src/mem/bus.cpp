#include "bus.h"

extern "C" {
#include "../memory.h"
}

Bus::Bus(Ram& ram) : ram_(ram) {}

uint64_t Bus::load(uint64_t addr, int size) {
    return mem_read(ram_.raw(), addr, size);
}

void Bus::store(uint64_t addr, uint64_t value, int size) {
    mem_write(ram_.raw(), addr, value, size);
}

void Bus::load_binary(const char* path, uint64_t addr) {
    mem_load_binary(ram_.raw(), path, addr);
}

Memory* Bus::raw_memory() {
    return ram_.raw();
}

const Memory* Bus::raw_memory() const {
    return ram_.raw();
}
