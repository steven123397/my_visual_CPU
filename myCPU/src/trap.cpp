#include "trap.h"

namespace {

constexpr uint64_t CAUSE_INT_BIT = 1ULL << 63;
constexpr uint64_t CAUSE_SUPERVISOR_EXTERNAL_INT = CAUSE_INT_BIT | 9ULL;
constexpr uint64_t CAUSE_SUPERVISOR_TIMER_INT = CAUSE_INT_BIT | 5ULL;
constexpr uint64_t CAUSE_MACHINE_EXTERNAL_INT = CAUSE_INT_BIT | 11ULL;
constexpr uint64_t CAUSE_MACHINE_TIMER_INT = CAUSE_INT_BIT | 7ULL;

struct PendingInterrupt {
    bool valid{false};
    uint64_t cause{0};
    uint64_t mip_mask{0};
};

uint64_t interrupt_cause_code(uint64_t cause) {
    return cause & ~CAUSE_INT_BIT;
}

bool is_interrupt_cause(uint64_t cause) {
    return (cause & CAUSE_INT_BIT) != 0;
}

bool ranges_overlap(uint64_t lhs_addr, int lhs_size, uint64_t rhs_addr, int rhs_size) {
    return lhs_addr < rhs_addr + static_cast<uint64_t>(rhs_size) &&
           rhs_addr < lhs_addr + static_cast<uint64_t>(lhs_size);
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

PendingInterrupt select_serviceable_interrupt(const CoreState& core, const CsrFile& csr) {
    const uint64_t mstatus = csr.read(CSR_MSTATUS, core);
    const uint64_t mie = csr.read(CSR_MIE, core);
    const uint64_t mip = csr.read(CSR_MIP, core);
    const uint64_t mideleg = csr.read(CSR_MIDELEG, core);

    if ((mie & MIE_MEIE) && (mip & MIE_MEIE) && machine_interrupts_enabled(core, mstatus)) {
        return PendingInterrupt{true, CAUSE_MACHINE_EXTERNAL_INT, MIE_MEIE};
    }

    if ((mie & MIE_MTIE) && (mip & MIE_MTIE) && machine_interrupts_enabled(core, mstatus)) {
        return PendingInterrupt{true, CAUSE_MACHINE_TIMER_INT, MIE_MTIE};
    }

    if ((mideleg & MIE_SEIE) && (mie & MIE_SEIE) && (mip & MIE_SEIE) &&
        supervisor_interrupts_enabled(core, mstatus)) {
        return PendingInterrupt{true, CAUSE_SUPERVISOR_EXTERNAL_INT, MIE_SEIE};
    }

    if ((mideleg & MIE_STIE) && (mie & MIE_STIE) && (mip & MIE_STIE) &&
        supervisor_interrupts_enabled(core, mstatus)) {
        return PendingInterrupt{true, CAUSE_SUPERVISOR_TIMER_INT, MIE_STIE};
    }

    return {};
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
    if (next_mode != PrivilegeMode::Machine) {
        mstatus &= ~MSTATUS_MPRV;
    }
    csr_.write(CSR_MSTATUS, mstatus, core_);

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
    mstatus &= ~MSTATUS_MPRV;
    csr_.write(CSR_MSTATUS, mstatus, core_);

    core_.set_privilege_mode(next_mode);
    core_.set_pc(csr_.read(CSR_SEPC, core_));
}

void TrapController::sync_platform_events(const PlatformEvents& events) {
    if (events.timer_interrupt_pending) {
        raise_timer_interrupt();
    } else if (timer_pending_mask_ != 0) {
        const uint64_t mip = csr_.read(CSR_MIP, core_);
        csr_.write(CSR_MIP, mip & ~timer_pending_mask_, core_);
        timer_pending_mask_ = 0;
    }
    sync_external_interrupts(events);
}

void TrapController::handle_platform_events(const PlatformEvents& events) {
    sync_platform_events(events);
    service_pending_interrupts();
}

void TrapController::sync_external_interrupts(const PlatformEvents& events) {
    uint64_t mip = csr_.read(CSR_MIP, core_);

    if (events.machine_external_interrupt_pending) {
        mip |= MIE_MEIE;
    } else {
        mip &= ~MIE_MEIE;
    }

    if (events.supervisor_external_interrupt_pending) {
        mip |= MIE_SEIE;
    } else {
        mip &= ~MIE_SEIE;
    }

    csr_.write(CSR_MIP, mip, core_);
}

void TrapController::raise_timer_interrupt() {
    const uint64_t mideleg = csr_.read(CSR_MIDELEG, core_);
    const uint64_t mip = csr_.read(CSR_MIP, core_);
    if (mideleg & MIE_STIE) {
        csr_.write(CSR_MIP, (mip | MIE_STIE) & ~MIE_MTIE, core_);
        timer_pending_mask_ = MIE_STIE;
        return;
    }
    csr_.write(CSR_MIP, (mip | MIE_MTIE) & ~MIE_STIE, core_);
    timer_pending_mask_ = MIE_MTIE;
}

bool TrapController::has_serviceable_interrupt() const {
    return select_serviceable_interrupt(core_, csr_).valid;
}

bool TrapController::service_pending_interrupts() {
    const PendingInterrupt pending = select_serviceable_interrupt(core_, csr_);
    if (!pending.valid) {
        return false;
    }

    const uint64_t mip = csr_.read(CSR_MIP, core_);
    csr_.write(CSR_MIP, mip & ~pending.mip_mask, core_);
    enter_interrupt(pending.cause);
    return true;
}

void TrapController::clear_reservation() {
    reservation_valid_ = false;
    reservation_paddr_ = 0;
    reservation_size_ = 0;
}

void TrapController::set_reservation(uint64_t paddr, int size) {
    reservation_valid_ = true;
    reservation_paddr_ = paddr;
    reservation_size_ = size;
}

bool TrapController::reservation_matches(uint64_t paddr, int size) const {
    return reservation_valid_ &&
           reservation_paddr_ == paddr &&
           reservation_size_ == size;
}

void TrapController::invalidate_reservation(uint64_t paddr, int size) {
    if (!reservation_valid_) {
        return;
    }
    if (!ranges_overlap(reservation_paddr_, reservation_size_, paddr, size)) {
        return;
    }
    clear_reservation();
}

void TrapController::enter_trap(uint64_t cause, uint64_t tval) {
    clear_reservation();
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
        csr_.write(CSR_MSTATUS, mstatus, core_);

        csr_.write(CSR_SEPC, core_.pc(), core_);
        csr_.write(CSR_SCAUSE, cause, core_);
        csr_.write(CSR_STVAL, tval, core_);

        core_.set_privilege_mode(PrivilegeMode::Supervisor);
        core_.set_pc(trap_vector_base(csr_.read(CSR_STVEC, core_), cause));
        return;
    }

    uint64_t mstatus = csr_.read(CSR_MSTATUS, core_);
    const uint64_t mie = (mstatus >> 3) & 1ULL;
    mstatus = (mstatus & ~MSTATUS_MPIE) | (mie << 7);
    mstatus &= ~MSTATUS_MIE;
    mstatus = (mstatus & ~MSTATUS_MPP_MASK) | (encode_privilege_mode(core_.privilege_mode()) << MSTATUS_MPP_SHIFT);
    csr_.write(CSR_MSTATUS, mstatus, core_);

    csr_.write(CSR_MEPC, core_.pc(), core_);
    csr_.write(CSR_MCAUSE, cause, core_);
    csr_.write(CSR_MTVAL, tval, core_);

    core_.set_privilege_mode(PrivilegeMode::Machine);
    core_.set_pc(trap_vector_base(csr_.read(CSR_MTVEC, core_), cause));
}
