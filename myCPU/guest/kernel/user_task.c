#include "user_task.h"

/* 把进程描述符清空（无地址空间、无 region）。 */
static void clear_user_task_process(user_task_t* user_task) {
    size_t i = 0;

    if (user_task == NULL) {
        return;
    }

    user_task->process.address_space = NULL;
    user_task->process.entry_pc = 0;
    user_task->process.user_sp = 0;
    for (i = 0; i < VM_PROCESS_MAX_USER_REGIONS; ++i) {
        user_task->process.user_regions[i] = NULL;
    }
}

/* 是否处于可 create 状态（无地址空间且进程未绑定）。 */
static bool user_task_create_ready(const user_task_t* user_task) {
    return user_task != NULL && user_task->address_space == NULL &&
           user_task->process.address_space == NULL;
}

/* user_task 是否已创建（有地址空间且进程已绑定）。 */
static bool user_task_created(const user_task_t* user_task) {
    return user_task != NULL &&
           user_task->address_space != NULL &&
           user_task->process.address_space == user_task->address_space;
}

/* 取可写进程指针（未创建返回 NULL）。 */
static vm_process_t* user_task_process_mut(user_task_t* user_task) {
    return user_task_created(user_task) ? &user_task->process : NULL;
}

/* 取只读进程指针（未创建返回 NULL）。 */
static const vm_process_t* user_task_process_view(const user_task_t* user_task) {
    return user_task_created(user_task) ? &user_task->process : NULL;
}

/* 释放 runtime：活跃则先 deactivate。 */
static bool user_task_release_runtime(user_task_t* user_task) {
    return user_task != NULL &&
           (!trap_user_runtime_is_active(&user_task->runtime) ||
            trap_user_runtime_deactivate(&user_task->runtime));
}

/* 复位进程并销毁地址空间。 */
static bool user_task_release_process_and_address_space(user_task_t* user_task) {
    vm_address_space_t* address_space = NULL;
    vm_process_t* process = user_task_process_mut(user_task);
    bool process_reset_ok = false;

    if (process == NULL) {
        return false;
    }

    address_space = user_task->address_space;
    process_reset_ok = vm_process_reset(process);
    if (!process_reset_ok) {
        return false;
    }

    user_task->address_space = NULL;
    return vm_address_space_destroy(address_space);
}

void user_task_init(user_task_t* user_task) {
    if (user_task == NULL) {
        return;
    }

    user_task->address_space = NULL;
    clear_user_task_process(user_task);
    trap_user_runtime_init(&user_task->runtime);
}

bool user_task_create(user_task_t* user_task) {
    vm_address_space_t* address_space = NULL;

    if (!user_task_create_ready(user_task)) {
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
    bool released_ok = false;

    if (!user_task_created(user_task)) {
        return false;
    }

    if (!user_task_release_runtime(user_task)) {
        return false;
    }

    released_ok = user_task_release_process_and_address_space(user_task);
    if (!released_ok) {
        if (user_task->address_space == NULL &&
            user_task->process.address_space == NULL) {
            user_task_init(user_task);
        }
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
    vm_process_t* process = user_task_process_mut(user_task);

    return process != NULL &&
           vm_process_bind_user_regions(process, bindings, binding_count);
}

bool user_task_map_object_region_at(user_task_t* user_task,
                                    vm_user_region_t* region,
                                    uintptr_t vaddr,
                                    size_t size,
                                    uint64_t flags,
                                    vm_object_t* object,
                                    size_t object_offset) {
    vm_process_t* process = user_task_process_mut(user_task);

    return process != NULL &&
           vm_process_map_object_region_at(process,
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
    vm_process_t* process = user_task_process_mut(user_task);

    return process != NULL &&
           vm_process_set_fault_object_region_at(process,
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
    vm_process_t* process = user_task_process_mut(user_task);

    return process != NULL &&
           trap_user_runtime_prepare_standard(
               &user_task->runtime,
               trap_context,
               process,
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
    return user_task_created(user_task) &&
           trap_user_runtime_activate(&user_task->runtime);
}

bool user_task_deactivate(user_task_t* user_task) {
    return user_task_created(user_task) &&
           trap_user_runtime_deactivate(&user_task->runtime);
}

bool user_task_is_active(const user_task_t* user_task) {
    return user_task_created(user_task) &&
           trap_user_runtime_is_active(&user_task->runtime);
}

bool user_task_is_runnable(const user_task_t* user_task) {
    const vm_process_t* process = user_task_process_view(user_task);

    return process != NULL && vm_process_is_runnable(process);
}

bool user_task_enter(const user_task_t* user_task) {
    return user_task_created(user_task) &&
           trap_user_runtime_enter(&user_task->runtime);
}
