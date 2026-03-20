#include "bus.h"

Bus::Bus(Ram& ram, Uart16550& uart, Clint& clint)
    : ram_(ram), uart_(uart), clint_(clint) {}

uint64_t Bus::load(uint64_t addr, int size) {
    if (uart_.contains(addr)) {
        return uart_.load(addr, size);
    }
    if (clint_.contains(addr)) {
        return clint_.load(addr, size);
    }
    return ram_.load(addr, size);
}

void Bus::store(uint64_t addr, uint64_t value, int size) {
    if (uart_.contains(addr)) {
        uart_.store(addr, value, size);
        return;
    }
    if (clint_.contains(addr)) {
        clint_.store(addr, value, size);
        return;
    }
    ram_.store(addr, value, size);
}

void Bus::load_binary(const char* path, uint64_t addr) {
    ram_.load_binary(path, addr);
}

void Bus::tick() {
    clint_.tick();
}

bool Bus::timer_pending() const {
    return clint_.timer_pending();
}

Memory* Bus::raw_ram() {
    return ram_.raw_memory();
}

const Memory* Bus::raw_ram() const {
    return ram_.raw_memory();
}
