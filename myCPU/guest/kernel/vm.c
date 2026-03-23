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
#define SV39_PPN_MASK ((1ULL << 44) - 1ULL)

static uint64_t* root_table = NULL;
static uintptr_t root_table_pa = 0;
static uint64_t satp_value = 0;
static bool enabled = false;

static size_t vpn_index(uintptr_t vaddr, unsigned level) {
    return (size_t)((vaddr >> (SV39_PAGE_SHIFT + level * SV39_LEVEL_BITS)) &
                    (SV39_LEVEL_ENTRIES - 1U));
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

static uint64_t* ensure_next_table(uint64_t* table, size_t index) {
    uint64_t entry = table[index];

    if ((entry & SV39_PTE_VALID) == 0) {
        uint64_t* next = alloc_table_page();
        if (next == NULL) {
            return NULL;
        }
        table[index] = pte_from_pa((uintptr_t)next, SV39_PTE_VALID);
        return next;
    }

    if ((entry & SV39_PTE_LEAF_MASK) != 0) {
        return NULL;
    }

    return table_from_pte(entry);
}

static uint64_t* next_table_if_present(uint64_t* table, size_t index) {
    const uint64_t entry = table[index];

    if ((entry & SV39_PTE_VALID) == 0 || (entry & SV39_PTE_LEAF_MASK) != 0) {
        return NULL;
    }

    return table_from_pte(entry);
}

bool vm_init(void) {
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
    return true;
}

bool vm_map_identity_1g(uintptr_t base, uint64_t flags) {
    const size_t index = vpn_index(base, 2);

    if (root_table == NULL ||
        (base & (SV39_SUPERPAGE_SIZE_1G - 1U)) != 0 ||
        flags == 0 ||
        (flags & SV39_PTE_LEAF_MASK) == 0) {
        return false;
    }

    if ((root_table[index] & SV39_PTE_VALID) != 0) {
        return false;
    }

    root_table[index] = pte_from_pa(base, SV39_PTE_VALID | flags);
    return true;
}

bool vm_map_page(uintptr_t vaddr, uintptr_t paddr, uint64_t flags) {
    uint64_t* level1 = NULL;
    uint64_t* level0 = NULL;
    const size_t root_index = vpn_index(vaddr, 2);
    const size_t level1_index = vpn_index(vaddr, 1);
    const size_t level0_index = vpn_index(vaddr, 0);

    if (root_table == NULL ||
        (vaddr & (MEMORY_PAGE_SIZE - 1U)) != 0 ||
        (paddr & (MEMORY_PAGE_SIZE - 1U)) != 0 ||
        flags == 0 ||
        (flags & SV39_PTE_LEAF_MASK) == 0) {
        return false;
    }

    level1 = ensure_next_table(root_table, root_index);
    if (level1 == NULL) {
        return false;
    }

    level0 = ensure_next_table(level1, level1_index);
    if (level0 == NULL) {
        return false;
    }

    if ((level0[level0_index] & SV39_PTE_VALID) != 0) {
        return false;
    }

    level0[level0_index] = pte_from_pa(paddr, SV39_PTE_VALID | flags);
    return true;
}

bool vm_map_range(uintptr_t vaddr, uintptr_t paddr, size_t size, uint64_t flags) {
    size_t offset = 0;

    if (size == 0) {
        return true;
    }

    if ((size & (MEMORY_PAGE_SIZE - 1U)) != 0) {
        return false;
    }

    while (offset < size) {
        if (!vm_map_page(vaddr + offset, paddr + offset, flags)) {
            return false;
        }
        offset += MEMORY_PAGE_SIZE;
    }

    return true;
}

bool vm_unmap_page(uintptr_t vaddr) {
    uint64_t* level1 = NULL;
    uint64_t* level0 = NULL;
    const size_t root_index = vpn_index(vaddr, 2);
    const size_t level1_index = vpn_index(vaddr, 1);
    const size_t level0_index = vpn_index(vaddr, 0);

    if (root_table == NULL || (vaddr & (MEMORY_PAGE_SIZE - 1U)) != 0) {
        return false;
    }

    level1 = next_table_if_present(root_table, root_index);
    if (level1 == NULL) {
        return false;
    }

    level0 = next_table_if_present(level1, level1_index);
    if (level0 == NULL || (level0[level0_index] & SV39_PTE_VALID) == 0) {
        return false;
    }

    level0[level0_index] = 0;
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
