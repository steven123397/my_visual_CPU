bool user_program_smoke_validate_vm_lifecycle(uintptr_t user_region_vaddr,
                                              size_t expected_free_pages) {
    const uint64_t user_rw_flags =
        VM_PAGE_READ | VM_PAGE_WRITE | VM_PAGE_USER;
    vm_address_space_t* lifecycle_space = NULL;
    vm_address_space_t* recycled_space = NULL;
    vm_process_t lifecycle_process = {0};
    vm_user_region_t lifecycle_region = {0};
    vm_object_t lifecycle_object = {0};

    return vm_range_is_user(user_region_vaddr, MEMORY_PAGE_SIZE) &&
           vm_address_space_create(&lifecycle_space) &&
           lifecycle_space != NULL &&
           vm_process_create(&lifecycle_process, lifecycle_space) &&
           vm_address_space_activate(lifecycle_space) &&
           vm_address_space_is_active(lifecycle_space) &&
           runtime_context_active_address_space() == lifecycle_space &&
           vm_address_space_disable(lifecycle_space) &&
           !vm_address_space_is_active(lifecycle_space) &&
           runtime_context_active_address_space() == NULL &&
           vm_address_space_map_identity_1g(lifecycle_space,
                                            0,
                                            VM_PAGE_READ | VM_PAGE_WRITE |
                                                VM_PAGE_EXEC) &&
           vm_process_user_region_init(&lifecycle_process,
                                       &lifecycle_region,
                                       user_region_vaddr,
                                       MEMORY_PAGE_SIZE,
                                       user_rw_flags) &&
           !vm_process_set_user_context(&lifecycle_process,
                                        user_region_vaddr,
                                        user_region_vaddr +
                                            MEMORY_PAGE_SIZE) &&
           vm_object_init_anon(&lifecycle_object, MEMORY_PAGE_SIZE) &&
           !vm_object_init_anon(&lifecycle_object, MEMORY_PAGE_SIZE) &&
           vm_user_region_map_object(&lifecycle_region, &lifecycle_object) &&
           !vm_user_region_map_object(&lifecycle_region, &lifecycle_object) &&
           !vm_user_region_set_fault_object(&lifecycle_region,
                                            &lifecycle_object) &&
           !vm_object_reset(&lifecycle_object) &&
           vm_user_region_clear_object(&lifecycle_region) &&
           vm_user_region_set_fault_object(&lifecycle_region,
                                           &lifecycle_object) &&
           !vm_user_region_map_object(&lifecycle_region, &lifecycle_object) &&
           !vm_object_reset(&lifecycle_object) &&
           vm_user_region_clear_object(&lifecycle_region) &&
           !vm_address_space_destroy(lifecycle_space) &&
           vm_process_remove_user_region(&lifecycle_process, &lifecycle_region) &&
           !lifecycle_region.registered &&
           lifecycle_region.address_space == NULL &&
           lifecycle_region.object == NULL &&
           !vm_process_create(&lifecycle_process, lifecycle_space) &&
           vm_process_reset(&lifecycle_process) &&
           vm_object_reset(&lifecycle_object) &&
           lifecycle_process.address_space == NULL &&
           lifecycle_process.entry_pc == 0 &&
           lifecycle_process.user_sp == 0 &&
           lifecycle_process.user_regions[0] == NULL &&
           !lifecycle_object.initialized &&
           lifecycle_object.backing_kind == VM_OBJECT_BACKING_NONE &&
           lifecycle_object.attachment_count == 0 &&
           vm_object_init_anon(&lifecycle_object, MEMORY_PAGE_SIZE) &&
           vm_address_space_destroy(lifecycle_space) &&
           vm_address_space_create(&lifecycle_space) &&
           vm_address_space_create(&recycled_space) &&
           vm_address_space_destroy(lifecycle_space) &&
           vm_address_space_destroy(recycled_space) &&
           vm_object_reset(&lifecycle_object) &&
           pmm_free_pages() == expected_free_pages;
}

bool user_program_smoke_validate_lifecycle(trap_context_t* trap_context,
                                           uintptr_t exec_symbol,
                                           uintptr_t ecall_symbol,
                                           uintptr_t alias_backing_paddr,
                                           uintptr_t user_stack_paddr,
                                           uintptr_t arg0,
                                           void* trap_stack_base,
                                           size_t trap_stack_size) {
    return trap_context != NULL &&
           user_program_smoke_validate_runtime_reprepare(
               exec_symbol,
               ecall_symbol,
               alias_backing_paddr,
               user_stack_paddr,
               arg0,
               trap_stack_base,
               trap_stack_size) &&
           user_program_smoke_validate_active_destroy_cleanup(
               trap_context,
               exec_symbol,
               ecall_symbol,
               alias_backing_paddr,
               user_stack_paddr,
               arg0,
               trap_stack_base,
               trap_stack_size) &&
           user_program_smoke_validate_destroy_recreate(trap_context,
                                                        exec_symbol,
                                                        ecall_symbol,
                                                        alias_backing_paddr,
                                                        user_stack_paddr,
                                                        arg0,
                                                        trap_stack_base,
                                                        trap_stack_size);
}

static bool user_program_smoke_validate_runtime_reprepare(
    uintptr_t exec_symbol,
    uintptr_t ecall_symbol,
    uintptr_t alias_backing_paddr,
    uintptr_t user_stack_paddr,
    uintptr_t arg0,
    void* trap_stack_base,
    size_t trap_stack_size) {
    user_program_t program;
    trap_user_runtime_t* runtime = NULL;
    trap_context_t first_context;
    trap_context_t second_context;
    uint32_t* signal_page = (uint32_t*)trap_stack_base;
    const size_t free_before = pmm_free_pages();
    bool ok = false;
    bool created = false;

    if (trap_stack_base == NULL ||
        trap_stack_size < TRAP_USER_RUNTIME_MIN_STACK_SIZE ||
        trap_stack_size < (2U * sizeof(uint32_t))) {
        return false;
    }

    user_program_init(&program);
    trap_context_init(&first_context);
    trap_context_init(&second_context);

    if (!user_program_plan_standard(&program, exec_symbol, ecall_symbol) ||
        !user_program_create(&program, alias_backing_paddr, user_stack_paddr)) {
        return false;
    }
    created = true;
    runtime = user_program_runtime(&program);
    if (runtime == NULL ||
        !user_program_prepare_standard(&program,
                                       &first_context,
                                       arg0,
                                       trap_stack_base,
                                       trap_stack_size,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL) ||
        !user_program_is_runnable(&program) ||
        runtime->trap_context != &first_context ||
        runtime->arg0 != arg0 ||
        !runtime_policies_bound(&first_context, runtime) ||
        !runtime_policies_cleared(&second_context) ||
        runtime->timer_signal.armed || runtime->timer_signal.delivered ||
        runtime->external_signal.armed || runtime->external_signal.delivered ||
        !trap_user_runtime_arm_timer_signal(runtime, signal_page, 0U, 1U) ||
        !trap_user_runtime_arm_external_signal(runtime, signal_page, 1U, 2U)) {
        goto cleanup;
    }

    runtime->timer_signal.delivered = true;
    runtime->external_signal.delivered = true;
    if (!runtime->timer_signal.armed || !runtime->timer_signal.delivered ||
        !runtime->external_signal.armed ||
        !runtime->external_signal.delivered ||
        !user_program_prepare_standard(&program,
                                       &second_context,
                                       arg0 + 1U,
                                       trap_stack_base,
                                       trap_stack_size,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL) ||
        runtime->trap_context != &second_context ||
        runtime->arg0 != arg0 + 1U ||
        !runtime_policies_cleared(&first_context) ||
        !runtime_policies_bound(&second_context, runtime) ||
        runtime->timer_signal.armed || runtime->timer_signal.delivered ||
        runtime->external_signal.armed || runtime->external_signal.delivered) {
        goto cleanup;
    }

    ok = true;

cleanup:
    if (!created) {
        return ok;
    }

    return user_program_destroy(&program) && runtime_policies_cleared(&first_context) &&
           runtime_policies_cleared(&second_context) &&
           pmm_free_pages() == free_before && ok;
}

static bool user_program_smoke_validate_active_destroy_cleanup(
    trap_context_t* trap_context,
    uintptr_t exec_symbol,
    uintptr_t ecall_symbol,
    uintptr_t alias_backing_paddr,
    uintptr_t user_stack_paddr,
    uintptr_t arg0,
    void* trap_stack_base,
    size_t trap_stack_size) {
    user_program_t program;
    user_program_smoke_t smoke;
    trap_user_runtime_t* runtime = NULL;
    const size_t free_before = pmm_free_pages();

    if (trap_context == NULL || !trap_context_is_active(trap_context) ||
        trap_active_context() != trap_context || trap_stack_base == NULL ||
        trap_stack_size < TRAP_USER_RUNTIME_MIN_STACK_SIZE) {
        return false;
    }

    user_program_init(&program);
    user_program_smoke_init(&smoke);
    if (!user_program_plan_standard(&program, exec_symbol, ecall_symbol) ||
        !user_program_create(&program, alias_backing_paddr, user_stack_paddr) ||
        !map_standard_kernel_ranges(&program) ||
        !user_program_prepare_standard(&program,
                                       trap_context,
                                       arg0,
                                       trap_stack_base,
                                       trap_stack_size,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL)) {
        return false;
    }

    runtime = user_program_runtime(&program);
    smoke.program = &program;
    if (runtime == NULL || !user_program_is_runnable(&program) ||
        !runtime_policies_bound(trap_context, runtime)) {
        return false;
    }

    if (!user_program_smoke_activate_supervisor_access(&smoke, trap_context)) {
        return false;
    }

    if (!user_program_destroy(&program)) {
        return false;
    }

    return runtime_context_active_process() == NULL &&
           runtime_context_active_address_space() == NULL &&
           trap_active_context() == trap_context &&
           trap_active_user_runtime() == NULL &&
           runtime_policies_cleared(trap_context) &&
           riscv_read_satp() == 0 &&
           (riscv_read_sstatus() & RISCV_SSTATUS_SUM) == 0 &&
           pmm_free_pages() == free_before;
}

static bool user_program_smoke_validate_destroy_recreate(
    trap_context_t* trap_context,
    uintptr_t exec_symbol,
    uintptr_t ecall_symbol,
    uintptr_t alias_backing_paddr,
    uintptr_t user_stack_paddr,
    uintptr_t arg0,
    void* trap_stack_base,
    size_t trap_stack_size) {
    user_program_t program;
    trap_user_runtime_t* runtime = NULL;
    const size_t free_before = pmm_free_pages();
    bool ok = false;

    if (trap_context == NULL || !trap_context_is_active(trap_context) ||
        trap_active_context() != trap_context || trap_stack_base == NULL ||
        trap_stack_size < TRAP_USER_RUNTIME_MIN_STACK_SIZE) {
        return false;
    }

    user_program_init(&program);
    if (!user_program_plan_standard(&program, exec_symbol, ecall_symbol) ||
        !user_program_create(&program, alias_backing_paddr, user_stack_paddr) ||
        !user_program_prepare_standard(&program,
                                       trap_context,
                                       arg0,
                                       trap_stack_base,
                                       trap_stack_size,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL)) {
        goto cleanup;
    }

    runtime = user_program_runtime(&program);
    if (runtime == NULL || !user_program_is_runnable(&program) ||
        runtime->trap_context != trap_context || runtime->arg0 != arg0 ||
        !runtime_policies_bound(trap_context, runtime) ||
        !user_program_destroy(&program) ||
        !runtime_policies_cleared(trap_context) ||
        pmm_free_pages() != free_before ||
        !user_program_plan_standard(&program, exec_symbol, ecall_symbol) ||
        !user_program_create(&program, alias_backing_paddr, user_stack_paddr) ||
        !user_program_prepare_standard(&program,
                                       trap_context,
                                       arg0 + 1U,
                                       trap_stack_base,
                                       trap_stack_size,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL,
                                       NULL)) {
        goto cleanup;
    }

    runtime = user_program_runtime(&program);
    if (runtime == NULL || !user_program_is_runnable(&program) ||
        runtime->trap_context != trap_context || runtime->arg0 != arg0 + 1U ||
        !runtime_policies_bound(trap_context, runtime) ||
        runtime->timer_signal.armed || runtime->timer_signal.delivered ||
        runtime->external_signal.armed || runtime->external_signal.delivered) {
        goto cleanup;
    }

    ok = true;

cleanup:
    return user_program_destroy(&program) && runtime_policies_cleared(trap_context) &&
           pmm_free_pages() == free_before && ok;
}

