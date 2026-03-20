#pragma once

#include <cstdint>

class Device {
public:
    Device(uint64_t base, uint64_t size)
        : base_(base), size_(size) {}

    virtual ~Device() = default;

    bool contains(uint64_t addr) const {
        return addr >= base_ && addr < base_ + size_;
    }

    virtual uint64_t load(uint64_t addr, int size) = 0;
    virtual void store(uint64_t addr, uint64_t value, int size) = 0;

private:
    uint64_t base_;
    uint64_t size_;
};
