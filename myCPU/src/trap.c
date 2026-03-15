#include "trap.h"
#include <stdio.h>

// Interrupt cause bits
#define CAUSE_INT_BIT (1UL << 63)

void trap_enter(CPU *cpu, uint64_t cause, uint64_t tval) {
    uint64_t mstatus = csr_read(cpu, CSR_MSTATUS);
    // Save MIE -> MPIE, clear MIE
    uint64_t mie = (mstatus >> 3) & 1;
    mstatus = (mstatus & ~MSTATUS_MPIE) | (mie << 7);
    mstatus &= ~MSTATUS_MIE;
    csr_write(cpu, CSR_MSTATUS, mstatus);

    csr_write(cpu, CSR_MEPC,   cpu->pc);
    csr_write(cpu, CSR_MCAUSE, cause);
    csr_write(cpu, CSR_MTVAL,  tval);

    uint64_t mtvec = csr_read(cpu, CSR_MTVEC);
    if ((mtvec & 3) == 1 && (cause & CAUSE_INT_BIT)) {
        // Vectored mode for interrupts
        cpu->pc = (mtvec & ~3UL) + 4 * (cause & ~CAUSE_INT_BIT);
    } else {
        cpu->pc = mtvec & ~3UL;
    }
}

void trap_return(CPU *cpu) {
    uint64_t mstatus = csr_read(cpu, CSR_MSTATUS);
    // Restore MIE from MPIE, set MPIE=1
    uint64_t mpie = (mstatus >> 7) & 1;
    mstatus = (mstatus & ~MSTATUS_MIE) | (mpie << 3);
    mstatus |= MSTATUS_MPIE;
    csr_write(cpu, CSR_MSTATUS, mstatus);

    cpu->pc = csr_read(cpu, CSR_MEPC);
}
