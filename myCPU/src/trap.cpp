#include "trap.h"

namespace {

constexpr uint64_t CAUSE_INT_BIT = 1ULL << 63;
constexpr uint64_t CAUSE_SUPERVISOR_TIMER_INT = CAUSE_INT_BIT | 5ULL;
constexpr uint64_t CAUSE_MACHINE_TIMER_INT = CAUSE_INT_BIT | 7ULL;

uint64_t interrupt_cause_code(uint64_t cause) {
    return cause & ~CAUSE_INT_BIT;
}

bool is_interrupt_cause(uint64_t cause) {
    return (cause & CAUSE_INT_BIT) != 0;
}

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

bool machine_interrupts_enabled(const CoreState& core, uint64_t mstatus) {
    switch (core.privilege_mode()) {
    case PrivilegeMode::Machine:
        return (mstatus & MSTATUS_MIE) != 0;
    case PrivilegeMode::Supervisor:
    case PrivilegeMode::User:
        return true;
    }
    return false;
}

bool supervisor_interrupts_enabled(const CoreState& core, uint64_t mstatus) {
    switch (core.privilege_mode()) {
    case PrivilegeMode::Machine:
        return false;
    case PrivilegeMode::Supervisor:
        return (mstatus & MSTATUS_SIE) != 0;
    case PrivilegeMode::User:
        return true;
    }
    return false;
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
    const uint64_t mideleg = csr_.read(CSR_MIDELEG, core_);
    const uint64_t mip = csr_.read(CSR_MIP, core_);
    if (mideleg & MIE_STIE) {
        csr_.write(CSR_MIP, (mip | MIE_STIE) & ~MIE_MTIE);
        return;
    }
    csr_.write(CSR_MIP, (mip | MIE_MTIE) & ~MIE_STIE);
}

void TrapController::service_pending_interrupts() {
    const uint64_t mstatus = csr_.read(CSR_MSTATUS, core_);
    const uint64_t mie = csr_.read(CSR_MIE, core_);
    const uint64_t mip = csr_.read(CSR_MIP, core_);
    const uint64_t mideleg = csr_.read(CSR_MIDELEG, core_);

    if ((mie & MIE_MTIE) && (mip & MIE_MTIE) && machine_interrupts_enabled(core_, mstatus)) {
        csr_.write(CSR_MIP, mip & ~MIE_MTIE);
        enter_interrupt(CAUSE_MACHINE_TIMER_INT);
        return;
    }

    if ((mideleg & MIE_STIE) && (mie & MIE_STIE) && (mip & MIE_STIE) && supervisor_interrupts_enabled(core_, mstatus)) {
        csr_.write(CSR_MIP, mip & ~MIE_STIE);
        enter_interrupt(CAUSE_SUPERVISOR_TIMER_INT);
    }
}

void TrapController::enter_trap(uint64_t cause, uint64_t tval) {
    const bool delegated_to_supervisor =
        core_.privilege_mode() != PrivilegeMode::Machine &&
        ((is_interrupt_cause(cause) &&
          (csr_.read(CSR_MIDELEG, core_) & (1ULL << interrupt_cause_code(cause)))) ||
         (!is_interrupt_cause(cause) &&
          (csr_.read(CSR_MEDELEG, core_) & (1ULL << cause))));

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
