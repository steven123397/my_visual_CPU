bool user_program_smoke_activate_supervisor_access(
    user_program_smoke_t* smoke,
    trap_context_t* expected_trap_context) {
    vm_address_space_t* address_space = NULL;
    vm_process_t* process = NULL;

    if (!smoke_ready(smoke) || expected_trap_context == NULL) {
        return false;
    }

    address_space = user_program_address_space(smoke->program);
    process = user_program_process(smoke->program);
    if (address_space == NULL || process == NULL ||
        !user_program_is_runnable(smoke->program) ||
        !user_program_activate(smoke->program) ||
        !user_program_is_active(smoke->program) ||
        runtime_context_active_process() != process ||
        runtime_context_active_address_space() != address_space ||
        trap_active_context() != expected_trap_context ||
        !vm_address_space_is_active(address_space) ||
        !vm_address_space_is_enabled(address_space) ||
        riscv_read_satp() != vm_address_space_satp_value(address_space) ||
        (riscv_read_sstatus() & RISCV_SSTATUS_SUM) != 0) {
        user_program_smoke_best_effort_deactivate(smoke);
        return false;
    }

    riscv_set_sstatus_bits(RISCV_SSTATUS_SUM);
    if ((riscv_read_sstatus() & RISCV_SSTATUS_SUM) != 0) {
        return true;
    }

    user_program_smoke_best_effort_deactivate(smoke);
    return false;
}

bool user_program_smoke_deactivate_supervisor_only(
    user_program_smoke_t* smoke,
    trap_context_t* expected_trap_context) {
    trap_user_runtime_t* runtime = NULL;
    vm_address_space_t* address_space = NULL;

    if (!smoke_ready(smoke) || expected_trap_context == NULL ||
        !user_program_is_active(smoke->program)) {
        return false;
    }

    runtime = user_program_runtime(smoke->program);
    address_space = user_program_address_space(smoke->program);
    if (runtime == NULL || address_space == NULL ||
        !user_program_deactivate(smoke->program)) {
        return false;
    }

    return !user_program_is_active(smoke->program) &&
           user_program_is_runnable(smoke->program) &&
           runtime_context_active_process() == NULL &&
           runtime_context_active_address_space() == NULL &&
           trap_active_context() == expected_trap_context &&
           trap_active_user_runtime() == NULL &&
           !vm_address_space_is_active(address_space) &&
           !vm_address_space_is_enabled(address_space) &&
           riscv_read_satp() == 0 &&
           (riscv_read_sstatus() & RISCV_SSTATUS_SUM) == 0;
}

bool user_program_smoke_enter_round(user_program_smoke_t* smoke,
                                    const user_program_smoke_round_t* round) {
    const user_program_smoke_round_stage_t stage = {
        .smoke = smoke,
        .round = round,
    };

    if (user_program_smoke_round_activate_if_needed(&stage) &&
        user_program_smoke_round_enter_active(&stage)) {
        return true;
    }

    user_program_smoke_best_effort_deactivate(smoke);
    return false;
}

bool user_program_smoke_exercise_active_memory(
    user_program_smoke_t* smoke,
    uint32_t* backing_page,
    uint32_t* remap_page,
    const user_program_smoke_active_phase_t* phase) {
    user_program_smoke_active_memory_stage_t stage;

    return user_program_smoke_active_memory_stage_init(&stage,
                                                       smoke,
                                                       backing_page,
                                                       remap_page,
                                                       phase) &&
           user_program_smoke_active_memory_exercise_alias_backing(&stage) &&
           user_program_smoke_active_memory_exercise_anon_regions(&stage) &&
           user_program_smoke_active_memory_exercise_remap_rebind(&stage) &&
           user_program_smoke_active_memory_exercise_faults(&stage);
}

static bool user_program_smoke_round_enter_active(
    const user_program_smoke_round_stage_t* stage) {
    return user_program_smoke_round_stage_valid(stage) &&
           user_program_is_active(stage->smoke->program) &&
           user_program_smoke_enter_with_interrupt_signals(
               stage->smoke,
               stage->round->timer_signal_page,
               stage->round->timer_signal_index,
               stage->round->timer_signal_value,
               stage->round->external_signal_page,
               stage->round->external_signal_index,
               stage->round->external_signal_value,
               stage->round->timer_delta);
}

static bool user_program_smoke_enter_with_interrupt_signals(
    user_program_smoke_t* smoke,
    uint32_t* timer_signal_page,
    size_t timer_signal_index,
    uint32_t timer_signal_value,
    uint32_t* external_signal_page,
    size_t external_signal_index,
    uint32_t external_signal_value,
    uint64_t timer_delta) {
    trap_user_runtime_t* runtime = NULL;
    const bool external_signal_enabled = external_signal_page != NULL;

    if (!smoke_ready(smoke) || timer_signal_page == NULL || timer_delta == 0 ||
        !user_program_is_active(smoke->program)) {
        return false;
    }

    runtime = user_program_runtime(smoke->program);
    if (runtime == NULL) {
        return false;
    }

    riscv_clear_sstatus_bits(RISCV_SSTATUS_SUM);
    if (external_signal_enabled) {
        riscv_clear_sstatus_bits(RISCV_SSTATUS_SIE);
    }

    if ((riscv_read_sstatus() & RISCV_SSTATUS_SUM) != 0 ||
        (external_signal_enabled &&
         (riscv_read_sstatus() & RISCV_SSTATUS_SIE) != 0) ||
        !user_program_unmap_region_base_page(smoke->program,
                                             USER_PROGRAM_REGION_ALIAS) ||
        !trap_user_runtime_arm_timer_signal(runtime,
                                            timer_signal_page,
                                            timer_signal_index,
                                            timer_signal_value) ||
        trap_user_runtime_timer_signal_delivered(runtime)) {
        return false;
    }

    if (external_signal_enabled &&
        (!trap_user_runtime_arm_external_signal(runtime,
                                               external_signal_page,
                                               external_signal_index,
                                               external_signal_value) ||
         trap_user_runtime_external_signal_delivered(runtime))) {
        return false;
    }

    if (external_signal_enabled) {
        platform_uart_enable_thre_irq();
    }
    timer_schedule_delta(timer_delta);
    return user_program_enter(smoke->program);
}

static bool user_program_smoke_unmap_remap_page(user_program_smoke_t* smoke) {
    return smoke_ready(smoke) && smoke->remap_region.registered &&
           vm_user_region_unmap_page(&smoke->remap_region,
                                     smoke->remap_region.vaddr);
}

static bool user_program_smoke_rebind_alias_fault_object(
    user_program_smoke_t* smoke) {
    return smoke_ready(smoke) &&
           user_program_rebind_region_fault_object(smoke->program,
                                                  USER_PROGRAM_REGION_ALIAS,
                                                  &smoke->remap_object);
}

static void user_program_smoke_best_effort_deactivate(user_program_smoke_t* smoke) {
    if (!smoke_ready(smoke) || !user_program_is_active(smoke->program)) {
        return;
    }

    (void)user_program_deactivate(smoke->program);
}
