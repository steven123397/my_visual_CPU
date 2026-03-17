#include "trap.h"

namespace {

constexpr uint64_t CAUSE_INT_BIT = 1ULL << 63;

}  // namespace

void trap_enter(CPU& cpu, uint64_t cause, uint64_t tval) {
    CoreState& core = cpu.core();

    uint64_t mstatus = csr_read(cpu, CSR_MSTATUS);
    const uint64_t mie = (mstatus >> 3) & 1ULL;
    mstatus = (mstatus & ~MSTATUS_MPIE) | (mie << 7);
    mstatus &= ~MSTATUS_MIE;
    csr_write(cpu, CSR_MSTATUS, mstatus);

    csr_write(cpu, CSR_MEPC, core.pc());
    csr_write(cpu, CSR_MCAUSE, cause);
    csr_write(cpu, CSR_MTVAL, tval);

    const uint64_t mtvec = csr_read(cpu, CSR_MTVEC);
    if ((mtvec & 3ULL) == 1 && (cause & CAUSE_INT_BIT)) {
        core.set_pc((mtvec & ~3ULL) + 4 * (cause & ~CAUSE_INT_BIT));
    } else {
        core.set_pc(mtvec & ~3ULL);
    }
}

void trap_return(CPU& cpu) {
    CoreState& core = cpu.core();

    uint64_t mstatus = csr_read(cpu, CSR_MSTATUS);
    const uint64_t mpie = (mstatus >> 7) & 1ULL;
    mstatus = (mstatus & ~MSTATUS_MIE) | (mpie << 3);
    mstatus |= MSTATUS_MPIE;
    csr_write(cpu, CSR_MSTATUS, mstatus);

    core.set_pc(csr_read(cpu, CSR_MEPC));
}
