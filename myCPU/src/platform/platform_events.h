#pragma once

struct PlatformEvents {
    bool timer_interrupt_pending{false};
    bool machine_external_interrupt_pending{false};
    bool supervisor_external_interrupt_pending{false};

    void merge(const PlatformEvents& other) {
        timer_interrupt_pending = timer_interrupt_pending || other.timer_interrupt_pending;
        machine_external_interrupt_pending =
            machine_external_interrupt_pending || other.machine_external_interrupt_pending;
        supervisor_external_interrupt_pending =
            supervisor_external_interrupt_pending || other.supervisor_external_interrupt_pending;
    }
};
