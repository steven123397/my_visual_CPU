#include "vm.h"

#include <stddef.h>
#include <stdint.h>

#include "platform_mmio.h"
#include "memory.h"
#include "pmm.h"
#include "riscv.h"
#include "runtime_context.h"

#define SV39_PAGE_SHIFT 12U
#define SV39_LEVEL_BITS 9U
#define SV39_LEVEL_ENTRIES 512U
#define SV39_SUPERPAGE_SIZE_1G (1UL << 30)
#define SV39_PTE_VALID (1ULL << 0)
#define SV39_PTE_LEAF_MASK (VM_PAGE_READ | VM_PAGE_WRITE | VM_PAGE_EXEC)
#define SV39_PTE_FLAG_MASK \
    (VM_PAGE_READ | VM_PAGE_WRITE | VM_PAGE_EXEC | VM_PAGE_USER)
#define SV39_PPN_MASK ((1ULL << 44) - 1ULL)
#define VM_MAX_KERNEL_MAPPINGS 16U
#define VM_MAX_KERNEL_FAULT_RANGES 16U
#define VM_MAX_USER_REGIONS 16U
#define VM_MAX_ADDRESS_SPACES 2U
#define VM_MAX_FAULT_ACTIONS 16U
#define VM_USER_VADDR_BASE ((uintptr_t)0)
#define VM_USER_VADDR_LIMIT ((uintptr_t)MEM_BASE)
#define VM_KERNEL_VADDR_BASE ((uintptr_t)MEM_BASE)
#define VM_KERNEL_VADDR_LIMIT ((uintptr_t)MEM_BASE + (uintptr_t)MEM_SIZE)

struct VmFaultRange {
    bool valid;
    uintptr_t vaddr;
    uintptr_t paddr;
    size_t size;
    uint64_t flags;
};

typedef enum VmFaultAction {
    VM_FAULT_ACTION_SKIP_INSTRUCTION = 0,
    VM_FAULT_ACTION_RESUME_AT_SLOT,
} vm_fault_action_t;

struct VmFaultActionRule {
    bool valid;
    uint64_t cause;
    uintptr_t vaddr;
    size_t size;
    vm_fault_action_t action;
    volatile uintptr_t* resume_pc_slot;
};

struct VmAddressSpace {
    bool allocated;
    uint64_t* root_table;
    uintptr_t root_table_pa;
    uint64_t satp_value;
    bool enabled;
    struct VmFaultRange kernel_mappings[VM_MAX_KERNEL_MAPPINGS];
    struct VmFaultRange kernel_fault_ranges[VM_MAX_KERNEL_FAULT_RANGES];
    struct VmFaultActionRule fault_actions[VM_MAX_FAULT_ACTIONS];
    vm_user_region_t* user_regions[VM_MAX_USER_REGIONS];
};

static vm_address_space_t address_space_pool[VM_MAX_ADDRESS_SPACES];

static void flush_tlb_if_enabled(void);
static bool region_contains_vaddr(const vm_user_region_t* region, uintptr_t vaddr);

static size_t vpn_index(uintptr_t vaddr, unsigned level) {
    return (size_t)((vaddr >> (SV39_PAGE_SHIFT + level * SV39_LEVEL_BITS)) &
                    (SV39_LEVEL_ENTRIES - 1U));
}

static uintptr_t align_down_page(uintptr_t value) {
    return value & ~((uintptr_t)MEMORY_PAGE_SIZE - 1U);
}

static bool range_overflows(uintptr_t base, size_t size) {
    if (size == 0) {
        return false;
    }

    return base > UINTPTR_MAX - ((uintptr_t)size - 1U);
}

static bool span_args_valid(uintptr_t base, size_t size) {
    return size != 0 && !range_overflows(base, size);
}

static bool range_within_window(uintptr_t base,
                                size_t size,
                                uintptr_t window_base,
                                uintptr_t window_limit) {
    const uintptr_t end = base + (uintptr_t)size;

    if (!span_args_valid(base, size)) {
        return false;
    }

    return base >= window_base && end <= window_limit;
}

static bool page_span_args_valid(uintptr_t base, size_t size) {
    if (size == 0 ||
        (base & (MEMORY_PAGE_SIZE - 1U)) != 0 ||
        (size & (MEMORY_PAGE_SIZE - 1U)) != 0) {
        return false;
    }

    return !range_overflows(base, size);
}

static bool mapped_range_args_valid(uintptr_t vaddr, uintptr_t paddr, size_t size) {
    if (!page_span_args_valid(vaddr, size) ||
        (paddr & (MEMORY_PAGE_SIZE - 1U)) != 0) {
        return false;
    }

    return !range_overflows(paddr, size);
}

static bool flags_valid(uint64_t flags) {
    if ((flags & ~SV39_PTE_FLAG_MASK) != 0) {
        return false;
    }

    if ((flags & SV39_PTE_LEAF_MASK) == 0) {
        return false;
    }

    if ((flags & VM_PAGE_WRITE) != 0 && (flags & VM_PAGE_READ) == 0) {
        return false;
    }

    return true;
}

static bool user_flags_valid(uint64_t flags) {
    return flags_valid(flags) && (flags & VM_PAGE_USER) != 0;
}

static bool kernel_flags_valid(uint64_t flags) {
    return flags_valid(flags) && (flags & VM_PAGE_USER) == 0;
}

static bool page_fault_cause_valid(uint64_t cause) {
    return cause == RISCV_EXC_INSN_PAGE_FAULT ||
           cause == RISCV_EXC_LOAD_PAGE_FAULT ||
           cause == RISCV_EXC_STORE_PAGE_FAULT;
}

static uint64_t pte_from_pa(uintptr_t paddr, uint64_t flags) {
    return (((uint64_t)(paddr >> SV39_PAGE_SHIFT) & SV39_PPN_MASK) << 10) |
           flags;
}

static uint64_t* table_from_pte(uint64_t pte) {
    const uintptr_t table_pa =
        (uintptr_t)(((pte >> 10) & SV39_PPN_MASK) << SV39_PAGE_SHIFT);
    return (uint64_t*)table_pa;
}

static void zero_page(void* page) {
    size_t i = 0;
    uint64_t* words = (uint64_t*)page;

    for (i = 0; i < MEMORY_PAGE_SIZE / sizeof(uint64_t); ++i) {
        words[i] = 0;
    }
}

static uint64_t* alloc_table_page(void) {
    uint64_t* page = (uint64_t*)pmm_alloc_page();

    if (page == NULL) {
        return NULL;
    }

    zero_page(page);
    return page;
}

static uint64_t* lookup_level0_slot(vm_address_space_t* address_space,
                                    uintptr_t vaddr,
                                    bool create,
                                    bool* conflict) {
    uint64_t* level1 = NULL;
    uint64_t* level0 = NULL;
    const size_t root_index = vpn_index(vaddr, 2);
    const size_t level1_index = vpn_index(vaddr, 1);
    const size_t level0_index = vpn_index(vaddr, 0);
    uint64_t entry = 0;

    if (address_space == NULL || address_space->root_table == NULL) {
        return NULL;
    }

    if (conflict != NULL) {
        *conflict = false;
    }

    entry = address_space->root_table[root_index];
    if ((entry & SV39_PTE_VALID) == 0) {
        if (!create) {
            return NULL;
        }

        level1 = alloc_table_page();
        if (level1 == NULL) {
            return NULL;
        }

        address_space->root_table[root_index] =
            pte_from_pa((uintptr_t)level1, SV39_PTE_VALID);
    } else if ((entry & SV39_PTE_LEAF_MASK) != 0) {
        if (conflict != NULL) {
            *conflict = true;
        }
        return NULL;
    } else {
        level1 = table_from_pte(entry);
    }

    entry = level1[level1_index];
    if ((entry & SV39_PTE_VALID) == 0) {
        if (!create) {
            return NULL;
        }

        level0 = alloc_table_page();
        if (level0 == NULL) {
            return NULL;
        }

        level1[level1_index] = pte_from_pa((uintptr_t)level0, SV39_PTE_VALID);
    } else if ((entry & SV39_PTE_LEAF_MASK) != 0) {
        if (conflict != NULL) {
            *conflict = true;
        }
        return NULL;
    } else {
        level0 = table_from_pte(entry);
    }

    return &level0[level0_index];
}

static bool map_page_internal(vm_address_space_t* address_space,
                              uintptr_t vaddr,
                              uintptr_t paddr,
                              uint64_t flags) {
    bool conflict = false;
    uint64_t* slot = NULL;

    if (!mapped_range_args_valid(vaddr, paddr, MEMORY_PAGE_SIZE) ||
        !flags_valid(flags)) {
        return false;
    }

    slot = lookup_level0_slot(address_space, vaddr, true, &conflict);
    if (slot == NULL || conflict || (*slot & SV39_PTE_VALID) != 0) {
        return false;
    }

    *slot = pte_from_pa(paddr, SV39_PTE_VALID | flags);
    return true;
}

static bool unmap_page_internal(vm_address_space_t* address_space, uintptr_t vaddr) {
    bool conflict = false;
    uint64_t* slot = NULL;

    if (address_space == NULL || address_space->root_table == NULL ||
        (vaddr & (MEMORY_PAGE_SIZE - 1U)) != 0) {
        return false;
    }

    slot = lookup_level0_slot(address_space, vaddr, false, &conflict);
    if (slot == NULL || conflict || (*slot & SV39_PTE_VALID) == 0) {
        return false;
    }

    *slot = 0;
    return true;
}

static bool can_map_page(vm_address_space_t* address_space, uintptr_t vaddr) {
    bool conflict = false;
    uint64_t* slot = lookup_level0_slot(address_space, vaddr, false, &conflict);

    if (conflict) {
        return false;
    }

    if (slot == NULL) {
        return true;
    }

    return (*slot & SV39_PTE_VALID) == 0;
}

static bool ranges_overlap(uintptr_t start_a,
                           size_t size_a,
                           uintptr_t start_b,
                           size_t size_b) {
    const uintptr_t end_a = start_a + (uintptr_t)size_a;
    const uintptr_t end_b = start_b + (uintptr_t)size_b;

    return start_a < end_b && start_b < end_a;
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

static bool user_region_descriptor_valid(const vm_user_region_t* region) {
    return region != NULL && region->address_space != NULL && region->registered &&
           page_span_args_valid(region->vaddr, region->size) &&
           user_flags_valid(region->flags) &&
           vm_range_is_user(region->vaddr, region->size);
}

static bool object_descriptor_valid(const vm_object_t* object) {
    return object != NULL && object->initialized &&
           object->size != 0 &&
           (object->size & (MEMORY_PAGE_SIZE - 1U)) == 0 &&
           (object->paddr & (MEMORY_PAGE_SIZE - 1U)) == 0 &&
           !range_overflows(object->paddr, object->size);
}

static bool region_object_compatible(const vm_user_region_t* region,
                                     const vm_object_t* object) {
    if (!user_region_descriptor_valid(region) || !object_descriptor_valid(object) ||
        object->size < region->size) {
        return false;
    }

    return mapped_range_args_valid(region->vaddr, object->paddr, region->size);
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

static vm_user_region_t** find_free_process_region_slot(vm_process_t* process) {
    size_t i = 0;

    if (process == NULL) {
        return NULL;
    }

    for (i = 0; i < VM_PROCESS_MAX_USER_REGIONS; ++i) {
        if (process->user_regions[i] == NULL) {
            return &process->user_regions[i];
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

static vm_user_region_t** find_process_region_slot(vm_process_t* process,
                                                   const vm_user_region_t* region) {
    size_t i = 0;

    if (process == NULL || region == NULL) {
        return NULL;
    }

    for (i = 0; i < VM_PROCESS_MAX_USER_REGIONS; ++i) {
        if (process->user_regions[i] == region) {
            return &process->user_regions[i];
        }
    }

    return NULL;
}

static bool process_owns_region(const vm_process_t* process,
                                const vm_user_region_t* region) {
    size_t i = 0;

    if (process == NULL || region == NULL) {
        return false;
    }

    for (i = 0; i < VM_PROCESS_MAX_USER_REGIONS; ++i) {
        if (process->user_regions[i] == region) {
            return true;
        }
    }

    return false;
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
    region->object_mode = VM_REGION_OBJECT_NONE;
    *slot = region;
    return true;
}

static const vm_user_region_t* find_process_region_containing(
    const vm_process_t* process,
    uintptr_t vaddr,
    size_t size,
    uint64_t required_flags) {
    size_t i = 0;

    if (process == NULL || process->address_space == NULL ||
        !span_args_valid(vaddr, size)) {
        return NULL;
    }

    for (i = 0; i < VM_PROCESS_MAX_USER_REGIONS; ++i) {
        const vm_user_region_t* region = process->user_regions[i];

        if (!user_region_descriptor_valid(region) ||
            region->address_space != process->address_space ||
            (region->flags & required_flags) != required_flags) {
            continue;
        }

        if (range_within_window(vaddr,
                                size,
                                region->vaddr,
                                region->vaddr + (uintptr_t)region->size)) {
            return region;
        }
    }

    return NULL;
}

static bool region_contains_vaddr(const vm_user_region_t* region, uintptr_t vaddr) {
    return user_region_descriptor_valid(region) &&
           vaddr >= region->vaddr &&
           vaddr < region->vaddr + (uintptr_t)region->size;
}

static bool fault_range_allows_access(uint64_t flags, uint64_t cause) {
    switch (cause) {
    case RISCV_EXC_INSN_PAGE_FAULT:
        return (flags & VM_PAGE_EXEC) != 0;
    case RISCV_EXC_LOAD_PAGE_FAULT:
        return (flags & VM_PAGE_READ) != 0;
    case RISCV_EXC_STORE_PAGE_FAULT:
        return (flags & VM_PAGE_WRITE) != 0;
    default:
        return false;
    }
}

static bool clear_region_page_mappings(vm_user_region_t* region, bool* changed) {
    uintptr_t offset = 0;

    if (changed != NULL) {
        *changed = false;
    }

    if (!user_region_descriptor_valid(region)) {
        return false;
    }

    while (offset < region->size) {
        bool conflict = false;
        uint64_t* slot = lookup_level0_slot(region->address_space,
                                            region->vaddr + offset,
                                            false,
                                            &conflict);

        if (conflict) {
            return false;
        }

        if (slot != NULL && (*slot & SV39_PTE_VALID) != 0) {
            *slot = 0;
            if (changed != NULL) {
                *changed = true;
            }
        }

        offset += MEMORY_PAGE_SIZE;
    }

    return true;
}

static void clear_region_descriptor(vm_user_region_t* region) {
    if (region == NULL) {
        return;
    }

    region->address_space = NULL;
    region->vaddr = 0;
    region->size = 0;
    region->flags = 0;
    region->registered = false;
    region->object = NULL;
    region->object_mode = VM_REGION_OBJECT_NONE;
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

static const struct VmFaultRange* find_kernel_fault_range(
    const vm_address_space_t* address_space,
    uintptr_t fault_page) {
    size_t i = 0;

    if (address_space == NULL) {
        return NULL;
    }

    for (i = 0; i < VM_MAX_KERNEL_FAULT_RANGES; ++i) {
        const struct VmFaultRange* range = &address_space->kernel_fault_ranges[i];

        if (!range->valid) {
            continue;
        }

        if (fault_page >= range->vaddr &&
            fault_page < range->vaddr + (uintptr_t)range->size) {
            return range;
        }
    }

    return NULL;
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

    if (address_space == NULL || address_space->root_table == NULL ||
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

static bool register_kernel_fault_range(vm_address_space_t* address_space,
                                        uintptr_t vaddr,
                                        uintptr_t paddr,
                                        size_t size,
                                        uint64_t flags) {
    struct VmFaultRange* record = NULL;

    if (address_space == NULL || address_space->root_table == NULL ||
        !mapped_range_args_valid(vaddr, paddr, size) ||
        !kernel_flags_valid(flags) ||
        range_overlaps_kernel_globals(address_space, vaddr, size) ||
        range_overlaps_user_regions(address_space, vaddr, size)) {
        return false;
    }

    record = find_free_fault_range_slot(address_space->kernel_fault_ranges,
                                        VM_MAX_KERNEL_FAULT_RANGES);
    if (record == NULL) {
        return false;
    }

    record->valid = true;
    record->vaddr = vaddr;
    record->paddr = paddr;
    record->size = size;
    record->flags = flags;
    return true;
}

static void flush_tlb_if_enabled(void) {
    vm_address_space_t* address_space = runtime_context_active_address_space();

    if (address_space != NULL && address_space->enabled) {
        vm_flush_tlb();
    }
}

static const struct VmFaultActionRule* find_fault_action(
    const vm_address_space_t* address_space,
    uint64_t cause,
    uintptr_t fault_addr) {
    size_t i = 0;

    if (address_space == NULL) {
        return NULL;
    }

    for (i = 0; i < VM_MAX_FAULT_ACTIONS; ++i) {
        const struct VmFaultActionRule* rule = &address_space->fault_actions[i];

        if (!rule->valid || rule->cause != cause) {
            continue;
        }

        if (fault_addr >= rule->vaddr &&
            fault_addr < rule->vaddr + (uintptr_t)rule->size) {
            return rule;
        }
    }

    return NULL;
}

bool vm_address_space_create(vm_address_space_t** out_space) {
    size_t i = 0;
    size_t j = 0;
    vm_address_space_t* space = NULL;

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

    space->root_table = alloc_table_page();
    if (space->root_table == NULL) {
        return false;
    }

    space->allocated = true;
    space->root_table_pa = (uintptr_t)space->root_table;
    space->satp_value =
        RISCV_SATP_MODE_SV39 |
        (((uint64_t)(space->root_table_pa >> SV39_PAGE_SHIFT)) & SV39_PPN_MASK);
    space->enabled = false;

    for (j = 0; j < VM_MAX_KERNEL_MAPPINGS; ++j) {
        space->kernel_mappings[j].valid = false;
    }

    for (j = 0; j < VM_MAX_KERNEL_FAULT_RANGES; ++j) {
        space->kernel_fault_ranges[j].valid = false;
    }

    for (j = 0; j < VM_MAX_FAULT_ACTIONS; ++j) {
        space->fault_actions[j].valid = false;
    }

    for (j = 0; j < VM_MAX_USER_REGIONS; ++j) {
        space->user_regions[j] = NULL;
    }

    *out_space = space;
    return true;
}

bool vm_address_space_activate(vm_address_space_t* address_space) {
    if (address_space == NULL || !address_space->allocated ||
        address_space->root_table == NULL) {
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
    if (address_space == NULL || !address_space->allocated ||
        address_space->root_table == NULL) {
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

    if (address_space == NULL || !address_space->allocated ||
        address_space->root_table == NULL) {
        return false;
    }

    for (i = 0; i < VM_MAX_USER_REGIONS; ++i) {
        if (address_space->user_regions[i] != NULL) {
            return false;
        }
    }

    if (!vm_address_space_disable(address_space) ||
        !free_table_pages_recursive(address_space->root_table, 2U)) {
        return false;
    }

    address_space->allocated = false;
    address_space->root_table = NULL;
    address_space->root_table_pa = 0;
    address_space->satp_value = 0;
    address_space->enabled = false;
    clear_fault_ranges(address_space->kernel_mappings, VM_MAX_KERNEL_MAPPINGS);
    clear_fault_ranges(address_space->kernel_fault_ranges,
                       VM_MAX_KERNEL_FAULT_RANGES);
    clear_fault_actions(address_space->fault_actions, VM_MAX_FAULT_ACTIONS);
    for (i = 0; i < VM_MAX_USER_REGIONS; ++i) {
        address_space->user_regions[i] = NULL;
    }

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
    if (!vm_range_is_kernel(vaddr, size)) {
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
    region->object_mode = VM_REGION_OBJECT_NONE;

    return register_user_region(address_space, region);
}

bool vm_address_space_register_fault_range(vm_address_space_t* address_space,
                                           uintptr_t vaddr,
                                           uintptr_t paddr,
                                           size_t size,
                                           uint64_t flags) {
    if ((flags & VM_PAGE_USER) != 0) {
        return false;
    }

    return register_kernel_fault_range(address_space, vaddr, paddr, size, flags);
}

static bool register_fault_action(vm_address_space_t* address_space,
                                  uint64_t cause,
                                  uintptr_t vaddr,
                                  size_t size,
                                  vm_fault_action_t action,
                                  volatile uintptr_t* resume_pc_slot) {
    size_t i = 0;
    struct VmFaultActionRule* free_slot = NULL;

    if (address_space == NULL || address_space->root_table == NULL ||
        !page_fault_cause_valid(cause) || !span_args_valid(vaddr, size)) {
        return false;
    }

    if (action == VM_FAULT_ACTION_RESUME_AT_SLOT && resume_pc_slot == NULL) {
        return false;
    }

    for (i = 0; i < VM_MAX_FAULT_ACTIONS; ++i) {
        struct VmFaultActionRule* rule = &address_space->fault_actions[i];

        if (!rule->valid) {
            if (free_slot == NULL) {
                free_slot = rule;
            }
            continue;
        }

        if (rule->cause == cause &&
            ranges_overlap(vaddr, size, rule->vaddr, rule->size)) {
            return false;
        }
    }

    if (free_slot == NULL) {
        return false;
    }

    free_slot->valid = true;
    free_slot->cause = cause;
    free_slot->vaddr = vaddr;
    free_slot->size = size;
    free_slot->action = action;
    free_slot->resume_pc_slot = resume_pc_slot;
    return true;
}

bool vm_address_space_register_fault_skip(vm_address_space_t* address_space,
                                          uint64_t cause,
                                          uintptr_t vaddr,
                                          size_t size) {
    return register_fault_action(address_space,
                                 cause,
                                 vaddr,
                                 size,
                                 VM_FAULT_ACTION_SKIP_INSTRUCTION,
                                 NULL);
}

bool vm_address_space_register_fault_resume_slot(
    vm_address_space_t* address_space,
    uint64_t cause,
    uintptr_t vaddr,
    size_t size,
    volatile uintptr_t* resume_pc_slot) {
    return register_fault_action(address_space,
                                 cause,
                                 vaddr,
                                 size,
                                 VM_FAULT_ACTION_RESUME_AT_SLOT,
                                 resume_pc_slot);
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

bool vm_process_create(vm_process_t* process, vm_address_space_t* address_space) {
    size_t i = 0;

    if (process == NULL || address_space == NULL || !address_space->allocated ||
        address_space->root_table == NULL || process->address_space != NULL ||
        process->entry_pc != 0 || process->user_sp != 0) {
        return false;
    }

    for (i = 0; i < VM_PROCESS_MAX_USER_REGIONS; ++i) {
        if (process->user_regions[i] != NULL) {
            return false;
        }
    }

    process->address_space = address_space;
    process->entry_pc = 0;
    process->user_sp = 0;
    for (i = 0; i < VM_PROCESS_MAX_USER_REGIONS; ++i) {
        process->user_regions[i] = NULL;
    }

    return true;
}

bool vm_process_activate(vm_process_t* process) {
    if (!vm_process_is_runnable(process) ||
        !vm_address_space_enable(process->address_space)) {
        return false;
    }

    runtime_context_activate_process(process);
    return true;
}

bool vm_process_is_active(const vm_process_t* process) {
    return runtime_context_process_is_active(process);
}

bool vm_process_remove_user_region(vm_process_t* process,
                                   vm_user_region_t* region) {
    vm_user_region_t** process_slot = NULL;
    vm_user_region_t** address_space_slot = NULL;

    if (process == NULL || process->address_space == NULL || region == NULL ||
        !user_region_descriptor_valid(region) ||
        region->address_space != process->address_space) {
        return false;
    }

    process_slot = find_process_region_slot(process, region);
    address_space_slot =
        find_address_space_region_slot(process->address_space, region);
    if (process_slot == NULL || address_space_slot == NULL ||
        !vm_user_region_clear_object(region)) {
        return false;
    }

    *process_slot = NULL;
    *address_space_slot = NULL;
    if (region_contains_vaddr(region, process->entry_pc)) {
        process->entry_pc = 0;
    }
    if (process->user_sp > 0 && region_contains_vaddr(region, process->user_sp - 1U)) {
        process->user_sp = 0;
    }
    clear_region_descriptor(region);
    return true;
}

bool vm_process_reset(vm_process_t* process) {
    size_t i = 0;

    if (process == NULL) {
        return false;
    }

    while (i < VM_PROCESS_MAX_USER_REGIONS) {
        vm_user_region_t* region = process->user_regions[i];

        if (region != NULL) {
            if (!vm_process_remove_user_region(process, region)) {
                return false;
            }
        } else {
            ++i;
        }
    }

    runtime_context_clear_process(process);
    process->address_space = NULL;
    process->entry_pc = 0;
    process->user_sp = 0;
    for (i = 0; i < VM_PROCESS_MAX_USER_REGIONS; ++i) {
        process->user_regions[i] = NULL;
    }

    return true;
}

bool vm_process_user_region_init(vm_process_t* process,
                                 vm_user_region_t* region,
                                 uintptr_t vaddr,
                                 size_t size,
                                 uint64_t flags) {
    vm_user_region_t** slot = NULL;

    if (process == NULL || process->address_space == NULL || region == NULL ||
        process_owns_region(process, region)) {
        return false;
    }

    slot = find_free_process_region_slot(process);
    if (slot == NULL ||
        !vm_address_space_user_region_init(process->address_space,
                                           region,
                                           vaddr,
                                           size,
                                           flags)) {
        return false;
    }

    *slot = region;
    return true;
}

bool vm_process_set_user_context(vm_process_t* process,
                                 uintptr_t entry_pc,
                                 uintptr_t user_sp) {
    if (process == NULL || process->address_space == NULL ||
        !process->address_space->allocated || !vm_range_is_user(entry_pc, 1U) ||
        user_sp <= vm_user_base() || user_sp > vm_user_limit() ||
        find_process_region_containing(process,
                                       entry_pc,
                                       1U,
                                       VM_PAGE_EXEC | VM_PAGE_USER) == NULL ||
        find_process_region_containing(process,
                                       user_sp - 1U,
                                       1U,
                                       VM_PAGE_WRITE | VM_PAGE_USER) == NULL) {
        return false;
    }

    process->entry_pc = entry_pc;
    process->user_sp = user_sp;
    return true;
}

bool vm_process_is_runnable(const vm_process_t* process) {
    return process != NULL && process->address_space != NULL &&
           process->address_space->allocated &&
           find_process_region_containing(process,
                                          process->entry_pc,
                                          1U,
                                          VM_PAGE_EXEC | VM_PAGE_USER) != NULL &&
           process->user_sp > vm_user_base() &&
           process->user_sp <= vm_user_limit() &&
           find_process_region_containing(process,
                                          process->user_sp - 1U,
                                          1U,
                                          VM_PAGE_WRITE | VM_PAGE_USER) != NULL;
}

void vm_object_reset(vm_object_t* object) {
    if (object == NULL) {
        return;
    }

    object->initialized = false;
    object->paddr = 0;
    object->size = 0;
}

bool vm_object_init_physical(vm_object_t* object, uintptr_t paddr, size_t size) {
    if (object == NULL || object->initialized || size == 0 ||
        (size & (MEMORY_PAGE_SIZE - 1U)) != 0 ||
        (paddr & (MEMORY_PAGE_SIZE - 1U)) != 0 ||
        !span_args_valid(paddr, size)) {
        return false;
    }

    object->initialized = true;
    object->paddr = paddr;
    object->size = size;
    return true;
}

bool vm_user_region_clear_object(vm_user_region_t* region) {
    bool changed = false;

    if (!clear_region_page_mappings(region, &changed)) {
        return false;
    }

    region->object = NULL;
    region->object_mode = VM_REGION_OBJECT_NONE;
    if (changed) {
        flush_tlb_if_enabled();
    }

    return true;
}

bool vm_user_region_map_object(vm_user_region_t* region, vm_object_t* object) {
    if (!region_object_compatible(region, object) ||
        region->object_mode != VM_REGION_OBJECT_NONE) {
        return false;
    }

    if (!map_range_pages_internal(region->address_space,
                                  region->vaddr,
                                  object->paddr,
                                  region->size,
                                  region->flags)) {
        return false;
    }

    region->object = object;
    region->object_mode = VM_REGION_OBJECT_MAPPED;
    flush_tlb_if_enabled();
    return true;
}

bool vm_user_region_set_fault_object(vm_user_region_t* region, vm_object_t* object) {
    if (!region_object_compatible(region, object) ||
        region->object_mode != VM_REGION_OBJECT_NONE) {
        return false;
    }

    region->object = object;
    region->object_mode = VM_REGION_OBJECT_FAULT;
    return true;
}

bool vm_user_region_unmap_page(vm_user_region_t* region, uintptr_t vaddr) {
    if (!user_region_descriptor_valid(region) ||
        !vm_user_region_contains(region, vaddr, MEMORY_PAGE_SIZE) ||
        !unmap_page_internal(region->address_space, vaddr)) {
        return false;
    }

    flush_tlb_if_enabled();
    return true;
}

bool vm_user_region_contains(const vm_user_region_t* region,
                             uintptr_t vaddr,
                             size_t size) {
    if (!user_region_descriptor_valid(region)) {
        return false;
    }

    return range_within_window(vaddr, size, region->vaddr,
                               region->vaddr + (uintptr_t)region->size);
}

bool vm_range_is_kernel(uintptr_t vaddr, size_t size) {
    return range_within_window(vaddr, size, VM_KERNEL_VADDR_BASE, VM_KERNEL_VADDR_LIMIT);
}

bool vm_range_is_user(uintptr_t vaddr, size_t size) {
    return range_within_window(vaddr, size, VM_USER_VADDR_BASE, VM_USER_VADDR_LIMIT);
}

uintptr_t vm_kernel_base(void) {
    return VM_KERNEL_VADDR_BASE;
}

uintptr_t vm_kernel_limit(void) {
    return VM_KERNEL_VADDR_LIMIT;
}

uintptr_t vm_user_base(void) {
    return VM_USER_VADDR_BASE;
}

uintptr_t vm_user_limit(void) {
    return VM_USER_VADDR_LIMIT;
}

bool vm_handle_page_fault(const vm_process_t* process,
                          vm_address_space_t* address_space,
                          uint64_t cause,
                          uint64_t epc,
                          uint64_t tval) {
    const vm_user_region_t* user_region = NULL;
    const struct VmFaultRange* range = NULL;
    const struct VmFaultActionRule* action = NULL;
    const uintptr_t fault_page = align_down_page((uintptr_t)tval);
    const uintptr_t fault_addr = (uintptr_t)tval;
    uintptr_t offset = 0;
    uintptr_t resume_pc = 0;

    if (address_space == NULL || address_space->root_table == NULL) {
        return false;
    }

    if (process != NULL && process->address_space == address_space) {
        user_region = find_process_region_containing(process, fault_page, 1U, 0);
    }
    if (user_region != NULL &&
        user_region->object_mode == VM_REGION_OBJECT_FAULT &&
        object_descriptor_valid(user_region->object) &&
        fault_range_allows_access(user_region->flags, cause)) {
        offset = fault_page - user_region->vaddr;
        if (!map_page_internal(address_space,
                               fault_page,
                               user_region->object->paddr + offset,
                               user_region->flags)) {
            return false;
        }

        vm_flush_tlb();
        return true;
    }

    range = find_kernel_fault_range(address_space, fault_page);
    if (range != NULL && fault_range_allows_access(range->flags, cause)) {
        offset = fault_page - range->vaddr;
        if (!map_page_internal(address_space,
                               fault_page,
                               range->paddr + offset,
                               range->flags)) {
            return false;
        }

        vm_flush_tlb();
        return true;
    }

    action = find_fault_action(address_space, cause, fault_addr);
    if (action == NULL) {
        return false;
    }

    switch (action->action) {
    case VM_FAULT_ACTION_SKIP_INSTRUCTION:
        riscv_write_sepc(epc + 4U);
        return true;
    case VM_FAULT_ACTION_RESUME_AT_SLOT:
        resume_pc = (uintptr_t)(*action->resume_pc_slot);
        if (resume_pc == 0) {
            return false;
        }
        riscv_write_sepc(resume_pc);
        *action->resume_pc_slot = 0;
        return true;
    }

    return false;
}

void vm_flush_tlb(void) {
    riscv_sfence_vma();
}
