#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../guest/include/kernel_bringup.h"
#include "../../guest/include/kernel_runtime.h"
#include "../../guest/include/storage.h"
#include "../../guest/include/supervisor_runtime.h"

static trap_context_t* g_external_policy_trap_context = NULL;
static supervisor_runtime_interrupt_state_t* g_external_policy_state = NULL;
static bool g_external_policy_result = true;
static trap_context_t* g_interrupt_policies_trap_context = NULL;
static supervisor_runtime_interrupt_state_t* g_interrupt_policies_state = NULL;
static bool g_interrupt_policies_result = true;
static trap_context_t* g_common_bringup_trap_context = NULL;
static vm_address_space_t** g_common_bringup_out_space = NULL;
static kernel_bringup_options_t g_common_bringup_options_storage = {0};
static const kernel_bringup_options_t* g_common_bringup_options = NULL;
static vm_address_space_t* g_common_bringup_address_space = NULL;
static bool g_common_bringup_result = true;
static int g_console_chars[16];
static size_t g_console_char_count = 0;
static int g_plic_init_calls = 0;
static int g_memory_init_calls = 0;
static int g_runtime_context_reset_calls = 0;
static trap_context_t* g_trap_context_init_arg = NULL;
static trap_context_t* g_trap_context_activate_arg = NULL;
static bool g_trap_context_activate_result = true;
static const trap_context_t* g_trap_context_is_active_arg = NULL;
static bool g_trap_context_is_active_result = true;
static trap_context_t* g_trap_active_context = NULL;
static int g_pmm_init_calls = 0;
static size_t g_pmm_total_pages_value = 0;
static size_t g_pmm_free_pages_value = 0;
static bool g_vm_create_result = true;
static vm_address_space_t* g_vm_create_address_space = NULL;
static int g_vm_map_identity_calls = 0;
static uintptr_t g_vm_map_identity_bases[4];
static uint64_t g_vm_map_identity_flags[4];
static bool g_vm_map_identity_results[4] = {true, true, true, true};
static bool g_vm_enable_result = true;
static bool g_vm_is_enabled_result = true;
static bool g_vm_is_active_result = true;
static uint64_t g_vm_satp_value = 0;
static uint64_t g_riscv_satp_value = 0;
static volatile uint32_t* g_external_wait_counter = NULL;
static uint64_t g_external_wait_timeout = 0;
static bool g_external_wait_result = true;
static volatile uint32_t* g_timer_wait_counter = NULL;
static uint64_t g_timer_wait_delta = 0;
static uint64_t g_timer_wait_timeout = 0;
static bool g_timer_wait_result = true;
static supervisor_runtime_interrupt_state_t* g_platform_interrupt_wait_state = NULL;
static uint64_t g_platform_interrupt_wait_delta = 0;
static uint64_t g_platform_interrupt_wait_timeout = 0;
static bool g_platform_interrupt_wait_result = true;
static bool g_storage_probe_result = true;
static storage_info_t g_storage_probe_info = {0};
static int g_storage_probe_calls = 0;
static uint8_t g_storage_page[4096];
static bool g_storage_page_allocated = false;
static uint64_t g_storage_read_block_result = 0;
static uint64_t g_storage_read_block_lba = UINT64_MAX;
static int g_storage_read_block_calls = 0;
static void* g_last_freed_page = NULL;
static bool g_pmm_free_result = true;

static void reset_stub_state(void);
static int fail(const char* message);
static void stub_timer_post_handler(void* context);
static void stub_external_post_handler(uint32_t source_id, void* context);
static int test_runtime_init_and_bind_self_handlers(void);
static int test_runtime_entry_bringup_helper(void);
static int test_runtime_identity_superpage_bringup_helper(void);
static int test_external_policy_adapter(void);
static int test_interrupt_policy_adapter(void);
static int test_runtime_bringup_helper(void);
static int test_common_bringup_wrapper(void);
static int test_plic_phase_helper(void);
static int test_external_wait_helper(void);
static int test_timer_wait_helper(void);
static int test_storage_probe_helper(void);
static int test_storage_signature_helper(void);
static int test_storage_platform_tail_helper(void);

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
    if (options != NULL) {
        g_common_bringup_options_storage = *options;
        g_common_bringup_options = &g_common_bringup_options_storage;
    } else {
        g_common_bringup_options = NULL;
    }
    if (g_common_bringup_result && out_space != NULL) {
        *out_space = g_common_bringup_address_space;
    }
    return g_common_bringup_result;
}

void console_putc(char ch) {
    if (g_console_char_count < (sizeof(g_console_chars) / sizeof(g_console_chars[0]))) {
        g_console_chars[g_console_char_count++] = (unsigned char)ch;
    }
}

void platform_plic_supervisor_init(void) {
    g_plic_init_calls += 1;
}

void memory_init(void) {
    g_memory_init_calls += 1;
}

void runtime_context_reset(void) {
    g_runtime_context_reset_calls += 1;
}

void trap_context_init(trap_context_t* trap_context) {
    g_trap_context_init_arg = trap_context;
}

bool trap_context_activate(trap_context_t* trap_context) {
    g_trap_context_activate_arg = trap_context;
    return g_trap_context_activate_result;
}

bool trap_context_is_active(const trap_context_t* trap_context) {
    g_trap_context_is_active_arg = trap_context;
    return g_trap_context_is_active_result;
}

trap_context_t* trap_active_context(void) {
    return g_trap_active_context;
}

size_t pmm_total_pages(void) {
    return g_pmm_total_pages_value;
}

void pmm_init(void) {
    g_pmm_init_calls += 1;
}

size_t pmm_free_pages(void) {
    return g_pmm_free_pages_value;
}

bool vm_address_space_create(vm_address_space_t** out_space) {
    if (out_space != NULL) {
        *out_space = g_vm_create_address_space;
    }
    return g_vm_create_result;
}

bool vm_address_space_map_identity_1g(vm_address_space_t* address_space,
                                      uintptr_t base,
                                      uint64_t flags) {
    (void)address_space;
    if (g_vm_map_identity_calls <
        (int)(sizeof(g_vm_map_identity_bases) /
              sizeof(g_vm_map_identity_bases[0]))) {
        g_vm_map_identity_bases[g_vm_map_identity_calls] = base;
        g_vm_map_identity_flags[g_vm_map_identity_calls] = flags;
    }

    if (g_vm_map_identity_calls <
        (int)(sizeof(g_vm_map_identity_results) /
              sizeof(g_vm_map_identity_results[0]))) {
        return g_vm_map_identity_results[g_vm_map_identity_calls++];
    }

    g_vm_map_identity_calls += 1;
    return false;
}

bool vm_address_space_enable(vm_address_space_t* address_space) {
    (void)address_space;
    return g_vm_enable_result;
}

bool vm_address_space_is_enabled(const vm_address_space_t* address_space) {
    (void)address_space;
    return g_vm_is_enabled_result;
}

bool vm_address_space_is_active(const vm_address_space_t* address_space) {
    (void)address_space;
    return g_vm_is_active_result;
}

uint64_t vm_address_space_satp_value(const vm_address_space_t* address_space) {
    (void)address_space;
    return g_vm_satp_value;
}

uintptr_t vm_kernel_base(void) {
    return UINT64_C(0x80000000);
}

uint64_t riscv_read_satp(void) {
    return g_riscv_satp_value;
}

bool supervisor_runtime_enable_uart_thre_and_wait(
    volatile uint32_t* external_counter,
    uint64_t timeout_delta) {
    g_external_wait_counter = external_counter;
    g_external_wait_timeout = timeout_delta;
    return g_external_wait_result;
}

bool supervisor_runtime_schedule_timer_and_wait(volatile uint32_t* timer_counter,
                                                uint64_t timer_delta,
                                                uint64_t timeout_delta) {
    g_timer_wait_counter = timer_counter;
    g_timer_wait_delta = timer_delta;
    g_timer_wait_timeout = timeout_delta;
    return g_timer_wait_result;
}

bool supervisor_runtime_schedule_platform_interrupts_and_wait(
    supervisor_runtime_interrupt_state_t* state,
    uint64_t timer_delta,
    uint64_t timeout_delta) {
    g_platform_interrupt_wait_state = state;
    g_platform_interrupt_wait_delta = timer_delta;
    g_platform_interrupt_wait_timeout = timeout_delta;
    return g_platform_interrupt_wait_result;
}

bool storage_probe(storage_info_t* info) {
    g_storage_probe_calls += 1;
    if (info != NULL) {
        *info = g_storage_probe_info;
    }
    return g_storage_probe_result;
}

uint64_t storage_read_block(uint64_t lba, void* destination) {
    g_storage_read_block_calls += 1;
    g_storage_read_block_lba = lba;
    if (destination != NULL) {
        memcpy(destination, g_storage_page, sizeof(g_storage_page));
    }
    return g_storage_read_block_result;
}

void* pmm_alloc_page(void) {
    return g_storage_page_allocated ? g_storage_page : NULL;
}

bool pmm_free_page(void* page) {
    g_last_freed_page = page;
    return page == g_storage_page && g_pmm_free_result;
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
    memset(g_console_chars, 0, sizeof(g_console_chars));
    g_console_char_count = 0;
    g_plic_init_calls = 0;
    g_memory_init_calls = 0;
    g_runtime_context_reset_calls = 0;
    g_trap_context_init_arg = NULL;
    g_trap_context_activate_arg = NULL;
    g_trap_context_activate_result = true;
    g_trap_context_is_active_arg = NULL;
    g_trap_context_is_active_result = true;
    g_trap_active_context = NULL;
    g_pmm_init_calls = 0;
    g_pmm_total_pages_value = 256U;
    g_pmm_free_pages_value = 256U;
    g_vm_create_result = true;
    g_vm_create_address_space = (vm_address_space_t*)(uintptr_t)0x2468U;
    g_vm_map_identity_calls = 0;
    memset(g_vm_map_identity_bases, 0, sizeof(g_vm_map_identity_bases));
    memset(g_vm_map_identity_flags, 0, sizeof(g_vm_map_identity_flags));
    g_vm_map_identity_results[0] = true;
    g_vm_map_identity_results[1] = true;
    g_vm_map_identity_results[2] = true;
    g_vm_map_identity_results[3] = true;
    g_vm_enable_result = true;
    g_vm_is_enabled_result = true;
    g_vm_is_active_result = true;
    g_vm_satp_value = UINT64_C(0x1234);
    g_riscv_satp_value = UINT64_C(0x1234);
    g_external_wait_counter = NULL;
    g_external_wait_timeout = 0;
    g_external_wait_result = true;
    g_timer_wait_counter = NULL;
    g_timer_wait_delta = 0;
    g_timer_wait_timeout = 0;
    g_timer_wait_result = true;
    g_platform_interrupt_wait_state = NULL;
    g_platform_interrupt_wait_delta = 0;
    g_platform_interrupt_wait_timeout = 0;
    g_platform_interrupt_wait_result = true;
    g_storage_probe_result = true;
    memset(&g_storage_probe_info, 0, sizeof(g_storage_probe_info));
    g_storage_probe_calls = 0;
    memset(g_storage_page, 0, sizeof(g_storage_page));
    g_storage_page_allocated = false;
    g_storage_read_block_result = 0;
    g_storage_read_block_lba = UINT64_MAX;
    g_storage_read_block_calls = 0;
    g_last_freed_page = NULL;
    g_pmm_free_result = true;
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

static int test_runtime_entry_bringup_helper(void) {
    kernel_runtime_t runtime;

    reset_stub_state();
    kernel_runtime_init(&runtime);
    g_trap_active_context = &runtime.trap_context;
    if (!kernel_runtime_run_entry_bringup(&runtime)) {
        return fail("expected runtime entry bring-up helper to succeed");
    }

    if (g_memory_init_calls != 1 || g_runtime_context_reset_calls != 1 ||
        g_trap_context_init_arg != &runtime.trap_context ||
        g_trap_context_activate_arg != &runtime.trap_context ||
        g_trap_context_is_active_arg != &runtime.trap_context) {
        return fail("expected runtime entry bring-up helper to initialize and activate trap context");
    }

    reset_stub_state();
    kernel_runtime_init(&runtime);
    g_trap_context_activate_result = false;
    if (kernel_runtime_run_entry_bringup(&runtime)) {
        return fail("expected runtime entry bring-up helper to propagate activation failure");
    }

    reset_stub_state();
    kernel_runtime_init(&runtime);
    g_trap_active_context = &runtime.trap_context;
    g_trap_context_is_active_result = false;
    if (kernel_runtime_run_entry_bringup(&runtime)) {
        return fail("expected runtime entry bring-up helper to reject inactive trap context");
    }

    if (kernel_runtime_run_entry_bringup(NULL)) {
        return fail("expected runtime entry bring-up helper to reject null runtime");
    }

    return 0;
}

static int test_runtime_identity_superpage_bringup_helper(void) {
    kernel_runtime_t runtime;

    reset_stub_state();
    kernel_runtime_init(&runtime);
    g_trap_active_context = &runtime.trap_context;
    if (!kernel_runtime_run_identity_superpage_bringup(&runtime)) {
        return fail("expected runtime identity superpage bring-up helper to succeed");
    }

    if (g_memory_init_calls != 1 || g_runtime_context_reset_calls != 1 ||
        g_pmm_init_calls != 1 ||
        g_pmm_total_pages_value == 0 || g_pmm_free_pages_value == 0 ||
        g_vm_map_identity_calls != 2 ||
        g_vm_map_identity_bases[0] != UINT64_C(0x80000000) ||
        g_vm_map_identity_bases[1] != 0U ||
        runtime.address_space != g_vm_create_address_space ||
        g_console_char_count != 3 || g_console_chars[0] != 'K' ||
        g_console_chars[1] != 'M' || g_console_chars[2] != 'V') {
        return fail("expected runtime identity superpage bring-up helper to run KMV flow");
    }

    reset_stub_state();
    kernel_runtime_init(&runtime);
    g_trap_active_context = &runtime.trap_context;
    g_vm_map_identity_results[1] = false;
    if (kernel_runtime_run_identity_superpage_bringup(&runtime)) {
        return fail("expected runtime identity superpage bring-up helper to propagate mapping failure");
    }

    reset_stub_state();
    kernel_runtime_init(&runtime);
    g_trap_active_context = &runtime.trap_context;
    g_riscv_satp_value = UINT64_C(0x9999);
    if (kernel_runtime_run_identity_superpage_bringup(&runtime)) {
        return fail("expected runtime identity superpage bring-up helper to validate satp");
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

static int test_runtime_bringup_helper(void) {
    kernel_runtime_t runtime;

    reset_stub_state();
    kernel_runtime_init(&runtime);
    if (!kernel_runtime_run_bringup(&runtime,
                                    KERNEL_BRINGUP_MMIO_UART |
                                        KERNEL_BRINGUP_MMIO_CLINT,
                                    UINT64_C(0xABCDEF),
                                    kernel_runtime_install_external_counter_policy_adapter)) {
        return fail("expected runtime bring-up helper to succeed");
    }

    if (g_common_bringup_trap_context != &runtime.trap_context ||
        g_common_bringup_out_space != &runtime.address_space ||
        g_common_bringup_options == NULL ||
        g_common_bringup_options->mmio_mask !=
            (KERNEL_BRINGUP_MMIO_UART | KERNEL_BRINGUP_MMIO_CLINT) ||
        g_common_bringup_options->pmm_probe_marker != UINT64_C(0xABCDEF) ||
        g_common_bringup_options->pre_vm_setup !=
            kernel_runtime_install_external_counter_policy_adapter ||
        g_common_bringup_options->pre_vm_context != &runtime ||
        !g_common_bringup_options->map_managed_memory) {
        return fail("expected runtime bring-up helper to bind runtime as pre-vm context");
    }

    reset_stub_state();
    kernel_runtime_init(&runtime);
    if (!kernel_runtime_run_bringup(&runtime,
                                    KERNEL_BRINGUP_MMIO_STORAGE,
                                    0,
                                    NULL)) {
        return fail("expected runtime bring-up helper without pre-vm setup to succeed");
    }

    if (g_common_bringup_options == NULL ||
        g_common_bringup_options->mmio_mask != KERNEL_BRINGUP_MMIO_STORAGE ||
        g_common_bringup_options->pmm_probe_marker != 0 ||
        g_common_bringup_options->pre_vm_setup != NULL ||
        g_common_bringup_options->pre_vm_context != NULL ||
        !g_common_bringup_options->map_managed_memory) {
        return fail("expected runtime bring-up helper to clear pre-vm context when unused");
    }

    if (kernel_runtime_run_bringup(NULL,
                                   KERNEL_BRINGUP_MMIO_UART,
                                   0,
                                   NULL)) {
        return fail("expected null runtime bring-up helper to fail");
    }

    return 0;
}

static int test_common_bringup_wrapper(void) {
    kernel_runtime_t runtime;
    int explicit_context = 7;
    const kernel_bringup_options_t implicit_self_context_options = {
        .mmio_mask = KERNEL_BRINGUP_MMIO_UART,
        .pmm_probe_marker = UINT64_C(0x13579BDF),
        .pre_vm_setup = kernel_runtime_install_external_counter_policy_adapter,
        .pre_vm_context = NULL,
        .map_managed_memory = false,
    };
    const kernel_bringup_options_t explicit_context_options = {
        .mmio_mask = KERNEL_BRINGUP_MMIO_STORAGE,
        .pmm_probe_marker = 0,
        .pre_vm_setup = kernel_runtime_install_interrupt_counter_policies_adapter,
        .pre_vm_context = &explicit_context,
        .map_managed_memory = true,
    };

    reset_stub_state();
    kernel_runtime_init(&runtime);
    if (!kernel_runtime_run_common_bringup(&runtime,
                                           &implicit_self_context_options)) {
        return fail("expected runtime common bring-up wrapper to succeed");
    }

    if (g_common_bringup_trap_context != &runtime.trap_context ||
        g_common_bringup_out_space != &runtime.address_space ||
        g_common_bringup_options == NULL ||
        g_common_bringup_options->mmio_mask != KERNEL_BRINGUP_MMIO_UART ||
        g_common_bringup_options->pmm_probe_marker != UINT64_C(0x13579BDF) ||
        g_common_bringup_options->pre_vm_setup !=
            kernel_runtime_install_external_counter_policy_adapter ||
        g_common_bringup_options->pre_vm_context != &runtime ||
        g_common_bringup_options->map_managed_memory ||
        runtime.address_space != g_common_bringup_address_space) {
        return fail("expected runtime common bring-up wrapper to bind runtime as default pre-vm context");
    }

    reset_stub_state();
    kernel_runtime_init(&runtime);
    if (!kernel_runtime_run_common_bringup(&runtime, &explicit_context_options)) {
        return fail("expected runtime common bring-up wrapper with explicit context to succeed");
    }

    if (g_common_bringup_options == NULL ||
        g_common_bringup_options->mmio_mask != KERNEL_BRINGUP_MMIO_STORAGE ||
        g_common_bringup_options->pre_vm_setup !=
            kernel_runtime_install_interrupt_counter_policies_adapter ||
        g_common_bringup_options->pre_vm_context != &explicit_context ||
        !g_common_bringup_options->map_managed_memory) {
        return fail("expected runtime common bring-up wrapper to preserve explicit pre-vm context");
    }

    reset_stub_state();
    kernel_runtime_init(&runtime);
    g_common_bringup_result = false;
    if (kernel_runtime_run_common_bringup(&runtime,
                                          &implicit_self_context_options)) {
        return fail("expected runtime common bring-up wrapper failure to propagate");
    }

    return 0;
}

static int test_plic_phase_helper(void) {
    reset_stub_state();
    kernel_runtime_begin_plic_supervisor_phase('P');

    if (g_plic_init_calls != 1 || g_console_char_count != 1 ||
        g_console_chars[0] != 'P') {
        return fail("expected runtime PLIC phase helper to init platform and print marker");
    }

    reset_stub_state();
    kernel_runtime_begin_plic_supervisor_phase('\0');
    if (g_plic_init_calls != 1 || g_console_char_count != 0) {
        return fail("expected runtime PLIC phase helper to allow silent marker");
    }

    return 0;
}

static int test_external_wait_helper(void) {
    kernel_runtime_t runtime;

    reset_stub_state();
    kernel_runtime_init(&runtime);
    runtime.interrupts.external_interrupts = 3U;
    if (!kernel_runtime_wait_for_first_external_delivery(&runtime, 64U)) {
        return fail("expected runtime external wait helper to propagate success");
    }

    if (g_external_wait_counter != &runtime.interrupts.external_interrupts ||
        g_external_wait_timeout != 64U) {
        return fail("expected runtime external wait helper to forward interrupt counter");
    }

    reset_stub_state();
    g_external_wait_result = false;
    if (kernel_runtime_wait_for_first_external_delivery(&runtime, 32U)) {
        return fail("expected runtime external wait helper to propagate failure");
    }

    if (kernel_runtime_wait_for_first_external_delivery(NULL, 32U)) {
        return fail("expected runtime external wait helper to reject null runtime");
    }

    return 0;
}

static int test_timer_wait_helper(void) {
    kernel_runtime_t runtime;

    reset_stub_state();
    kernel_runtime_init(&runtime);
    runtime.interrupts.timer_interrupts = 5U;
    if (!kernel_runtime_wait_for_first_timer_delivery(&runtime, 8U, 96U)) {
        return fail("expected runtime timer wait helper to propagate success");
    }

    if (g_timer_wait_counter != &runtime.interrupts.timer_interrupts ||
        g_timer_wait_delta != 8U || g_timer_wait_timeout != 96U) {
        return fail("expected runtime timer wait helper to forward interrupt counter");
    }

    reset_stub_state();
    g_timer_wait_result = false;
    if (kernel_runtime_wait_for_first_timer_delivery(&runtime, 4U, 48U)) {
        return fail("expected runtime timer wait helper to propagate failure");
    }

    if (kernel_runtime_wait_for_first_timer_delivery(NULL, 4U, 48U)) {
        return fail("expected runtime timer wait helper to reject null runtime");
    }

    return 0;
}

static int test_storage_probe_helper(void) {
    reset_stub_state();
    g_storage_probe_info.capacity_blocks = 2U;
    if (!kernel_runtime_complete_storage_probe('D')) {
        return fail("expected runtime storage probe helper to accept ready media");
    }

    if (g_storage_probe_calls != 1 || g_console_char_count != 1 ||
        g_console_chars[0] != 'D') {
        return fail("expected runtime storage probe helper to probe and print marker");
    }

    reset_stub_state();
    g_storage_probe_info.capacity_blocks = 0U;
    if (kernel_runtime_complete_storage_probe('D')) {
        return fail("expected runtime storage probe helper to reject zero-capacity media");
    }

    reset_stub_state();
    g_storage_probe_info.capacity_blocks = 3U;
    if (!kernel_runtime_complete_storage_probe('\0') || g_console_char_count != 0) {
        return fail("expected runtime storage probe helper to allow silent marker");
    }

    return 0;
}

static int test_storage_signature_helper(void) {
    reset_stub_state();
    g_storage_page_allocated = true;
    memcpy(g_storage_page, "Stor", 4);
    if (!kernel_runtime_complete_storage_signature_check('S')) {
        return fail("expected runtime storage signature helper to accept Stor prefix");
    }

    if (g_storage_read_block_calls != 1 || g_storage_read_block_lba != 0U ||
        g_last_freed_page != g_storage_page || g_console_char_count != 1 ||
        g_console_chars[0] != 'S') {
        return fail("expected runtime storage signature helper to read, free and print");
    }

    reset_stub_state();
    g_storage_page_allocated = true;
    memcpy(g_storage_page, "Fail", 4);
    if (kernel_runtime_complete_storage_signature_check('S')) {
        return fail("expected runtime storage signature helper to reject bad signature");
    }

    reset_stub_state();
    g_storage_page_allocated = true;
    memcpy(g_storage_page, "Stor", 4);
    if (!kernel_runtime_complete_storage_signature_check('\0') ||
        g_console_char_count != 0) {
        return fail("expected runtime storage signature helper to allow silent marker");
    }

    return 0;
}

static int test_storage_platform_tail_helper(void) {
    kernel_runtime_t runtime;

    reset_stub_state();
    kernel_runtime_init(&runtime);
    runtime.interrupts.timer_interrupts = 8U;
    runtime.interrupts.external_interrupts = 5U;
    g_storage_page_allocated = true;
    memcpy(g_storage_page, "Stor", 4);
    if (!kernel_runtime_complete_storage_signature_and_wait_platform_interrupts(
            &runtime.interrupts,
            8U,
            96U)) {
        return fail("expected runtime platform tail helper to succeed");
    }

    if (g_storage_read_block_calls != 1 ||
        g_platform_interrupt_wait_state != &runtime.interrupts ||
        g_platform_interrupt_wait_delta != 8U ||
        g_platform_interrupt_wait_timeout != 96U) {
        return fail("expected runtime platform tail helper to forward storage and interrupt contracts");
    }

    reset_stub_state();
    kernel_runtime_init(&runtime);
    g_storage_page_allocated = true;
    memcpy(g_storage_page, "Fail", 4);
    if (kernel_runtime_complete_storage_signature_and_wait_platform_interrupts(
            &runtime.interrupts,
            8U,
            96U)) {
        return fail("expected runtime platform tail helper to fail on bad signature");
    }

    reset_stub_state();
    kernel_runtime_init(&runtime);
    g_storage_page_allocated = true;
    memcpy(g_storage_page, "Stor", 4);
    g_platform_interrupt_wait_result = false;
    if (kernel_runtime_complete_storage_signature_and_wait_platform_interrupts(
            &runtime.interrupts,
            8U,
            96U)) {
        return fail("expected runtime platform tail helper to propagate interrupt wait failure");
    }

    if (kernel_runtime_complete_storage_signature_and_wait_platform_interrupts(
            NULL,
            8U,
            96U)) {
        return fail("expected runtime platform tail helper to reject null runtime");
    }

    return 0;
}

int main(void) {
    if (test_runtime_init_and_bind_self_handlers() != 0 ||
        test_runtime_entry_bringup_helper() != 0 ||
        test_runtime_identity_superpage_bringup_helper() != 0 ||
        test_external_policy_adapter() != 0 ||
        test_interrupt_policy_adapter() != 0 ||
        test_runtime_bringup_helper() != 0 ||
        test_common_bringup_wrapper() != 0 ||
        test_plic_phase_helper() != 0 ||
        test_external_wait_helper() != 0 ||
        test_timer_wait_helper() != 0 ||
        test_storage_probe_helper() != 0 ||
        test_storage_signature_helper() != 0 ||
        test_storage_platform_tail_helper() != 0) {
        return 1;
    }

    return 0;
}
