#pragma once

#include <cstdint>

#include "../platform/platform_events.h"

class Device {
public:
    Device(uint64_t base, uint64_t size)
        : base_(base), size_(size) {}

    virtual ~Device() = default;

    bool contains(uint64_t addr, uint64_t size) const {
        if (addr < base_) {
            return false;
        }

        const uint64_t offset = addr - base_;
        if (offset > size_) {
            return false;
        }

        return size <= (size_ - offset);
    }

    virtual uint64_t load(uint64_t addr, int size) = 0;
    virtual void store(uint64_t addr, uint64_t value, int size) = 0;
    virtual PlatformEvents tick() {
        return {};
    }

private:
    uint64_t base_;
    uint64_t size_;
};
