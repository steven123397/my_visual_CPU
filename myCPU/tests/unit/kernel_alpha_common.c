#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../guest/include/kernel_alpha.h"
#include "../../guest/include/kernel_runtime.h"
#include "../../guest/include/storage.h"

static int g_console_chars[16];
static size_t g_console_char_count = 0;
static int g_plic_init_calls = 0;
static int g_runtime_plic_markers[4];
static size_t g_runtime_plic_marker_count = 0;
static volatile uint32_t* g_external_wait_counter = NULL;
static uint64_t g_external_wait_timeout = 0;
static bool g_external_wait_result = true;
static kernel_runtime_t* g_runtime_external_wait_runtime = NULL;
static uint64_t g_runtime_external_wait_timeout = 0;
static bool g_runtime_external_wait_result = true;
static volatile uint32_t* g_timer_wait_counter = NULL;
static uint64_t g_timer_wait_delta = 0;
static uint64_t g_timer_wait_timeout = 0;
static bool g_timer_wait_result = true;
static kernel_runtime_t* g_runtime_timer_wait_runtime = NULL;
static uint64_t g_runtime_timer_wait_delta = 0;
static uint64_t g_runtime_timer_wait_timeout = 0;
static bool g_runtime_timer_wait_result = true;
static bool g_storage_probe_result = true;
static storage_info_t g_storage_probe_info = {0};
static int g_storage_probe_calls = 0;
static uint8_t g_storage_page[4096];
static bool g_storage_page_allocated = false;
static int g_storage_read_block_result = 0;
static uint64_t g_storage_read_block_lba = UINT64_MAX;
static void* g_last_freed_page = NULL;
static bool g_pmm_free_result = true;
static int g_runtime_storage_probe_markers[4];
static size_t g_runtime_storage_probe_marker_count = 0;
static bool g_runtime_storage_probe_result = true;
static int g_runtime_storage_signature_markers[4];
static size_t g_runtime_storage_signature_marker_count = 0;
static bool g_runtime_storage_signature_result = true;

static void reset_stub_state(void);
static int fail(const char* message);
static int test_begin_plic_supervisor_phase(void);
static int test_wait_for_first_external_delivery(void);
static int test_wait_for_first_timer_delivery(void);
static int test_complete_storage_probe(void);
static int test_complete_storage_signature_check(void);

void kernel_runtime_init(kernel_runtime_t* runtime) {
    if (runtime == NULL) {
        return;
    }

    runtime->address_space = NULL;
    runtime->interrupts.timer_interrupts = 0;
    runtime->interrupts.external_interrupts = 0;
    runtime->interrupts.expected_external_source_id = 0;
    runtime->interrupts.timer_post_handler = NULL;
    runtime->interrupts.timer_post_context = NULL;
    runtime->interrupts.external_post_handler = NULL;
    runtime->interrupts.external_post_context = NULL;
}

bool kernel_bringup_run_common(
    trap_context_t* trap_context,
    vm_address_space_t** out_space,
    const kernel_bringup_options_t* options) {
    (void)trap_context;
    (void)out_space;
    (void)options;
    return true;
}

void console_putc(char ch) {
    if (g_console_char_count < (sizeof(g_console_chars) / sizeof(g_console_chars[0]))) {
        g_console_chars[g_console_char_count++] = (unsigned char)ch;
    }
}

void platform_plic_supervisor_init(void) {
    g_plic_init_calls += 1;
}

void kernel_runtime_begin_plic_supervisor_phase(char marker) {
    if (g_runtime_plic_marker_count <
        (sizeof(g_runtime_plic_markers) / sizeof(g_runtime_plic_markers[0]))) {
        g_runtime_plic_markers[g_runtime_plic_marker_count++] = (unsigned char)marker;
    }
}

bool supervisor_runtime_enable_uart_thre_and_wait(
    volatile uint32_t* external_counter,
    uint64_t timeout_delta) {
    g_external_wait_counter = external_counter;
    g_external_wait_timeout = timeout_delta;
    return g_external_wait_result;
}

bool kernel_runtime_wait_for_first_external_delivery(kernel_runtime_t* runtime,
                                                     uint64_t timeout_delta) {
    g_runtime_external_wait_runtime = runtime;
    g_runtime_external_wait_timeout = timeout_delta;
    return g_runtime_external_wait_result;
}

bool supervisor_runtime_schedule_timer_and_wait(volatile uint32_t* timer_counter,
                                                uint64_t timer_delta,
                                                uint64_t timeout_delta) {
    g_timer_wait_counter = timer_counter;
    g_timer_wait_delta = timer_delta;
    g_timer_wait_timeout = timeout_delta;
    return g_timer_wait_result;
}

bool kernel_runtime_wait_for_first_timer_delivery(kernel_runtime_t* runtime,
                                                  uint64_t timer_delta,
                                                  uint64_t timeout_delta) {
    g_runtime_timer_wait_runtime = runtime;
    g_runtime_timer_wait_delta = timer_delta;
    g_runtime_timer_wait_timeout = timeout_delta;
    return g_runtime_timer_wait_result;
}

bool storage_probe(storage_info_t* out_info) {
    g_storage_probe_calls += 1;
    if (out_info != NULL) {
        *out_info = g_storage_probe_info;
    }
    return g_storage_probe_result;
}

uint64_t storage_read_block(uint64_t lba, void* buffer) {
    g_storage_read_block_lba = lba;
    if (buffer != NULL) {
        memcpy(buffer, g_storage_page, sizeof(g_storage_page));
    }
    return (uint64_t)g_storage_read_block_result;
}

void* pmm_alloc_page(void) {
    return g_storage_page_allocated ? g_storage_page : NULL;
}

bool pmm_free_page(void* page) {
    g_last_freed_page = page;
    return g_pmm_free_result;
}

bool kernel_runtime_complete_storage_probe(char marker) {
    if (g_runtime_storage_probe_marker_count <
        (sizeof(g_runtime_storage_probe_markers) /
         sizeof(g_runtime_storage_probe_markers[0]))) {
        g_runtime_storage_probe_markers[g_runtime_storage_probe_marker_count++] =
            (unsigned char)marker;
    }
    return g_runtime_storage_probe_result;
}

bool kernel_runtime_complete_storage_signature_check(char marker) {
    if (g_runtime_storage_signature_marker_count <
        (sizeof(g_runtime_storage_signature_markers) /
         sizeof(g_runtime_storage_signature_markers[0]))) {
        g_runtime_storage_signature_markers
            [g_runtime_storage_signature_marker_count++] = (unsigned char)marker;
    }
    return g_runtime_storage_signature_result;
}

static void reset_stub_state(void) {
    memset(g_console_chars, 0, sizeof(g_console_chars));
    g_console_char_count = 0;
    g_plic_init_calls = 0;
    memset(g_runtime_plic_markers, 0, sizeof(g_runtime_plic_markers));
    g_runtime_plic_marker_count = 0;
    g_external_wait_counter = NULL;
    g_external_wait_timeout = 0;
    g_external_wait_result = true;
    g_runtime_external_wait_runtime = NULL;
    g_runtime_external_wait_timeout = 0;
    g_runtime_external_wait_result = true;
    g_timer_wait_counter = NULL;
    g_timer_wait_delta = 0;
    g_timer_wait_timeout = 0;
    g_timer_wait_result = true;
    g_runtime_timer_wait_runtime = NULL;
    g_runtime_timer_wait_delta = 0;
    g_runtime_timer_wait_timeout = 0;
    g_runtime_timer_wait_result = true;
    g_storage_probe_result = true;
    memset(&g_storage_probe_info, 0, sizeof(g_storage_probe_info));
    g_storage_probe_calls = 0;
    memset(g_storage_page, 0, sizeof(g_storage_page));
    g_storage_page_allocated = false;
    g_storage_read_block_result = 0;
    g_storage_read_block_lba = UINT64_MAX;
    g_last_freed_page = NULL;
    g_pmm_free_result = true;
    memset(g_runtime_storage_probe_markers,
           0,
           sizeof(g_runtime_storage_probe_markers));
    g_runtime_storage_probe_marker_count = 0;
    g_runtime_storage_probe_result = true;
    memset(g_runtime_storage_signature_markers,
           0,
           sizeof(g_runtime_storage_signature_markers));
    g_runtime_storage_signature_marker_count = 0;
    g_runtime_storage_signature_result = true;
}

static int fail(const char* message) {
    fprintf(stderr, "%s\n", message);
    return 1;
}

static int test_begin_plic_supervisor_phase(void) {
    reset_stub_state();
    kernel_alpha_begin_plic_supervisor_phase();

    if (g_runtime_plic_marker_count != 1 ||
        g_runtime_plic_markers[0] != 'P' ||
        g_plic_init_calls != 0 ||
        g_console_char_count != 0) {
        return fail("expected alpha PLIC phase helper to delegate to kernel_runtime");
    }

    return 0;
}

static int test_wait_for_first_external_delivery(void) {
    kernel_runtime_t runtime;

    reset_stub_state();
    kernel_runtime_init(&runtime);
    runtime.interrupts.external_interrupts = 3U;

    if (!kernel_alpha_wait_for_first_external_delivery(&runtime, 64U)) {
        return fail("expected external delivery helper to propagate success");
    }

    if (g_runtime_external_wait_runtime != &runtime ||
        g_runtime_external_wait_timeout != 64U ||
        g_external_wait_counter != NULL) {
        return fail("expected alpha external delivery helper to delegate to kernel_runtime");
    }

    reset_stub_state();
    kernel_runtime_init(&runtime);
    g_runtime_external_wait_result = false;
    if (kernel_alpha_wait_for_first_external_delivery(&runtime, 32U)) {
        return fail("expected external delivery helper to propagate failure");
    }

    return 0;
}

static int test_wait_for_first_timer_delivery(void) {
    kernel_runtime_t runtime;

    reset_stub_state();
    kernel_runtime_init(&runtime);
    runtime.interrupts.timer_interrupts = 5U;

    if (!kernel_alpha_wait_for_first_timer_delivery(&runtime, 8U, 96U)) {
        return fail("expected timer delivery helper to propagate success");
    }

    if (g_runtime_timer_wait_runtime != &runtime ||
        g_runtime_timer_wait_delta != 8U ||
        g_runtime_timer_wait_timeout != 96U ||
        g_timer_wait_counter != NULL) {
        return fail("expected alpha timer delivery helper to delegate to kernel_runtime");
    }

    reset_stub_state();
    kernel_runtime_init(&runtime);
    g_runtime_timer_wait_result = false;
    if (kernel_alpha_wait_for_first_timer_delivery(&runtime, 4U, 48U)) {
        return fail("expected timer delivery helper to propagate failure");
    }

    return 0;
}

static int test_complete_storage_probe(void) {
    reset_stub_state();
    if (!kernel_alpha_complete_storage_probe()) {
        return fail("expected storage probe helper to succeed with capacity");
    }

    if (g_runtime_storage_probe_marker_count != 1 ||
        g_runtime_storage_probe_markers[0] != 'D' ||
        g_storage_probe_calls != 0 ||
        g_console_char_count != 0) {
        return fail("expected alpha storage probe helper to delegate to kernel_runtime");
    }

    reset_stub_state();
    g_runtime_storage_probe_result = false;
    if (kernel_alpha_complete_storage_probe()) {
        return fail("expected delegated storage probe failure to propagate");
    }

    return 0;
}

static int test_complete_storage_signature_check(void) {
    reset_stub_state();
    if (!kernel_alpha_complete_storage_signature_check()) {
        return fail("expected storage signature helper to accept Stor prefix");
    }

    if (g_runtime_storage_signature_marker_count != 1 ||
        g_runtime_storage_signature_markers[0] != 'S' ||
        g_storage_read_block_lba != UINT64_MAX ||
        g_last_freed_page != NULL ||
        g_console_char_count != 0) {
        return fail("expected alpha storage signature helper to delegate to kernel_runtime");
    }

    reset_stub_state();
    g_runtime_storage_signature_result = false;
    if (kernel_alpha_complete_storage_signature_check()) {
        return fail("expected delegated storage signature failure to propagate");
    }

    return 0;
}

int main(void) {
    if (test_begin_plic_supervisor_phase() != 0 ||
        test_wait_for_first_external_delivery() != 0 ||
        test_wait_for_first_timer_delivery() != 0 ||
        test_complete_storage_probe() != 0 ||
        test_complete_storage_signature_check() != 0) {
        return 1;
    }

    return 0;
}
