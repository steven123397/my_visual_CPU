#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../guest/include/kernel_bringup.h"
#include "../../guest/include/memory.h"
#include "../../guest/include/platform.h"

struct VmAddressSpace {
    bool enabled;
    bool active;
    uint64_t satp_value;
};

typedef struct RangeCall {
    uintptr_t vaddr;
    uintptr_t paddr;
    size_t size;
    uint64_t flags;
} range_call_t;

static char g_console_chars[16];
static size_t g_console_char_count = 0;
static int g_memory_init_calls = 0;
static int g_runtime_context_reset_calls = 0;
static int g_trap_context_init_calls = 0;
static int g_trap_context_activate_calls = 0;
static int g_pmm_init_calls = 0;
static int g_vm_create_calls = 0;
static int g_vm_enable_calls = 0;
static int g_vm_destroy_calls = 0;
static int g_pre_vm_calls = 0;
static int g_pmm_alloc_calls = 0;
static int g_pmm_free_calls = 0;
static bool g_trap_context_activate_result = true;
static trap_context_t* g_active_trap_context = NULL;
static bool g_vm_create_result = true;
static bool g_vm_enable_result = true;
static bool g_vm_destroy_result = true;
static range_call_t g_map_calls[8];
static size_t g_map_call_count = 0;
static range_call_t g_fault_calls[4];
static size_t g_fault_call_count = 0;
static struct VmAddressSpace g_address_space = {0};
static uint64_t g_riscv_satp_value = 0;
static uintptr_t g_text_start = 0;
static uintptr_t g_text_end = 0;
static uintptr_t g_rodata_start = 0;
static uintptr_t g_rodata_end = 0;
static uintptr_t g_data_start = 0;
static uintptr_t g_bss_end = 0;
static uintptr_t g_heap_start = 0;
static uintptr_t g_managed_start = 0;
static uintptr_t g_managed_end = 0;
static size_t g_pmm_total_pages = 0;
static size_t g_pmm_free_pages_count = 0;
static uint64_t g_probe_page[MEMORY_PAGE_SIZE / sizeof(uint64_t)];
static bool g_probe_page_available = true;
static bool g_pmm_free_result = true;
static trap_context_t* g_pre_vm_trap_context = NULL;
static void* g_pre_vm_context = NULL;
static bool g_pre_vm_result = true;

static void reset_stub_state(void);
static int fail(const char* message);
static int expect_console_output(const char* expected);
static bool range_call_matches(const range_call_t* call,
                               uintptr_t vaddr,
                               uintptr_t paddr,
                               size_t size,
                               uint64_t flags);
static int test_common_bringup_maps_fixed_ranges_and_selected_mmio(void);
static int test_common_bringup_skips_managed_map_when_disabled(void);
static int test_common_bringup_propagates_pre_vm_failure(void);
static int test_common_bringup_rolls_back_vm_on_pmm_probe_failure(void);
static int test_common_bringup_rolls_back_vm_on_setup_failure(void);
static bool stub_pre_vm_setup(trap_context_t* trap_context, void* context);

void memory_init(void) {
    g_memory_init_calls += 1;
}

uintptr_t memory_text_start(void) {
    return g_text_start;
}

uintptr_t memory_text_end(void) {
    return g_text_end;
}

uintptr_t memory_rodata_start(void) {
    return g_rodata_start;
}

uintptr_t memory_rodata_end(void) {
    return g_rodata_end;
}

uintptr_t memory_data_start(void) {
    return g_data_start;
}

uintptr_t memory_bss_end(void) {
    return g_bss_end;
}

uintptr_t memory_heap_start(void) {
    return g_heap_start;
}

void runtime_context_reset(void) {
    g_runtime_context_reset_calls += 1;
}

void trap_context_init(trap_context_t* trap_context) {
    g_trap_context_init_calls += 1;
    if (trap_context != NULL) {
        memset(trap_context, 0, sizeof(*trap_context));
    }
}

bool trap_context_activate(trap_context_t* trap_context) {
    g_trap_context_activate_calls += 1;
    if (!g_trap_context_activate_result || trap_context == NULL) {
        return false;
    }

    g_active_trap_context = trap_context;
    return true;
}

bool trap_context_is_active(const trap_context_t* trap_context) {
    return trap_context != NULL && trap_context == g_active_trap_context;
}

trap_context_t* trap_active_context(void) {
    return g_active_trap_context;
}

void console_putc(char ch) {
    if (g_console_char_count < sizeof(g_console_chars)) {
        g_console_chars[g_console_char_count++] = ch;
    }
}

void pmm_init(void) {
    g_pmm_init_calls += 1;
}

void* pmm_alloc_page(void) {
    g_pmm_alloc_calls += 1;
    return g_probe_page_available ? g_probe_page : NULL;
}

bool pmm_free_page(void* page) {
    g_pmm_free_calls += 1;
    return page == g_probe_page && g_pmm_free_result;
}

uintptr_t pmm_managed_start(void) {
    return g_managed_start;
}

uintptr_t pmm_managed_end(void) {
    return g_managed_end;
}

size_t pmm_total_pages(void) {
    return g_pmm_total_pages;
}

size_t pmm_free_pages(void) {
    return g_pmm_free_pages_count;
}

bool vm_address_space_create(vm_address_space_t** out_space) {
    g_vm_create_calls += 1;
    if (!g_vm_create_result || out_space == NULL) {
        return false;
    }

    g_address_space.enabled = false;
    g_address_space.active = false;
    *out_space = &g_address_space;
    return true;
}

bool vm_address_space_map_kernel_range(vm_address_space_t* address_space,
                                       uintptr_t vaddr,
                                       uintptr_t paddr,
                                       size_t size,
                                       uint64_t flags) {
    if (address_space == NULL || g_map_call_count >= (sizeof(g_map_calls) / sizeof(g_map_calls[0]))) {
        return false;
    }

    g_map_calls[g_map_call_count++] = (range_call_t){
        .vaddr = vaddr,
        .paddr = paddr,
        .size = size,
        .flags = flags,
    };
    return true;
}

bool vm_address_space_register_fault_range(vm_address_space_t* address_space,
                                           uintptr_t vaddr,
                                           uintptr_t paddr,
                                           size_t size,
                                           uint64_t flags) {
    if (address_space == NULL ||
        g_fault_call_count >= (sizeof(g_fault_calls) / sizeof(g_fault_calls[0]))) {
        return false;
    }

    g_fault_calls[g_fault_call_count++] = (range_call_t){
        .vaddr = vaddr,
        .paddr = paddr,
        .size = size,
        .flags = flags,
    };
    return true;
}

bool vm_address_space_enable(vm_address_space_t* address_space) {
    g_vm_enable_calls += 1;
    if (!g_vm_enable_result || address_space == NULL) {
        return false;
    }

    address_space->enabled = true;
    address_space->active = true;
    return true;
}

bool vm_address_space_destroy(vm_address_space_t* address_space) {
    g_vm_destroy_calls += 1;
    if (!g_vm_destroy_result || address_space == NULL) {
        return false;
    }

    address_space->enabled = false;
    address_space->active = false;
    return true;
}

bool vm_address_space_is_enabled(const vm_address_space_t* address_space) {
    return address_space != NULL && address_space->enabled;
}

bool vm_address_space_is_active(const vm_address_space_t* address_space) {
    return address_space != NULL && address_space->active;
}

uint64_t vm_address_space_satp_value(const vm_address_space_t* address_space) {
    return address_space != NULL ? address_space->satp_value : 0;
}

uint64_t riscv_read_satp(void) {
    return g_riscv_satp_value;
}

static void reset_stub_state(void) {
    memset(g_console_chars, 0, sizeof(g_console_chars));
    g_console_char_count = 0;
    g_memory_init_calls = 0;
    g_runtime_context_reset_calls = 0;
    g_trap_context_init_calls = 0;
    g_trap_context_activate_calls = 0;
    g_pmm_init_calls = 0;
    g_vm_create_calls = 0;
    g_vm_enable_calls = 0;
    g_vm_destroy_calls = 0;
    g_pre_vm_calls = 0;
    g_pmm_alloc_calls = 0;
    g_pmm_free_calls = 0;
    g_trap_context_activate_result = true;
    g_active_trap_context = NULL;
    g_vm_create_result = true;
    g_vm_enable_result = true;
    g_vm_destroy_result = true;
    memset(g_map_calls, 0, sizeof(g_map_calls));
    g_map_call_count = 0;
    memset(g_fault_calls, 0, sizeof(g_fault_calls));
    g_fault_call_count = 0;
    g_address_space.enabled = false;
    g_address_space.active = false;
    g_address_space.satp_value = UINT64_C(0x8000000000001234);
    g_riscv_satp_value = g_address_space.satp_value;
    g_text_start = UINT64_C(0x80000000);
    g_text_end = UINT64_C(0x80001000);
    g_rodata_start = UINT64_C(0x80001000);
    g_rodata_end = UINT64_C(0x80002000);
    g_data_start = UINT64_C(0x80002000);
    g_bss_end = UINT64_C(0x80004000);
    g_heap_start = UINT64_C(0x80004000);
    g_managed_start = UINT64_C(0x80006000);
    g_managed_end = UINT64_C(0x80008000);
    g_pmm_total_pages = 32U;
    g_pmm_free_pages_count = 16U;
    memset(g_probe_page, 0, sizeof(g_probe_page));
    g_probe_page_available = true;
    g_pmm_free_result = true;
    g_pre_vm_trap_context = NULL;
    g_pre_vm_context = NULL;
    g_pre_vm_result = true;
}

static int fail(const char* message) {
    fprintf(stderr, "%s\n", message);
    return 1;
}

static int expect_console_output(const char* expected) {
    const size_t expected_len = strlen(expected);

    if (g_console_char_count != expected_len ||
        memcmp(g_console_chars, expected, expected_len) != 0) {
        return fail("unexpected console output");
    }

    return 0;
}

static bool range_call_matches(const range_call_t* call,
                               uintptr_t vaddr,
                               uintptr_t paddr,
                               size_t size,
                               uint64_t flags) {
    return call != NULL && call->vaddr == vaddr && call->paddr == paddr &&
           call->size == size && call->flags == flags;
}

static bool stub_pre_vm_setup(trap_context_t* trap_context, void* context) {
    g_pre_vm_calls += 1;
    g_pre_vm_trap_context = trap_context;
    g_pre_vm_context = context;
    return g_pre_vm_result;
}

static int test_common_bringup_maps_fixed_ranges_and_selected_mmio(void) {
    trap_context_t trap_context;
    vm_address_space_t* address_space = NULL;
    int pre_vm_token = 7;
    const uint64_t data_flags = VM_PAGE_READ | VM_PAGE_WRITE;
    const kernel_bringup_options_t options = {
        .mmio_mask = KERNEL_BRINGUP_MMIO_UART | KERNEL_BRINGUP_MMIO_PLIC,
        .pmm_probe_marker = UINT64_C(0xABCDEF01),
        .pre_vm_setup = stub_pre_vm_setup,
        .pre_vm_context = &pre_vm_token,
        .map_managed_memory = true,
    };

    reset_stub_state();
    if (!kernel_bringup_run_common(&trap_context, &address_space, &options)) {
        return fail("expected common bring-up to succeed");
    }

    if (g_memory_init_calls != 1 || g_runtime_context_reset_calls != 1 ||
        g_trap_context_init_calls != 1 || g_trap_context_activate_calls != 1 ||
        g_pmm_init_calls != 1 || g_vm_create_calls != 1 ||
        g_vm_enable_calls != 1 || g_pre_vm_calls != 1 ||
        g_pmm_alloc_calls != 1 || g_pmm_free_calls != 1) {
        return fail("expected common bring-up to execute setup steps once");
    }

    if (expect_console_output("KMV") != 0) {
        return 1;
    }

    if (address_space != &g_address_space || g_pre_vm_trap_context != &trap_context ||
        g_pre_vm_context != &pre_vm_token || g_probe_page[0] != options.pmm_probe_marker) {
        return fail("expected common bring-up to forward trap/pre-vm state");
    }

    if (g_map_call_count != 5 ||
        !range_call_matches(&g_map_calls[0],
                            g_text_start,
                            g_text_start,
                            g_text_end - g_text_start,
                            VM_PAGE_READ | VM_PAGE_EXEC) ||
        !range_call_matches(&g_map_calls[1],
                            g_rodata_start,
                            g_rodata_start,
                            g_rodata_end - g_rodata_start,
                            VM_PAGE_READ) ||
        !range_call_matches(&g_map_calls[2],
                            g_data_start,
                            g_data_start,
                            g_bss_end - g_data_start,
                            data_flags) ||
        !range_call_matches(&g_map_calls[3],
                            g_heap_start,
                            g_heap_start,
                            g_managed_start - g_heap_start,
                            data_flags) ||
        !range_call_matches(&g_map_calls[4],
                            g_managed_start,
                            g_managed_start,
                            g_managed_end - g_managed_start,
                            data_flags)) {
        return fail("expected common bring-up to map fixed kernel ranges");
    }

    if (g_fault_call_count != 2 ||
        !range_call_matches(&g_fault_calls[0],
                            UART_BASE,
                            UART_BASE,
                            MEMORY_PAGE_SIZE,
                            data_flags) ||
        !range_call_matches(&g_fault_calls[1],
                            PLIC_BASE,
                            PLIC_BASE,
                            PLIC_SIZE,
                            data_flags)) {
        return fail("expected common bring-up to register selected MMIO fault ranges");
    }

    return 0;
}

static int test_common_bringup_skips_managed_map_when_disabled(void) {
    trap_context_t trap_context;
    vm_address_space_t* address_space = NULL;
    const uint64_t data_flags = VM_PAGE_READ | VM_PAGE_WRITE;
    const kernel_bringup_options_t options = {
        .mmio_mask = 0,
        .pmm_probe_marker = 0,
        .pre_vm_setup = NULL,
        .pre_vm_context = NULL,
        .map_managed_memory = false,
    };

    reset_stub_state();
    if (!kernel_bringup_run_common(&trap_context, &address_space, &options)) {
        return fail("expected common bring-up without managed map to succeed");
    }

    if (expect_console_output("KMV") != 0) {
        return 1;
    }

    if (address_space != &g_address_space || g_map_call_count != 4 ||
        g_fault_call_count != 0 || g_pmm_alloc_calls != 0) {
        return fail("expected managed map skip to avoid extra mappings and probe");
    }

    if (!range_call_matches(&g_map_calls[3],
                            g_heap_start,
                            g_heap_start,
                            g_managed_start - g_heap_start,
                            data_flags)) {
        return fail("expected early heap map to remain intact when managed map disabled");
    }

    return 0;
}

static int test_common_bringup_propagates_pre_vm_failure(void) {
    trap_context_t trap_context;
    vm_address_space_t* address_space = (vm_address_space_t*)(uintptr_t)0x1;
    const kernel_bringup_options_t options = {
        .mmio_mask = KERNEL_BRINGUP_MMIO_STORAGE,
        .pmm_probe_marker = UINT64_C(0x55),
        .pre_vm_setup = stub_pre_vm_setup,
        .pre_vm_context = &trap_context,
        .map_managed_memory = true,
    };

    reset_stub_state();
    g_pre_vm_result = false;
    if (kernel_bringup_run_common(&trap_context, &address_space, &options)) {
        return fail("expected pre-vm setup failure to propagate");
    }

    if (expect_console_output("KM") != 0) {
        return 1;
    }

    if (address_space != NULL || g_pre_vm_calls != 1 || g_map_call_count != 0 ||
        g_fault_call_count != 0 || g_vm_enable_calls != 0 || g_pmm_alloc_calls != 0) {
        return fail("expected pre-vm failure to stop before VM setup");
    }

    return 0;
}

static int test_common_bringup_rolls_back_vm_on_pmm_probe_failure(void) {
    trap_context_t trap_context;
    vm_address_space_t* address_space = (vm_address_space_t*)(uintptr_t)0x1;
    const kernel_bringup_options_t options = {
        .mmio_mask = KERNEL_BRINGUP_MMIO_STORAGE,
        .pmm_probe_marker = UINT64_C(0x10203040),
        .pre_vm_setup = NULL,
        .pre_vm_context = NULL,
        .map_managed_memory = true,
    };

    reset_stub_state();
    g_pmm_free_result = false;
    if (kernel_bringup_run_common(&trap_context, &address_space, &options)) {
        return fail("expected PMM probe failure to propagate");
    }

    if (expect_console_output("KM") != 0) {
        return 1;
    }

    if (address_space != NULL || g_vm_create_calls != 1 || g_vm_enable_calls != 1 ||
        g_vm_destroy_calls != 1 || g_pmm_alloc_calls != 1 || g_pmm_free_calls != 1 ||
        g_probe_page[0] != options.pmm_probe_marker) {
        return fail("expected PMM probe failure to rollback created address space");
    }

    return 0;
}

static int test_common_bringup_rolls_back_vm_on_setup_failure(void) {
    trap_context_t trap_context;
    vm_address_space_t* address_space = (vm_address_space_t*)(uintptr_t)0x1;
    const kernel_bringup_options_t options = {
        .mmio_mask = KERNEL_BRINGUP_MMIO_STORAGE,
        .pmm_probe_marker = 0,
        .pre_vm_setup = NULL,
        .pre_vm_context = NULL,
        .map_managed_memory = true,
    };

    reset_stub_state();
    g_riscv_satp_value = UINT64_C(0xDEADBEEF);
    if (kernel_bringup_run_common(&trap_context, &address_space, &options)) {
        return fail("expected VM setup validation failure to propagate");
    }

    if (expect_console_output("KM") != 0) {
        return 1;
    }

    if (address_space != NULL || g_vm_create_calls != 1 || g_vm_enable_calls != 1 ||
        g_vm_destroy_calls != 1 || g_pmm_alloc_calls != 0 || g_pmm_free_calls != 0) {
        return fail("expected VM setup failure to rollback created address space");
    }

    return 0;
}

int main(void) {
    if (test_common_bringup_maps_fixed_ranges_and_selected_mmio() != 0 ||
        test_common_bringup_skips_managed_map_when_disabled() != 0 ||
        test_common_bringup_propagates_pre_vm_failure() != 0 ||
        test_common_bringup_rolls_back_vm_on_pmm_probe_failure() != 0 ||
        test_common_bringup_rolls_back_vm_on_setup_failure() != 0) {
        return 1;
    }

    return 0;
}
