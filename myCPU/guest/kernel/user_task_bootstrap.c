#include "user_task_bootstrap.h"

#include "memory.h"

static uintptr_t align_down_page(uintptr_t value) {
    return value & ~((uintptr_t)MEMORY_PAGE_SIZE - 1U);
}

static void clear_bootstrap_layout(user_task_bootstrap_t* bootstrap) {
    if (bootstrap == NULL) {
        return;
    }

    bootstrap->planned = false;
    bootstrap->configured = false;
    bootstrap->bound = false;
    bootstrap->user_task = NULL;
    bootstrap->exec_page_paddr = 0;
    bootstrap->exec_vaddr = 0;
    bootstrap->stack_vaddr = 0;
    bootstrap->alias_vaddr = 0;
    bootstrap->anon_vaddr = 0;
    bootstrap->anon_tail_vaddr = 0;
    bootstrap->entry_pc = 0;
    bootstrap->expected_ecall_pc = 0;
    bootstrap->user_sp = 0;
}

static void clear_region(vm_user_region_t* region) {
    if (region == NULL) {
        return;
    }

    region->address_space = NULL;
    region->vaddr = 0;
    region->size = 0;
    region->flags = 0;
    region->registered = false;
    region->object = NULL;
    region->object_offset = 0;
    region->object_mode = VM_REGION_OBJECT_NONE;
}

static void clear_object(vm_object_t* object) {
    if (object == NULL) {
        return;
    }

    object->initialized = false;
    object->backing_kind = VM_OBJECT_BACKING_NONE;
    object->size = 0;
    object->attachment_count = 0;
    object->backing.anon.page_slots = NULL;
    object->backing.anon.page_count = 0;
}

static void clear_bootstrap(user_task_bootstrap_t* bootstrap) {
    if (bootstrap == NULL) {
        return;
    }

    clear_bootstrap_layout(bootstrap);
    clear_region(&bootstrap->exec_region);
    clear_region(&bootstrap->stack_region);
    clear_region(&bootstrap->alias_region);
    clear_region(&bootstrap->anon_region);
    clear_region(&bootstrap->anon_tail_region);
    clear_object(&bootstrap->exec_object);
    clear_object(&bootstrap->stack_object);
    clear_object(&bootstrap->alias_object);
    clear_object(&bootstrap->anon_object);
}

static bool reset_object_if_initialized(vm_object_t* object) {
    if (object == NULL || !object->initialized) {
        return true;
    }

    return vm_object_reset(object);
}

static bool user_task_ready(user_task_t* user_task) {
    return user_task != NULL && user_task_address_space(user_task) != NULL &&
           user_task_process(user_task) != NULL &&
           user_task_runtime(user_task) != NULL;
}

static bool reset_bootstrap_objects(user_task_bootstrap_t* bootstrap) {
    return bootstrap != NULL &&
           reset_object_if_initialized(&bootstrap->anon_object) &&
           reset_object_if_initialized(&bootstrap->alias_object) &&
           reset_object_if_initialized(&bootstrap->stack_object) &&
           reset_object_if_initialized(&bootstrap->exec_object);
}

static bool layout_symbols_share_page(uintptr_t exec_symbol,
                                      uintptr_t ecall_symbol,
                                      uintptr_t* exec_page_paddr) {
    const uintptr_t exec_page = align_down_page(exec_symbol);

    if (exec_page_paddr != NULL) {
        *exec_page_paddr = exec_page;
    }

    return align_down_page(ecall_symbol) == exec_page;
}

static bool layout_slots_are_user_accessible(uintptr_t anon_tail_vaddr,
                                             uintptr_t anon_vaddr,
                                             uintptr_t stack_vaddr,
                                             uintptr_t exec_vaddr) {
    return vm_range_is_user(anon_tail_vaddr, MEMORY_PAGE_SIZE) &&
           vm_range_is_user(anon_vaddr, MEMORY_PAGE_SIZE) &&
           vm_range_is_user(stack_vaddr, MEMORY_PAGE_SIZE) &&
           vm_range_is_user(exec_vaddr, MEMORY_PAGE_SIZE);
}

static void assign_planned_layout(user_task_bootstrap_t* bootstrap,
                                  uintptr_t exec_page_paddr,
                                  uintptr_t exec_vaddr,
                                  uintptr_t stack_vaddr,
                                  uintptr_t alias_vaddr,
                                  uintptr_t anon_vaddr,
                                  uintptr_t anon_tail_vaddr,
                                  uintptr_t exec_symbol,
                                  uintptr_t ecall_symbol) {
    bootstrap->planned = true;
    bootstrap->exec_page_paddr = exec_page_paddr;
    bootstrap->exec_vaddr = exec_vaddr;
    bootstrap->stack_vaddr = stack_vaddr;
    bootstrap->alias_vaddr = alias_vaddr;
    bootstrap->anon_vaddr = anon_vaddr;
    bootstrap->anon_tail_vaddr = anon_tail_vaddr;
    bootstrap->entry_pc = exec_vaddr + (exec_symbol - exec_page_paddr);
    bootstrap->expected_ecall_pc = exec_vaddr + (ecall_symbol - exec_page_paddr);
    bootstrap->user_sp = stack_vaddr + MEMORY_PAGE_SIZE;
}

static bool configure_bootstrap_objects(user_task_bootstrap_t* bootstrap,
                                        uintptr_t alias_backing_paddr,
                                        uintptr_t user_stack_paddr) {
    return bootstrap != NULL &&
           vm_object_init_physical(&bootstrap->exec_object,
                                   bootstrap->exec_page_paddr,
                                   MEMORY_PAGE_SIZE) &&
           vm_object_init_physical(&bootstrap->stack_object,
                                   user_stack_paddr,
                                   MEMORY_PAGE_SIZE) &&
           vm_object_init_physical(&bootstrap->alias_object,
                                   alias_backing_paddr,
                                   MEMORY_PAGE_SIZE) &&
           vm_object_init_anon(&bootstrap->anon_object, 2U * MEMORY_PAGE_SIZE);
}

static void set_region_binding(vm_process_user_region_binding_t* binding,
                               vm_user_region_t* region,
                               uintptr_t vaddr,
                               uint64_t flags,
                               vm_object_t* object,
                               size_t object_offset,
                               vm_region_object_mode_t object_mode) {
    if (binding == NULL) {
        return;
    }

    binding->region = region;
    binding->vaddr = vaddr;
    binding->size = MEMORY_PAGE_SIZE;
    binding->flags = flags;
    binding->object = object;
    binding->object_offset = object_offset;
    binding->object_mode = object_mode;
}

static void build_standard_bindings(user_task_bootstrap_t* bootstrap,
                                    vm_process_user_region_binding_t bindings[5]) {
    const uint64_t user_rw_flags =
        VM_PAGE_READ | VM_PAGE_WRITE | VM_PAGE_USER;
    const uint64_t user_rx_flags =
        VM_PAGE_READ | VM_PAGE_EXEC | VM_PAGE_USER;

    if (bootstrap == NULL || bindings == NULL) {
        return;
    }

    set_region_binding(&bindings[0],
                       &bootstrap->exec_region,
                       bootstrap->exec_vaddr,
                       user_rx_flags,
                       &bootstrap->exec_object,
                       0,
                       VM_REGION_OBJECT_MAPPED);
    set_region_binding(&bindings[1],
                       &bootstrap->stack_region,
                       bootstrap->stack_vaddr,
                       user_rw_flags,
                       &bootstrap->stack_object,
                       0,
                       VM_REGION_OBJECT_MAPPED);
    set_region_binding(&bindings[2],
                       &bootstrap->alias_region,
                       bootstrap->alias_vaddr,
                       user_rw_flags,
                       &bootstrap->alias_object,
                       0,
                       VM_REGION_OBJECT_MAPPED);
    set_region_binding(&bindings[3],
                       &bootstrap->anon_region,
                       bootstrap->anon_vaddr,
                       user_rw_flags,
                       &bootstrap->anon_object,
                       0,
                       VM_REGION_OBJECT_FAULT);
    set_region_binding(&bindings[4],
                       &bootstrap->anon_tail_region,
                       bootstrap->anon_tail_vaddr,
                       user_rw_flags,
                       &bootstrap->anon_object,
                       MEMORY_PAGE_SIZE,
                       VM_REGION_OBJECT_FAULT);
}

void user_task_bootstrap_init(user_task_bootstrap_t* bootstrap) {
    clear_bootstrap(bootstrap);
}

bool user_task_bootstrap_reset(user_task_bootstrap_t* bootstrap) {
    if (!reset_bootstrap_objects(bootstrap)) {
        return false;
    }

    clear_bootstrap(bootstrap);
    return true;
}

bool user_task_bootstrap_plan_layout(user_task_bootstrap_t* bootstrap,
                                     uintptr_t exec_symbol,
                                     uintptr_t ecall_symbol) {
    const uintptr_t user_limit = vm_user_limit();
    uintptr_t exec_page_paddr = 0;
    const uintptr_t alias_vaddr = user_limit - MEMORY_PAGE_SIZE;
    const uintptr_t anon_tail_vaddr = alias_vaddr - 5U * MEMORY_PAGE_SIZE;
    const uintptr_t anon_vaddr = alias_vaddr - 4U * MEMORY_PAGE_SIZE;
    const uintptr_t stack_vaddr = alias_vaddr - 3U * MEMORY_PAGE_SIZE;
    const uintptr_t exec_vaddr = alias_vaddr - 2U * MEMORY_PAGE_SIZE;

    if (bootstrap == NULL || bootstrap->planned ||
        !layout_symbols_share_page(exec_symbol, ecall_symbol, &exec_page_paddr) ||
        !layout_slots_are_user_accessible(anon_tail_vaddr,
                                          anon_vaddr,
                                          stack_vaddr,
                                          exec_vaddr)) {
        return false;
    }

    assign_planned_layout(bootstrap,
                          exec_page_paddr,
                          exec_vaddr,
                          stack_vaddr,
                          alias_vaddr,
                          anon_vaddr,
                          anon_tail_vaddr,
                          exec_symbol,
                          ecall_symbol);
    return true;
}

bool user_task_bootstrap_configure(user_task_bootstrap_t* bootstrap,
                                   user_task_t* user_task,
                                   uintptr_t alias_backing_paddr,
                                   uintptr_t user_stack_paddr) {
    if (bootstrap == NULL || !bootstrap->planned || bootstrap->configured ||
        bootstrap->bound || !user_task_ready(user_task)) {
        return false;
    }

    if (!configure_bootstrap_objects(bootstrap,
                                     alias_backing_paddr,
                                     user_stack_paddr)) {
        if (!reset_bootstrap_objects(bootstrap)) {
            return false;
        }
        clear_bootstrap(bootstrap);
        return false;
    }

    bootstrap->user_task = user_task;
    bootstrap->configured = true;
    return true;
}

bool user_task_bootstrap_bind(user_task_bootstrap_t* bootstrap) {
    vm_process_user_region_binding_t bindings[5];

    if (bootstrap == NULL || !bootstrap->configured || bootstrap->bound ||
        !user_task_ready(bootstrap->user_task)) {
        return false;
    }

    build_standard_bindings(bootstrap, bindings);

    if (!user_task_bind_regions(bootstrap->user_task, bindings, 5U)) {
        return false;
    }

    bootstrap->bound = true;
    return true;
}

bool user_task_bootstrap_prepare_standard(
    user_task_bootstrap_t* bootstrap,
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
    if (bootstrap == NULL || !bootstrap->bound ||
        !user_task_ready(bootstrap->user_task)) {
        return false;
    }

    return user_task_prepare_standard(bootstrap->user_task,
                                      trap_context,
                                      bootstrap->entry_pc,
                                      bootstrap->user_sp,
                                      arg0,
                                      trap_stack_base,
                                      trap_stack_size,
                                      bootstrap->expected_ecall_pc,
                                      validate,
                                      validate_context,
                                      supervisor_timer_post_handler,
                                      supervisor_timer_post_context,
                                      supervisor_external_post_handler,
                                      supervisor_external_post_context);
}
