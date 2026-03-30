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

static int g_map_page_calls = 0;
static page_map_call_t g_last_map = {0};
static bool g_map_page_result = true;
static int g_object_resolve_calls = 0;
static vm_object_t* g_last_resolve_object = NULL;
static size_t g_last_resolve_offset = 0;
static bool g_last_resolve_create = false;
static uintptr_t g_resolved_paddr = 0;
static bool g_object_resolve_result = true;
static int g_vm_flush_tlb_calls = 0;
static uint64_t g_last_sepc_written = UINT64_MAX;

static void reset_stub_state(void);
static int fail(const char* message);
static int test_register_fault_actions(void);
static int test_handle_page_fault_maps_user_region_object(void);
static int test_handle_page_fault_maps_kernel_fault_range(void);
static int test_handle_page_fault_applies_skip_and_resume_actions(void);

bool map_page_internal(vm_address_space_t* address_space,
                       uintptr_t vaddr,
                       uintptr_t paddr,
                       uint64_t flags) {
    if (address_space == NULL || !g_map_page_result) {
        return false;
    }

    g_map_page_calls += 1;
    g_last_map = (page_map_call_t){
        .vaddr = vaddr,
        .paddr = paddr,
        .flags = flags,
    };
    return true;
}

bool object_resolve_page(vm_object_t* object,
                         size_t offset,
                         bool create,
                         uintptr_t* out_paddr) {
    g_object_resolve_calls += 1;
    g_last_resolve_object = object;
    g_last_resolve_offset = offset;
    g_last_resolve_create = create;
    if (!g_object_resolve_result || out_paddr == NULL) {
        return false;
    }

    *out_paddr = g_resolved_paddr;
    return true;
}

void vm_flush_tlb(void) {
    g_vm_flush_tlb_calls += 1;
}

void riscv_write_sepc(uint64_t value) {
    g_last_sepc_written = value;
}

bool vm_user_region_contains(const vm_user_region_t* region,
                             uintptr_t vaddr,
                             size_t size) {
    return region != NULL && region->registered &&
           range_within_window(vaddr,
                               size,
                               region->vaddr,
                               region->vaddr + (uintptr_t)region->size);
}

static void reset_stub_state(void) {
    g_map_page_calls = 0;
    memset(&g_last_map, 0, sizeof(g_last_map));
    g_map_page_result = true;
    g_object_resolve_calls = 0;
    g_last_resolve_object = NULL;
    g_last_resolve_offset = 0;
    g_last_resolve_create = false;
    g_resolved_paddr = 0;
    g_object_resolve_result = true;
    g_vm_flush_tlb_calls = 0;
    g_last_sepc_written = UINT64_MAX;
}

static int fail(const char* message) {
    fprintf(stderr, "%s\n", message);
    return 1;
}

static int test_register_fault_actions(void) {
    vm_address_space_t address_space = {
        .allocated = true,
        .root_table = (uint64_t*)MEM_BASE,
    };
    uintptr_t resume_pc_slot = 0;

    reset_stub_state();
    if (!vm_address_space_register_fault_skip(&address_space,
                                              RISCV_EXC_LOAD_PAGE_FAULT,
                                              0x1000,
                                              MEMORY_PAGE_SIZE)) {
        return fail("expected fault-skip registration to succeed");
    }

    if (vm_address_space_register_fault_skip(&address_space,
                                             RISCV_EXC_LOAD_PAGE_FAULT,
                                             0x1800,
                                             MEMORY_PAGE_SIZE)) {
        return fail("expected overlapping fault-skip registration to fail");
    }

    if (vm_address_space_register_fault_resume_slot(&address_space,
                                                    RISCV_EXC_STORE_PAGE_FAULT,
                                                    0x3000,
                                                    MEMORY_PAGE_SIZE,
                                                    NULL)) {
        return fail("expected resume-slot registration without slot to fail");
    }

    if (!vm_address_space_register_fault_resume_slot(&address_space,
                                                     RISCV_EXC_STORE_PAGE_FAULT,
                                                     0x3000,
                                                     MEMORY_PAGE_SIZE,
                                                     &resume_pc_slot)) {
        return fail("expected resume-slot registration to succeed");
    }

    if (!address_space.fault_actions[0].valid ||
        address_space.fault_actions[0].action != VM_FAULT_ACTION_SKIP_INSTRUCTION ||
        !address_space.fault_actions[1].valid ||
        address_space.fault_actions[1].action != VM_FAULT_ACTION_RESUME_AT_SLOT) {
        return fail("expected fault-action registration to record action metadata");
    }

    return 0;
}

static int test_handle_page_fault_maps_user_region_object(void) {
    vm_address_space_t address_space = {
        .allocated = true,
        .root_table = (uint64_t*)MEM_BASE,
    };
    vm_object_t object = {
        .initialized = true,
        .backing_kind = VM_OBJECT_BACKING_PHYSICAL,
        .size = MEMORY_PAGE_SIZE,
        .backing.physical.base_paddr = 9U * MEMORY_PAGE_SIZE,
    };
    vm_user_region_t region = {
        .address_space = &address_space,
        .vaddr = 0,
        .size = MEMORY_PAGE_SIZE,
        .flags = VM_PAGE_READ | VM_PAGE_WRITE | VM_PAGE_USER,
        .registered = true,
        .object = &object,
        .object_mode = VM_REGION_OBJECT_FAULT,
    };
    vm_process_t process = {
        .address_space = &address_space,
        .user_regions = {&region},
    };

    reset_stub_state();
    g_resolved_paddr = 11U * MEMORY_PAGE_SIZE;
    if (!vm_handle_page_fault(&process,
                              &address_space,
                              RISCV_EXC_LOAD_PAGE_FAULT,
                              0x200,
                              0x80)) {
        return fail("expected user-region page fault handling to succeed");
    }

    if (g_object_resolve_calls != 1 || g_last_resolve_object != &object ||
        g_last_resolve_offset != 0 || !g_last_resolve_create ||
        g_map_page_calls != 1 || g_last_map.vaddr != 0 ||
        g_last_map.paddr != g_resolved_paddr ||
        g_last_map.flags != region.flags || g_vm_flush_tlb_calls != 1) {
        return fail("expected user-region page fault to resolve object page and map it");
    }

    return 0;
}

static int test_handle_page_fault_maps_kernel_fault_range(void) {
    vm_address_space_t address_space = {
        .allocated = true,
        .root_table = (uint64_t*)MEM_BASE,
    };

    reset_stub_state();
    address_space.kernel_fault_ranges[0] = (struct VmFaultRange){
        .valid = true,
        .vaddr = 0x4000,
        .paddr = 0x8000,
        .size = 2U * MEMORY_PAGE_SIZE,
        .flags = VM_PAGE_READ,
    };

    if (!vm_handle_page_fault(NULL,
                              &address_space,
                              RISCV_EXC_LOAD_PAGE_FAULT,
                              0x100,
                              0x5008)) {
        return fail("expected kernel fault-range page fault handling to succeed");
    }

    if (g_object_resolve_calls != 0 || g_map_page_calls != 1 ||
        g_last_map.vaddr != 0x5000 || g_last_map.paddr != 0x9000 ||
        g_last_map.flags != VM_PAGE_READ || g_vm_flush_tlb_calls != 1) {
        return fail("expected kernel fault range to map faulting page directly");
    }

    return 0;
}

static int test_handle_page_fault_applies_skip_and_resume_actions(void) {
    vm_address_space_t address_space = {
        .allocated = true,
        .root_table = (uint64_t*)MEM_BASE,
    };
    uintptr_t resume_pc_slot = 0x9000;

    reset_stub_state();
    if (!vm_address_space_register_fault_skip(&address_space,
                                              RISCV_EXC_LOAD_PAGE_FAULT,
                                              0x1000,
                                              MEMORY_PAGE_SIZE) ||
        !vm_address_space_register_fault_resume_slot(&address_space,
                                                     RISCV_EXC_STORE_PAGE_FAULT,
                                                     0x2000,
                                                     MEMORY_PAGE_SIZE,
                                                     &resume_pc_slot)) {
        return fail("expected fault actions to register before handling");
    }

    if (!vm_handle_page_fault(NULL,
                              &address_space,
                              RISCV_EXC_LOAD_PAGE_FAULT,
                              0x1234,
                              0x1008) ||
        g_last_sepc_written != 0x1238) {
        return fail("expected skip action to advance sepc");
    }

    if (!vm_handle_page_fault(NULL,
                              &address_space,
                              RISCV_EXC_STORE_PAGE_FAULT,
                              0x2222,
                              0x2008) ||
        g_last_sepc_written != 0x9000 || resume_pc_slot != 0) {
        return fail("expected resume-slot action to redirect sepc and clear slot");
    }

    return 0;
}

int main(void) {
    if (test_register_fault_actions() != 0 ||
        test_handle_page_fault_maps_user_region_object() != 0 ||
        test_handle_page_fault_maps_kernel_fault_range() != 0 ||
        test_handle_page_fault_applies_skip_and_resume_actions() != 0) {
        return 1;
    }

    return 0;
}
