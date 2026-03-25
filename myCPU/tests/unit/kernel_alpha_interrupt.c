#include <stdbool.h>
#include <setjmp.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../guest/include/kernel_alpha.h"
#include "../../include/platform_mmio.h"

static kernel_runtime_t* g_bringup_runtime = NULL;
static const kernel_bringup_options_t* g_bringup_options = NULL;
static bool g_bringup_result = true;
static int g_begin_plic_calls = 0;
static kernel_runtime_t* g_external_wait_runtime = NULL;
static uint64_t g_external_wait_timeout = 0;
static bool g_external_wait_result = true;
static kernel_runtime_t* g_timer_wait_runtime = NULL;
static uint64_t g_timer_wait_delta = 0;
static uint64_t g_timer_wait_timeout = 0;
static bool g_timer_wait_result = true;
static int g_cancel_timer_calls = 0;
static uint64_t g_wait_timeout_delta = 0;
static bool g_wait_timeout_result = true;
static int g_console_chars[16];
static size_t g_console_char_count = 0;
static int g_uart_disable_calls = 0;
static int g_panic_calls = 0;
static bool g_expect_panic = false;
static jmp_buf g_panic_jmp;

static void reset_stub_state(void);
static int fail(const char* message);
static int test_run_interrupt_bringup(void);
static int test_complete_platform_interrupt_readiness(void);
static int test_plic_not_ready_contract(void);
static int test_timer_not_ready_contract(void);
static int test_run_fault_bringup(void);
static int test_ready_and_panic_post_handlers(void);
static bool stub_external_policy_setup(trap_context_t* trap_context, void* context);
static bool stub_interrupt_policy_setup(trap_context_t* trap_context, void* context);

bool kernel_runtime_run_common_bringup(kernel_runtime_t* runtime,
                                       const kernel_bringup_options_t* options) {
    g_bringup_runtime = runtime;
    g_bringup_options = options;
    return g_bringup_result;
}

void kernel_alpha_begin_plic_supervisor_phase(void) {
    g_begin_plic_calls += 1;
}

bool kernel_alpha_wait_for_first_external_delivery(kernel_runtime_t* runtime,
                                                   uint64_t timeout_delta) {
    g_external_wait_runtime = runtime;
    g_external_wait_timeout = timeout_delta;
    return g_external_wait_result;
}

bool kernel_alpha_wait_for_first_timer_delivery(kernel_runtime_t* runtime,
                                                uint64_t timer_delta,
                                                uint64_t timeout_delta) {
    g_timer_wait_runtime = runtime;
    g_timer_wait_delta = timer_delta;
    g_timer_wait_timeout = timeout_delta;
    return g_timer_wait_result;
}

void supervisor_runtime_cancel_timer_delivery(void) {
    g_cancel_timer_calls += 1;
}

bool supervisor_runtime_wait_timeout(uint64_t timeout_delta) {
    g_wait_timeout_delta = timeout_delta;
    return g_wait_timeout_result;
}

void console_putc(char ch) {
    if (g_console_char_count < (sizeof(g_console_chars) / sizeof(g_console_chars[0]))) {
        g_console_chars[g_console_char_count++] = (unsigned char)ch;
    }
}

void platform_uart_disable_irq(void) {
    g_uart_disable_calls += 1;
}

void panic_shutdown(void) {
    g_panic_calls += 1;
    if (g_expect_panic) {
        longjmp(g_panic_jmp, 1);
    }

    abort();
}

static void reset_stub_state(void) {
    g_bringup_runtime = NULL;
    g_bringup_options = NULL;
    g_bringup_result = true;
    g_begin_plic_calls = 0;
    g_external_wait_runtime = NULL;
    g_external_wait_timeout = 0;
    g_external_wait_result = true;
    g_timer_wait_runtime = NULL;
    g_timer_wait_delta = 0;
    g_timer_wait_timeout = 0;
    g_timer_wait_result = true;
    g_cancel_timer_calls = 0;
    g_wait_timeout_delta = 0;
    g_wait_timeout_result = true;
    memset(g_console_chars, 0, sizeof(g_console_chars));
    g_console_char_count = 0;
    g_uart_disable_calls = 0;
    g_panic_calls = 0;
    g_expect_panic = false;
}

static int fail(const char* message) {
    fprintf(stderr, "%s\n", message);
    return 1;
}

static bool stub_external_policy_setup(trap_context_t* trap_context, void* context) {
    (void)trap_context;
    (void)context;
    return true;
}

static bool stub_interrupt_policy_setup(trap_context_t* trap_context, void* context) {
    (void)trap_context;
    (void)context;
    return true;
}

static int test_run_interrupt_bringup(void) {
    kernel_runtime_t runtime;

    reset_stub_state();
    if (!kernel_alpha_run_interrupt_bringup(
            &runtime,
            UINT64_C(0x123456789ABCDEF0),
            stub_external_policy_setup)) {
        return fail("expected interrupt bring-up helper to succeed");
    }

    if (g_bringup_runtime != &runtime || g_bringup_options == NULL ||
        g_bringup_options->mmio_mask !=
            (KERNEL_ALPHA_MMIO_UART | KERNEL_ALPHA_MMIO_CLINT |
             KERNEL_ALPHA_MMIO_PLIC) ||
        g_bringup_options->pmm_probe_marker != UINT64_C(0x123456789ABCDEF0) ||
        g_bringup_options->pre_vm_setup != stub_external_policy_setup ||
        g_bringup_options->pre_vm_context != &runtime) {
        return fail("expected interrupt bring-up helper to forward runtime options");
    }

    reset_stub_state();
    g_bringup_result = false;
    if (kernel_alpha_run_interrupt_bringup(
            &runtime,
            1U,
            stub_interrupt_policy_setup)) {
        return fail("expected interrupt bring-up helper to propagate failure");
    }

    return 0;
}

static int test_complete_platform_interrupt_readiness(void) {
    kernel_runtime_t runtime;

    reset_stub_state();
    runtime.interrupts.external_interrupts = 2U;
    runtime.interrupts.timer_interrupts = 3U;
    if (!kernel_alpha_complete_platform_interrupt_readiness(&runtime, 8U, 64U)) {
        return fail("expected platform interrupt readiness helper to succeed");
    }

    if (g_begin_plic_calls != 1 || g_external_wait_runtime != &runtime ||
        g_external_wait_timeout != 64U || g_timer_wait_runtime != &runtime ||
        g_timer_wait_delta != 8U || g_timer_wait_timeout != 64U) {
        return fail("expected platform interrupt readiness helper to run P/E/T phases");
    }

    reset_stub_state();
    g_external_wait_result = false;
    if (kernel_alpha_complete_platform_interrupt_readiness(&runtime, 4U, 32U)) {
        return fail("expected platform interrupt readiness helper to stop on external wait failure");
    }

    reset_stub_state();
    g_timer_wait_result = false;
    if (kernel_alpha_complete_platform_interrupt_readiness(&runtime, 4U, 32U)) {
        return fail("expected platform interrupt readiness helper to stop on timer wait failure");
    }

    return 0;
}

static int test_plic_not_ready_contract(void) {
    kernel_runtime_t runtime;

    reset_stub_state();
    runtime.interrupts.external_interrupts = 0U;
    g_external_wait_result = false;
    if (!kernel_alpha_validate_plic_not_ready_contract(&runtime, 48U, 'P')) {
        return fail("expected plic-not-ready helper to accept timeout with zero deliveries");
    }

    if (g_external_wait_runtime != &runtime || g_external_wait_timeout != 48U ||
        g_console_char_count != 1 ||
        g_console_chars[0] != 'P') {
        return fail("expected plic-not-ready helper to wait and print marker");
    }

    reset_stub_state();
    runtime.interrupts.external_interrupts = 1U;
    g_external_wait_result = false;
    if (kernel_alpha_validate_plic_not_ready_contract(&runtime, 48U, 'P')) {
        return fail("expected plic-not-ready helper to reject stray interrupt deliveries");
    }

    reset_stub_state();
    g_external_wait_result = true;
    if (kernel_alpha_validate_plic_not_ready_contract(&runtime, 48U, 'P')) {
        return fail("expected plic-not-ready helper to reject unexpected success");
    }

    return 0;
}

static int test_timer_not_ready_contract(void) {
    reset_stub_state();
    if (!kernel_alpha_validate_timer_not_ready_contract(96U, 'T')) {
        return fail("expected timer-not-ready helper to accept timeout");
    }

    if (g_cancel_timer_calls != 1 || g_wait_timeout_delta != 96U ||
        g_console_char_count != 1 || g_console_chars[0] != 'T') {
        return fail("expected timer-not-ready helper to cancel timer and print marker");
    }

    reset_stub_state();
    g_wait_timeout_result = false;
    if (kernel_alpha_validate_timer_not_ready_contract(24U, 'T')) {
        return fail("expected timer-not-ready helper to propagate timeout wait failure");
    }

    return 0;
}

static int test_run_fault_bringup(void) {
    kernel_runtime_t runtime;

    reset_stub_state();
    if (!kernel_alpha_run_fault_bringup(&runtime)) {
        return fail("expected fault bring-up helper to succeed");
    }

    if (g_bringup_runtime != &runtime || g_bringup_options == NULL ||
        g_bringup_options->mmio_mask != KERNEL_ALPHA_MMIO_UART ||
        g_bringup_options->pmm_probe_marker != 0 ||
        g_bringup_options->pre_vm_setup != NULL ||
        g_bringup_options->pre_vm_context != NULL) {
        return fail("expected fault bring-up helper to forward UART-only options");
    }

    reset_stub_state();
    g_bringup_result = false;
    if (kernel_alpha_run_fault_bringup(&runtime)) {
        return fail("expected fault bring-up helper to propagate failure");
    }

    return 0;
}

static int test_ready_and_panic_post_handlers(void) {
    reset_stub_state();
    kernel_alpha_timer_post_handler_emit_ready((void*)1);
    if (g_console_char_count != 1 || g_console_chars[0] != 'T' ||
        g_panic_calls != 0) {
        return fail("expected ready timer post handler to print T");
    }

    reset_stub_state();
    kernel_alpha_external_post_handler_emit_ready(PLIC_SOURCE_UART_THRE, (void*)1);
    if (g_uart_disable_calls != 1 || g_console_char_count != 1 ||
        g_console_chars[0] != 'E' || g_panic_calls != 0) {
        return fail("expected ready external post handler to disable IRQ and print E");
    }

    reset_stub_state();
    g_expect_panic = true;
    if (setjmp(g_panic_jmp) == 0) {
        kernel_alpha_timer_post_handler_panic((void*)1);
        return fail("expected panic timer post handler to jump out");
    }
    g_expect_panic = false;
    if (g_panic_calls != 1) {
        return fail("expected panic timer post handler to panic exactly once");
    }

    reset_stub_state();
    g_expect_panic = true;
    if (setjmp(g_panic_jmp) == 0) {
        kernel_alpha_external_post_handler_panic_on_delivery(PLIC_SOURCE_UART_THRE,
                                                             (void*)1);
        return fail("expected panic external post handler to jump out");
    }
    g_expect_panic = false;
    if (g_uart_disable_calls != 1 || g_panic_calls != 1) {
        return fail("expected panic external post handler to disable IRQ then panic");
    }

    return 0;
}

int main(void) {
    if (test_run_interrupt_bringup() != 0 ||
        test_complete_platform_interrupt_readiness() != 0 ||
        test_plic_not_ready_contract() != 0 ||
        test_timer_not_ready_contract() != 0 ||
        test_run_fault_bringup() != 0 ||
        test_ready_and_panic_post_handlers() != 0) {
        return 1;
    }

    return 0;
}
