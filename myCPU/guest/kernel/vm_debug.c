#include "vm_debug.h"

#include "vm_private.h"

bool vm_debug_walk(const vm_address_space_t* address_space,
                   uintptr_t vaddr,
                   vm_debug_walk_result_t* out_result) {
    vm_debug_walk_result_t result = {0};
    uint64_t* table;
    unsigned level;
    if (address_space == NULL || out_result == NULL ||
        address_space->root_table == NULL) {
        return false;
    }

    table = address_space->root_table;
    level = 2U;
    while (true) {
        const size_t index = vpn_index(vaddr, level);
        const uint64_t entry = table[index];

        result.entries[level] = entry;
        result.entry_valid[level] = (entry & SV39_PTE_VALID) != 0;
        if (!result.entry_valid[level]) {
            *out_result = result;
            return false;
        }

        if ((entry & SV39_PTE_LEAF_MASK) != 0) {
            const uintptr_t page_size =
                (uintptr_t)1ULL << (SV39_PAGE_SHIFT + level * SV39_LEVEL_BITS);
            const uintptr_t page_mask = page_size - 1U;
            const uintptr_t pa_base =
                (uintptr_t)(((entry >> 10) & SV39_PPN_MASK) << SV39_PAGE_SHIFT);

            result.valid = true;
            result.leaf_level = level;
            result.leaf_pte = entry;
            result.resolved_pa = (pa_base & ~page_mask) | (vaddr & page_mask);
            *out_result = result;
            return true;
        }

        if (level == 0U) {
            *out_result = result;
            return false;
        }

        table = table_from_pte(entry);
        level -= 1U;
    }
}
