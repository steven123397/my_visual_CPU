#include "plic.h"

Plic::Plic() : Device(PLIC_BASE, PLIC_SIZE) {
    claimed_by_.fill(kUnclaimedContext);
}

uint64_t Plic::load(uint64_t addr, int size) {
    if (size != 4) {
        invalid_access(addr, size);
    }

    const uint32_t offset = static_cast<uint32_t>(addr - PLIC_BASE);

    if (offset == PLIC_PRIORITY_OFFSET(PLIC_SOURCE_UART_THRE)) {
        return priorities_[1];
    }
    if (offset == kPendingOffset) {
        return pending_[1] ? (1U << 1) : 0;
    }
    if (offset == kMachineEnableOffset) {
        return machine_context_.enables;
    }
    if (offset == kSupervisorEnableOffset) {
        return supervisor_context_.enables;
    }
    if (offset == kMachineContextOffset) {
        return machine_context_.threshold;
    }
    if (offset == kSupervisorContextOffset) {
        return supervisor_context_.threshold;
    }
    if (offset == kMachineContextOffset + 4) {
        return claim(PLIC_CONTEXT_MACHINE, machine_context_);
    }
    if (offset == kSupervisorContextOffset + 4) {
        return claim(PLIC_CONTEXT_SUPERVISOR, supervisor_context_);
    }

    invalid_access(addr, size);
}

void Plic::store(uint64_t addr, uint64_t value, int size) {
    if (size != 4) {
        invalid_access(addr, size);
    }

    const uint32_t offset = static_cast<uint32_t>(addr - PLIC_BASE);
    const uint32_t value32 = static_cast<uint32_t>(value);

    if (offset == PLIC_PRIORITY_OFFSET(PLIC_SOURCE_UART_THRE)) {
        priorities_[1] = value32 & 0x7;
        return;
    }
    if (offset == kMachineEnableOffset) {
        machine_context_.enables = value32;
        return;
    }
    if (offset == kSupervisorEnableOffset) {
        supervisor_context_.enables = value32;
        return;
    }
    if (offset == kMachineContextOffset) {
        machine_context_.threshold = value32 & 0x7;
        return;
    }
    if (offset == kSupervisorContextOffset) {
        supervisor_context_.threshold = value32 & 0x7;
        return;
    }
    if (offset == kMachineContextOffset + 4 || offset == kSupervisorContextOffset + 4) {
        const uint32_t context_id =
            (offset == kMachineContextOffset + 4) ? PLIC_CONTEXT_MACHINE : PLIC_CONTEXT_SUPERVISOR;
        complete(context_id, value32);
        return;
    }

    invalid_access(addr, size);
}

PlatformEvents Plic::peek_events() const {
    return PlatformEvents{
        .machine_external_interrupt_pending = context_has_pending(machine_context_),
        .supervisor_external_interrupt_pending = context_has_pending(supervisor_context_),
    };
}

PlatformEvents Plic::tick() {
    return peek_events();
}

void Plic::set_source_level(uint32_t source_id, bool asserted) {
    if (source_id == 0 || source_id > kNumSources) {
        return;
    }

    levels_[source_id] = asserted;
    if (asserted && claimed_by_[source_id] == kUnclaimedContext) {
        pending_[source_id] = true;
    }
}

uint32_t Plic::priority(uint32_t source_id) const {
    if (source_id == 0 || source_id > kNumSources) {
        return 0;
    }
    return priorities_[source_id];
}

bool Plic::source_level(uint32_t source_id) const {
    if (source_id == 0 || source_id > kNumSources) {
        return false;
    }
    return levels_[source_id];
}

bool Plic::source_pending(uint32_t source_id) const {
    if (source_id == 0 || source_id > kNumSources) {
        return false;
    }
    return pending_[source_id];
}

bool Plic::source_claimed(uint32_t source_id) const {
    if (source_id == 0 || source_id > kNumSources) {
        return false;
    }
    return claimed_by_[source_id] != kUnclaimedContext;
}

uint32_t Plic::machine_enables() const {
    return machine_context_.enables;
}

uint32_t Plic::supervisor_enables() const {
    return supervisor_context_.enables;
}

uint32_t Plic::machine_threshold() const {
    return machine_context_.threshold;
}

uint32_t Plic::supervisor_threshold() const {
    return supervisor_context_.threshold;
}

bool Plic::machine_has_pending() const {
    return context_has_pending(machine_context_);
}

bool Plic::supervisor_has_pending() const {
    return context_has_pending(supervisor_context_);
}

uint32_t Plic::best_pending_source(const ContextState& context) const {
    uint32_t best_source = 0;
    uint32_t best_priority = 0;

    for (uint32_t source_id = 1; source_id <= kNumSources; ++source_id) {
        const uint32_t source_bit = 1U << source_id;
        const uint32_t priority = priorities_[source_id];
        if (!pending_[source_id] || claimed_by_[source_id] != kUnclaimedContext || (context.enables & source_bit) == 0) {
            continue;
        }
        if (priority == 0 || priority <= context.threshold) {
            continue;
        }
        if (priority > best_priority) {
            best_source = source_id;
            best_priority = priority;
        }
    }

    return best_source;
}

bool Plic::context_has_pending(const ContextState& context) const {
    return best_pending_source(context) != 0;
}

uint32_t Plic::claim(uint32_t context_id, ContextState& context) {
    const uint32_t source_id = best_pending_source(context);
    if (source_id == 0) {
        return 0;
    }

    pending_[source_id] = false;
    claimed_by_[source_id] = static_cast<uint8_t>(context_id);
    return source_id;
}

void Plic::complete(uint32_t context_id, uint32_t source_id) {
    if (source_id == 0 || source_id > kNumSources) {
        return;
    }

    if (claimed_by_[source_id] != static_cast<uint8_t>(context_id)) {
        return;
    }

    claimed_by_[source_id] = kUnclaimedContext;
    pending_[source_id] = levels_[source_id];
}
