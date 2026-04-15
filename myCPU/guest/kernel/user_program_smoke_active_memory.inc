static bool active_memory_args_valid(
    const user_program_smoke_t* smoke,
    uint32_t* backing_page,
    uint32_t* remap_page,
    const user_program_smoke_active_phase_t* phase,
    uintptr_t alias_vaddr,
    uintptr_t anon_vaddr,
    uintptr_t anon_tail_vaddr);
static bool user_program_smoke_active_memory_stage_init(
    user_program_smoke_active_memory_stage_t* stage,
    user_program_smoke_t* smoke,
    uint32_t* backing_page,
    uint32_t* remap_page,
    const user_program_smoke_active_phase_t* phase);
static bool user_program_smoke_active_memory_exercise_alias_backing(
    user_program_smoke_active_memory_stage_t* stage);
static bool user_program_smoke_active_memory_exercise_anon_regions(
    user_program_smoke_active_memory_stage_t* stage);
static bool user_program_smoke_active_memory_exercise_remap_rebind(
    user_program_smoke_active_memory_stage_t* stage);
static bool user_program_smoke_active_memory_exercise_faults(
    const user_program_smoke_active_memory_stage_t* stage);

static bool reject_invalid_region_paths(user_program_smoke_t* smoke);
static bool user_program_smoke_validate_runtime_reprepare(
    uintptr_t exec_symbol,
    uintptr_t ecall_symbol,
    uintptr_t alias_backing_paddr,
    uintptr_t user_stack_paddr,
    uintptr_t arg0,
    void* trap_stack_base,
    size_t trap_stack_size);
static bool user_program_smoke_validate_active_destroy_cleanup(
    trap_context_t* trap_context,
    uintptr_t exec_symbol,
    uintptr_t ecall_symbol,
    uintptr_t alias_backing_paddr,
    uintptr_t user_stack_paddr,
    uintptr_t arg0,
    void* trap_stack_base,
    size_t trap_stack_size);
static bool user_program_smoke_validate_destroy_recreate(
    trap_context_t* trap_context,
    uintptr_t exec_symbol,
    uintptr_t ecall_symbol,
    uintptr_t alias_backing_paddr,
    uintptr_t user_stack_paddr,
    uintptr_t arg0,
    void* trap_stack_base,
    size_t trap_stack_size);
static bool user_program_smoke_validate_created_program(user_program_t* program);
static bool user_program_smoke_prepare_address_space(
    user_program_smoke_t* smoke,
    user_program_t* program,
    uintptr_t backing_page_paddr,
    uintptr_t remap_page_paddr,
    uintptr_t fault_skip_vaddr,
    size_t fault_skip_size,
    uintptr_t fault_resume_vaddr,
    size_t fault_resume_size,
    volatile uintptr_t* fault_resume_pc_slot);
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
    void* supervisor_external_post_context);
static bool user_program_smoke_enter_with_interrupt_signals(
    user_program_smoke_t* smoke,
    uint32_t* timer_signal_page,
    size_t timer_signal_index,
    uint32_t timer_signal_value,
    uint32_t* external_signal_page,
    size_t external_signal_index,
    uint32_t external_signal_value,
    uint64_t timer_delta);
static bool user_program_smoke_unmap_remap_page(user_program_smoke_t* smoke);
static bool user_program_smoke_rebind_alias_fault_object(
    user_program_smoke_t* smoke);
static void user_program_smoke_best_effort_deactivate(user_program_smoke_t* smoke);

bool user_program_smoke_is_reset(const user_program_smoke_t* smoke) {
    return smoke != NULL && smoke->program == NULL &&
           region_cleared(&smoke->remap_region) &&
           region_cleared(&smoke->invalid_region) &&
           object_cleared(&smoke->remap_object);
}

user_program_t* user_program_smoke_program(user_program_smoke_t* smoke) {
    return smoke != NULL ? smoke->program : NULL;
}

trap_user_runtime_t* user_program_smoke_runtime(user_program_smoke_t* smoke) {
    user_program_t* program = user_program_smoke_program(smoke);

    return program != NULL ? user_program_runtime(program) : NULL;
}

__attribute__((noinline)) static void provoke_rodata_store_fault(
    volatile const uint32_t* marker) {
    volatile uint32_t* marker_ptr = (volatile uint32_t*)(uintptr_t)marker;

    *marker_ptr = 0xDEADBEEFU;
}

__attribute__((noinline)) static void provoke_instruction_page_fault(
    uintptr_t target,
    volatile uintptr_t* fault_resume_pc_slot) {
#ifdef UNIT_TEST_HOST
    (void)target;
    if (fault_resume_pc_slot != NULL) {
        *fault_resume_pc_slot = 0;
    }
#else
    __asm__ volatile(
        "la t0, 1f\n"
        "sd t0, 0(%1)\n"
        "jalr ra, 0(%0)\n"
        "1:\n"
        "sd zero, 0(%1)\n"
        :
        : "r"(target), "r"(fault_resume_pc_slot)
        : "t0", "ra", "memory");
#endif
}

static bool reject_invalid_kernel_mapping_paths(user_program_t* program,
                                                uintptr_t remap_page_paddr) {
    vm_address_space_t* address_space = NULL;
    const uintptr_t alias_vaddr =
        user_program_value(program, USER_PROGRAM_VALUE_ALIAS_VADDR);

    address_space = user_program_address_space(program);
    return address_space != NULL &&
           !vm_address_space_map_kernel_range(address_space,
                                             alias_vaddr,
                                             remap_page_paddr,
                                             MEMORY_PAGE_SIZE,
                                             VM_PAGE_WRITE) &&
           !vm_address_space_map_kernel_range(
               address_space,
               memory_text_start(),
               memory_text_start(),
               memory_text_end() - memory_text_start(),
               VM_PAGE_READ | VM_PAGE_EXEC | VM_PAGE_USER);
}

static bool map_standard_kernel_ranges(user_program_t* program) {
    vm_address_space_t* address_space = user_program_address_space(program);

    return address_space != NULL &&
           vm_address_space_map_identity_1g(address_space,
                                           0,
                                           VM_PAGE_READ | VM_PAGE_WRITE |
                                               VM_PAGE_EXEC) &&
           vm_address_space_map_kernel_range(address_space,
                                            memory_text_start(),
                                            memory_text_start(),
                                            memory_text_end() - memory_text_start(),
                                            VM_PAGE_READ | VM_PAGE_EXEC) &&
           vm_address_space_map_kernel_range(address_space,
                                            memory_rodata_start(),
                                            memory_rodata_start(),
                                            memory_rodata_end() -
                                                memory_rodata_start(),
                                            VM_PAGE_READ) &&
           vm_address_space_map_kernel_range(address_space,
                                            memory_data_start(),
                                            memory_data_start(),
                                            pmm_managed_start() -
                                                memory_data_start(),
                                            VM_PAGE_READ | VM_PAGE_WRITE) &&
           vm_address_space_map_kernel_range(address_space,
                                            pmm_managed_start(),
                                            pmm_managed_start(),
                                            pmm_managed_end() -
                                                pmm_managed_start(),
                                            VM_PAGE_READ | VM_PAGE_WRITE);
}

static bool reject_invalid_fault_range_paths(user_program_t* program,
                                             uintptr_t backing_page_paddr,
                                             uintptr_t remap_page_paddr) {
    vm_address_space_t* address_space = NULL;
    const uintptr_t alias_vaddr =
        user_program_value(program, USER_PROGRAM_VALUE_ALIAS_VADDR);

    address_space = user_program_address_space(program);
    return address_space != NULL &&
           !vm_address_space_register_fault_range(address_space,
                                                 alias_vaddr + 1U,
                                                 backing_page_paddr,
                                                 MEMORY_PAGE_SIZE,
                                                 VM_PAGE_READ) &&
           !user_program_unmap_region_page(program,
                                           USER_PROGRAM_REGION_ALIAS,
                                           memory_text_start()) &&
           !vm_address_space_register_fault_range(address_space,
                                                 alias_vaddr,
                                                 remap_page_paddr,
                                                 MEMORY_PAGE_SIZE,
                                                 VM_PAGE_READ |
                                                     VM_PAGE_WRITE);
}

static bool prepare_fault_orchestration(user_program_smoke_t* smoke,
                                        uintptr_t remap_page_paddr,
                                        uintptr_t fault_skip_vaddr,
                                        size_t fault_skip_size,
                                        uintptr_t fault_resume_vaddr,
                                        size_t fault_resume_size,
                                        volatile uintptr_t* fault_resume_pc_slot) {
    const uint64_t user_rw_flags =
        VM_PAGE_READ | VM_PAGE_WRITE | VM_PAGE_USER;
    const uintptr_t alias_vaddr = user_program_value(
        smoke->program, USER_PROGRAM_VALUE_ALIAS_VADDR);
    const uintptr_t remap_vaddr =
        alias_vaddr >= MEMORY_PAGE_SIZE ? alias_vaddr - MEMORY_PAGE_SIZE : 0;
    vm_address_space_t* address_space = user_program_address_space(smoke->program);

    return address_space != NULL &&
           vm_object_init_physical(&smoke->remap_object,
                                   remap_page_paddr,
                                   MEMORY_PAGE_SIZE) &&
           reject_invalid_region_paths(smoke) &&
           vm_address_space_register_fault_skip(address_space,
                                               RISCV_EXC_STORE_PAGE_FAULT,
                                               fault_skip_vaddr,
                                               fault_skip_size) &&
           vm_address_space_register_fault_resume_slot(address_space,
                                                       RISCV_EXC_INSN_PAGE_FAULT,
                                                       fault_resume_vaddr,
                                                       fault_resume_size,
                                                       fault_resume_pc_slot) &&
           user_program_map_object_region(smoke->program,
                                          &smoke->remap_region,
                                          remap_vaddr,
                                          MEMORY_PAGE_SIZE,
                                          user_rw_flags,
                                          &smoke->remap_object);
}

static bool reject_invalid_region_paths(user_program_smoke_t* smoke) {
    vm_process_t* process = NULL;
    vm_object_t* anon_object = NULL;
    const uint64_t user_rw_flags =
        VM_PAGE_READ | VM_PAGE_WRITE | VM_PAGE_USER;
    const uintptr_t alias_vaddr = user_program_value(
        smoke->program, USER_PROGRAM_VALUE_ALIAS_VADDR);
    const uintptr_t anon_tail_vaddr = user_program_value(
        smoke->program, USER_PROGRAM_VALUE_ANON_TAIL_VADDR);

    process = user_program_process(smoke->program);
    anon_object = user_program_object(smoke->program, USER_PROGRAM_OBJECT_ANON);
    if (process == NULL || anon_object == NULL || alias_vaddr < MEMORY_PAGE_SIZE ||
        anon_tail_vaddr < MEMORY_PAGE_SIZE) {
        return false;
    }

    if (user_program_set_region_fault_object(smoke->program,
                                            USER_PROGRAM_REGION_ALIAS,
                                            &smoke->remap_object) ||
        user_program_set_fault_object_region_at(smoke->program,
                                                &smoke->invalid_region,
                                                anon_tail_vaddr - MEMORY_PAGE_SIZE,
                                                MEMORY_PAGE_SIZE,
                                                user_rw_flags,
                                                anon_object,
                                                2U * MEMORY_PAGE_SIZE) ||
        !invalid_region_state_ok(&smoke->invalid_region) ||
        vm_process_user_region_init(process,
                                    &smoke->invalid_region,
                                    0,
                                    MEMORY_PAGE_SIZE,
                                    user_rw_flags) ||
        !invalid_region_state_ok(&smoke->invalid_region) ||
        vm_process_user_region_init(process,
                                    &smoke->invalid_region,
                                    memory_text_start(),
                                    memory_text_end() - memory_text_start(),
                                    VM_PAGE_READ | VM_PAGE_EXEC | VM_PAGE_USER) ||
        !invalid_region_state_ok(&smoke->invalid_region) ||
        vm_process_user_region_init(process,
                                    &smoke->invalid_region,
                                    alias_vaddr,
                                    MEMORY_PAGE_SIZE,
                                    user_rw_flags) ||
        !invalid_region_state_ok(&smoke->invalid_region) ||
        vm_user_region_set_fault_object(&smoke->invalid_region,
                                        &smoke->remap_object) ||
        !invalid_region_state_ok(&smoke->invalid_region)) {
        return false;
    }

    return true;
}

void user_program_smoke_init(user_program_smoke_t* smoke) {
    if (smoke == NULL) {
        return;
    }

    smoke->program = NULL;
    clear_region(&smoke->remap_region);
    clear_region(&smoke->invalid_region);
    clear_object(&smoke->remap_object);
}

bool user_program_smoke_plan_standard(user_program_t* program,
                                      uintptr_t exec_symbol,
                                      uintptr_t ecall_symbol) {
    return program != NULL &&
           user_program_plan_standard(program, exec_symbol, ecall_symbol);
}

bool user_program_smoke_validate_standard_plan(const user_program_t* program,
                                               uintptr_t user_base,
                                               uintptr_t user_limit) {
    user_program_standard_plan_values_t values = {0};

    return load_standard_plan_values(program, &values) && user_base == 0 &&
           user_limit == memory_kernel_start() &&
           standard_plan_layout_within_user_range(&values, user_base, user_limit) &&
           standard_plan_pages_aligned(&values) &&
           values.user_sp == values.stack_vaddr + MEMORY_PAGE_SIZE &&
           standard_plan_exec_range_valid(&values);
}

static bool load_standard_plan_values(
    const user_program_t* program,
    user_program_standard_plan_values_t* values) {
    if (program == NULL || values == NULL) {
        return false;
    }

    values->exec_page_paddr =
        user_program_value(program, USER_PROGRAM_VALUE_EXEC_PAGE_PADDR);
    values->exec_vaddr = user_program_value(program, USER_PROGRAM_VALUE_EXEC_VADDR);
    values->stack_vaddr =
        user_program_value(program, USER_PROGRAM_VALUE_STACK_VADDR);
    values->alias_vaddr =
        user_program_value(program, USER_PROGRAM_VALUE_ALIAS_VADDR);
    values->anon_tail_vaddr =
        user_program_value(program, USER_PROGRAM_VALUE_ANON_TAIL_VADDR);
    values->entry_pc = user_program_value(program, USER_PROGRAM_VALUE_ENTRY_PC);
    values->expected_ecall_pc =
        user_program_value(program, USER_PROGRAM_VALUE_EXPECTED_ECALL_PC);
    values->user_sp = user_program_value(program, USER_PROGRAM_VALUE_USER_SP);
    return true;
}

static bool standard_plan_pages_aligned(
    const user_program_standard_plan_values_t* values) {
    return values != NULL &&
           (values->alias_vaddr & (MEMORY_PAGE_SIZE - 1U)) == 0 &&
           (values->anon_tail_vaddr & (MEMORY_PAGE_SIZE - 1U)) == 0 &&
           (values->stack_vaddr & (MEMORY_PAGE_SIZE - 1U)) == 0 &&
           (values->exec_vaddr & (MEMORY_PAGE_SIZE - 1U)) == 0;
}

static bool standard_plan_layout_within_user_range(
    const user_program_standard_plan_values_t* values,
    uintptr_t user_base,
    uintptr_t user_limit) {
    return values != NULL && values->anon_tail_vaddr < user_limit &&
           values->stack_vaddr >= user_base &&
           values->stack_vaddr < user_limit &&
           values->exec_vaddr >= user_base && values->exec_vaddr < user_limit &&
           values->alias_vaddr >= user_base &&
           values->alias_vaddr < user_limit;
}

static bool standard_plan_exec_range_valid(
    const user_program_standard_plan_values_t* values) {
    return values != NULL &&
           values->exec_page_paddr >= memory_text_start() &&
           values->exec_page_paddr < memory_text_end() &&
           values->entry_pc >= values->exec_vaddr &&
           values->entry_pc < values->exec_vaddr + MEMORY_PAGE_SIZE &&
           values->expected_ecall_pc >= values->exec_vaddr &&
           values->expected_ecall_pc < values->exec_vaddr + MEMORY_PAGE_SIZE;
}

static bool prepare_args_valid(const user_program_smoke_prepare_t* prepare) {
    return prepare != NULL && prepare->trap_context != NULL &&
           prepare->backing_page_paddr != 0 &&
           prepare->user_stack_paddr != 0 &&
           prepare->remap_page_paddr != 0 && prepare->fault_skip_vaddr != 0 &&
           prepare->fault_skip_size != 0 &&
           prepare->fault_resume_vaddr != 0 &&
           prepare->fault_resume_size != 0 &&
           prepare->fault_resume_pc_slot != NULL &&
           prepare->trap_stack_base != NULL &&
           prepare->trap_stack_size != 0;
}

static bool round_args_valid(const user_program_smoke_round_t* round) {
    return round != NULL && round->timer_signal_page != NULL &&
           round->timer_delta != 0;
}

static bool user_program_smoke_prepare_stage_valid(
    const user_program_smoke_prepare_stage_t* stage) {
    return stage != NULL && stage->smoke != NULL && stage->program != NULL &&
           prepare_args_valid(stage->prepare);
}

static bool user_program_smoke_create_staged_program(
    const user_program_smoke_prepare_stage_t* stage) {
    return user_program_smoke_prepare_stage_valid(stage) &&
           user_program_create(stage->program,
                               stage->prepare->backing_page_paddr,
                               stage->prepare->user_stack_paddr) &&
           user_program_smoke_validate_created_program(stage->program);
}

static bool user_program_smoke_prepare_stage_address_space(
    const user_program_smoke_prepare_stage_t* stage) {
    return user_program_smoke_prepare_stage_valid(stage) &&
           user_program_smoke_prepare_address_space(
               stage->smoke,
               stage->program,
               stage->prepare->backing_page_paddr,
               stage->prepare->remap_page_paddr,
               stage->prepare->fault_skip_vaddr,
               stage->prepare->fault_skip_size,
               stage->prepare->fault_resume_vaddr,
               stage->prepare->fault_resume_size,
               stage->prepare->fault_resume_pc_slot);
}

static bool user_program_smoke_prepare_stage_runtime(
    const user_program_smoke_prepare_stage_t* stage) {
    return user_program_smoke_prepare_stage_valid(stage) &&
           user_program_smoke_prepare_runtime(
               stage->smoke,
               stage->prepare->trap_context,
               stage->prepare->arg0,
               stage->prepare->trap_stack_base,
               stage->prepare->trap_stack_size,
               stage->prepare->validate,
               stage->prepare->validate_context,
               stage->prepare->supervisor_timer_post_handler,
               stage->prepare->supervisor_timer_post_context,
               stage->prepare->supervisor_external_post_handler,
               stage->prepare->supervisor_external_post_context);
}

static bool user_program_smoke_round_stage_valid(
    const user_program_smoke_round_stage_t* stage) {
    return stage != NULL && smoke_ready(stage->smoke) &&
           round_args_valid(stage->round);
}

static bool user_program_smoke_round_activate_if_needed(
    const user_program_smoke_round_stage_t* stage) {
    return user_program_smoke_round_stage_valid(stage) &&
           (user_program_is_active(stage->smoke->program) ||
            user_program_smoke_activate_supervisor_access(
                stage->smoke,
                stage->round->expected_trap_context));
}

static bool active_memory_args_valid(
    const user_program_smoke_t* smoke,
    uint32_t* backing_page,
    uint32_t* remap_page,
    const user_program_smoke_active_phase_t* phase,
    uintptr_t alias_vaddr,
    uintptr_t anon_vaddr,
    uintptr_t anon_tail_vaddr) {
    return smoke_ready(smoke) && user_program_is_active(smoke->program) &&
           backing_page != NULL && remap_page != NULL && phase != NULL &&
           phase->rodata_marker != NULL &&
           phase->instruction_fault_target != 0 &&
           phase->fault_resume_pc_slot != NULL &&
           (riscv_read_sstatus() & RISCV_SSTATUS_SUM) != 0 &&
           vm_range_is_user(alias_vaddr, MEMORY_PAGE_SIZE) &&
           vm_range_is_user(anon_vaddr, MEMORY_PAGE_SIZE) &&
           vm_range_is_user(anon_tail_vaddr, MEMORY_PAGE_SIZE);
}

static bool user_program_smoke_active_memory_stage_init(
    user_program_smoke_active_memory_stage_t* stage,
    user_program_smoke_t* smoke,
    uint32_t* backing_page,
    uint32_t* remap_page,
    const user_program_smoke_active_phase_t* phase) {
    if (stage == NULL) {
        return false;
    }

    *stage = (user_program_smoke_active_memory_stage_t){0};
    stage->smoke = smoke;
    stage->backing_page = backing_page;
    stage->remap_page = remap_page;
    stage->phase = phase;
    stage->alias_vaddr =
        smoke_ready(smoke)
            ? user_program_value(smoke->program, USER_PROGRAM_VALUE_ALIAS_VADDR)
            : 0;
    stage->anon_vaddr =
        smoke_ready(smoke)
            ? user_program_value(smoke->program, USER_PROGRAM_VALUE_ANON_VADDR)
            : 0;
    stage->anon_tail_vaddr =
        smoke_ready(smoke)
            ? user_program_value(smoke->program,
                                 USER_PROGRAM_VALUE_ANON_TAIL_VADDR)
            : 0;
    if (!active_memory_args_valid(smoke,
                                  backing_page,
                                  remap_page,
                                  phase,
                                  stage->alias_vaddr,
                                  stage->anon_vaddr,
                                  stage->anon_tail_vaddr)) {
        return false;
    }

    stage->alias_page = (uint32_t*)stage->alias_vaddr;
    stage->anon_page = (uint32_t*)stage->anon_vaddr;
    stage->anon_tail_page = (uint32_t*)stage->anon_tail_vaddr;
    stage->backing_word0 = backing_page[0];
    return true;
}

static bool user_program_smoke_active_memory_exercise_alias_backing(
    user_program_smoke_active_memory_stage_t* stage) {
    if (stage == NULL || stage->alias_page == NULL || stage->backing_page == NULL ||
        stage->phase == NULL) {
        return false;
    }

    if (stage->alias_page[0] != stage->backing_word0) {
        return false;
    }

    stage->alias_page[1] = stage->phase->alias_store_value;
    if (stage->backing_page[1] != stage->phase->alias_store_value) {
        return false;
    }

    stage->backing_page[2] = stage->phase->backing_store_value;
    return stage->backing_page[2] == stage->phase->backing_store_value &&
           *stage->phase->rodata_marker == stage->phase->rodata_expected &&
           !user_program_reset_object(stage->smoke->program,
                                      USER_PROGRAM_OBJECT_ANON);
}

static bool user_program_smoke_active_memory_exercise_anon_regions(
    user_program_smoke_active_memory_stage_t* stage) {
    if (stage == NULL || stage->anon_page == NULL || stage->anon_tail_page == NULL ||
        stage->phase == NULL) {
        return false;
    }

    if (stage->anon_page[0] != 0U || stage->anon_page[1] != 0U ||
        stage->anon_tail_page[0] != 0U ||
        stage->anon_tail_page[1] != 0U) {
        return false;
    }

    stage->anon_page[0] = stage->phase->anon_value0;
    stage->anon_page[1] = stage->phase->anon_value1;
    stage->anon_tail_page[0] = stage->phase->anon_tail_value0;
    stage->anon_tail_page[1] = stage->phase->anon_tail_value1;
    return user_program_unmap_region_base_page(stage->smoke->program,
                                               USER_PROGRAM_REGION_ANON) &&
           user_program_unmap_region_base_page(stage->smoke->program,
                                               USER_PROGRAM_REGION_ANON_TAIL) &&
           stage->anon_page[0] == stage->phase->anon_value0 &&
           stage->anon_page[1] == stage->phase->anon_value1 &&
           stage->anon_tail_page[0] == stage->phase->anon_tail_value0 &&
           stage->anon_tail_page[1] == stage->phase->anon_tail_value1;
}

static bool user_program_smoke_active_memory_exercise_remap_rebind(
    user_program_smoke_active_memory_stage_t* stage) {
    if (stage == NULL || stage->alias_page == NULL || stage->backing_page == NULL ||
        stage->remap_page == NULL || stage->phase == NULL) {
        return false;
    }

    if (!user_program_smoke_rebind_alias_fault_object(stage->smoke) ||
        stage->alias_page[0] != stage->remap_page[0] ||
        stage->backing_page[0] != stage->backing_word0) {
        return false;
    }

    stage->alias_page[1] = stage->phase->remap_store_value;
    return stage->remap_page[1] == stage->phase->remap_store_value &&
           stage->backing_page[1] == stage->phase->alias_store_value;
}

static bool user_program_smoke_active_memory_exercise_faults(
    const user_program_smoke_active_memory_stage_t* stage) {
    if (stage == NULL || stage->phase == NULL) {
        return false;
    }

    provoke_rodata_store_fault(stage->phase->rodata_marker);
    if (*stage->phase->rodata_marker != stage->phase->rodata_expected) {
        return false;
    }

    provoke_instruction_page_fault(stage->phase->instruction_fault_target,
                                   stage->phase->fault_resume_pc_slot);
    return *stage->phase->fault_resume_pc_slot == 0;
}

