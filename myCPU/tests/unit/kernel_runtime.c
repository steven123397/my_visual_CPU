#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "../../guest/include/kernel_bringup.h"
#include "../../guest/include/kernel_runtime.h"
#include "../../guest/include/supervisor_runtime.h"

static trap_context_t* g_external_policy_trap_context = NULL;
static supervisor_runtime_interrupt_state_t* g_external_policy_state = NULL;
static bool g_external_policy_result = true;
static trap_context_t* g_interrupt_policies_trap_context = NULL;
static supervisor_runtime_interrupt_state_t* g_interrupt_policies_state = NULL;
static bool g_interrupt_policies_result = true;
static trap_context_t* g_common_bringup_trap_context = NULL;
static vm_address_space_t** g_common_bringup_out_space = NULL;
static const kernel_bringup_options_t* g_common_bringup_options = NULL;
static vm_address_space_t* g_common_bringup_address_space = NULL;
static bool g_common_bringup_result = true;

static void reset_stub_state(void);
static int fail(const char* message);
static void stub_timer_post_handler(void* context);
static void stub_external_post_handler(uint32_t source_id, void* context);
static int test_runtime_init_and_bind_self_handlers(void);
static int test_external_policy_adapter(void);
static int test_interrupt_policy_adapter(void);
static int test_common_bringup_wrapper(void);

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

bool supervisor_runtime_install_external_counter_policy(
    trap_context_t* trap_context,
    supervisor_runtime_interrupt_state_t* state) {
    g_external_policy_trap_context = trap_context;
    g_external_policy_state = state;
    return g_external_policy_result;
}

bool supervisor_runtime_install_interrupt_counter_policies(
    trap_context_t* trap_context,
    supervisor_runtime_interrupt_state_t* state) {
    g_interrupt_policies_trap_context = trap_context;
    g_interrupt_policies_state = state;
    return g_interrupt_policies_result;
}

bool kernel_bringup_run_common(
    trap_context_t* trap_context,
    vm_address_space_t** out_space,
    const kernel_bringup_options_t* options) {
    g_common_bringup_trap_context = trap_context;
    g_common_bringup_out_space = out_space;
    g_common_bringup_options = options;
    if (g_common_bringup_result && out_space != NULL) {
        *out_space = g_common_bringup_address_space;
    }
    return g_common_bringup_result;
}

static void reset_stub_state(void) {
    g_external_policy_trap_context = NULL;
    g_external_policy_state = NULL;
    g_external_policy_result = true;
    g_interrupt_policies_trap_context = NULL;
    g_interrupt_policies_state = NULL;
    g_interrupt_policies_result = true;
    g_common_bringup_trap_context = NULL;
    g_common_bringup_out_space = NULL;
    g_common_bringup_options = NULL;
    g_common_bringup_address_space = (vm_address_space_t*)(uintptr_t)0x1234U;
    g_common_bringup_result = true;
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

static int test_runtime_init_and_bind_self_handlers(void) {
    kernel_runtime_t runtime;

    kernel_runtime_init(&runtime);
    if (kernel_runtime_address_space(&runtime) != NULL) {
        return fail("expected runtime init to clear address space");
    }

    if (!kernel_runtime_bind_self_interrupt_handlers(
            &runtime,
            7U,
            stub_timer_post_handler,
            stub_external_post_handler)) {
        return fail("expected runtime self handler binding to succeed");
    }

    if (runtime.interrupts.expected_external_source_id != 7U ||
        runtime.interrupts.timer_post_handler != stub_timer_post_handler ||
        runtime.interrupts.timer_post_context != &runtime.interrupts ||
        runtime.interrupts.external_post_handler != stub_external_post_handler ||
        runtime.interrupts.external_post_context != &runtime.interrupts) {
        return fail("expected runtime to bind self interrupt handlers");
    }

    if (kernel_runtime_bind_self_interrupt_handlers(
            NULL, 0, stub_timer_post_handler, stub_external_post_handler)) {
        return fail("expected null runtime self binding to fail");
    }

    return 0;
}

static int test_external_policy_adapter(void) {
    kernel_runtime_t runtime;
    trap_context_t trap_context;

    reset_stub_state();
    kernel_runtime_init(&runtime);
    if (!kernel_runtime_install_external_counter_policy_adapter(&trap_context,
                                                               &runtime)) {
        return fail("expected runtime external policy adapter to succeed");
    }

    if (g_external_policy_trap_context != &trap_context ||
        g_external_policy_state != &runtime.interrupts) {
        return fail("expected runtime external policy adapter to forward state");
    }

    reset_stub_state();
    kernel_runtime_init(&runtime);
    g_external_policy_result = false;
    if (kernel_runtime_install_external_counter_policy_adapter(&trap_context,
                                                               &runtime)) {
        return fail("expected runtime external policy adapter failure to propagate");
    }

    return 0;
}

static int test_interrupt_policy_adapter(void) {
    kernel_runtime_t runtime;
    trap_context_t trap_context;

    reset_stub_state();
    kernel_runtime_init(&runtime);
    if (!kernel_runtime_install_interrupt_counter_policies_adapter(
            &trap_context, &runtime)) {
        return fail("expected runtime interrupt policy adapter to succeed");
    }

    if (g_interrupt_policies_trap_context != &trap_context ||
        g_interrupt_policies_state != &runtime.interrupts) {
        return fail("expected runtime interrupt policy adapter to forward state");
    }

    reset_stub_state();
    kernel_runtime_init(&runtime);
    g_interrupt_policies_result = false;
    if (kernel_runtime_install_interrupt_counter_policies_adapter(
            &trap_context, &runtime)) {
        return fail("expected runtime interrupt policy adapter failure to propagate");
    }

    return 0;
}

static int test_common_bringup_wrapper(void) {
    kernel_runtime_t runtime;
    const kernel_bringup_options_t options = {
        .mmio_mask = KERNEL_BRINGUP_MMIO_UART,
        .pmm_probe_marker = 0,
        .pre_vm_setup = NULL,
        .pre_vm_context = NULL,
    };

    reset_stub_state();
    kernel_runtime_init(&runtime);
    if (!kernel_runtime_run_common_bringup(&runtime, &options)) {
        return fail("expected runtime common bring-up wrapper to succeed");
    }

    if (g_common_bringup_trap_context != &runtime.trap_context ||
        g_common_bringup_out_space != &runtime.address_space ||
        g_common_bringup_options != &options ||
        runtime.address_space != g_common_bringup_address_space) {
        return fail("expected runtime common bring-up wrapper to forward state");
    }

    reset_stub_state();
    kernel_runtime_init(&runtime);
    g_common_bringup_result = false;
    if (kernel_runtime_run_common_bringup(&runtime, &options)) {
        return fail("expected runtime common bring-up wrapper failure to propagate");
    }

    return 0;
}

int main(void) {
    if (test_runtime_init_and_bind_self_handlers() != 0 ||
        test_external_policy_adapter() != 0 ||
        test_interrupt_policy_adapter() != 0 ||
        test_common_bringup_wrapper() != 0) {
        return 1;
    }

    return 0;
}
