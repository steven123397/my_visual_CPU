#include "csr_file.h"

#include "core_state.h"

namespace {

constexpr uint64_t SSTATUS_MASK = MSTATUS_SIE | MSTATUS_SPIE | MSTATUS_SPP;
constexpr uint64_t SIE_MASK = MIE_SSIE | MIE_STIE | MIE_SEIE;

bool is_supported_csr(uint32_t addr) {
    switch (addr & 0xFFF) {
    case CSR_SSTATUS:
    case CSR_SIE:
    case CSR_STVEC:
    case CSR_SSCRATCH:
    case CSR_SEPC:
    case CSR_SCAUSE:
    case CSR_STVAL:
    case CSR_SIP:
    case CSR_MSTATUS:
    case CSR_MISA:
    case CSR_MEDELEG:
    case CSR_MIDELEG:
    case CSR_MIE:
    case CSR_MTVEC:
    case CSR_MSCRATCH:
    case CSR_MEPC:
    case CSR_MCAUSE:
    case CSR_MTVAL:
    case CSR_MIP:
    case CSR_CYCLE:
    case CSR_TIME:
        return true;
    default:
        return false;
    }
}

}

void CsrFile::reset() {
    regs_.fill(0);
}

uint64_t CsrFile::read(uint32_t addr, const CoreState& core) const {
    addr &= 0xFFF;
    if (addr == CSR_CYCLE || addr == CSR_TIME) {
        return core.cycle();
    }
    if (addr == CSR_SSTATUS) {
        return regs_[CSR_MSTATUS] & SSTATUS_MASK;
    }
    if (addr == CSR_SIE) {
        return regs_[CSR_MIE] & SIE_MASK;
    }
    if (addr == CSR_SIP) {
        return regs_[CSR_MIP] & SIE_MASK;
    }
    return regs_[addr];
}

void CsrFile::write(uint32_t addr, uint64_t value) {
    addr &= 0xFFF;
    if (addr == CSR_CYCLE || addr == CSR_TIME) {
        return;
    }
    if (addr == CSR_SSTATUS) {
        regs_[CSR_MSTATUS] = (regs_[CSR_MSTATUS] & ~SSTATUS_MASK) | (value & SSTATUS_MASK);
        return;
    }
    if (addr == CSR_SIE) {
        regs_[CSR_MIE] = (regs_[CSR_MIE] & ~SIE_MASK) | (value & SIE_MASK);
        return;
    }
    if (addr == CSR_SIP) {
        regs_[CSR_MIP] = (regs_[CSR_MIP] & ~SIE_MASK) | (value & SIE_MASK);
        return;
    }
    regs_[addr] = value;
}

bool CsrFile::is_implemented(uint32_t addr) const {
    return is_supported_csr(addr);
}
