#include <stddef.h>
#include <stdint.h>

#include "pmm.h"
#include "riscv.h"
#include "runtime_context.h"
#include "vm_private.h"

static vm_address_space_t address_space_pool[VM_MAX_ADDRESS_SPACES];

static uint64_t* alloc_table_page(void) {
    return (uint64_t*)alloc_zeroed_page();
}

static bool address_space_storage_ready(const vm_address_space_t* address_space) {
    return address_space != NULL && address_space->allocated &&
           address_space->root_table != NULL;
}

static bool range_overlaps_fault_ranges(const struct VmFaultRange* ranges,
                                        size_t count,
                                        uintptr_t vaddr,
                                        size_t size) {
    size_t i = 0;

    for (i = 0; i < count; ++i) {
        const struct VmFaultRange* range = &ranges[i];

        if (!range->valid) {
            continue;
        }

        if (ranges_overlap(vaddr, size, range->vaddr, range->size)) {
            return true;
        }
    }

    return false;
}

static bool range_overlaps_user_regions(const vm_address_space_t* address_space,
                                        uintptr_t vaddr,
                                        size_t size) {
    size_t i = 0;

    if (address_space == NULL) {
        return false;
    }

    for (i = 0; i < VM_MAX_USER_REGIONS; ++i) {
        const vm_user_region_t* region = address_space->user_regions[i];

        if (!user_region_descriptor_valid(region)) {
            continue;
        }

        if (ranges_overlap(vaddr, size, region->vaddr, region->size)) {
            return true;
        }
    }

    return false;
}

static bool range_overlaps_kernel_globals(const vm_address_space_t* address_space,
                                          uintptr_t vaddr,
                                          size_t size) {
    if (address_space == NULL) {
        return false;
    }

    return range_overlaps_fault_ranges(address_space->kernel_mappings,
                                       VM_MAX_KERNEL_MAPPINGS,
                                       vaddr,
                                       size) ||
           range_overlaps_fault_ranges(address_space->kernel_fault_ranges,
                                       VM_MAX_KERNEL_FAULT_RANGES,
                                       vaddr,
                                       size);
}

static bool range_is_platform_mmio(uintptr_t vaddr, size_t size) {
    return range_within_window(vaddr, size, UART_BASE, UART_BASE + MEMORY_PAGE_SIZE) ||
           range_within_window(vaddr, size, CLINT_BASE, CLINT_BASE + CLINT_SIZE) ||
           range_within_window(vaddr, size, PLIC_BASE, PLIC_BASE + PLIC_SIZE) ||
           range_within_window(vaddr,
                               size,
                               STORAGE_BASE,
                               STORAGE_BASE + MEMORY_PAGE_SIZE) ||
           range_within_window(vaddr,
                               size,
                               AI_ACCEL_BASE,
                               AI_ACCEL_BASE + MEMORY_PAGE_SIZE);
}

static struct VmFaultRange* find_free_fault_range_slot(struct VmFaultRange* ranges,
                                                       size_t count) {
    size_t i = 0;

    for (i = 0; i < count; ++i) {
        if (!ranges[i].valid) {
            return &ranges[i];
        }
    }

    return NULL;
}

static vm_user_region_t** find_free_user_region_slot(vm_address_space_t* address_space) {
    size_t i = 0;

    if (address_space == NULL) {
        return NULL;
    }

    for (i = 0; i < VM_MAX_USER_REGIONS; ++i) {
        if (address_space->user_regions[i] == NULL) {
            return &address_space->user_regions[i];
        }
    }

    return NULL;
}

static vm_user_region_t** find_address_space_region_slot(
    vm_address_space_t* address_space,
    const vm_user_region_t* region) {
    size_t i = 0;

    if (address_space == NULL || region == NULL) {
        return NULL;
    }

    for (i = 0; i < VM_MAX_USER_REGIONS; ++i) {
        if (address_space->user_regions[i] == region) {
            return &address_space->user_regions[i];
        }
    }

    return NULL;
}

static bool register_user_region(vm_address_space_t* address_space,
                                 vm_user_region_t* region) {
    vm_user_region_t** slot = NULL;

    if (address_space == NULL || address_space->root_table == NULL ||
        region == NULL || region->registered ||
        !page_span_args_valid(region->vaddr, region->size) ||
        !user_flags_valid(region->flags) ||
        !vm_range_is_user(region->vaddr, region->size) ||
        range_overlaps_kernel_globals(address_space, region->vaddr, region->size) ||
        range_overlaps_user_regions(address_space, region->vaddr, region->size)) {
        return false;
    }

    slot = find_free_user_region_slot(address_space);
    if (slot == NULL) {
        return false;
    }

    region->address_space = address_space;
    region->registered = true;
    region->object = NULL;
    region->object_offset = 0;
    region->object_mode = VM_REGION_OBJECT_NONE;
    *slot = region;
    return true;
}

static void clear_fault_ranges(struct VmFaultRange* ranges, size_t count) {
    size_t i = 0;

    for (i = 0; i < count; ++i) {
        ranges[i].valid = false;
        ranges[i].vaddr = 0;
        ranges[i].paddr = 0;
        ranges[i].size = 0;
        ranges[i].flags = 0;
    }
}

static void clear_fault_actions(struct VmFaultActionRule* actions, size_t count) {
    size_t i = 0;

    for (i = 0; i < count; ++i) {
        actions[i].valid = false;
        actions[i].cause = 0;
        actions[i].vaddr = 0;
        actions[i].size = 0;
        actions[i].action = VM_FAULT_ACTION_SKIP_INSTRUCTION;
        actions[i].resume_pc_slot = NULL;
    }
}

static void clear_user_region_slots(vm_user_region_t** user_regions, size_t count) {
    size_t i = 0;

    for (i = 0; i < count; ++i) {
        user_regions[i] = NULL;
    }
}

static void initialize_address_space_tracking(vm_address_space_t* address_space) {
    if (address_space == NULL) {
        return;
    }

    clear_fault_ranges(address_space->kernel_mappings, VM_MAX_KERNEL_MAPPINGS);
    clear_fault_ranges(address_space->kernel_fault_ranges,
                       VM_MAX_KERNEL_FAULT_RANGES);
    clear_fault_actions(address_space->fault_actions, VM_MAX_FAULT_ACTIONS);
    clear_user_region_slots(address_space->user_regions, VM_MAX_USER_REGIONS);
}

static void initialize_address_space(vm_address_space_t* address_space,
                                     uint64_t* root_table) {
    if (address_space == NULL || root_table == NULL) {
        return;
    }

    address_space->allocated = true;
    address_space->root_table = root_table;
    address_space->root_table_pa = (uintptr_t)root_table;
    address_space->satp_value =
        RISCV_SATP_MODE_SV39 |
        (((uint64_t)(address_space->root_table_pa >> SV39_PAGE_SHIFT)) &
         SV39_PPN_MASK);
    address_space->enabled = false;
    initialize_address_space_tracking(address_space);
}

static void reset_address_space(vm_address_space_t* address_space) {
    if (address_space == NULL) {
        return;
    }

    address_space->allocated = false;
    address_space->root_table = NULL;
    address_space->root_table_pa = 0;
    address_space->satp_value = 0;
    address_space->enabled = false;
    initialize_address_space_tracking(address_space);
}

static bool free_table_pages_recursive(uint64_t* table, unsigned level) {
    size_t i = 0;
    bool ok = true;

    if (table == NULL) {
        return false;
    }

    for (i = 0; i < SV39_LEVEL_ENTRIES; ++i) {
        const uint64_t entry = table[i];

        table[i] = 0;
        if ((entry & SV39_PTE_VALID) == 0 ||
            (entry & SV39_PTE_LEAF_MASK) != 0 ||
            level == 0) {
            continue;
        }

        if (!free_table_pages_recursive(table_from_pte(entry), level - 1U)) {
            ok = false;
        }
    }

    return pmm_free_page(table) && ok;
}

static bool map_range_pages_internal(vm_address_space_t* address_space,
                                     uintptr_t vaddr,
                                     uintptr_t paddr,
                                     size_t size,
                                     uint64_t flags) {
    size_t offset = 0;
    size_t mapped = 0;

    if (address_space == NULL || address_space->root_table == NULL ||
        !mapped_range_args_valid(vaddr, paddr, size) ||
        !flags_valid(flags)) {
        return false;
    }

    while (offset < size) {
        if (!can_map_page(address_space, vaddr + offset)) {
            return false;
        }
        offset += MEMORY_PAGE_SIZE;
    }

    offset = 0;
    while (offset < size) {
        if (!map_page_internal(address_space, vaddr + offset, paddr + offset, flags)) {
            size_t rollback = 0;

            while (rollback < mapped) {
                unmap_page_internal(address_space, vaddr + rollback);
                rollback += MEMORY_PAGE_SIZE;
            }
            return false;
        }

        mapped += MEMORY_PAGE_SIZE;
        offset += MEMORY_PAGE_SIZE;
    }

    return true;
}

static bool map_kernel_global_range(vm_address_space_t* address_space,
                                    uintptr_t vaddr,
                                    uintptr_t paddr,
                                    size_t size,
                                    uint64_t flags) {
    struct VmFaultRange* record = NULL;

    if (!address_space_storage_ready(address_space) ||
        !mapped_range_args_valid(vaddr, paddr, size) ||
        !kernel_flags_valid(flags) ||
        range_overlaps_kernel_globals(address_space, vaddr, size) ||
        range_overlaps_user_regions(address_space, vaddr, size)) {
        return false;
    }

    record = find_free_fault_range_slot(address_space->kernel_mappings,
                                        VM_MAX_KERNEL_MAPPINGS);
    if (record == NULL ||
        !map_range_pages_internal(address_space, vaddr, paddr, size, flags)) {
        return false;
    }

    record->valid = true;
    record->vaddr = vaddr;
    record->paddr = paddr;
    record->size = size;
    record->flags = flags;
    flush_tlb_if_enabled();
    return true;
}

static bool kernel_fault_range_args_valid(const vm_address_space_t* address_space,
                                          uintptr_t vaddr,
                                          uintptr_t paddr,
                                          size_t size,
                                          uint64_t flags) {
    const bool in_kernel_window = vm_range_is_kernel(vaddr, size);
    const bool in_platform_mmio = range_is_platform_mmio(vaddr, size);

    return address_space_storage_ready(address_space) &&
           mapped_range_args_valid(vaddr, paddr, size) &&
           kernel_flags_valid(flags) &&
           (in_kernel_window || in_platform_mmio) &&
           !range_overlaps_kernel_globals(address_space, vaddr, size) &&
           !range_overlaps_user_regions(address_space, vaddr, size);
}

static struct VmFaultRange* reserve_kernel_fault_range(
    vm_address_space_t* address_space,
    uintptr_t vaddr,
    uintptr_t paddr,
    size_t size,
    uint64_t flags) {
    if (!kernel_fault_range_args_valid(address_space, vaddr, paddr, size, flags)) {
        return NULL;
    }

    return find_free_fault_range_slot(address_space->kernel_fault_ranges,
                                      VM_MAX_KERNEL_FAULT_RANGES);
}

static void write_fault_range_record(struct VmFaultRange* record,
                                     uintptr_t vaddr,
                                     uintptr_t paddr,
                                     size_t size,
                                     uint64_t flags) {
    if (record == NULL) {
        return;
    }

    record->valid = true;
    record->vaddr = vaddr;
    record->paddr = paddr;
    record->size = size;
    record->flags = flags;
}

static bool register_kernel_fault_range(vm_address_space_t* address_space,
                                        uintptr_t vaddr,
                                        uintptr_t paddr,
                                        size_t size,
                                        uint64_t flags) {
    struct VmFaultRange* record = NULL;

    record = reserve_kernel_fault_range(address_space, vaddr, paddr, size, flags);
    if (record == NULL) {
        return false;
    }

    write_fault_range_record(record, vaddr, paddr, size, flags);
    return true;
}

bool vm_address_space_create(vm_address_space_t** out_space) {
    size_t i = 0;
    vm_address_space_t* space = NULL;
    uint64_t* root_table = NULL;

    if (out_space == NULL) {
        return false;
    }

    for (i = 0; i < VM_MAX_ADDRESS_SPACES; ++i) {
        if (!address_space_pool[i].allocated) {
            space = &address_space_pool[i];
            break;
        }
    }

    if (space == NULL) {
        return false;
    }

    root_table = alloc_table_page();
    if (root_table == NULL) {
        return false;
    }

    initialize_address_space(space, root_table);
    *out_space = space;
    return true;
}

bool vm_address_space_activate(vm_address_space_t* address_space) {
    if (!address_space_storage_ready(address_space)) {
        return false;
    }

    runtime_context_activate_address_space(address_space);
    return true;
}

bool vm_address_space_is_active(const vm_address_space_t* address_space) {
    return runtime_context_address_space_is_active(address_space);
}

bool vm_address_space_is_enabled(const vm_address_space_t* address_space) {
    return address_space != NULL && address_space->enabled;
}

bool vm_address_space_disable(vm_address_space_t* address_space) {
    if (!address_space_storage_ready(address_space)) {
        return false;
    }

    if (vm_address_space_is_active(address_space) && address_space->enabled) {
        riscv_write_satp(0);
        vm_flush_tlb();
    }

    runtime_context_clear_address_space(address_space);
    address_space->enabled = false;
    return true;
}

bool vm_address_space_destroy(vm_address_space_t* address_space) {
    size_t i = 0;
    bool disabled = false;
    bool freed = false;

    if (!address_space_storage_ready(address_space)) {
        return false;
    }

    for (i = 0; i < VM_MAX_USER_REGIONS; ++i) {
        if (address_space->user_regions[i] != NULL) {
            return false;
        }
    }

    disabled = vm_address_space_disable(address_space);
    freed = disabled &&
            free_table_pages_recursive(address_space->root_table, 2U);
    if (!disabled || !freed) {
        reset_address_space(address_space);
        return false;
    }

    reset_address_space(address_space);
    return true;
}

bool vm_address_space_map_identity_1g(vm_address_space_t* address_space,
                                      uintptr_t base,
                                      uint64_t flags) {
    const size_t index = vpn_index(base, 2);
    struct VmFaultRange* record = NULL;

    if (address_space == NULL || address_space->root_table == NULL ||
        (base & (SV39_SUPERPAGE_SIZE_1G - 1U)) != 0 ||
        !kernel_flags_valid(flags) ||
        range_overlaps_kernel_globals(address_space, base, SV39_SUPERPAGE_SIZE_1G) ||
        range_overlaps_user_regions(address_space, base, SV39_SUPERPAGE_SIZE_1G)) {
        return false;
    }

    if ((address_space->root_table[index] & SV39_PTE_VALID) != 0) {
        return false;
    }

    record = find_free_fault_range_slot(address_space->kernel_mappings,
                                        VM_MAX_KERNEL_MAPPINGS);
    if (record == NULL) {
        return false;
    }

    address_space->root_table[index] = pte_from_pa(base, SV39_PTE_VALID | flags);
    record->valid = true;
    record->vaddr = base;
    record->paddr = base;
    record->size = SV39_SUPERPAGE_SIZE_1G;
    record->flags = flags;
    flush_tlb_if_enabled();
    return true;
}

bool vm_address_space_map_kernel_range(vm_address_space_t* address_space,
                                       uintptr_t vaddr,
                                       uintptr_t paddr,
                                       size_t size,
                                       uint64_t flags) {
    if (!vm_range_is_kernel(vaddr, size) &&
        !range_is_platform_mmio(vaddr, size)) {
        return false;
    }

    return map_kernel_global_range(address_space, vaddr, paddr, size, flags);
}

bool vm_address_space_user_region_init(vm_address_space_t* address_space,
                                       vm_user_region_t* region,
                                       uintptr_t vaddr,
                                       size_t size,
                                       uint64_t flags) {
    if (region == NULL) {
        return false;
    }

    region->address_space = NULL;
    region->vaddr = vaddr;
    region->size = size;
    region->flags = flags;
    region->registered = false;
    region->object = NULL;
    region->object_offset = 0;
    region->object_mode = VM_REGION_OBJECT_NONE;

    return register_user_region(address_space, region);
}

bool vm_address_space_unregister_user_region_internal(
    vm_address_space_t* address_space,
    vm_user_region_t* region) {
    vm_user_region_t** slot = find_address_space_region_slot(address_space, region);

    if (slot == NULL) {
        return false;
    }

    *slot = NULL;
    return true;
}

bool vm_address_space_register_fault_range(vm_address_space_t* address_space,
                                           uintptr_t vaddr,
                                           uintptr_t paddr,
                                           size_t size,
                                           uint64_t flags) {
    return register_kernel_fault_range(address_space, vaddr, paddr, size, flags);
}

bool vm_address_space_enable(vm_address_space_t* address_space) {
    if (address_space == NULL || address_space->root_table == NULL ||
        address_space->satp_value == 0) {
        return false;
    }

    if (runtime_context_active_address_space() != NULL) {
        runtime_context_active_address_space()->enabled = false;
    }

    riscv_write_satp(address_space->satp_value);
    vm_flush_tlb();
    runtime_context_activate_address_space(address_space);
    address_space->enabled = true;
    return true;
}

uintptr_t vm_address_space_root_table(const vm_address_space_t* address_space) {
    if (address_space == NULL || !address_space->allocated) {
        return 0;
    }

    return address_space->root_table_pa;
}

uint64_t vm_address_space_satp_value(const vm_address_space_t* address_space) {
    if (address_space == NULL || !address_space->allocated) {
        return 0;
    }

    return address_space->satp_value;
}
