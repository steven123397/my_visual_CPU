#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "user_program.h"

typedef struct UserProgramSmoke {
    user_program_t* program;
    vm_user_region_t remap_region;
    vm_user_region_t invalid_region;
    vm_object_t remap_object;
} user_program_smoke_t;

void user_program_smoke_init(user_program_smoke_t* smoke);
bool user_program_smoke_prepare_fault_orchestration(
    user_program_smoke_t* smoke,
    user_program_t* program,
    uintptr_t remap_page_paddr,
    uintptr_t fault_skip_vaddr,
    size_t fault_skip_size,
    uintptr_t fault_resume_vaddr,
    size_t fault_resume_size,
    volatile uintptr_t* fault_resume_pc_slot);
bool user_program_smoke_unmap_remap_page(user_program_smoke_t* smoke);
bool user_program_smoke_rebind_alias_fault_object(user_program_smoke_t* smoke);
