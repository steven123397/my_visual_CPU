#include "bus.h"

Bus::Bus(Ram& ram, Clint& clint)
    : ram_(ram), clint_(clint) {}

void Bus::attach(Device& device) {
    devices_.push_back(&device);
}

Device* Bus::find_device(uint64_t addr) {
    for (Device* device : devices_) {
        if (device->contains(addr)) {
            return device;
        }
    }
    return nullptr;
}

uint64_t Bus::load(uint64_t addr, int size) {
    if (Device* device = find_device(addr)) {
        return device->load(addr, size);
    }
    return ram_.load(addr, size);
}

void Bus::store(uint64_t addr, uint64_t value, int size) {
    if (Device* device = find_device(addr)) {
        device->store(addr, value, size);
        return;
    }
    ram_.store(addr, value, size);
}

BusTickResult Bus::tick() {
    return BusTickResult{
        .timer_interrupt_pending = clint_.tick(),
    };
}
