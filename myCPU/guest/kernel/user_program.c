#include "user_program.h"

static user_task_t* user_program_task(user_program_t* program) {
    return program != NULL ? &program->user_task : NULL;
}

static const user_task_t* user_program_task_view(const user_program_t* program) {
    return program != NULL ? &program->user_task : NULL;
}

static bool user_program_created(const user_program_t* program) {
    return program != NULL && program->user_task.address_space != NULL &&
           program->user_task.process.address_space ==
               program->user_task.address_space;
}

static bool user_program_planned(const user_program_t* program) {
    return program != NULL && program->bootstrap.planned;
}

static bool user_program_ready_for_create(const user_program_t* program) {
    return program != NULL && !user_program_created(program) &&
           program->bootstrap.planned && !program->bootstrap.configured &&
           !program->bootstrap.bound;
}

static uintptr_t user_program_exec_symbol(const user_program_t* program) {
    return user_program_planned(program)
               ? program->bootstrap.exec_page_paddr +
                     (program->bootstrap.entry_pc - program->bootstrap.exec_vaddr)
               : 0;
}

static uintptr_t user_program_ecall_symbol(const user_program_t* program) {
    return user_program_planned(program)
               ? program->bootstrap.exec_page_paddr +
                     (program->bootstrap.expected_ecall_pc -
                      program->bootstrap.exec_vaddr)
               : 0;
}

static bool user_program_reset_lifecycle(user_program_t* program) {
    return program != NULL &&
           (!user_program_created(program) || user_task_destroy(&program->user_task)) &&
           user_task_bootstrap_reset(&program->bootstrap);
}

static bool user_program_replan_after_create_failure(user_program_t* program,
                                                     uintptr_t exec_symbol,
                                                     uintptr_t ecall_symbol) {
    return user_program_destroy(program) &&
           user_program_plan_standard(program, exec_symbol, ecall_symbol);
}

static bool user_program_configure_standard(user_program_t* program,
                                            uintptr_t alias_backing_paddr,
                                            uintptr_t user_stack_paddr) {
    return program != NULL &&
           user_task_bootstrap_configure(&program->bootstrap,
                                         &program->user_task,
                                         alias_backing_paddr,
                                         user_stack_paddr) &&
           user_task_bootstrap_bind(&program->bootstrap);
}

static const vm_user_region_t* user_program_region_view(
    const user_program_t* program,
    user_program_region_id_t region_id) {
    if (program == NULL) {
        return NULL;
    }

    switch (region_id) {
        case USER_PROGRAM_REGION_EXEC:
            return &program->bootstrap.exec_region;
        case USER_PROGRAM_REGION_STACK:
            return &program->bootstrap.stack_region;
        case USER_PROGRAM_REGION_ALIAS:
            return &program->bootstrap.alias_region;
        case USER_PROGRAM_REGION_ANON:
            return &program->bootstrap.anon_region;
        case USER_PROGRAM_REGION_ANON_TAIL:
            return &program->bootstrap.anon_tail_region;
    }

    return NULL;
}

static bool restore_region_object_binding(vm_user_region_t* region,
                                          vm_object_t* object,
                                          size_t object_offset,
                                          vm_region_object_mode_t object_mode) {
    if (region == NULL || object == NULL) {
        return true;
    }

    switch (object_mode) {
    case VM_REGION_OBJECT_MAPPED:
        return vm_user_region_map_object_at(region, object, object_offset);
    case VM_REGION_OBJECT_FAULT:
        return vm_user_region_set_fault_object_at(region, object, object_offset);
    case VM_REGION_OBJECT_NONE:
        return true;
    }

    return false;
}

void user_program_init(user_program_t* program) {
    if (program == NULL) {
        return;
    }

    user_task_init(&program->user_task);
    user_task_bootstrap_init(&program->bootstrap);
}

bool user_program_destroy(user_program_t* program) {
    if (!user_program_reset_lifecycle(program)) {
        return false;
    }

    user_program_init(program);
    return true;
}

bool user_program_plan_standard(user_program_t* program,
                                uintptr_t exec_symbol,
                                uintptr_t ecall_symbol) {
    if (program == NULL || user_program_created(program) ||
        user_program_planned(program)) {
        return false;
    }

    return user_task_bootstrap_plan_layout(&program->bootstrap,
                                           exec_symbol,
                                           ecall_symbol);
}

bool user_program_create(user_program_t* program,
                         uintptr_t alias_backing_paddr,
                         uintptr_t user_stack_paddr) {
    const uintptr_t exec_symbol = user_program_exec_symbol(program);
    const uintptr_t ecall_symbol = user_program_ecall_symbol(program);

    if (!user_program_ready_for_create(program)) {
        return false;
    }

    if (!user_task_create(&program->user_task)) {
        return false;
    }

    if (user_program_configure_standard(program,
                                        alias_backing_paddr,
                                        user_stack_paddr)) {
        return true;
    }

    (void)user_program_replan_after_create_failure(program,
                                                   exec_symbol,
                                                   ecall_symbol);
    return false;
}

vm_address_space_t* user_program_address_space(user_program_t* program) {
    user_task_t* user_task = NULL;

    if (!user_program_created(program)) {
        return NULL;
    }

    user_task = user_program_task(program);
    return user_task != NULL ? user_task_address_space(user_task) : NULL;
}

vm_process_t* user_program_process(user_program_t* program) {
    user_task_t* user_task = NULL;

    if (!user_program_created(program)) {
        return NULL;
    }

    user_task = user_program_task(program);
    return user_task != NULL ? user_task_process(user_task) : NULL;
}

trap_user_runtime_t* user_program_runtime(user_program_t* program) {
    user_task_t* user_task = NULL;

    if (!user_program_created(program)) {
        return NULL;
    }

    user_task = user_program_task(program);
    return user_task != NULL ? user_task_runtime(user_task) : NULL;
}

bool user_program_map_object_region_at(user_program_t* program,
                                       vm_user_region_t* region,
                                       uintptr_t vaddr,
                                       size_t size,
                                       uint64_t flags,
                                       vm_object_t* object,
                                       size_t object_offset) {
    user_task_t* user_task = user_program_task(program);

    return user_task != NULL && user_program_created(program) &&
           user_task_map_object_region_at(user_task,
                                          region,
                                          vaddr,
                                          size,
                                          flags,
                                          object,
                                          object_offset);
}

bool user_program_map_object_region(user_program_t* program,
                                    vm_user_region_t* region,
                                    uintptr_t vaddr,
                                    size_t size,
                                    uint64_t flags,
                                    vm_object_t* object) {
    return user_program_map_object_region_at(program,
                                             region,
                                             vaddr,
                                             size,
                                             flags,
                                             object,
                                             0);
}

bool user_program_set_fault_object_region_at(user_program_t* program,
                                             vm_user_region_t* region,
                                             uintptr_t vaddr,
                                             size_t size,
                                             uint64_t flags,
                                             vm_object_t* object,
                                             size_t object_offset) {
    user_task_t* user_task = user_program_task(program);

    return user_task != NULL && user_program_created(program) &&
           user_task_set_fault_object_region_at(user_task,
                                                region,
                                                vaddr,
                                                size,
                                                flags,
                                                object,
                                                object_offset);
}

bool user_program_set_fault_object_region(user_program_t* program,
                                          vm_user_region_t* region,
                                          uintptr_t vaddr,
                                          size_t size,
                                          uint64_t flags,
                                          vm_object_t* object) {
    return user_program_set_fault_object_region_at(program,
                                                   region,
                                                   vaddr,
                                                   size,
                                                   flags,
                                                   object,
                                                   0);
}

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
    void* supervisor_external_post_context) {
    return user_program_created(program) &&
           user_task_bootstrap_prepare_standard(
               &program->bootstrap,
               trap_context,
               arg0,
               trap_stack_base,
               trap_stack_size,
               validate,
               validate_context,
               supervisor_timer_post_handler,
               supervisor_timer_post_context,
               supervisor_external_post_handler,
               supervisor_external_post_context);
}

bool user_program_activate(user_program_t* program) {
    user_task_t* user_task = user_program_task(program);

    return user_task != NULL && user_program_created(program) &&
           user_task_activate(user_task);
}

bool user_program_deactivate(user_program_t* program) {
    user_task_t* user_task = user_program_task(program);

    return user_task != NULL && user_program_created(program) &&
           user_task_deactivate(user_task);
}

bool user_program_is_active(const user_program_t* program) {
    const user_task_t* user_task = user_program_task_view(program);

    return user_task != NULL && user_program_created(program) &&
           user_task_is_active(user_task);
}

bool user_program_is_runnable(const user_program_t* program) {
    const user_task_t* user_task = user_program_task_view(program);

    return user_task != NULL && user_program_created(program) &&
           user_task_is_runnable(user_task);
}

bool user_program_enter(const user_program_t* program) {
    const user_task_t* user_task = user_program_task_view(program);

    return user_task != NULL && user_program_created(program) &&
           user_task_enter(user_task);
}

uintptr_t user_program_value(const user_program_t* program,
                             user_program_value_id_t value_id) {
    if (program == NULL || !program->bootstrap.planned) {
        return 0;
    }

    switch (value_id) {
        case USER_PROGRAM_VALUE_EXEC_PAGE_PADDR:
            return program->bootstrap.exec_page_paddr;
        case USER_PROGRAM_VALUE_EXEC_VADDR:
            return program->bootstrap.exec_vaddr;
        case USER_PROGRAM_VALUE_STACK_VADDR:
            return program->bootstrap.stack_vaddr;
        case USER_PROGRAM_VALUE_ALIAS_VADDR:
            return program->bootstrap.alias_vaddr;
        case USER_PROGRAM_VALUE_ANON_VADDR:
            return program->bootstrap.anon_vaddr;
        case USER_PROGRAM_VALUE_ANON_TAIL_VADDR:
            return program->bootstrap.anon_tail_vaddr;
        case USER_PROGRAM_VALUE_ENTRY_PC:
            return program->bootstrap.entry_pc;
        case USER_PROGRAM_VALUE_EXPECTED_ECALL_PC:
            return program->bootstrap.expected_ecall_pc;
        case USER_PROGRAM_VALUE_USER_SP:
            return program->bootstrap.user_sp;
    }

    return 0;
}

bool user_program_region_contains(const user_program_t* program,
                                  user_program_region_id_t region_id,
                                  uintptr_t vaddr,
                                  size_t size) {
    const vm_user_region_t* region = user_program_region_view(program, region_id);

    return region != NULL && vm_user_region_contains(region, vaddr, size);
}

bool user_program_unmap_region_page(user_program_t* program,
                                    user_program_region_id_t region_id,
                                    uintptr_t vaddr) {
    vm_user_region_t* region = user_program_region(program, region_id);

    return region != NULL && vm_user_region_unmap_page(region, vaddr);
}

bool user_program_unmap_region_base_page(user_program_t* program,
                                         user_program_region_id_t region_id) {
    const vm_user_region_t* region = user_program_region_view(program, region_id);

    return region != NULL &&
           user_program_unmap_region_page(program, region_id, region->vaddr);
}

bool user_program_set_region_fault_object(user_program_t* program,
                                          user_program_region_id_t region_id,
                                          vm_object_t* object) {
    vm_user_region_t* region = user_program_region(program, region_id);

    return region != NULL && object != NULL &&
           vm_user_region_set_fault_object(region, object);
}

bool user_program_rebind_region_fault_object(
    user_program_t* program,
    user_program_region_id_t region_id,
    vm_object_t* object) {
    vm_user_region_t* region = user_program_region(program, region_id);
    vm_object_t* previous_object = NULL;
    size_t previous_offset = 0;
    vm_region_object_mode_t previous_mode = VM_REGION_OBJECT_NONE;

    if (region == NULL || object == NULL) {
        return false;
    }

    previous_object = region->object;
    previous_offset = region->object_offset;
    previous_mode = region->object_mode;
    if (!vm_user_region_clear_object(region)) {
        return false;
    }

    if (vm_user_region_set_fault_object(region, object)) {
        return true;
    }

    (void)restore_region_object_binding(region,
                                        previous_object,
                                        previous_offset,
                                        previous_mode);
    return false;
}

bool user_program_reset_object(user_program_t* program,
                               user_program_object_id_t object_id) {
    vm_object_t* object = user_program_object(program, object_id);

    return object != NULL && vm_object_reset(object);
}

vm_user_region_t* user_program_region(user_program_t* program,
                                      user_program_region_id_t region_id) {
    return (vm_user_region_t*)user_program_region_view(program, region_id);
}

vm_object_t* user_program_object(user_program_t* program,
                                 user_program_object_id_t object_id) {
    if (program == NULL) {
        return NULL;
    }

    switch (object_id) {
        case USER_PROGRAM_OBJECT_EXEC:
            return &program->bootstrap.exec_object;
        case USER_PROGRAM_OBJECT_STACK:
            return &program->bootstrap.stack_object;
        case USER_PROGRAM_OBJECT_ALIAS:
            return &program->bootstrap.alias_object;
        case USER_PROGRAM_OBJECT_ANON:
            return &program->bootstrap.anon_object;
    }

    return NULL;
}
