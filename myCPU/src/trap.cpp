#include "trap.h"

namespace {

constexpr uint64_t CAUSE_INT_BIT = 1ULL << 63;
constexpr uint64_t CAUSE_TIMER_INT = 0x8000000000000007ULL;

uint64_t encode_privilege_mode(PrivilegeMode mode) {
    return static_cast<uint64_t>(mode);
}

PrivilegeMode decode_privilege_mode(uint64_t encoded) {
    switch (encoded & 0x3ULL) {
    case 0:
        return PrivilegeMode::User;
    case 1:
        return PrivilegeMode::Supervisor;
    case 3:
        return PrivilegeMode::Machine;
    default:
        return PrivilegeMode::Machine;
    }
}

uint64_t trap_vector_base(uint64_t tvec, uint64_t cause) {
    if ((tvec & 3ULL) == 1 && (cause & CAUSE_INT_BIT)) {
        return (tvec & ~3ULL) + 4 * (cause & ~CAUSE_INT_BIT);
    }
    return tvec & ~3ULL;
}

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
    const PrivilegeMode next_mode = decode_privilege_mode((mstatus & MSTATUS_MPP_MASK) >> MSTATUS_MPP_SHIFT);
    mstatus = (mstatus & ~MSTATUS_MIE) | (mpie << 3);
    mstatus |= MSTATUS_MPIE;
    mstatus &= ~MSTATUS_MPP_MASK;
    csr_.write(CSR_MSTATUS, mstatus);

    core_.set_privilege_mode(next_mode);
    core_.set_pc(csr_.read(CSR_MEPC, core_));
}

void TrapController::return_from_sret() {
    uint64_t mstatus = csr_.read(CSR_MSTATUS, core_);
    const uint64_t spie = (mstatus & MSTATUS_SPIE) ? 1ULL : 0ULL;
    const PrivilegeMode next_mode = (mstatus & MSTATUS_SPP) ? PrivilegeMode::Supervisor : PrivilegeMode::User;
    mstatus = (mstatus & ~MSTATUS_SIE) | (spie << 1);
    mstatus |= MSTATUS_SPIE;
    mstatus &= ~MSTATUS_SPP;
    csr_.write(CSR_MSTATUS, mstatus);

    core_.set_privilege_mode(next_mode);
    core_.set_pc(csr_.read(CSR_SEPC, core_));
}

void TrapController::handle_platform_events(const PlatformEvents& events) {
    if (events.timer_interrupt_pending) {
        raise_timer_interrupt();
    }
    service_pending_interrupts();
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
    const bool delegated_to_supervisor =
        core_.privilege_mode() != PrivilegeMode::Machine &&
        !(cause & CAUSE_INT_BIT) &&
        (csr_.read(CSR_MEDELEG, core_) & (1ULL << cause));

    if (delegated_to_supervisor) {
        uint64_t mstatus = csr_.read(CSR_MSTATUS, core_);
        const uint64_t sie = (mstatus & MSTATUS_SIE) ? 1ULL : 0ULL;
        mstatus = (mstatus & ~MSTATUS_SPIE) | (sie << 5);
        mstatus &= ~MSTATUS_SIE;
        if (core_.privilege_mode() == PrivilegeMode::Supervisor) {
            mstatus |= MSTATUS_SPP;
        } else {
            mstatus &= ~MSTATUS_SPP;
        }
        csr_.write(CSR_MSTATUS, mstatus);

        csr_.write(CSR_SEPC, core_.pc());
        csr_.write(CSR_SCAUSE, cause);
        csr_.write(CSR_STVAL, tval);

        core_.set_privilege_mode(PrivilegeMode::Supervisor);
        core_.set_pc(trap_vector_base(csr_.read(CSR_STVEC, core_), cause));
        return;
    }

    uint64_t mstatus = csr_.read(CSR_MSTATUS, core_);
    const uint64_t mie = (mstatus >> 3) & 1ULL;
    mstatus = (mstatus & ~MSTATUS_MPIE) | (mie << 7);
    mstatus &= ~MSTATUS_MIE;
    mstatus = (mstatus & ~MSTATUS_MPP_MASK) | (encode_privilege_mode(core_.privilege_mode()) << MSTATUS_MPP_SHIFT);
    csr_.write(CSR_MSTATUS, mstatus);

    csr_.write(CSR_MEPC, core_.pc());
    csr_.write(CSR_MCAUSE, cause);
    csr_.write(CSR_MTVAL, tval);

    core_.set_privilege_mode(PrivilegeMode::Machine);
    core_.set_pc(trap_vector_base(csr_.read(CSR_MTVEC, core_), cause));
}
