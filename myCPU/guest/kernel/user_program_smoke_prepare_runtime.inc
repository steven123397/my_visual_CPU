bool user_program_smoke_prepare_standard(user_program_smoke_t* smoke,
                                         user_program_t* program,
                                         const user_program_smoke_prepare_t* prepare) {
    user_program_smoke_t staged_smoke;
    const user_program_smoke_prepare_stage_t stage = {
        .smoke = &staged_smoke,
        .program = program,
        .prepare = prepare,
    };

    if (smoke == NULL || !user_program_smoke_prepare_stage_valid(&stage)) {
        return false;
    }

    user_program_smoke_init(&staged_smoke);

    if (!user_program_smoke_create_staged_program(&stage) ||
        !user_program_smoke_prepare_stage_address_space(&stage) ||
        !user_program_smoke_prepare_stage_runtime(&stage) ||
        user_program_runtime(program) == NULL) {
        return user_program_destroy(program) && false;
    }

    *smoke = staged_smoke;
    return true;
}

static bool user_program_smoke_validate_created_program(user_program_t* program) {
    vm_address_space_t* user_address_space = user_program_address_space(program);

    return program != NULL && user_address_space != NULL &&
           user_program_process(program) != NULL &&
           user_program_runtime(program) != NULL &&
           user_program_region(program, USER_PROGRAM_REGION_STACK) != NULL &&
           user_program_region(program, USER_PROGRAM_REGION_ALIAS) != NULL &&
           user_program_region(program, USER_PROGRAM_REGION_ANON) != NULL &&
           user_program_region(program, USER_PROGRAM_REGION_ANON_TAIL) != NULL &&
           user_program_object(program, USER_PROGRAM_OBJECT_ANON) != NULL &&
           vm_address_space_root_table(user_address_space) != 0 &&
           (vm_address_space_root_table(user_address_space) &
            (MEMORY_PAGE_SIZE - 1U)) == 0 &&
           vm_address_space_satp_value(user_address_space) ==
               (RISCV_SATP_MODE_SV39 |
                ((uint64_t)vm_address_space_root_table(user_address_space) >>
                 12)) &&
           vm_kernel_base() == memory_kernel_start() &&
           vm_kernel_limit() == memory_heap_limit() &&
           !vm_address_space_is_active(user_address_space) &&
           !vm_range_is_user(memory_text_start(), MEMORY_PAGE_SIZE) &&
           !vm_range_is_kernel(
               user_program_value(program, USER_PROGRAM_VALUE_ALIAS_VADDR),
               MEMORY_PAGE_SIZE) &&
           vm_range_is_user(
               user_program_value(program, USER_PROGRAM_VALUE_ALIAS_VADDR),
               MEMORY_PAGE_SIZE) &&
           vm_range_is_kernel(memory_text_start(),
                              memory_text_end() - memory_text_start()) &&
           user_program_region_contains(
               program,
               USER_PROGRAM_REGION_STACK,
               user_program_value(program, USER_PROGRAM_VALUE_USER_SP) - 16U,
               16U) &&
           user_program_region_contains(
               program,
               USER_PROGRAM_REGION_ALIAS,
               user_program_value(program, USER_PROGRAM_VALUE_ALIAS_VADDR),
               MEMORY_PAGE_SIZE) &&
           !user_program_region_contains(
               program,
               USER_PROGRAM_REGION_ALIAS,
               user_program_value(program, USER_PROGRAM_VALUE_ALIAS_VADDR) -
                   MEMORY_PAGE_SIZE,
               MEMORY_PAGE_SIZE);
}

static bool user_program_smoke_prepare_address_space(
    user_program_smoke_t* smoke,
    user_program_t* program,
    uintptr_t backing_page_paddr,
    uintptr_t remap_page_paddr,
    uintptr_t fault_skip_vaddr,
    size_t fault_skip_size,
    uintptr_t fault_resume_vaddr,
    size_t fault_resume_size,
    volatile uintptr_t* fault_resume_pc_slot) {
    const uintptr_t alias_vaddr =
        program != NULL
            ? user_program_value(program, USER_PROGRAM_VALUE_ALIAS_VADDR)
            : 0;

    if (smoke == NULL || smoke->program != NULL || !address_space_ready(program) ||
        alias_vaddr < MEMORY_PAGE_SIZE || backing_page_paddr == 0 ||
        remap_page_paddr == 0 || fault_skip_vaddr == 0 || fault_skip_size == 0 ||
        fault_resume_vaddr == 0 ||
        fault_resume_size == 0 || fault_resume_pc_slot == NULL) {
        return false;
    }

    smoke->program = program;
    return smoke_ready(smoke) &&
           reject_invalid_kernel_mapping_paths(program, remap_page_paddr) &&
           map_standard_kernel_ranges(program) &&
           reject_invalid_fault_range_paths(program,
                                            backing_page_paddr,
                                            remap_page_paddr) &&
           prepare_fault_orchestration(smoke,
                                       remap_page_paddr,
                                       fault_skip_vaddr,
                                       fault_skip_size,
                                       fault_resume_vaddr,
                                       fault_resume_size,
                                       fault_resume_pc_slot);
}

static bool user_program_smoke_prepare_runtime(
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
    void* supervisor_external_post_context) {
    return smoke_ready(smoke) &&
           !user_program_is_active(smoke->program) &&
           user_program_smoke_unmap_remap_page(smoke) &&
           !user_program_is_runnable(smoke->program) &&
           user_program_prepare_standard(smoke->program,
                                         trap_context,
                                         arg0,
                                         trap_stack_base,
                                         trap_stack_size,
                                         validate,
                                         validate_context,
                                         supervisor_timer_post_handler,
                                         supervisor_timer_post_context,
                                         supervisor_external_post_handler,
                                         supervisor_external_post_context) &&
           user_program_is_runnable(smoke->program) &&
           !user_program_smoke_unmap_remap_page(smoke);
}

