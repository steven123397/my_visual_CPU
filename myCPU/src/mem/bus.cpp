#include "bus.h"

#include "ram.h"

Bus::Bus(Ram& ram) {
    attach(ram);
}

void Bus::attach(Device& device) {
    devices_.push_back(&device);
}

Device* Bus::find_device(uint64_t addr, int size) {
    for (Device* device : devices_) {
        if (device->contains(addr, static_cast<uint64_t>(size))) {
            return device;
        }
    }
    return nullptr;
}

bool Bus::try_load(uint64_t addr, int size, uint64_t& value) {
    if (Device* device = find_device(addr, size)) {
        value = device->load(addr, size);
        return true;
    }
    value = 0;
    return false;
}

bool Bus::try_store(uint64_t addr, uint64_t value, int size) {
    if (Device* device = find_device(addr, size)) {
        device->store(addr, value, size);
        return true;
    }
    return false;
}

PlatformEvents Bus::tick() {
    PlatformEvents events;
    for (Device* device : devices_) {
        events.merge(device->tick());
    }
    return events;
}
