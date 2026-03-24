#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "user_task_bootstrap.h"

typedef enum UserProgramValueId {
    USER_PROGRAM_VALUE_EXEC_PAGE_PADDR = 0,
    USER_PROGRAM_VALUE_EXEC_VADDR,
    USER_PROGRAM_VALUE_STACK_VADDR,
    USER_PROGRAM_VALUE_ALIAS_VADDR,
    USER_PROGRAM_VALUE_ANON_VADDR,
    USER_PROGRAM_VALUE_ANON_TAIL_VADDR,
    USER_PROGRAM_VALUE_ENTRY_PC,
    USER_PROGRAM_VALUE_EXPECTED_ECALL_PC,
    USER_PROGRAM_VALUE_USER_SP,
} user_program_value_id_t;

typedef enum UserProgramRegionId {
    USER_PROGRAM_REGION_EXEC = 0,
    USER_PROGRAM_REGION_STACK,
    USER_PROGRAM_REGION_ALIAS,
    USER_PROGRAM_REGION_ANON,
    USER_PROGRAM_REGION_ANON_TAIL,
} user_program_region_id_t;

typedef enum UserProgramObjectId {
    USER_PROGRAM_OBJECT_EXEC = 0,
    USER_PROGRAM_OBJECT_STACK,
    USER_PROGRAM_OBJECT_ALIAS,
    USER_PROGRAM_OBJECT_ANON,
} user_program_object_id_t;

typedef struct UserProgram {
    user_task_t user_task;
    user_task_bootstrap_t bootstrap;
} user_program_t;

void user_program_init(user_program_t* program);
bool user_program_destroy(user_program_t* program);
bool user_program_plan_standard(user_program_t* program,
                                uintptr_t exec_symbol,
                                uintptr_t ecall_symbol);
bool user_program_create(user_program_t* program,
                         uintptr_t alias_backing_paddr,
                         uintptr_t user_stack_paddr);
vm_address_space_t* user_program_address_space(user_program_t* program);
vm_process_t* user_program_process(user_program_t* program);
trap_user_runtime_t* user_program_runtime(user_program_t* program);
bool user_program_map_object_region_at(user_program_t* program,
                                       vm_user_region_t* region,
                                       uintptr_t vaddr,
                                       size_t size,
                                       uint64_t flags,
                                       vm_object_t* object,
                                       size_t object_offset);
bool user_program_map_object_region(user_program_t* program,
                                    vm_user_region_t* region,
                                    uintptr_t vaddr,
                                    size_t size,
                                    uint64_t flags,
                                    vm_object_t* object);
bool user_program_set_fault_object_region_at(user_program_t* program,
                                             vm_user_region_t* region,
                                             uintptr_t vaddr,
                                             size_t size,
                                             uint64_t flags,
                                             vm_object_t* object,
                                             size_t object_offset);
bool user_program_set_fault_object_region(user_program_t* program,
                                          vm_user_region_t* region,
                                          uintptr_t vaddr,
                                          size_t size,
                                          uint64_t flags,
                                          vm_object_t* object);
bool user_program_prepare_standard(
    user_program_t* program,
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
bool user_program_activate(user_program_t* program);
bool user_program_is_active(const user_program_t* program);
bool user_program_is_runnable(const user_program_t* program);
bool user_program_enter(const user_program_t* program);
uintptr_t user_program_value(const user_program_t* program,
                             user_program_value_id_t value_id);
bool user_program_region_contains(const user_program_t* program,
                                  user_program_region_id_t region_id,
                                  uintptr_t vaddr,
                                  size_t size);
bool user_program_unmap_region_page(user_program_t* program,
                                    user_program_region_id_t region_id,
                                    uintptr_t vaddr);
bool user_program_unmap_region_base_page(user_program_t* program,
                                         user_program_region_id_t region_id);
bool user_program_set_region_fault_object(user_program_t* program,
                                          user_program_region_id_t region_id,
                                          vm_object_t* object);
bool user_program_rebind_region_fault_object(
    user_program_t* program,
    user_program_region_id_t region_id,
    vm_object_t* object);
bool user_program_reset_object(user_program_t* program,
                               user_program_object_id_t object_id);
vm_user_region_t* user_program_region(user_program_t* program,
                                      user_program_region_id_t region_id);
vm_object_t* user_program_object(user_program_t* program,
                                 user_program_object_id_t object_id);
