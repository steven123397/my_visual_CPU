#include "bus.h"

#include <exception>
#include <stdexcept>

#include "ram.h"

namespace {

bool ranges_overlap(const Device& lhs, const Device& rhs) {
    return lhs.base() < rhs.end() && rhs.base() < lhs.end();
}

void record_unmapped_access(DebugBusAccess& access, bool write, uint64_t addr, uint64_t value, int size) {
    access.valid = true;
    access.success = false;
    access.write = write;
    access.mmio = false;
    access.addr = addr;
    access.value = value;
    access.size = size;
    access.device = "<unmapped>";
    access.detail = "no device mapped for access";
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
            record_access(*device, true, false, addr, value, size, "");
            return true;
        } catch (const std::exception& ex) {
            record_access(*device, false, false, addr, 0, size, ex.what());
            value = 0;
            return false;
        }
    }
    record_unmapped_access(last_access_, false, addr, 0, size);
    value = 0;
    return false;
}

bool Bus::try_store(uint64_t addr, uint64_t value, int size) {
    if (Device* device = find_device(addr, size)) {
        try {
            device->store(addr, value, size);
            record_access(*device, true, true, addr, value, size, "");
            return true;
        } catch (const std::exception& ex) {
            record_access(*device, false, true, addr, value, size, ex.what());
            return false;
        }
    }
    record_unmapped_access(last_access_, true, addr, value, size);
    return false;
}

PlatformEvents Bus::tick() {
    PlatformEvents events;
    for (Device* device : devices_) {
        events.merge(device->tick());
    }
    return events;
}

PlatformEvents Bus::peek_events() const {
    PlatformEvents events;
    for (const Device* device : devices_) {
        events.merge(device->peek_events());
    }
    return events;
}

const DebugBusAccess& Bus::last_access() const {
    return last_access_;
}

void Bus::record_access(
    const Device& device,
    bool success,
    bool write,
    uint64_t addr,
    uint64_t value,
    int size,
    const char* detail) {
    last_access_.valid = true;
    last_access_.success = success;
    last_access_.write = write;
    last_access_.mmio = device.is_mmio();
    last_access_.addr = addr;
    last_access_.value = value;
    last_access_.size = size;
    last_access_.device = device.debug_name();
    last_access_.detail = detail != nullptr ? detail : "";
}
