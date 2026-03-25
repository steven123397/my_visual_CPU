#include "clint.h"

namespace {

constexpr uint64_t kRegisterSize = sizeof(uint64_t);

}

Clint::Clint() : Device(CLINT_BASE, CLINT_SIZE) {}

uint64_t Clint::mask_for_size(int size) {
    switch (size) {
    case 1:
        return 0xFFULL;
    case 2:
        return 0xFFFFULL;
    case 4:
        return 0xFFFFFFFFULL;
    case 8:
        return ~0ULL;
    default:
        return 0;
    }
}

bool Clint::access_in_range(uint64_t offset, uint64_t reg_offset, int size) {
    return size == 1 || size == 2 || size == 4 || size == 8
        ? offset >= reg_offset && offset + static_cast<uint64_t>(size) <= reg_offset + kRegisterSize
        : false;
}

uint64_t Clint::load_register_slice(uint64_t reg, uint64_t reg_offset, uint64_t offset, int size) {
    const uint64_t shift = (offset - reg_offset) * 8;
    return (reg >> shift) & mask_for_size(size);
}

void Clint::store_register_slice(uint64_t& reg, uint64_t reg_offset, uint64_t offset, uint64_t value, int size) {
    const uint64_t shift = (offset - reg_offset) * 8;
    const uint64_t mask = mask_for_size(size) << shift;
    reg = (reg & ~mask) | ((value & mask_for_size(size)) << shift);
}

uint64_t Clint::load(uint64_t addr, int size) {
    const uint64_t offset = addr - CLINT_BASE;
    if (access_in_range(offset, CLINT_REG_MTIME, size)) {
        return load_register_slice(mtime_, CLINT_REG_MTIME, offset, size);
    }
    if (access_in_range(offset, CLINT_REG_MTIMECMP, size)) {
        return load_register_slice(mtimecmp_, CLINT_REG_MTIMECMP, offset, size);
    }

    invalid_access(addr, size);
}

void Clint::store(uint64_t addr, uint64_t value, int size) {
    const uint64_t offset = addr - CLINT_BASE;
    if (access_in_range(offset, CLINT_REG_MTIME, size)) {
        store_register_slice(mtime_, CLINT_REG_MTIME, offset, value, size);
        return;
    }
    if (access_in_range(offset, CLINT_REG_MTIMECMP, size)) {
        store_register_slice(mtimecmp_, CLINT_REG_MTIMECMP, offset, value, size);
        return;
    }

    invalid_access(addr, size);
}

PlatformEvents Clint::tick() {
    ++mtime_;
    return PlatformEvents{
        .timer_interrupt_pending = (mtime_ >= mtimecmp_),
    };
}

uint64_t Clint::mtime() const {
    return mtime_;
}

uint64_t Clint::mtimecmp() const {
    return mtimecmp_;
}

bool Clint::timer_interrupt_pending() const {
    return mtime_ >= mtimecmp_;
}
