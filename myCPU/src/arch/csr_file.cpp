#include "csr_file.h"

#include "core_state.h"

void CsrFile::reset() {
    regs_.fill(0);
}

uint64_t CsrFile::read(uint32_t addr, const CoreState& core) const {
    addr &= 0xFFF;
    if (addr == CSR_CYCLE || addr == CSR_TIME) {
        return core.cycle();
    }
    return regs_[addr];
}

void CsrFile::write(uint32_t addr, uint64_t value) {
    addr &= 0xFFF;
    if (addr == CSR_CYCLE || addr == CSR_TIME) {
        return;
    }
    regs_[addr] = value;
}
