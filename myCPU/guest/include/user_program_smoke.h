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

typedef struct UserProgramSmokeActivePhase {
    uint32_t alias_store_value;
    uint32_t backing_store_value;
    uint32_t anon_value0;
    uint32_t anon_value1;
    uint32_t anon_tail_value0;
    uint32_t anon_tail_value1;
    uint32_t remap_store_value;
    volatile const uint32_t* rodata_marker;
    uint32_t rodata_expected;
    uintptr_t instruction_fault_target;
    volatile uintptr_t* fault_resume_pc_slot;
} user_program_smoke_active_phase_t;

typedef struct UserProgramSmokePrepare {
    trap_context_t* trap_context;
    uintptr_t backing_page_paddr;
    uintptr_t user_stack_paddr;
    uintptr_t remap_page_paddr;
    uintptr_t fault_skip_vaddr;
    size_t fault_skip_size;
    uintptr_t fault_resume_vaddr;
    size_t fault_resume_size;
    volatile uintptr_t* fault_resume_pc_slot;
    uintptr_t arg0;
    void* trap_stack_base;
    size_t trap_stack_size;
    trap_user_runtime_validate_t validate;
    void* validate_context;
    trap_interrupt_handler_t supervisor_timer_post_handler;
    void* supervisor_timer_post_context;
    trap_supervisor_external_post_handler_t supervisor_external_post_handler;
    void* supervisor_external_post_context;
} user_program_smoke_prepare_t;

typedef struct UserProgramSmokeRound {
    trap_context_t* expected_trap_context;
    uint32_t* timer_signal_page;
    size_t timer_signal_index;
    uint32_t timer_signal_value;
    uint32_t* external_signal_page;
    size_t external_signal_index;
    uint32_t external_signal_value;
    uint64_t timer_delta;
} user_program_smoke_round_t;

void user_program_smoke_init(user_program_smoke_t* smoke);
bool user_program_smoke_plan_standard(user_program_t* program,
                                      uintptr_t exec_symbol,
                                      uintptr_t ecall_symbol);
bool user_program_smoke_validate_standard_plan(const user_program_t* program,
                                               uintptr_t user_base,
                                               uintptr_t user_limit);
bool user_program_smoke_validate_vm_lifecycle(uintptr_t user_region_vaddr,
                                              size_t expected_free_pages);
bool user_program_smoke_validate_lifecycle(
    trap_context_t* trap_context,
    uintptr_t exec_symbol,
    uintptr_t ecall_symbol,
    uintptr_t alias_backing_paddr,
    uintptr_t user_stack_paddr,
    uintptr_t arg0,
    void* trap_stack_base,
    size_t trap_stack_size);
bool user_program_smoke_prepare_standard(user_program_smoke_t* smoke,
                                         user_program_t* program,
                                         const user_program_smoke_prepare_t* prepare);
bool user_program_smoke_activate_supervisor_access(
    user_program_smoke_t* smoke,
    trap_context_t* expected_trap_context);
bool user_program_smoke_deactivate_supervisor_only(
    user_program_smoke_t* smoke,
    trap_context_t* expected_trap_context);
bool user_program_smoke_exercise_active_memory(
    user_program_smoke_t* smoke,
    uint32_t* backing_page,
    uint32_t* remap_page,
    const user_program_smoke_active_phase_t* phase);
bool user_program_smoke_enter_round(user_program_smoke_t* smoke,
                                    const user_program_smoke_round_t* round);
