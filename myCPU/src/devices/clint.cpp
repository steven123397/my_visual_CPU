#include "clint.h"

bool Clint::contains(uint64_t addr) const {
    return addr >= CLINT_BASE && addr < CLINT_BASE + CLINT_SIZE;
}

uint64_t Clint::load(uint64_t addr, int /*size*/) const {
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

void Clint::tick() {
    ++mtime_;
}

bool Clint::timer_pending() const {
    return mtime_ >= mtimecmp_;
}
