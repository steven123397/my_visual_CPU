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
bool user_program_smoke_validate_vm_lifecycle(uintptr_t user_region_vaddr,
                                              size_t expected_free_pages);
bool user_program_smoke_prepare_address_space(
    user_program_smoke_t* smoke,
    user_program_t* program,
    uintptr_t backing_page_paddr,
    uintptr_t remap_page_paddr,
    uintptr_t fault_skip_vaddr,
    size_t fault_skip_size,
    uintptr_t fault_resume_vaddr,
    size_t fault_resume_size,
    volatile uintptr_t* fault_resume_pc_slot);
bool user_program_smoke_prepare_runtime(
    user_program_smoke_t* smoke,
    trap_context_t* trap_context,
    uintptr_t arg0,
    void* trap_stack_base,
    size_t trap_stack_size,
    trap_user_runtime_validate_t validate,
    void* validate_context,
    trap_interrupt_handler_t supervisor_timer_post_handler,
    void* supervisor_timer_post_context,
    trap_supervisor_external_post_handler_t supervisor_external_post_handler,
    void* supervisor_external_post_context);
bool user_program_smoke_unmap_remap_page(user_program_smoke_t* smoke);
bool user_program_smoke_rebind_alias_fault_object(user_program_smoke_t* smoke);
