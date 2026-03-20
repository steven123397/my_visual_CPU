#include "clint.h"

Clint::Clint() : Device(CLINT_BASE, CLINT_SIZE) {}

uint64_t Clint::load(uint64_t addr, int /*size*/) {
    const uint64_t offset = addr - CLINT_BASE;
    if (offset == 0xBFF8) {
        return mtime_;
    }
    if (offset == 0x4000) {
        return mtimecmp_;
    }
    return 0;
}

void Clint::store(uint64_t addr, uint64_t value, int /*size*/) {
    const uint64_t offset = addr - CLINT_BASE;
    if (offset == 0xBFF8) {
        mtime_ = value;
    }
    if (offset == 0x4000) {
        mtimecmp_ = value;
    }
}

PlatformEvents Clint::tick() {
    ++mtime_;
    return PlatformEvents{
        .timer_interrupt_pending = (mtime_ >= mtimecmp_),
    };
}
