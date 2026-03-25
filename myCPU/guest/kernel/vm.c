#include <stddef.h>
#include <stdint.h>

#include "pmm.h"
#include "riscv.h"
#include "runtime_context.h"
#include "vm_private.h"

static void zero_page(void* page) {
    size_t i = 0;
    uint64_t* words = (uint64_t*)page;

    for (i = 0; i < MEMORY_PAGE_SIZE / sizeof(uint64_t); ++i) {
        words[i] = 0;
    }
}

void* alloc_zeroed_page(void) {
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

bool map_page_internal(vm_address_space_t* address_space,
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

bool unmap_page_internal(vm_address_space_t* address_space, uintptr_t vaddr) {
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

bool can_map_page(vm_address_space_t* address_space, uintptr_t vaddr) {
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

void flush_tlb_if_enabled(void) {
    vm_address_space_t* address_space = runtime_context_active_address_space();

    if (address_space != NULL && address_space->enabled) {
        vm_flush_tlb();
    }
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

void vm_flush_tlb(void) {
    riscv_sfence_vma();
}
