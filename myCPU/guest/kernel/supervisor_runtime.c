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
static bool counter_wait_args_valid(const volatile uint32_t* counter,
                                    uint64_t timeout_delta);
static uint64_t counter_wait_deadline(uint64_t timeout_delta);
static bool counter_reached(const volatile uint32_t* counter,
                            uint32_t target_value);
static uint32_t counter_baseline(const volatile uint32_t* counter);
static bool counter_baseline_valid(uint32_t baseline);
static void cleanup_failed_uart_wait(void);
static void cleanup_failed_platform_interrupt_wait(void);
static bool interrupt_wait_args_valid(const volatile uint32_t* timer_counter,
                                      const volatile uint32_t* external_counter,
                                      uint64_t timeout_delta);
static bool interrupt_targets_reached(
    const volatile uint32_t* timer_counter,
    uint32_t timer_target,
    const volatile uint32_t* external_counter,
    uint32_t external_target);

void supervisor_runtime_interrupt_state_configure(
    supervisor_runtime_interrupt_state_t* state,
    uint32_t expected_external_source_id,
    supervisor_runtime_timer_post_handler_t timer_post_handler,
    void* timer_post_context,
    supervisor_runtime_external_post_handler_t external_post_handler,
    void* external_post_context) {
    if (state == NULL) {
        return;
    }

    state->expected_external_source_id = expected_external_source_id;
    state->timer_post_handler = timer_post_handler;
    state->timer_post_context = timer_post_context;
    state->external_post_handler = external_post_handler;
    state->external_post_context = external_post_context;
}

void supervisor_runtime_interrupt_state_set_counters(
    supervisor_runtime_interrupt_state_t* state,
    uint32_t timer_interrupts,
    uint32_t external_interrupts) {
    if (state == NULL) {
        return;
    }

    state->timer_interrupts = timer_interrupts;
    state->external_interrupts = external_interrupts;
}

void supervisor_runtime_interrupt_state_init(
    supervisor_runtime_interrupt_state_t* state) {
    if (state == NULL) {
        return;
    }

    supervisor_runtime_interrupt_state_set_counters(state, 0U, 0U);
    supervisor_runtime_interrupt_state_configure(
        state,
        0U,
        NULL,
        NULL,
        NULL,
        NULL);
}

void supervisor_runtime_interrupt_state_bind_self_handlers(
    supervisor_runtime_interrupt_state_t* state,
    uint32_t expected_external_source_id,
    supervisor_runtime_timer_post_handler_t timer_post_handler,
    supervisor_runtime_external_post_handler_t external_post_handler) {
    if (state == NULL) {
        return;
    }

    supervisor_runtime_interrupt_state_configure(state,
                                                 expected_external_source_id,
                                                 timer_post_handler,
                                                 state,
                                                 external_post_handler,
                                                 state);
}

void supervisor_runtime_interrupt_state_reset_counters(
    supervisor_runtime_interrupt_state_t* state) {
    supervisor_runtime_interrupt_state_set_counters(state, 0U, 0U);
}

uint32_t supervisor_runtime_interrupt_state_timer_interrupts(
    const supervisor_runtime_interrupt_state_t* state) {
    return state != NULL ? state->timer_interrupts : 0U;
}

uint32_t supervisor_runtime_interrupt_state_external_interrupts(
    const supervisor_runtime_interrupt_state_t* state) {
    return state != NULL ? state->external_interrupts : 0U;
}

uint32_t supervisor_runtime_interrupt_state_expected_external_source_id(
    const supervisor_runtime_interrupt_state_t* state) {
    return state != NULL ? state->expected_external_source_id : 0U;
}

bool supervisor_runtime_interrupt_state_timer_delivered(
    const supervisor_runtime_interrupt_state_t* state) {
    return supervisor_runtime_interrupt_state_timer_interrupts(state) != 0U;
}

bool supervisor_runtime_interrupt_state_external_delivered(
    const supervisor_runtime_interrupt_state_t* state) {
    return supervisor_runtime_interrupt_state_external_interrupts(state) != 0U;
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

bool supervisor_runtime_wait_for_first_external_delivery(
    supervisor_runtime_interrupt_state_t* state,
    uint64_t timeout_delta) {
    return state != NULL &&
           supervisor_runtime_enable_uart_thre_and_wait(&state->external_interrupts,
                                                       timeout_delta);
}

bool supervisor_runtime_wait_for_first_timer_delivery(
    supervisor_runtime_interrupt_state_t* state,
    uint64_t timer_delta,
    uint64_t timeout_delta) {
    return state != NULL &&
           supervisor_runtime_schedule_timer_and_wait(&state->timer_interrupts,
                                                     timer_delta,
                                                     timeout_delta);
}

bool supervisor_runtime_wait_for_counter(volatile uint32_t* counter,
                                         uint32_t target_value,
                                         uint64_t timeout_delta) {
    const uint64_t deadline = counter_wait_deadline(timeout_delta);

    if (!counter_wait_args_valid(counter, timeout_delta)) {
        return false;
    }

    while (!counter_reached(counter, target_value)) {
        if (platform_clint_read_mtime() > deadline) {
            return false;
        }
    }

    return true;
}

bool supervisor_runtime_wait_for_next_counter(volatile uint32_t* counter,
                                              uint64_t timeout_delta) {
    const uint32_t baseline = counter_baseline(counter);

    if (!counter_baseline_valid(baseline)) {
        return false;
    }

    return supervisor_runtime_wait_for_counter(counter,
                                               baseline + 1U,
                                               timeout_delta);
}

bool supervisor_runtime_enable_uart_thre_and_wait(
    volatile uint32_t* external_counter,
    uint64_t timeout_delta) {
    const uint32_t baseline = counter_baseline(external_counter);

    if (!counter_baseline_valid(baseline)) {
        return false;
    }

    platform_uart_enable_thre_irq();
    if (supervisor_runtime_wait_for_counter(external_counter,
                                            baseline + 1U,
                                            timeout_delta)) {
        return true;
    }

    cleanup_failed_uart_wait();
    return false;
}

bool supervisor_runtime_schedule_timer_and_wait(
    volatile uint32_t* timer_counter,
    uint64_t timer_delta,
    uint64_t timeout_delta) {
    const uint32_t baseline = counter_baseline(timer_counter);

    if (!counter_baseline_valid(baseline) || timer_delta == 0) {
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

    cleanup_failed_platform_interrupt_wait();
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
    const uint64_t deadline = counter_wait_deadline(timeout_delta);

    if (!interrupt_wait_args_valid(timer_counter,
                                   external_counter,
                                   timeout_delta)) {
        return false;
    }

    while (!interrupt_targets_reached(timer_counter,
                                      timer_target,
                                      external_counter,
                                      external_target)) {
        if (platform_clint_read_mtime() > deadline) {
            return false;
        }
    }

    return true;
}

static bool counter_wait_args_valid(const volatile uint32_t* counter,
                                    uint64_t timeout_delta) {
    return counter != NULL && timeout_delta != 0;
}

static uint64_t counter_wait_deadline(uint64_t timeout_delta) {
    return timeout_delta != 0 ? platform_clint_read_mtime() + timeout_delta : 0;
}

static bool counter_reached(const volatile uint32_t* counter,
                            uint32_t target_value) {
    return counter != NULL && *counter >= target_value;
}

static uint32_t counter_baseline(const volatile uint32_t* counter) {
    return counter != NULL ? *counter : UINT32_MAX;
}

static bool counter_baseline_valid(uint32_t baseline) {
    return baseline != UINT32_MAX;
}

static void cleanup_failed_uart_wait(void) {
    platform_uart_disable_irq();
}

static void cleanup_failed_platform_interrupt_wait(void) {
    platform_uart_disable_irq();
    timer_handle_interrupt();
}

static bool interrupt_wait_args_valid(const volatile uint32_t* timer_counter,
                                      const volatile uint32_t* external_counter,
                                      uint64_t timeout_delta) {
    return timer_counter != NULL && external_counter != NULL &&
           timeout_delta != 0;
}

static bool interrupt_targets_reached(
    const volatile uint32_t* timer_counter,
    uint32_t timer_target,
    const volatile uint32_t* external_counter,
    uint32_t external_target) {
    return counter_reached(timer_counter, timer_target) &&
           counter_reached(external_counter, external_target);
}
