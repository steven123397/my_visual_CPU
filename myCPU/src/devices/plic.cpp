#include "plic.h"

Plic::Plic() : Device(PLIC_BASE, PLIC_SIZE) {}

uint64_t Plic::load(uint64_t addr, int /*size*/) {
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
        return claim(machine_context_);
    }
    if (offset == kSupervisorContextOffset + 4) {
        return claim(supervisor_context_);
    }

    return 0;
}

void Plic::store(uint64_t addr, uint64_t value, int /*size*/) {
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
        complete(value32);
    }
}

PlatformEvents Plic::tick() {
    return PlatformEvents{
        .machine_external_interrupt_pending = context_has_pending(machine_context_),
        .supervisor_external_interrupt_pending = context_has_pending(supervisor_context_),
    };
}

const char* Plic::debug_name() const {
    return "plic";
}

void Plic::set_source_level(uint32_t source_id, bool asserted) {
    if (source_id == 0 || source_id > kNumSources) {
        return;
    }

    levels_[source_id] = asserted;
    if (asserted && !claimed_[source_id]) {
        pending_[source_id] = true;
    }
}

void Plic::reset() {
    priorities_.fill(0);
    levels_.fill(false);
    pending_.fill(false);
    claimed_.fill(false);
    machine_context_ = {};
    supervisor_context_ = {};
}

uint32_t Plic::priority(uint32_t source_id) const {
    return source_id <= kNumSources ? priorities_[source_id] : 0;
}

bool Plic::source_level(uint32_t source_id) const {
    return source_id <= kNumSources ? levels_[source_id] : false;
}

bool Plic::source_pending(uint32_t source_id) const {
    return source_id <= kNumSources ? pending_[source_id] : false;
}

bool Plic::source_claimed(uint32_t source_id) const {
    return source_id <= kNumSources ? claimed_[source_id] : false;
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
        if (!pending_[source_id] || claimed_[source_id] || (context.enables & source_bit) == 0) {
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

uint32_t Plic::claim(ContextState& context) {
    const uint32_t source_id = best_pending_source(context);
    if (source_id == 0) {
        return 0;
    }

    pending_[source_id] = false;
    claimed_[source_id] = true;
    return source_id;
}

void Plic::complete(uint32_t source_id) {
    if (source_id == 0 || source_id > kNumSources || !claimed_[source_id]) {
        return;
    }

    claimed_[source_id] = false;
    pending_[source_id] = levels_[source_id];
}
