#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../guest/kernel/vm_private.h"

typedef struct PageMapCall {
    uintptr_t vaddr;
    uintptr_t paddr;
    uint64_t flags;
} page_map_call_t;

static uint64_t g_table_pages[8][SV39_LEVEL_ENTRIES]
    __attribute__((aligned(MEMORY_PAGE_SIZE)));
static size_t g_next_table_page = 0;
static int g_alloc_zeroed_page_calls = 0;
static int g_pmm_free_page_calls = 0;
static void* g_last_freed_page = NULL;
static vm_address_space_t* g_active_address_space = NULL;
static int g_can_map_page_calls = 0;
static uintptr_t g_can_map_page_vaddrs[8];
static bool g_can_map_page_result = true;
static int g_map_page_calls = 0;
static page_map_call_t g_map_page_records[8];
static bool g_map_page_result = true;
static int g_unmap_page_calls = 0;
static uintptr_t g_unmap_page_vaddrs[8];
static int g_flush_if_enabled_calls = 0;
static int g_vm_flush_tlb_calls = 0;
static uint64_t g_last_satp_written = UINT64_MAX;
static uint64_t g_riscv_satp_value = 0;

static void reset_stub_state(void);
static int fail(const char* message);
static int test_lifecycle_enable_disable_and_destroy(void);
static int test_map_kernel_range_records_mapping_contract(void);
static int test_fault_range_registration_blocks_overlap_and_user_flags(void);

void* alloc_zeroed_page(void) {
    uint64_t* page = NULL;

    g_alloc_zeroed_page_calls += 1;
    if (g_next_table_page >= (sizeof(g_table_pages) / sizeof(g_table_pages[0]))) {
        return NULL;
    }

    page = g_table_pages[g_next_table_page++];
    memset(page, 0, MEMORY_PAGE_SIZE);
    return page;
}

bool pmm_free_page(void* page) {
    g_pmm_free_page_calls += 1;
    g_last_freed_page = page;
    return true;
}

bool map_page_internal(vm_address_space_t* address_space,
                       uintptr_t vaddr,
                       uintptr_t paddr,
                       uint64_t flags) {
    if (address_space == NULL || !g_map_page_result ||
        g_map_page_calls >= (int)(sizeof(g_map_page_records) /
                                  sizeof(g_map_page_records[0]))) {
        return false;
    }

    g_map_page_records[g_map_page_calls++] = (page_map_call_t){
        .vaddr = vaddr,
        .paddr = paddr,
        .flags = flags,
    };
    return true;
}

bool unmap_page_internal(vm_address_space_t* address_space, uintptr_t vaddr) {
    if (address_space == NULL ||
        g_unmap_page_calls >= (int)(sizeof(g_unmap_page_vaddrs) /
                                    sizeof(g_unmap_page_vaddrs[0]))) {
        return false;
    }

    g_unmap_page_vaddrs[g_unmap_page_calls++] = vaddr;
    return true;
}

bool can_map_page(vm_address_space_t* address_space, uintptr_t vaddr) {
    if (address_space == NULL ||
        g_can_map_page_calls >= (int)(sizeof(g_can_map_page_vaddrs) /
                                      sizeof(g_can_map_page_vaddrs[0]))) {
        return false;
    }

    g_can_map_page_vaddrs[g_can_map_page_calls++] = vaddr;
    return g_can_map_page_result;
}

void flush_tlb_if_enabled(void) {
    g_flush_if_enabled_calls += 1;
}

void runtime_context_activate_address_space(vm_address_space_t* address_space) {
    g_active_address_space = address_space;
}

bool runtime_context_address_space_is_active(
    const vm_address_space_t* address_space) {
    return address_space != NULL && address_space == g_active_address_space;
}

vm_address_space_t* runtime_context_active_address_space(void) {
    return g_active_address_space;
}

void runtime_context_clear_address_space(
    const vm_address_space_t* address_space) {
    if (address_space == g_active_address_space) {
        g_active_address_space = NULL;
    }
}

void vm_flush_tlb(void) {
    g_vm_flush_tlb_calls += 1;
}

uint64_t riscv_read_satp(void) {
    return g_riscv_satp_value;
}

void riscv_write_satp(uint64_t value) {
    g_last_satp_written = value;
    g_riscv_satp_value = value;
}

void riscv_sfence_vma(void) {}

bool vm_range_is_kernel(uintptr_t vaddr, size_t size) {
    return range_within_window(vaddr, size, VM_KERNEL_VADDR_BASE, VM_KERNEL_VADDR_LIMIT);
}

bool vm_range_is_user(uintptr_t vaddr, size_t size) {
    return range_within_window(vaddr, size, VM_USER_VADDR_BASE, VM_USER_VADDR_LIMIT);
}

static void reset_stub_state(void) {
    memset(g_table_pages, 0, sizeof(g_table_pages));
    g_next_table_page = 0;
    g_alloc_zeroed_page_calls = 0;
    g_pmm_free_page_calls = 0;
    g_last_freed_page = NULL;
    g_active_address_space = NULL;
    g_can_map_page_calls = 0;
    memset(g_can_map_page_vaddrs, 0, sizeof(g_can_map_page_vaddrs));
    g_can_map_page_result = true;
    g_map_page_calls = 0;
    memset(g_map_page_records, 0, sizeof(g_map_page_records));
    g_map_page_result = true;
    g_unmap_page_calls = 0;
    memset(g_unmap_page_vaddrs, 0, sizeof(g_unmap_page_vaddrs));
    g_flush_if_enabled_calls = 0;
    g_vm_flush_tlb_calls = 0;
    g_last_satp_written = UINT64_MAX;
    g_riscv_satp_value = 0;
}

static int fail(const char* message) {
    fprintf(stderr, "%s\n", message);
    return 1;
}

static int test_lifecycle_enable_disable_and_destroy(void) {
    vm_address_space_t* first = NULL;
    vm_address_space_t* second = NULL;
    const uint64_t* first_root = NULL;
    const uint64_t* second_root = NULL;

    reset_stub_state();
    if (!vm_address_space_create(&first) || !vm_address_space_create(&second)) {
        return fail("expected create to allocate two address spaces");
    }

    if (g_alloc_zeroed_page_calls != 2 || first == NULL || second == NULL ||
        !first->allocated || !second->allocated || first->enabled ||
        second->enabled || first->root_table == NULL || second->root_table == NULL ||
        first->satp_value == 0 || second->satp_value == 0) {
        return fail("expected create to initialize address-space metadata");
    }

    first_root = first->root_table;
    second_root = second->root_table;
    if (!vm_address_space_activate(first) || !vm_address_space_is_active(first)) {
        return fail("expected activate to bind runtime active address space");
    }

    if (!vm_address_space_enable(first) || !first->enabled ||
        g_last_satp_written != first->satp_value || g_vm_flush_tlb_calls != 1 ||
        g_active_address_space != first) {
        return fail("expected enable to write satp, flush tlb and activate runtime");
    }

    if (!vm_address_space_enable(second) || first->enabled || !second->enabled ||
        g_last_satp_written != second->satp_value || g_vm_flush_tlb_calls != 2 ||
        g_active_address_space != second) {
        return fail("expected enable to transfer active ownership to the new address space");
    }

    first->kernel_mappings[0].valid = true;
    first->kernel_fault_ranges[0].valid = true;
    first->fault_actions[0].valid = true;

    if (!vm_address_space_disable(second) || second->enabled ||
        g_last_satp_written != 0 || g_vm_flush_tlb_calls != 3 ||
        g_active_address_space != NULL) {
        return fail("expected disable to clear active satp state");
    }

    if (!vm_address_space_destroy(first) || !vm_address_space_destroy(second)) {
        return fail("expected destroy to succeed for unused address spaces");
    }

    if (g_pmm_free_page_calls != 2 || g_last_freed_page == NULL ||
        first->allocated || first->root_table != NULL || first->root_table_pa != 0 ||
        first->satp_value != 0 || first->enabled ||
        first->kernel_mappings[0].valid || first->kernel_fault_ranges[0].valid ||
        first->fault_actions[0].valid || g_last_freed_page != second_root) {
        return fail("expected destroy to clear lifecycle and bookkeeping state");
    }

    if (first_root == NULL || second_root == NULL) {
        return fail("expected root table pointers to be captured before destroy");
    }

    return 0;
}

static int test_map_kernel_range_records_mapping_contract(void) {
    vm_address_space_t* address_space = NULL;
    const uintptr_t vaddr = MEM_BASE + MEMORY_PAGE_SIZE;
    const uintptr_t paddr = MEM_BASE + 4U * MEMORY_PAGE_SIZE;
    const size_t size = 2U * MEMORY_PAGE_SIZE;
    const uint64_t flags = VM_PAGE_READ | VM_PAGE_WRITE;

    reset_stub_state();
    if (!vm_address_space_create(&address_space)) {
        return fail("expected create to succeed before kernel range map");
    }

    if (!vm_address_space_map_kernel_range(address_space, vaddr, paddr, size, flags)) {
        return fail("expected kernel range map to succeed");
    }

    if (g_can_map_page_calls != 2 || g_map_page_calls != 2 ||
        g_flush_if_enabled_calls != 1 || g_unmap_page_calls != 0) {
        return fail("expected kernel range map to validate pages and flush once");
    }

    if (g_can_map_page_vaddrs[0] != vaddr ||
        g_can_map_page_vaddrs[1] != vaddr + MEMORY_PAGE_SIZE ||
        g_map_page_records[0].vaddr != vaddr ||
        g_map_page_records[0].paddr != paddr ||
        g_map_page_records[0].flags != flags ||
        g_map_page_records[1].vaddr != vaddr + MEMORY_PAGE_SIZE ||
        g_map_page_records[1].paddr != paddr + MEMORY_PAGE_SIZE ||
        g_map_page_records[1].flags != flags) {
        return fail("expected kernel range map to forward per-page mapping coordinates");
    }

    if (!address_space->kernel_mappings[0].valid ||
        address_space->kernel_mappings[0].vaddr != vaddr ||
        address_space->kernel_mappings[0].paddr != paddr ||
        address_space->kernel_mappings[0].size != size ||
        address_space->kernel_mappings[0].flags != flags) {
        return fail("expected kernel range map to record kernel mapping metadata");
    }

    if (!vm_address_space_destroy(address_space)) {
        return fail("expected destroy after kernel range map to succeed");
    }

    return 0;
}

static int test_fault_range_registration_blocks_overlap_and_user_flags(void) {
    vm_address_space_t* address_space = NULL;
    const uintptr_t vaddr = MEM_BASE + 8U * MEMORY_PAGE_SIZE;
    const uintptr_t paddr = MEM_BASE + 12U * MEMORY_PAGE_SIZE;
    const size_t size = MEMORY_PAGE_SIZE;
    const uint64_t flags = VM_PAGE_READ | VM_PAGE_WRITE;

    reset_stub_state();
    if (!vm_address_space_create(&address_space)) {
        return fail("expected create to succeed before fault-range registration");
    }

    if (!vm_address_space_register_fault_range(address_space, vaddr, paddr, size, flags)) {
        return fail("expected fault-range registration to succeed");
    }

    if (!address_space->kernel_fault_ranges[0].valid ||
        address_space->kernel_fault_ranges[0].vaddr != vaddr ||
        address_space->kernel_fault_ranges[0].paddr != paddr ||
        address_space->kernel_fault_ranges[0].size != size ||
        address_space->kernel_fault_ranges[0].flags != flags ||
        g_map_page_calls != 0 || g_flush_if_enabled_calls != 0) {
        return fail("expected fault-range registration to record metadata only");
    }

    if (vm_address_space_register_fault_range(address_space,
                                              vaddr + MEMORY_PAGE_SIZE,
                                              paddr + MEMORY_PAGE_SIZE,
                                              size,
                                              flags | VM_PAGE_USER)) {
        return fail("expected user fault-range registration to be rejected");
    }

    if (vm_address_space_map_kernel_range(address_space, vaddr, paddr, size, flags)) {
        return fail("expected mapped kernel range to reject overlap with fault range");
    }

    if (!vm_address_space_destroy(address_space)) {
        return fail("expected destroy after fault-range registration to succeed");
    }

    return 0;
}

int main(void) {
    if (test_lifecycle_enable_disable_and_destroy() != 0 ||
        test_map_kernel_range_records_mapping_contract() != 0 ||
        test_fault_range_registration_blocks_overlap_and_user_flags() != 0) {
        return 1;
    }

    return 0;
}
