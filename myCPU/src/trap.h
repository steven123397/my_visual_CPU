#pragma once

#include <cstdint>

#include "arch/core_state.h"
#include "arch/csr_file.h"
#include "platform/platform_events.h"

class TrapController {
public:
    TrapController(CoreState& core, CsrFile& csr);

    void enter_exception(uint64_t cause, uint64_t tval);
    void enter_interrupt(uint64_t cause);
    void return_from_mret();
    void return_from_sret();
    void sync_platform_events(const PlatformEvents& events);
    void handle_platform_events(const PlatformEvents& events);
    void raise_timer_interrupt();
    bool has_serviceable_interrupt() const;
    bool service_pending_interrupts();

private:
    void enter_trap(uint64_t cause, uint64_t tval);
    void sync_external_interrupts(const PlatformEvents& events);

    CoreState& core_;
    CsrFile& csr_;
    uint64_t timer_pending_mask_{0};
};
