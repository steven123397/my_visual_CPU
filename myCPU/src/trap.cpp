#include "trap.h"

namespace {

constexpr uint64_t CAUSE_INT_BIT = 1ULL << 63;
constexpr uint64_t CAUSE_TIMER_INT = 0x8000000000000007ULL;

}  // namespace

TrapController::TrapController(CoreState& core, CsrFile& csr)
    : core_(core), csr_(csr) {}

void TrapController::enter_exception(uint64_t cause, uint64_t tval) {
    enter_trap(cause, tval);
}

void TrapController::enter_interrupt(uint64_t cause) {
    enter_trap(cause, 0);
}

void TrapController::return_from_mret() {
    uint64_t mstatus = csr_.read(CSR_MSTATUS, core_);
    const uint64_t mpie = (mstatus >> 7) & 1ULL;
    mstatus = (mstatus & ~MSTATUS_MIE) | (mpie << 3);
    mstatus |= MSTATUS_MPIE;
    csr_.write(CSR_MSTATUS, mstatus);

    core_.set_pc(csr_.read(CSR_MEPC, core_));
}

void TrapController::raise_timer_interrupt() {
    const uint64_t mip = csr_.read(CSR_MIP, core_);
    csr_.write(CSR_MIP, mip | MIE_MTIE);
}

void TrapController::service_pending_interrupts() {
    if (!(csr_.read(CSR_MSTATUS, core_) & MSTATUS_MIE)) {
        return;
    }

    const uint64_t mie = csr_.read(CSR_MIE, core_);
    const uint64_t mip = csr_.read(CSR_MIP, core_);
    if ((mie & MIE_MTIE) && (mip & MIE_MTIE)) {
        csr_.write(CSR_MIP, mip & ~MIE_MTIE);
        enter_interrupt(CAUSE_TIMER_INT);
    }
}

void TrapController::enter_trap(uint64_t cause, uint64_t tval) {
    uint64_t mstatus = csr_.read(CSR_MSTATUS, core_);
    const uint64_t mie = (mstatus >> 3) & 1ULL;
    mstatus = (mstatus & ~MSTATUS_MPIE) | (mie << 7);
    mstatus &= ~MSTATUS_MIE;
    csr_.write(CSR_MSTATUS, mstatus);

    csr_.write(CSR_MEPC, core_.pc());
    csr_.write(CSR_MCAUSE, cause);
    csr_.write(CSR_MTVAL, tval);

    const uint64_t mtvec = csr_.read(CSR_MTVEC, core_);
    if ((mtvec & 3ULL) == 1 && (cause & CAUSE_INT_BIT)) {
        core_.set_pc((mtvec & ~3ULL) + 4 * (cause & ~CAUSE_INT_BIT));
    } else {
        core_.set_pc(mtvec & ~3ULL);
    }
}
