#include "vm.h"

#include <stddef.h>
#include <stdint.h>

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
#define VM_MAX_FAULT_RANGES 16U

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

static struct VmFaultRange fault_ranges[VM_MAX_FAULT_RANGES];

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

static bool range_is_page_aligned(uintptr_t vaddr, uintptr_t paddr, size_t size) {
    return (vaddr & (MEMORY_PAGE_SIZE - 1U)) == 0 &&
           (paddr & (MEMORY_PAGE_SIZE - 1U)) == 0 &&
           (size & (MEMORY_PAGE_SIZE - 1U)) == 0;
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

static bool range_args_valid(uintptr_t vaddr, uintptr_t paddr, size_t size) {
    if (size == 0 || !range_is_page_aligned(vaddr, paddr, size)) {
        return false;
    }

    return !range_overflows(vaddr, size) && !range_overflows(paddr, size);
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

    if (!range_args_valid(vaddr, paddr, MEMORY_PAGE_SIZE) || !flags_valid(flags)) {
        return false;
    }

    slot = lookup_level0_slot(vaddr, true, &conflict);
    if (slot == NULL || conflict || (*slot & SV39_PTE_VALID) != 0) {
        return false;
    }

    *slot = pte_from_pa(paddr, SV39_PTE_VALID | flags);
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

static const struct VmFaultRange* find_fault_range(uintptr_t fault_page) {
    size_t i = 0;

    for (i = 0; i < VM_MAX_FAULT_RANGES; ++i) {
        const struct VmFaultRange* range = &fault_ranges[i];

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

    for (i = 0; i < VM_MAX_FAULT_RANGES; ++i) {
        fault_ranges[i].valid = false;
    }

    return true;
}

bool vm_map_identity_1g(uintptr_t base, uint64_t flags) {
    const size_t index = vpn_index(base, 2);

    if (root_table == NULL ||
        (base & (SV39_SUPERPAGE_SIZE_1G - 1U)) != 0 ||
        !flags_valid(flags)) {
        return false;
    }

    if ((root_table[index] & SV39_PTE_VALID) != 0) {
        return false;
    }

    root_table[index] = pte_from_pa(base, SV39_PTE_VALID | flags);
    return true;
}

bool vm_map_page(uintptr_t vaddr, uintptr_t paddr, uint64_t flags) {
    if (root_table == NULL) {
        return false;
    }

    return map_page_internal(vaddr, paddr, flags);
}

bool vm_map_range(uintptr_t vaddr, uintptr_t paddr, size_t size, uint64_t flags) {
    size_t offset = 0;
    size_t mapped = 0;

    if (root_table == NULL ||
        !range_args_valid(vaddr, paddr, size) ||
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
                vm_unmap_page(vaddr + rollback);
                rollback += MEMORY_PAGE_SIZE;
            }
            return false;
        }

        mapped += MEMORY_PAGE_SIZE;
        offset += MEMORY_PAGE_SIZE;
    }

    return true;
}

bool vm_map_kernel_range(uintptr_t vaddr, uintptr_t paddr, size_t size, uint64_t flags) {
    if ((flags & VM_PAGE_USER) != 0) {
        return false;
    }

    return vm_map_range(vaddr, paddr, size, flags);
}

bool vm_map_user_range(uintptr_t vaddr, uintptr_t paddr, size_t size, uint64_t flags) {
    if ((flags & VM_PAGE_USER) == 0) {
        return false;
    }

    return vm_map_range(vaddr, paddr, size, flags);
}

bool vm_unmap_page(uintptr_t vaddr) {
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

bool vm_register_fault_range(uintptr_t vaddr,
                             uintptr_t paddr,
                             size_t size,
                             uint64_t flags) {
    size_t i = 0;
    struct VmFaultRange* free_slot = NULL;

    if (root_table == NULL ||
        !range_args_valid(vaddr, paddr, size) ||
        !flags_valid(flags)) {
        return false;
    }

    for (i = 0; i < VM_MAX_FAULT_RANGES; ++i) {
        struct VmFaultRange* range = &fault_ranges[i];

        if (!range->valid) {
            if (free_slot == NULL) {
                free_slot = range;
            }
            continue;
        }

        if (ranges_overlap(vaddr, size, range->vaddr, range->size)) {
            return false;
        }
    }

    if (free_slot == NULL) {
        return false;
    }

    free_slot->valid = true;
    free_slot->vaddr = vaddr;
    free_slot->paddr = paddr;
    free_slot->size = size;
    free_slot->flags = flags;
    return true;
}

bool vm_handle_page_fault(uint64_t cause, uint64_t epc, uint64_t tval) {
    const struct VmFaultRange* range = NULL;
    const uintptr_t fault_page = align_down_page((uintptr_t)tval);
    uintptr_t offset = 0;

    (void)epc;

    if (root_table == NULL) {
        return false;
    }

    range = find_fault_range(fault_page);
    if (range == NULL || !fault_range_allows_access(range->flags, cause)) {
        return false;
    }

    offset = fault_page - range->vaddr;
    if (!map_page_internal(fault_page, range->paddr + offset, range->flags)) {
        return false;
    }

    vm_flush_tlb();
    return true;
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
