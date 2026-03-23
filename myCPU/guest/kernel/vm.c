#include "vm.h"

#include <stddef.h>
#include <stdint.h>

#include "platform_mmio.h"
#include "memory.h"
#include "pmm.h"
#include "riscv.h"

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
#define VM_MAX_FAULT_ACTIONS 16U
#define VM_USER_VADDR_BASE ((uintptr_t)0)
#define VM_USER_VADDR_LIMIT ((uintptr_t)MEM_BASE)
#define VM_KERNEL_VADDR_BASE ((uintptr_t)MEM_BASE)
#define VM_KERNEL_VADDR_LIMIT ((uintptr_t)MEM_BASE + (uintptr_t)MEM_SIZE)

static uint64_t* root_table = NULL;
static uintptr_t root_table_pa = 0;
static uint64_t satp_value = 0;
static bool enabled = false;

struct VmFaultRange {
    bool valid;
    uintptr_t vaddr;
    uintptr_t paddr;
    size_t size;
    uint64_t flags;
};

struct VmUserRegion {
    bool valid;
    uintptr_t vaddr;
    size_t size;
    uint64_t flags;
    bool has_fault_backing;
    uintptr_t fault_paddr;
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

static struct VmFaultRange kernel_mappings[VM_MAX_KERNEL_MAPPINGS];
static struct VmFaultRange kernel_fault_ranges[VM_MAX_KERNEL_FAULT_RANGES];
static struct VmUserRegion user_regions[VM_MAX_USER_REGIONS];
static struct VmFaultActionRule fault_actions[VM_MAX_FAULT_ACTIONS];

static void flush_tlb_if_enabled(void);

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

static bool range_matches(uintptr_t base_a,
                          size_t size_a,
                          uintptr_t base_b,
                          size_t size_b) {
    return base_a == base_b && size_a == size_b;
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

static uint64_t* lookup_level0_slot(uintptr_t vaddr, bool create, bool* conflict) {
    uint64_t* level1 = NULL;
    uint64_t* level0 = NULL;
    const size_t root_index = vpn_index(vaddr, 2);
    const size_t level1_index = vpn_index(vaddr, 1);
    const size_t level0_index = vpn_index(vaddr, 0);
    uint64_t entry = 0;

    if (root_table == NULL) {
        return NULL;
    }

    if (conflict != NULL) {
        *conflict = false;
    }

    entry = root_table[root_index];
    if ((entry & SV39_PTE_VALID) == 0) {
        if (!create) {
            return NULL;
        }

        level1 = alloc_table_page();
        if (level1 == NULL) {
            return NULL;
        }

        root_table[root_index] = pte_from_pa((uintptr_t)level1, SV39_PTE_VALID);
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

static bool map_page_internal(uintptr_t vaddr, uintptr_t paddr, uint64_t flags) {
    bool conflict = false;
    uint64_t* slot = NULL;

    if (!mapped_range_args_valid(vaddr, paddr, MEMORY_PAGE_SIZE) ||
        !flags_valid(flags)) {
        return false;
    }

    slot = lookup_level0_slot(vaddr, true, &conflict);
    if (slot == NULL || conflict || (*slot & SV39_PTE_VALID) != 0) {
        return false;
    }

    *slot = pte_from_pa(paddr, SV39_PTE_VALID | flags);
    return true;
}

static bool unmap_page_internal(uintptr_t vaddr) {
    bool conflict = false;
    uint64_t* slot = NULL;

    if (root_table == NULL || (vaddr & (MEMORY_PAGE_SIZE - 1U)) != 0) {
        return false;
    }

    slot = lookup_level0_slot(vaddr, false, &conflict);
    if (slot == NULL || conflict || (*slot & SV39_PTE_VALID) == 0) {
        return false;
    }

    *slot = 0;
    return true;
}

static bool can_map_page(uintptr_t vaddr) {
    bool conflict = false;
    uint64_t* slot = lookup_level0_slot(vaddr, false, &conflict);

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

static bool range_overlaps_user_regions(uintptr_t vaddr, size_t size) {
    size_t i = 0;

    for (i = 0; i < VM_MAX_USER_REGIONS; ++i) {
        const struct VmUserRegion* region = &user_regions[i];

        if (!region->valid) {
            continue;
        }

        if (ranges_overlap(vaddr, size, region->vaddr, region->size)) {
            return true;
        }
    }

    return false;
}

static bool range_overlaps_kernel_globals(uintptr_t vaddr, size_t size) {
    return range_overlaps_fault_ranges(kernel_mappings,
                                       VM_MAX_KERNEL_MAPPINGS,
                                       vaddr,
                                       size) ||
           range_overlaps_fault_ranges(kernel_fault_ranges,
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

static struct VmUserRegion* ensure_user_region(uintptr_t vaddr,
                                               size_t size,
                                               uint64_t flags,
                                               bool* created) {
    size_t i = 0;
    struct VmUserRegion* free_slot = NULL;

    if (root_table == NULL || !page_span_args_valid(vaddr, size) ||
        !user_flags_valid(flags) || !vm_range_is_user(vaddr, size) ||
        range_overlaps_kernel_globals(vaddr, size)) {
        return NULL;
    }

    if (created != NULL) {
        *created = false;
    }

    for (i = 0; i < VM_MAX_USER_REGIONS; ++i) {
        struct VmUserRegion* region = &user_regions[i];

        if (!region->valid) {
            if (free_slot == NULL) {
                free_slot = region;
            }
            continue;
        }

        if (!ranges_overlap(vaddr, size, region->vaddr, region->size)) {
            continue;
        }

        if (!range_matches(vaddr, size, region->vaddr, region->size) ||
            region->flags != flags) {
            return NULL;
        }

        return region;
    }

    if (free_slot == NULL) {
        return NULL;
    }

    if (created != NULL) {
        *created = true;
    }
    return free_slot;
}

static const struct VmUserRegion* find_user_region_containing(uintptr_t vaddr) {
    size_t i = 0;

    for (i = 0; i < VM_MAX_USER_REGIONS; ++i) {
        const struct VmUserRegion* region = &user_regions[i];

        if (!region->valid) {
            continue;
        }

        if (vaddr >= region->vaddr &&
            vaddr < region->vaddr + (uintptr_t)region->size) {
            return region;
        }
    }

    return NULL;
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

static const struct VmFaultRange* find_kernel_fault_range(uintptr_t fault_page) {
    size_t i = 0;

    for (i = 0; i < VM_MAX_KERNEL_FAULT_RANGES; ++i) {
        const struct VmFaultRange* range = &kernel_fault_ranges[i];

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

static bool map_range_pages_internal(uintptr_t vaddr,
                                     uintptr_t paddr,
                                     size_t size,
                                     uint64_t flags) {
    size_t offset = 0;
    size_t mapped = 0;

    if (root_table == NULL ||
        !mapped_range_args_valid(vaddr, paddr, size) ||
        !flags_valid(flags)) {
        return false;
    }

    while (offset < size) {
        if (!can_map_page(vaddr + offset)) {
            return false;
        }
        offset += MEMORY_PAGE_SIZE;
    }

    offset = 0;
    while (offset < size) {
        if (!map_page_internal(vaddr + offset, paddr + offset, flags)) {
            size_t rollback = 0;

            while (rollback < mapped) {
                unmap_page_internal(vaddr + rollback);
                rollback += MEMORY_PAGE_SIZE;
            }
            return false;
        }

        mapped += MEMORY_PAGE_SIZE;
        offset += MEMORY_PAGE_SIZE;
    }

    return true;
}

static bool map_kernel_global_range(uintptr_t vaddr,
                                    uintptr_t paddr,
                                    size_t size,
                                    uint64_t flags) {
    struct VmFaultRange* record = NULL;

    if (root_table == NULL ||
        !mapped_range_args_valid(vaddr, paddr, size) ||
        !kernel_flags_valid(flags) ||
        range_overlaps_kernel_globals(vaddr, size) ||
        range_overlaps_user_regions(vaddr, size)) {
        return false;
    }

    record = find_free_fault_range_slot(kernel_mappings, VM_MAX_KERNEL_MAPPINGS);
    if (record == NULL || !map_range_pages_internal(vaddr, paddr, size, flags)) {
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

static bool register_kernel_fault_range(uintptr_t vaddr,
                                        uintptr_t paddr,
                                        size_t size,
                                        uint64_t flags) {
    struct VmFaultRange* record = NULL;

    if (root_table == NULL ||
        !mapped_range_args_valid(vaddr, paddr, size) ||
        !kernel_flags_valid(flags) ||
        range_overlaps_kernel_globals(vaddr, size) ||
        range_overlaps_user_regions(vaddr, size)) {
        return false;
    }

    record =
        find_free_fault_range_slot(kernel_fault_ranges, VM_MAX_KERNEL_FAULT_RANGES);
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
    if (enabled) {
        vm_flush_tlb();
    }
}

static const struct VmFaultActionRule* find_fault_action(uint64_t cause,
                                                         uintptr_t fault_addr) {
    size_t i = 0;

    for (i = 0; i < VM_MAX_FAULT_ACTIONS; ++i) {
        const struct VmFaultActionRule* rule = &fault_actions[i];

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

bool vm_init(void) {
    size_t i = 0;

    if (root_table != NULL) {
        return false;
    }

    root_table = alloc_table_page();
    if (root_table == NULL) {
        root_table_pa = 0;
        satp_value = 0;
        enabled = false;
        return false;
    }

    root_table_pa = (uintptr_t)root_table;
    satp_value = RISCV_SATP_MODE_SV39 |
                 (((uint64_t)(root_table_pa >> SV39_PAGE_SHIFT)) & SV39_PPN_MASK);
    enabled = false;

    for (i = 0; i < VM_MAX_KERNEL_MAPPINGS; ++i) {
        kernel_mappings[i].valid = false;
    }

    for (i = 0; i < VM_MAX_KERNEL_FAULT_RANGES; ++i) {
        kernel_fault_ranges[i].valid = false;
    }

    for (i = 0; i < VM_MAX_USER_REGIONS; ++i) {
        user_regions[i].valid = false;
    }

    for (i = 0; i < VM_MAX_FAULT_ACTIONS; ++i) {
        fault_actions[i].valid = false;
    }

    return true;
}

bool vm_map_identity_1g(uintptr_t base, uint64_t flags) {
    const size_t index = vpn_index(base, 2);
    struct VmFaultRange* record = NULL;

    if (root_table == NULL ||
        (base & (SV39_SUPERPAGE_SIZE_1G - 1U)) != 0 ||
        !kernel_flags_valid(flags) ||
        range_overlaps_kernel_globals(base, SV39_SUPERPAGE_SIZE_1G) ||
        range_overlaps_user_regions(base, SV39_SUPERPAGE_SIZE_1G)) {
        return false;
    }

    if ((root_table[index] & SV39_PTE_VALID) != 0) {
        return false;
    }

    record = find_free_fault_range_slot(kernel_mappings, VM_MAX_KERNEL_MAPPINGS);
    if (record == NULL) {
        return false;
    }

    root_table[index] = pte_from_pa(base, SV39_PTE_VALID | flags);
    record->valid = true;
    record->vaddr = base;
    record->paddr = base;
    record->size = SV39_SUPERPAGE_SIZE_1G;
    record->flags = flags;
    flush_tlb_if_enabled();
    return true;
}

bool vm_map_page(uintptr_t vaddr, uintptr_t paddr, uint64_t flags) {
    return vm_map_range(vaddr, paddr, MEMORY_PAGE_SIZE, flags);
}

bool vm_map_range(uintptr_t vaddr, uintptr_t paddr, size_t size, uint64_t flags) {
    if ((flags & VM_PAGE_USER) != 0) {
        return vm_map_user_range(vaddr, paddr, size, flags);
    }

    return map_kernel_global_range(vaddr, paddr, size, flags);
}

bool vm_map_kernel_range(uintptr_t vaddr, uintptr_t paddr, size_t size, uint64_t flags) {
    if (!vm_range_is_kernel(vaddr, size)) {
        return false;
    }

    return map_kernel_global_range(vaddr, paddr, size, flags);
}

bool vm_map_user_range(uintptr_t vaddr, uintptr_t paddr, size_t size, uint64_t flags) {
    bool created = false;
    struct VmUserRegion* region = NULL;

    region = ensure_user_region(vaddr, size, flags, &created);
    if (region == NULL || !mapped_range_args_valid(vaddr, paddr, size)) {
        return false;
    }

    if (!map_range_pages_internal(vaddr, paddr, size, flags)) {
        return false;
    }

    if (created) {
        region->valid = true;
        region->vaddr = vaddr;
        region->size = size;
        region->flags = flags;
        region->has_fault_backing = false;
        region->fault_paddr = 0;
    }

    flush_tlb_if_enabled();
    return true;
}

bool vm_unmap_page(uintptr_t vaddr) {
    if (find_user_region_containing(vaddr) == NULL || !unmap_page_internal(vaddr)) {
        return false;
    }

    flush_tlb_if_enabled();
    return true;
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

bool vm_register_fault_range(uintptr_t vaddr,
                             uintptr_t paddr,
                             size_t size,
                             uint64_t flags) {
    if ((flags & VM_PAGE_USER) != 0) {
        return false;
    }

    return register_kernel_fault_range(vaddr, paddr, size, flags);
}

bool vm_register_user_fault_range(uintptr_t vaddr,
                                  uintptr_t paddr,
                                  size_t size,
                                  uint64_t flags) {
    bool created = false;
    struct VmUserRegion* region = NULL;

    region = ensure_user_region(vaddr, size, flags, &created);
    if (region == NULL || !mapped_range_args_valid(vaddr, paddr, size) ||
        region->has_fault_backing) {
        return false;
    }

    if (created) {
        region->valid = true;
        region->vaddr = vaddr;
        region->size = size;
        region->flags = flags;
        region->has_fault_backing = false;
        region->fault_paddr = 0;
    }

    region->has_fault_backing = true;
    region->fault_paddr = paddr;
    return true;
}

static bool register_fault_action(uint64_t cause,
                                  uintptr_t vaddr,
                                  size_t size,
                                  vm_fault_action_t action,
                                  volatile uintptr_t* resume_pc_slot) {
    size_t i = 0;
    struct VmFaultActionRule* free_slot = NULL;

    if (root_table == NULL || !page_fault_cause_valid(cause) ||
        !span_args_valid(vaddr, size)) {
        return false;
    }

    if (action == VM_FAULT_ACTION_RESUME_AT_SLOT && resume_pc_slot == NULL) {
        return false;
    }

    for (i = 0; i < VM_MAX_FAULT_ACTIONS; ++i) {
        struct VmFaultActionRule* rule = &fault_actions[i];

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

bool vm_register_fault_skip(uint64_t cause, uintptr_t vaddr, size_t size) {
    return register_fault_action(cause,
                                 vaddr,
                                 size,
                                 VM_FAULT_ACTION_SKIP_INSTRUCTION,
                                 NULL);
}

bool vm_register_fault_resume_slot(uint64_t cause,
                                   uintptr_t vaddr,
                                   size_t size,
                                   volatile uintptr_t* resume_pc_slot) {
    return register_fault_action(cause,
                                 vaddr,
                                 size,
                                 VM_FAULT_ACTION_RESUME_AT_SLOT,
                                 resume_pc_slot);
}

bool vm_handle_page_fault(uint64_t cause, uint64_t epc, uint64_t tval) {
    const struct VmUserRegion* user_region = NULL;
    const struct VmFaultRange* range = NULL;
    const struct VmFaultActionRule* action = NULL;
    const uintptr_t fault_page = align_down_page((uintptr_t)tval);
    const uintptr_t fault_addr = (uintptr_t)tval;
    uintptr_t offset = 0;
    uintptr_t resume_pc = 0;

    if (root_table == NULL) {
        return false;
    }

    user_region = find_user_region_containing(fault_page);
    if (user_region != NULL && user_region->has_fault_backing &&
        fault_range_allows_access(user_region->flags, cause)) {
        offset = fault_page - user_region->vaddr;
        if (!map_page_internal(fault_page,
                               user_region->fault_paddr + offset,
                               user_region->flags)) {
            return false;
        }

        vm_flush_tlb();
        return true;
    }

    range = find_kernel_fault_range(fault_page);
    if (range != NULL && fault_range_allows_access(range->flags, cause)) {
        offset = fault_page - range->vaddr;
        if (!map_page_internal(fault_page, range->paddr + offset, range->flags)) {
            return false;
        }

        vm_flush_tlb();
        return true;
    }

    action = find_fault_action(cause, fault_addr);
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

bool vm_enable(void) {
    if (root_table == NULL || satp_value == 0) {
        return false;
    }

    riscv_write_satp(satp_value);
    vm_flush_tlb();
    enabled = true;
    return true;
}

uintptr_t vm_root_table(void) {
    return root_table_pa;
}

uint64_t vm_satp_value(void) {
    return satp_value;
}

bool vm_is_enabled(void) {
    return enabled;
}
