#include "bus.h"

#include <cstdio>

#include "ram.h"

Bus::Bus(Ram& ram) {
    attach(ram);
}

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
    std::fprintf(stderr, "bus load: unmapped addr 0x%lx size %d\n", addr, size);
    return 0;
}

void Bus::store(uint64_t addr, uint64_t value, int size) {
    if (Device* device = find_device(addr)) {
        device->store(addr, value, size);
        return;
    }
    std::fprintf(stderr, "bus store: unmapped addr 0x%lx size %d value 0x%lx\n", addr, size, value);
}

PlatformEvents Bus::tick() {
    PlatformEvents events;
    for (Device* device : devices_) {
        events.merge(device->tick());
    }
    return events;
}
