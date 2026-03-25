#include "supervisor_runtime.h"

#include <stddef.h>
#include <limits.h>
#include <stdint.h>

#include "panic.h"
#include "platform.h"
#include "riscv.h"
#include "timer.h"
#include "trap.h"

static bool supervisor_runtime_wait_for_interrupts(
    volatile uint32_t* timer_counter,
    uint32_t timer_target,
    volatile uint32_t* external_counter,
    uint32_t external_target,
    uint64_t timeout_delta);

void supervisor_runtime_interrupt_state_init(
    supervisor_runtime_interrupt_state_t* state) {
    if (state == NULL) {
        return;
    }

    state->timer_interrupts = 0;
    state->external_interrupts = 0;
    state->expected_external_source_id = 0;
    state->timer_post_handler = NULL;
    state->timer_post_context = NULL;
    state->external_post_handler = NULL;
    state->external_post_context = NULL;
}

void supervisor_runtime_interrupt_state_bind_self_handlers(
    supervisor_runtime_interrupt_state_t* state,
    uint32_t expected_external_source_id,
    supervisor_runtime_timer_post_handler_t timer_post_handler,
    supervisor_runtime_external_post_handler_t external_post_handler) {
    if (state == NULL) {
        return;
    }

    state->expected_external_source_id = expected_external_source_id;
    state->timer_post_handler = timer_post_handler;
    state->timer_post_context = state;
    state->external_post_handler = external_post_handler;
    state->external_post_context = state;
}

void supervisor_runtime_interrupt_state_reset_counters(
    supervisor_runtime_interrupt_state_t* state) {
    if (state == NULL) {
        return;
    }

    state->timer_interrupts = 0;
    state->external_interrupts = 0;
}

void supervisor_runtime_timer_counter_post_handler(uint64_t cause,
                                                   void* context) {
    supervisor_runtime_interrupt_state_t* state =
        (supervisor_runtime_interrupt_state_t*)context;

    if (cause != RISCV_SUPERVISOR_TIMER_INTERRUPT || state == NULL) {
        panic_shutdown();
    }

    state->timer_interrupts += 1U;
    if (state->timer_post_handler != NULL) {
        state->timer_post_handler(state->timer_post_context);
    }
}

void supervisor_runtime_external_counter_post_handler(uint64_t cause,
                                                      uint32_t source_id,
                                                      void* context) {
    supervisor_runtime_interrupt_state_t* state =
        (supervisor_runtime_interrupt_state_t*)context;

    if (cause != RISCV_SUPERVISOR_EXTERNAL_INTERRUPT || state == NULL ||
        (state->expected_external_source_id != 0 &&
         source_id != state->expected_external_source_id)) {
        panic_shutdown();
    }

    state->external_interrupts += 1U;
    if (state->external_post_handler != NULL) {
        state->external_post_handler(source_id, state->external_post_context);
    }
}

bool supervisor_runtime_install_timer_counter_policy(
    trap_context_t* trap_context,
    supervisor_runtime_interrupt_state_t* state) {
    return trap_context != NULL && state != NULL &&
           trap_context_install_supervisor_timer_policy(
               trap_context,
               supervisor_runtime_timer_counter_post_handler,
               state);
}

bool supervisor_runtime_install_external_counter_policy(
    trap_context_t* trap_context,
    supervisor_runtime_interrupt_state_t* state) {
    return trap_context != NULL && state != NULL &&
           trap_context_install_supervisor_external_policy(
               trap_context,
               supervisor_runtime_external_counter_post_handler,
               state);
}

bool supervisor_runtime_install_external_counter_policy_adapter(
    trap_context_t* trap_context,
    void* context) {
    return supervisor_runtime_install_external_counter_policy(
        trap_context,
        (supervisor_runtime_interrupt_state_t*)context);
}

bool supervisor_runtime_install_interrupt_counter_policies(
    trap_context_t* trap_context,
    supervisor_runtime_interrupt_state_t* state) {
    return supervisor_runtime_install_timer_counter_policy(trap_context, state) &&
           supervisor_runtime_install_external_counter_policy(trap_context, state);
}

bool supervisor_runtime_install_interrupt_counter_policies_adapter(
    trap_context_t* trap_context,
    void* context) {
    return supervisor_runtime_install_interrupt_counter_policies(
        trap_context,
        (supervisor_runtime_interrupt_state_t*)context);
}

bool supervisor_runtime_wait_for_counter(volatile uint32_t* counter,
                                         uint32_t target_value,
                                         uint64_t timeout_delta) {
    const uint64_t deadline =
        counter != NULL && timeout_delta != 0
            ? platform_clint_read_mtime() + timeout_delta
            : 0;

    if (counter == NULL || timeout_delta == 0) {
        return false;
    }

    while (*counter < target_value) {
        if (platform_clint_read_mtime() > deadline) {
            return false;
        }
    }

    return true;
}

bool supervisor_runtime_wait_for_next_counter(volatile uint32_t* counter,
                                              uint64_t timeout_delta) {
    const uint32_t baseline = counter != NULL ? *counter : UINT32_MAX;

    if (baseline == UINT32_MAX) {
        return false;
    }

    return supervisor_runtime_wait_for_counter(counter,
                                               baseline + 1U,
                                               timeout_delta);
}

bool supervisor_runtime_enable_uart_thre_and_wait(
    volatile uint32_t* external_counter,
    uint64_t timeout_delta) {
    const uint32_t baseline =
        external_counter != NULL ? *external_counter : UINT32_MAX;

    if (baseline == UINT32_MAX) {
        return false;
    }

    platform_uart_enable_thre_irq();
    if (supervisor_runtime_wait_for_counter(external_counter,
                                            baseline + 1U,
                                            timeout_delta)) {
        return true;
    }

    platform_uart_disable_irq();
    return false;
}

bool supervisor_runtime_schedule_timer_and_wait(
    volatile uint32_t* timer_counter,
    uint64_t timer_delta,
    uint64_t timeout_delta) {
    const uint32_t baseline = timer_counter != NULL ? *timer_counter : UINT32_MAX;

    if (baseline == UINT32_MAX || timer_delta == 0) {
        return false;
    }

    timer_schedule_delta(timer_delta);
    return supervisor_runtime_wait_for_counter(timer_counter,
                                               baseline + 1U,
                                               timeout_delta);
}

bool supervisor_runtime_schedule_platform_interrupts_and_wait(
    supervisor_runtime_interrupt_state_t* state,
    uint64_t timer_delta,
    uint64_t timeout_delta) {
    if (state == NULL || timer_delta == 0 || timeout_delta == 0) {
        return false;
    }

    supervisor_runtime_interrupt_state_reset_counters(state);
    timer_schedule_delta(timer_delta);
    platform_uart_enable_thre_irq();
    if (supervisor_runtime_wait_for_interrupts(&state->timer_interrupts,
                                               1U,
                                               &state->external_interrupts,
                                               1U,
                                               timeout_delta)) {
        return true;
    }

    platform_uart_disable_irq();
    timer_handle_interrupt();
    return false;
}

bool supervisor_runtime_wait_timeout(uint64_t timeout_delta) {
    const uint64_t deadline =
        timeout_delta != 0 ? platform_clint_read_mtime() + timeout_delta : 0;

    if (timeout_delta == 0) {
        return false;
    }

    while (platform_clint_read_mtime() <= deadline) {
    }

    return true;
}

void supervisor_runtime_cancel_timer_delivery(void) {
    timer_handle_interrupt();
}

static bool supervisor_runtime_wait_for_interrupts(
    volatile uint32_t* timer_counter,
    uint32_t timer_target,
    volatile uint32_t* external_counter,
    uint32_t external_target,
    uint64_t timeout_delta) {
    const uint64_t deadline =
        timer_counter != NULL && external_counter != NULL && timeout_delta != 0
            ? platform_clint_read_mtime() + timeout_delta
            : 0;

    if (timer_counter == NULL || external_counter == NULL || timeout_delta == 0) {
        return false;
    }

    while (*timer_counter < timer_target || *external_counter < external_target) {
        if (platform_clint_read_mtime() > deadline) {
            return false;
        }
    }

    return true;
}
