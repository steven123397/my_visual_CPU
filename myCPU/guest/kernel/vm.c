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
#define VM_OBJECT_ANON_PAGE_SLOTS (MEMORY_PAGE_SIZE / sizeof(uintptr_t))

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

static void* alloc_zeroed_page(void) {
    void* page = pmm_alloc_page();

    if (page == NULL) {
        return NULL;
    }

    zero_page(page);
    return page;
}

static uint64_t* alloc_table_page(void) {
    return (uint64_t*)alloc_zeroed_page();
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

static size_t object_page_count(size_t size) {
    return size / MEMORY_PAGE_SIZE;
}

static bool physical_object_descriptor_valid(const vm_object_t* object) {
    return (object->backing.physical.base_paddr & (MEMORY_PAGE_SIZE - 1U)) == 0 &&
           !range_overflows(object->backing.physical.base_paddr, object->size);
}

static bool anon_object_descriptor_valid(const vm_object_t* object) {
    const size_t expected_page_count = object_page_count(object->size);

    return expected_page_count != 0 &&
           expected_page_count <= VM_OBJECT_ANON_PAGE_SLOTS &&
           object->backing.anon.page_slots != NULL &&
           object->backing.anon.page_count == expected_page_count &&
           (((uintptr_t)object->backing.anon.page_slots) &
            (MEMORY_PAGE_SIZE - 1U)) == 0;
}

static bool object_descriptor_valid(const vm_object_t* object) {
    if (object == NULL || !object->initialized || object->size == 0 ||
        (object->size & (MEMORY_PAGE_SIZE - 1U)) != 0) {
        return false;
    }

    switch (object->backing_kind) {
    case VM_OBJECT_BACKING_PHYSICAL:
        return physical_object_descriptor_valid(object);
    case VM_OBJECT_BACKING_ANON:
        return anon_object_descriptor_valid(object);
    default:
        return false;
    }
}

static bool object_range_compatible(const vm_object_t* object,
                                    size_t object_offset,
                                    size_t size) {
    return object_descriptor_valid(object) &&
           page_span_args_valid(object_offset, size) &&
           range_within_window(object_offset, size, 0, object->size);
}

static void clear_object_descriptor(vm_object_t* object) {
    if (object == NULL) {
        return;
    }

    object->initialized = false;
    object->backing_kind = VM_OBJECT_BACKING_NONE;
    object->size = 0;
    object->attachment_count = 0;
    object->backing.anon.page_slots = NULL;
    object->backing.anon.page_count = 0;
}

static bool object_resolve_page(vm_object_t* object,
                                size_t offset,
                                bool create,
                                uintptr_t* out_paddr) {
    size_t page_index = 0;
    void* page = NULL;

    if (!object_descriptor_valid(object) || out_paddr == NULL ||
        offset >= object->size ||
        (offset & (MEMORY_PAGE_SIZE - 1U)) != 0) {
        return false;
    }

    switch (object->backing_kind) {
    case VM_OBJECT_BACKING_PHYSICAL:
        *out_paddr = object->backing.physical.base_paddr + (uintptr_t)offset;
        return true;
    case VM_OBJECT_BACKING_ANON:
        page_index = offset / MEMORY_PAGE_SIZE;
        if (page_index >= object->backing.anon.page_count) {
            return false;
        }

        if (object->backing.anon.page_slots[page_index] == 0) {
            if (!create) {
                return false;
            }

            page = alloc_zeroed_page();
            if (page == NULL) {
                return false;
            }

            object->backing.anon.page_slots[page_index] = (uintptr_t)page;
        }

        if ((object->backing.anon.page_slots[page_index] &
             (MEMORY_PAGE_SIZE - 1U)) != 0) {
            return false;
        }

        *out_paddr = object->backing.anon.page_slots[page_index];
        return true;
    default:
        return false;
    }
}

static bool region_object_compatible(const vm_user_region_t* region,
                                     const vm_object_t* object,
                                     size_t object_offset) {
    if (!user_region_descriptor_valid(region) ||
        !object_range_compatible(object, object_offset, region->size)) {
        return false;
    }

    switch (object->backing_kind) {
    case VM_OBJECT_BACKING_PHYSICAL:
        return mapped_range_args_valid(region->vaddr,
                                       object->backing.physical.base_paddr +
                                           (uintptr_t)object_offset,
                                       region->size);
    case VM_OBJECT_BACKING_ANON:
        return true;
    default:
        return false;
    }
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
    region->object_offset = 0;
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

static vm_user_region_t* find_mutable_process_region_containing(
    vm_process_t* process,
    uintptr_t vaddr,
    size_t size,
    uint64_t required_flags) {
    return (vm_user_region_t*)find_process_region_containing(process,
                                                             vaddr,
                                                             size,
                                                             required_flags);
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
    region->object_offset = 0;
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

static bool map_region_object_pages(vm_user_region_t* region,
                                    vm_object_t* object,
                                    size_t object_offset) {
    size_t offset = 0;
    size_t mapped = 0;
    uintptr_t paddr = 0;

    if (!region_object_compatible(region, object, object_offset)) {
        return false;
    }

    while (offset < region->size) {
        if (!can_map_page(region->address_space, region->vaddr + offset)) {
            return false;
        }
        offset += MEMORY_PAGE_SIZE;
    }

    offset = 0;
    while (offset < region->size) {
        if (!object_resolve_page(object, object_offset + offset, true, &paddr) ||
            !map_page_internal(region->address_space,
                               region->vaddr + offset,
                               paddr,
                               region->flags)) {
            size_t rollback = 0;

            while (rollback < mapped) {
                unmap_page_internal(region->address_space, region->vaddr + rollback);
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
    region->object_offset = 0;
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

static bool process_bind_object_region(vm_process_t* process,
                                       vm_user_region_t* region,
                                       uintptr_t vaddr,
                                       size_t size,
                                       uint64_t flags,
                                       vm_object_t* object,
                                       size_t object_offset,
                                       bool map_now) {
    bool ok = false;

    if (process == NULL || region == NULL || object == NULL ||
        !vm_process_user_region_init(process, region, vaddr, size, flags)) {
        return false;
    }

    if (map_now) {
        ok = vm_user_region_map_object_at(region, object, object_offset);
    } else {
        ok = vm_user_region_set_fault_object_at(region, object, object_offset);
    }

    if (ok) {
        return true;
    }

    if (!vm_process_remove_user_region(process, region)) {
        return false;
    }

    return false;
}

bool vm_process_bind_user_regions(
    vm_process_t* process,
    const vm_process_user_region_binding_t* bindings,
    size_t binding_count) {
    vm_user_region_t* bound_regions[VM_PROCESS_MAX_USER_REGIONS];
    size_t i = 0;
    size_t bound_count = 0;
    bool ok = false;

    if (process == NULL ||
        (binding_count != 0 && bindings == NULL) ||
        binding_count > VM_PROCESS_MAX_USER_REGIONS) {
        return false;
    }

    while (i < binding_count) {
        const vm_process_user_region_binding_t* binding = &bindings[i];

        if (binding->region == NULL || binding->object == NULL) {
            ok = false;
            break;
        }

        switch (binding->object_mode) {
        case VM_REGION_OBJECT_MAPPED:
            ok = process_bind_object_region(process,
                                            binding->region,
                                            binding->vaddr,
                                            binding->size,
                                            binding->flags,
                                            binding->object,
                                            binding->object_offset,
                                            true);
            break;
        case VM_REGION_OBJECT_FAULT:
            ok = process_bind_object_region(process,
                                            binding->region,
                                            binding->vaddr,
                                            binding->size,
                                            binding->flags,
                                            binding->object,
                                            binding->object_offset,
                                            false);
            break;
        default:
            ok = false;
            break;
        }

        if (!ok) {
            break;
        }

        bound_regions[bound_count++] = binding->region;
        ++i;
    }

    if (i == binding_count) {
        return true;
    }

    while (bound_count > 0) {
        bound_count -= 1;
        if (!vm_process_remove_user_region(process, bound_regions[bound_count])) {
            return false;
        }
    }

    return false;
}

bool vm_process_map_object_region_at(vm_process_t* process,
                                     vm_user_region_t* region,
                                     uintptr_t vaddr,
                                     size_t size,
                                     uint64_t flags,
                                     vm_object_t* object,
                                     size_t object_offset) {
    return process_bind_object_region(process,
                                      region,
                                      vaddr,
                                      size,
                                      flags,
                                      object,
                                      object_offset,
                                      true);
}

bool vm_process_map_object_region(vm_process_t* process,
                                  vm_user_region_t* region,
                                  uintptr_t vaddr,
                                  size_t size,
                                  uint64_t flags,
                                  vm_object_t* object) {
    return vm_process_map_object_region_at(process,
                                           region,
                                           vaddr,
                                           size,
                                           flags,
                                           object,
                                           0);
}

bool vm_process_set_fault_object_region_at(vm_process_t* process,
                                           vm_user_region_t* region,
                                           uintptr_t vaddr,
                                           size_t size,
                                           uint64_t flags,
                                           vm_object_t* object,
                                           size_t object_offset) {
    return process_bind_object_region(process,
                                      region,
                                      vaddr,
                                      size,
                                      flags,
                                      object,
                                      object_offset,
                                      false);
}

bool vm_process_set_fault_object_region(vm_process_t* process,
                                        vm_user_region_t* region,
                                        uintptr_t vaddr,
                                        size_t size,
                                        uint64_t flags,
                                        vm_object_t* object) {
    return vm_process_set_fault_object_region_at(process,
                                                 region,
                                                 vaddr,
                                                 size,
                                                 flags,
                                                 object,
                                                 0);
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

bool vm_object_reset(vm_object_t* object) {
    size_t i = 0;

    if (object == NULL) {
        return false;
    }

    if (!object->initialized) {
        clear_object_descriptor(object);
        return true;
    }

    if (!object_descriptor_valid(object) || object->attachment_count != 0) {
        return false;
    }

    switch (object->backing_kind) {
    case VM_OBJECT_BACKING_PHYSICAL:
        clear_object_descriptor(object);
        return true;
    case VM_OBJECT_BACKING_ANON:
        for (i = 0; i < object->backing.anon.page_count; ++i) {
            const uintptr_t page = object->backing.anon.page_slots[i];

            if (page == 0) {
                continue;
            }

            if (!pmm_free_page((void*)page)) {
                return false;
            }

            object->backing.anon.page_slots[i] = 0;
        }

        if (!pmm_free_page(object->backing.anon.page_slots)) {
            return false;
        }

        clear_object_descriptor(object);
        return true;
    default:
        return false;
    }
}

bool vm_object_init_physical(vm_object_t* object, uintptr_t paddr, size_t size) {
    if (object == NULL || object->initialized || object->attachment_count != 0 ||
        size == 0 ||
        (size & (MEMORY_PAGE_SIZE - 1U)) != 0 ||
        (paddr & (MEMORY_PAGE_SIZE - 1U)) != 0 ||
        !span_args_valid(paddr, size)) {
        return false;
    }

    object->initialized = true;
    object->backing_kind = VM_OBJECT_BACKING_PHYSICAL;
    object->size = size;
    object->attachment_count = 0;
    object->backing.physical.base_paddr = paddr;
    return true;
}

bool vm_object_init_anon(vm_object_t* object, size_t size) {
    uintptr_t* page_slots = NULL;
    const size_t page_count = object_page_count(size);

    if (object == NULL || object->initialized || object->attachment_count != 0 ||
        size == 0 ||
        (size & (MEMORY_PAGE_SIZE - 1U)) != 0 ||
        page_count == 0 ||
        page_count > VM_OBJECT_ANON_PAGE_SLOTS) {
        return false;
    }

    page_slots = (uintptr_t*)alloc_zeroed_page();
    if (page_slots == NULL) {
        return false;
    }

    object->initialized = true;
    object->backing_kind = VM_OBJECT_BACKING_ANON;
    object->size = size;
    object->attachment_count = 0;
    object->backing.anon.page_slots = page_slots;
    object->backing.anon.page_count = page_count;
    return true;
}

bool vm_user_region_clear_object(vm_user_region_t* region) {
    bool changed = false;
    vm_object_t* object = NULL;

    if (!clear_region_page_mappings(region, &changed)) {
        return false;
    }

    object = region->object;
    if (object != NULL) {
        if (!object_descriptor_valid(object) || object->attachment_count == 0) {
            return false;
        }
        object->attachment_count -= 1;
    }

    region->object = NULL;
    region->object_offset = 0;
    region->object_mode = VM_REGION_OBJECT_NONE;
    if (changed) {
        flush_tlb_if_enabled();
    }

    return true;
}

static bool bind_region_object(vm_user_region_t* region,
                               vm_object_t* object,
                               size_t object_offset,
                               vm_region_object_mode_t object_mode) {
    const bool map_now = object_mode == VM_REGION_OBJECT_MAPPED;

    if (!region_object_compatible(region, object, object_offset) ||
        region->object != NULL ||
        region->object_mode != VM_REGION_OBJECT_NONE ||
        object->attachment_count == (size_t)-1) {
        return false;
    }

    if (map_now &&
        !map_region_object_pages(region, object, object_offset)) {
        return false;
    }

    object->attachment_count += 1;
    region->object = object;
    region->object_offset = object_offset;
    region->object_mode = object_mode;
    if (map_now) {
        flush_tlb_if_enabled();
    }
    return true;
}

bool vm_user_region_map_object_at(vm_user_region_t* region,
                                  vm_object_t* object,
                                  size_t object_offset) {
    return bind_region_object(region,
                              object,
                              object_offset,
                              VM_REGION_OBJECT_MAPPED);
}

bool vm_user_region_map_object(vm_user_region_t* region, vm_object_t* object) {
    return vm_user_region_map_object_at(region, object, 0);
}

bool vm_user_region_set_fault_object_at(vm_user_region_t* region,
                                        vm_object_t* object,
                                        size_t object_offset) {
    return bind_region_object(region,
                              object,
                              object_offset,
                              VM_REGION_OBJECT_FAULT);
}

bool vm_user_region_set_fault_object(vm_user_region_t* region, vm_object_t* object) {
    return vm_user_region_set_fault_object_at(region, object, 0);
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

bool vm_handle_page_fault(vm_process_t* process,
                          vm_address_space_t* address_space,
                          uint64_t cause,
                          uint64_t epc,
                          uint64_t tval) {
    vm_user_region_t* user_region = NULL;
    const struct VmFaultRange* range = NULL;
    const struct VmFaultActionRule* action = NULL;
    const uintptr_t fault_page = align_down_page((uintptr_t)tval);
    const uintptr_t fault_addr = (uintptr_t)tval;
    uintptr_t offset = 0;
    uintptr_t paddr = 0;
    uintptr_t resume_pc = 0;

    if (address_space == NULL || address_space->root_table == NULL) {
        return false;
    }

    if (process != NULL && process->address_space == address_space) {
        user_region =
            find_mutable_process_region_containing(process, fault_page, 1U, 0);
    }
    if (user_region != NULL &&
        user_region->object != NULL &&
        user_region->object_mode != VM_REGION_OBJECT_NONE &&
        object_descriptor_valid(user_region->object) &&
        fault_range_allows_access(user_region->flags, cause)) {
        offset = fault_page - user_region->vaddr;
        if (!object_resolve_page(user_region->object,
                                 user_region->object_offset + offset,
                                 true,
                                 &paddr) ||
            !map_page_internal(address_space,
                               fault_page,
                               paddr,
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
