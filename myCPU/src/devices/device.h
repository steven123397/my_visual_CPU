#pragma once

#include <cstdint>
#include <stdexcept>

#include "../platform/platform_events.h"

class Device {
public:
    Device(uint64_t base, uint64_t size)
        : base_(base), size_(size) {}

    virtual ~Device() = default;

    bool contains(uint64_t addr, uint64_t size) const {
        if (size == 0) {
            return false;
        }
        if (addr < base_) {
            return false;
        }

        const uint64_t offset = addr - base_;
        if (offset >= size_) {
            return false;
        }

        return size <= (size_ - offset);
    }

    uint64_t base() const {
        return base_;
    }

    uint64_t size() const {
        return size_;
    }

    uint64_t end() const {
        return base_ + size_;
    }

    virtual uint64_t load(uint64_t addr, int size) = 0;
    virtual void store(uint64_t addr, uint64_t value, int size) = 0;
    virtual PlatformEvents peek_events() const {
        return {};
    }
    virtual PlatformEvents tick() {
        return peek_events();
    }
    virtual const char* debug_name() const {
        return "device";
    }
    virtual bool is_mmio() const {
        return true;
    }

protected:
    [[noreturn]] void invalid_access(uint64_t, int) const {
        throw std::runtime_error("invalid MMIO access");
    }

private:
    uint64_t base_;
    uint64_t size_;
};
