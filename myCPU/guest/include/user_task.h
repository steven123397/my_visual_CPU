#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "trap.h"
#include "vm.h"

typedef struct UserTask {
    vm_address_space_t* address_space;
    vm_process_t process;
    trap_user_runtime_t runtime;
} user_task_t;

void user_task_init(user_task_t* user_task);
bool user_task_create(user_task_t* user_task);
bool user_task_destroy(user_task_t* user_task);
vm_address_space_t* user_task_address_space(user_task_t* user_task);
vm_process_t* user_task_process(user_task_t* user_task);
trap_user_runtime_t* user_task_runtime(user_task_t* user_task);
bool user_task_bind_regions(
    user_task_t* user_task,
    const vm_process_user_region_binding_t* bindings,
    size_t binding_count);
bool user_task_map_object_region_at(user_task_t* user_task,
                                    vm_user_region_t* region,
                                    uintptr_t vaddr,
                                    size_t size,
                                    uint64_t flags,
                                    vm_object_t* object,
                                    size_t object_offset);
bool user_task_map_object_region(user_task_t* user_task,
                                 vm_user_region_t* region,
                                 uintptr_t vaddr,
                                 size_t size,
                                 uint64_t flags,
                                 vm_object_t* object);
bool user_task_set_fault_object_region_at(user_task_t* user_task,
                                          vm_user_region_t* region,
                                          uintptr_t vaddr,
                                          size_t size,
                                          uint64_t flags,
                                          vm_object_t* object,
                                          size_t object_offset);
bool user_task_set_fault_object_region(user_task_t* user_task,
                                       vm_user_region_t* region,
                                       uintptr_t vaddr,
                                       size_t size,
                                       uint64_t flags,
                                       vm_object_t* object);
bool user_task_prepare_standard(
    user_task_t* user_task,
    trap_context_t* trap_context,
    uintptr_t entry_pc,
    uintptr_t user_sp,
    uintptr_t arg0,
    void* trap_stack_base,
    size_t trap_stack_size,
    uintptr_t expected_ecall_pc,
    trap_user_runtime_validate_t validate,
    void* validate_context,
    trap_interrupt_handler_t supervisor_timer_post_handler,
    void* supervisor_timer_post_context,
    trap_supervisor_external_post_handler_t supervisor_external_post_handler,
    void* supervisor_external_post_context);
bool user_task_activate(user_task_t* user_task);
bool user_task_deactivate(user_task_t* user_task);
bool user_task_is_active(const user_task_t* user_task);
bool user_task_is_runnable(const user_task_t* user_task);
bool user_task_enter(const user_task_t* user_task);
