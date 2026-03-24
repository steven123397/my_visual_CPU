#include "bus.h"

#include <exception>
#include <stdexcept>

#include "ram.h"

namespace {

bool ranges_overlap(const Device& lhs, const Device& rhs) {
    return lhs.base() < rhs.end() && rhs.base() < lhs.end();
}

}  // namespace

Bus::Bus(Ram& ram) {
    attach(ram);
}

void Bus::attach(Device& device) {
    for (Device* existing : devices_) {
        if (ranges_overlap(*existing, device)) {
            throw std::runtime_error("device address range overlap");
        }
    }
    devices_.push_back(&device);
}

Device* Bus::find_device(uint64_t addr, int size) {
    if (size <= 0) {
        return nullptr;
    }
    for (Device* device : devices_) {
        if (device->contains(addr, static_cast<uint64_t>(size))) {
            return device;
        }
    }
    return nullptr;
}

bool Bus::try_load(uint64_t addr, int size, uint64_t& value) {
    if (Device* device = find_device(addr, size)) {
        try {
            value = device->load(addr, size);
            return true;
        } catch (const std::exception&) {
        }
    }
    value = 0;
    return false;
}

bool Bus::try_store(uint64_t addr, uint64_t value, int size) {
    if (Device* device = find_device(addr, size)) {
        try {
            device->store(addr, value, size);
            return true;
        } catch (const std::exception&) {
        }
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
