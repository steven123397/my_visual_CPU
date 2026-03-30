#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../guest/include/supervisor_runtime.h"
#include "../../guest/include/trap.h"

static uint64_t g_mtime = 0;
static int g_uart_enable_calls = 0;
static int g_uart_disable_calls = 0;
static int g_timer_schedule_calls = 0;
static int g_timer_cancel_calls = 0;
static uint64_t g_timer_schedule_delta = 0;
static bool g_auto_deliver_interrupts = false;
static uint64_t g_auto_deliver_at_mtime = 0;
static supervisor_runtime_interrupt_state_t* g_delivery_state = NULL;
static volatile uint32_t* g_auto_counter = NULL;
static uint32_t g_auto_counter_value = 0;
static uint64_t g_auto_counter_at_mtime = 0;
static trap_context_t* g_timer_policy_trap_context = NULL;
static trap_interrupt_handler_t g_timer_policy_handler = NULL;
static void* g_timer_policy_context = NULL;
static bool g_timer_policy_result = true;
static trap_context_t* g_external_policy_trap_context = NULL;
static trap_supervisor_external_post_handler_t g_external_policy_handler = NULL;
static void* g_external_policy_context = NULL;
static bool g_external_policy_result = true;

static void reset_stub_state(void);
static int fail(const char* message);
static void stub_timer_post_handler(void* context);
static void stub_external_post_handler(uint32_t source_id, void* context);
static int test_bind_self_handlers(void);
static int test_external_policy_adapter(void);
static int test_interrupt_policy_adapter(void);
static int test_counter_wait_helpers(void);
static int test_uart_and_timer_wait_wrappers(void);
static int test_schedule_platform_interrupts_success(void);
static int test_schedule_platform_interrupts_timeout_cleanup(void);
static int test_wait_timeout_and_cancel(void);

void platform_uart_enable_thre_irq(void) {
    g_uart_enable_calls += 1;
}

void platform_uart_disable_irq(void) {
    g_uart_disable_calls += 1;
}

uint64_t platform_clint_read_mtime(void) {
    const uint64_t value = g_mtime;

    if (g_auto_deliver_interrupts && g_delivery_state != NULL &&
        value >= g_auto_deliver_at_mtime) {
        g_delivery_state->timer_interrupts = 1U;
        g_delivery_state->external_interrupts = 1U;
    }
    if (g_auto_counter != NULL && value >= g_auto_counter_at_mtime) {
        *g_auto_counter = g_auto_counter_value;
    }

    g_mtime = value + 1U;
    return value;
}

void timer_schedule_delta(uint64_t delta) {
    g_timer_schedule_calls += 1;
    g_timer_schedule_delta = delta;
}

void timer_handle_interrupt(void) {
    g_timer_cancel_calls += 1;
}

void panic_shutdown(void) {
    fail("panic_shutdown should not be called");
    abort();
}

bool trap_context_install_supervisor_timer_policy(
    trap_context_t* trap_context,
    trap_interrupt_handler_t post_handler,
    void* post_context) {
    g_timer_policy_trap_context = trap_context;
    g_timer_policy_handler = post_handler;
    g_timer_policy_context = post_context;
    return g_timer_policy_result;
}

bool trap_context_install_supervisor_external_policy(
    trap_context_t* trap_context,
    trap_supervisor_external_post_handler_t post_handler,
    void* post_context) {
    g_external_policy_trap_context = trap_context;
    g_external_policy_handler = post_handler;
    g_external_policy_context = post_context;
    return g_external_policy_result;
}

static void reset_stub_state(void) {
    g_mtime = 0;
    g_uart_enable_calls = 0;
    g_uart_disable_calls = 0;
    g_timer_schedule_calls = 0;
    g_timer_cancel_calls = 0;
    g_timer_schedule_delta = 0;
    g_auto_deliver_interrupts = false;
    g_auto_deliver_at_mtime = 0;
    g_delivery_state = NULL;
    g_auto_counter = NULL;
    g_auto_counter_value = 0;
    g_auto_counter_at_mtime = 0;
    g_timer_policy_trap_context = NULL;
    g_timer_policy_handler = NULL;
    g_timer_policy_context = NULL;
    g_timer_policy_result = true;
    g_external_policy_trap_context = NULL;
    g_external_policy_handler = NULL;
    g_external_policy_context = NULL;
    g_external_policy_result = true;
}

static int fail(const char* message) {
    fprintf(stderr, "%s\n", message);
    return 1;
}

static void stub_timer_post_handler(void* context) {
    (void)context;
}

static void stub_external_post_handler(uint32_t source_id, void* context) {
    (void)source_id;
    (void)context;
}

static int test_bind_self_handlers(void) {
    supervisor_runtime_interrupt_state_t state;

    supervisor_runtime_interrupt_state_init(&state);
    supervisor_runtime_interrupt_state_bind_self_handlers(
        &state, 7U, stub_timer_post_handler, stub_external_post_handler);

    if (state.expected_external_source_id != 7U ||
        state.timer_post_handler != stub_timer_post_handler ||
        state.timer_post_context != &state ||
        state.external_post_handler != stub_external_post_handler ||
        state.external_post_context != &state) {
        return fail("expected self handler binding to populate interrupt state");
    }

    return 0;
}

static int test_external_policy_adapter(void) {
    trap_context_t trap_context;
    supervisor_runtime_interrupt_state_t state;

    reset_stub_state();
    if (!supervisor_runtime_install_external_counter_policy_adapter(
            &trap_context, &state)) {
        return fail("expected external policy adapter to succeed");
    }

    if (g_external_policy_trap_context != &trap_context ||
        g_external_policy_handler !=
            supervisor_runtime_external_counter_post_handler ||
        g_external_policy_context != &state) {
        return fail("expected external policy adapter to forward state");
    }

    reset_stub_state();
    g_external_policy_result = false;
    if (supervisor_runtime_install_external_counter_policy_adapter(&trap_context,
                                                                  &state)) {
        return fail("expected external policy adapter failure to propagate");
    }

    return 0;
}

static int test_interrupt_policy_adapter(void) {
    trap_context_t trap_context;
    supervisor_runtime_interrupt_state_t state;

    reset_stub_state();
    if (!supervisor_runtime_install_interrupt_counter_policies_adapter(
            &trap_context, &state)) {
        return fail("expected interrupt policy adapter to succeed");
    }

    if (g_timer_policy_trap_context != &trap_context ||
        g_timer_policy_handler != supervisor_runtime_timer_counter_post_handler ||
        g_timer_policy_context != &state ||
        g_external_policy_trap_context != &trap_context ||
        g_external_policy_handler !=
            supervisor_runtime_external_counter_post_handler ||
        g_external_policy_context != &state) {
        return fail("expected interrupt policy adapter to install both policies");
    }

    reset_stub_state();
    g_external_policy_result = false;
    if (supervisor_runtime_install_interrupt_counter_policies_adapter(
            &trap_context, &state)) {
        return fail("expected combined policy adapter failure to propagate");
    }

    return 0;
}

static int test_counter_wait_helpers(void) {
    uint32_t counter = 1U;

    reset_stub_state();
    g_auto_counter = &counter;
    g_auto_counter_value = 2U;
    g_auto_counter_at_mtime = 2U;
    if (!supervisor_runtime_wait_for_counter(&counter, 2U, 8U)) {
        return fail("expected counter wait to observe delivered target");
    }

    counter = 3U;
    reset_stub_state();
    g_auto_counter = &counter;
    g_auto_counter_value = 4U;
    g_auto_counter_at_mtime = 1U;
    if (!supervisor_runtime_wait_for_next_counter(&counter, 1U)) {
        return fail("expected next-counter wait to observe delivered increment");
    }

    counter = 0U;
    reset_stub_state();
    if (supervisor_runtime_wait_for_counter(&counter, 1U, 1U) ||
        supervisor_runtime_wait_for_next_counter(NULL, 4U)) {
        return fail("expected counter wait helpers to reject timeout/null cases");
    }

    return 0;
}

static int test_uart_and_timer_wait_wrappers(void) {
    uint32_t external_counter = 0U;
    uint32_t timer_counter = 0U;

    reset_stub_state();
    g_auto_counter = &external_counter;
    g_auto_counter_value = 1U;
    g_auto_counter_at_mtime = 1U;
    if (!supervisor_runtime_enable_uart_thre_and_wait(&external_counter, 8U) ||
        g_uart_enable_calls != 1 || g_uart_disable_calls != 0) {
        return fail("expected UART wait wrapper to enable and observe interrupt");
    }

    reset_stub_state();
    if (supervisor_runtime_enable_uart_thre_and_wait(&external_counter, 1U) ||
        g_uart_enable_calls != 1 || g_uart_disable_calls != 1) {
        return fail("expected UART wait timeout to disable IRQ on failure");
    }

    reset_stub_state();
    g_auto_counter = &timer_counter;
    g_auto_counter_value = 1U;
    g_auto_counter_at_mtime = 1U;
    if (!supervisor_runtime_schedule_timer_and_wait(&timer_counter, 6U, 8U) ||
        g_timer_schedule_calls != 1 || g_timer_schedule_delta != 6U) {
        return fail("expected timer wait wrapper to schedule requested delta");
    }

    reset_stub_state();
    if (supervisor_runtime_schedule_timer_and_wait(&timer_counter, 0U, 4U)) {
        return fail("expected timer wait wrapper to reject zero timer delta");
    }

    return 0;
}

static int test_schedule_platform_interrupts_success(void) {
    supervisor_runtime_interrupt_state_t state;

    supervisor_runtime_interrupt_state_init(&state);
    state.timer_interrupts = 4U;
    state.external_interrupts = 5U;

    reset_stub_state();
    g_auto_deliver_interrupts = true;
    g_auto_deliver_at_mtime = 2U;
    g_delivery_state = &state;
    if (!supervisor_runtime_schedule_platform_interrupts_and_wait(&state,
                                                                  8U,
                                                                  32U)) {
        return fail("expected shared platform interrupt wait to succeed");
    }

    if (state.timer_interrupts != 1U || state.external_interrupts != 1U ||
        g_timer_schedule_calls != 1 || g_timer_schedule_delta != 8U ||
        g_uart_enable_calls != 1 || g_uart_disable_calls != 0 ||
        g_timer_cancel_calls != 0) {
        return fail("unexpected success-path platform interrupt state");
    }

    return 0;
}

static int test_schedule_platform_interrupts_timeout_cleanup(void) {
    supervisor_runtime_interrupt_state_t state;

    supervisor_runtime_interrupt_state_init(&state);
    state.timer_interrupts = 2U;
    state.external_interrupts = 3U;

    reset_stub_state();
    if (supervisor_runtime_schedule_platform_interrupts_and_wait(&state,
                                                                 4U,
                                                                 3U)) {
        return fail("expected shared platform interrupt wait to time out");
    }

    if (state.timer_interrupts != 0U || state.external_interrupts != 0U ||
        g_timer_schedule_calls != 1 || g_timer_schedule_delta != 4U ||
        g_uart_enable_calls != 1 || g_uart_disable_calls != 1 ||
        g_timer_cancel_calls != 1) {
        return fail("expected timeout cleanup to disable UART and cancel timer");
    }

    return 0;
}

static int test_wait_timeout_and_cancel(void) {
    reset_stub_state();
    if (!supervisor_runtime_wait_timeout(2U) ||
        supervisor_runtime_wait_timeout(0U)) {
        return fail("expected timeout wait helper to respect zero/non-zero delta");
    }

    supervisor_runtime_cancel_timer_delivery();
    if (g_timer_cancel_calls != 1) {
        return fail("expected timer delivery cancel helper to forward interrupt clear");
    }

    return 0;
}

int main(void) {
    if (test_bind_self_handlers() != 0 ||
        test_external_policy_adapter() != 0 ||
        test_interrupt_policy_adapter() != 0 ||
        test_counter_wait_helpers() != 0 ||
        test_uart_and_timer_wait_wrappers() != 0 ||
        test_schedule_platform_interrupts_success() != 0 ||
        test_schedule_platform_interrupts_timeout_cleanup() != 0 ||
        test_wait_timeout_and_cancel() != 0) {
        return 1;
    }

    return 0;
}
