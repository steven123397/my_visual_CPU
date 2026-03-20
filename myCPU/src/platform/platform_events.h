#pragma once

struct PlatformEvents {
    bool timer_interrupt_pending{false};

    void merge(const PlatformEvents& other) {
        timer_interrupt_pending = timer_interrupt_pending || other.timer_interrupt_pending;
    }
};
