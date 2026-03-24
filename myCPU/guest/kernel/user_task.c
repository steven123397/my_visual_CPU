#include "user_task.h"

static bool user_task_created(const user_task_t* user_task) {
    return user_task != NULL &&
           user_task->address_space != NULL &&
           user_task->process.address_space == user_task->address_space;
}

void user_task_init(user_task_t* user_task) {
    size_t i = 0;

    if (user_task == NULL) {
        return;
    }

    user_task->address_space = NULL;
    user_task->process.address_space = NULL;
    user_task->process.entry_pc = 0;
    user_task->process.user_sp = 0;
    for (i = 0; i < VM_PROCESS_MAX_USER_REGIONS; ++i) {
        user_task->process.user_regions[i] = NULL;
    }
    trap_user_runtime_init(&user_task->runtime);
}

bool user_task_create(user_task_t* user_task) {
    vm_address_space_t* address_space = NULL;

    if (user_task == NULL || user_task->address_space != NULL ||
        user_task->process.address_space != NULL) {
        return false;
    }

    if (!vm_address_space_create(&address_space)) {
        return false;
    }

    if (!vm_process_create(&user_task->process, address_space)) {
        if (!vm_address_space_destroy(address_space)) {
            return false;
        }
        return false;
    }

    user_task->address_space = address_space;
    return true;
}

bool user_task_destroy(user_task_t* user_task) {
    vm_address_space_t* address_space = NULL;

    if (!user_task_created(user_task)) {
        return false;
    }

    address_space = user_task->address_space;
    if (!vm_process_reset(&user_task->process) ||
        !vm_address_space_destroy(address_space)) {
        return false;
    }

    user_task_init(user_task);
    return true;
}

vm_address_space_t* user_task_address_space(user_task_t* user_task) {
    if (!user_task_created(user_task)) {
        return NULL;
    }

    return user_task->address_space;
}

vm_process_t* user_task_process(user_task_t* user_task) {
    if (!user_task_created(user_task)) {
        return NULL;
    }

    return &user_task->process;
}

trap_user_runtime_t* user_task_runtime(user_task_t* user_task) {
    if (user_task == NULL) {
        return NULL;
    }

    return &user_task->runtime;
}

bool user_task_bind_regions(
    user_task_t* user_task,
    const vm_process_user_region_binding_t* bindings,
    size_t binding_count) {
    if (!user_task_created(user_task)) {
        return false;
    }

    return vm_process_bind_user_regions(&user_task->process,
                                        bindings,
                                        binding_count);
}

bool user_task_map_object_region_at(user_task_t* user_task,
                                    vm_user_region_t* region,
                                    uintptr_t vaddr,
                                    size_t size,
                                    uint64_t flags,
                                    vm_object_t* object,
                                    size_t object_offset) {
    if (!user_task_created(user_task)) {
        return false;
    }

    return vm_process_map_object_region_at(&user_task->process,
                                           region,
                                           vaddr,
                                           size,
                                           flags,
                                           object,
                                           object_offset);
}

bool user_task_map_object_region(user_task_t* user_task,
                                 vm_user_region_t* region,
                                 uintptr_t vaddr,
                                 size_t size,
                                 uint64_t flags,
                                 vm_object_t* object) {
    return user_task_map_object_region_at(user_task,
                                          region,
                                          vaddr,
                                          size,
                                          flags,
                                          object,
                                          0);
}

bool user_task_set_fault_object_region_at(user_task_t* user_task,
                                          vm_user_region_t* region,
                                          uintptr_t vaddr,
                                          size_t size,
                                          uint64_t flags,
                                          vm_object_t* object,
                                          size_t object_offset) {
    if (!user_task_created(user_task)) {
        return false;
    }

    return vm_process_set_fault_object_region_at(&user_task->process,
                                                 region,
                                                 vaddr,
                                                 size,
                                                 flags,
                                                 object,
                                                 object_offset);
}

bool user_task_set_fault_object_region(user_task_t* user_task,
                                       vm_user_region_t* region,
                                       uintptr_t vaddr,
                                       size_t size,
                                       uint64_t flags,
                                       vm_object_t* object) {
    return user_task_set_fault_object_region_at(user_task,
                                                region,
                                                vaddr,
                                                size,
                                                flags,
                                                object,
                                                0);
}

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
    void* supervisor_external_post_context) {
    if (!user_task_created(user_task)) {
        return false;
    }

    return trap_user_runtime_prepare_standard(
        &user_task->runtime,
        trap_context,
        &user_task->process,
        entry_pc,
        user_sp,
        arg0,
        trap_stack_base,
        trap_stack_size,
        expected_ecall_pc,
        validate,
        validate_context,
        supervisor_timer_post_handler,
        supervisor_timer_post_context,
        supervisor_external_post_handler,
        supervisor_external_post_context);
}

bool user_task_activate(user_task_t* user_task) {
    if (!user_task_created(user_task)) {
        return false;
    }

    return trap_user_runtime_activate(&user_task->runtime);
}

bool user_task_deactivate(user_task_t* user_task) {
    if (!user_task_created(user_task)) {
        return false;
    }

    return trap_user_runtime_deactivate(&user_task->runtime);
}

bool user_task_is_active(const user_task_t* user_task) {
    return user_task_created(user_task) &&
           trap_user_runtime_is_active(&user_task->runtime);
}

bool user_task_is_runnable(const user_task_t* user_task) {
    return user_task_created(user_task) &&
           vm_process_is_runnable(&user_task->process);
}

bool user_task_enter(const user_task_t* user_task) {
    return user_task_created(user_task) &&
           trap_user_runtime_enter(&user_task->runtime);
}
