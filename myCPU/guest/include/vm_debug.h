#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "vm.h"

typedef struct VmDebugWalkResult {
    bool valid;
    unsigned leaf_level;
    uintptr_t resolved_pa;
    uint64_t leaf_pte;
    uint64_t entries[3];
    bool entry_valid[3];
} vm_debug_walk_result_t;

bool vm_debug_walk(const vm_address_space_t* address_space,
                   uintptr_t vaddr,
                   vm_debug_walk_result_t* out_result);
