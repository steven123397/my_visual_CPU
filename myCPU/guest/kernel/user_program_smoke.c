#include "user_program_smoke.h"

#include "memory.h"
#include "platform.h"
#include "pmm.h"
#include "riscv.h"
#include "runtime_context.h"
#include "timer.h"

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

static bool invalid_region_state_ok(const vm_user_region_t* region) {
    return region != NULL && region->address_space == NULL &&
           !region->registered && region->object == NULL;
}

static bool runtime_policies_bound(const trap_context_t* trap_context,
                                   const trap_user_runtime_t* user_runtime) {
    return trap_context != NULL && user_runtime != NULL &&
           trap_context->supervisor_timer_policy.user_runtime == user_runtime &&
           trap_context->supervisor_external_policy.user_runtime ==
               user_runtime &&
           trap_context->user_ecall_policy.user_runtime == user_runtime;
}

static bool runtime_policies_cleared(const trap_context_t* trap_context) {
    return trap_context != NULL &&
           trap_context->supervisor_timer_policy.user_runtime == NULL &&
           trap_context->supervisor_external_policy.user_runtime == NULL &&
           trap_context->user_ecall_policy.user_runtime == NULL;
}

static bool address_space_ready(const user_program_t* program) {
    return program != NULL &&
           user_program_address_space((user_program_t*)program) != NULL;
}

static bool smoke_ready(const user_program_smoke_t* smoke) {
    return smoke != NULL && smoke->program != NULL &&
           user_program_address_space(smoke->program) != NULL &&
           user_program_process(smoke->program) != NULL;
}

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
static bool user_program_smoke_reactivate_and_enter_with_interrupt_signals(
    user_program_smoke_t* smoke,
    trap_context_t* expected_trap_context,
    uint32_t* timer_signal_page,
    size_t timer_signal_index,
    uint32_t timer_signal_value,
    uint32_t* external_signal_page,
    size_t external_signal_index,
    uint32_t external_signal_value,
    uint64_t timer_delta);
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

__attribute__((noinline)) static void provoke_rodata_store_fault(
    volatile const uint32_t* marker) {
    volatile uint32_t* marker_ptr = (volatile uint32_t*)(uintptr_t)marker;

    *marker_ptr = 0xDEADBEEFU;
}

__attribute__((noinline)) static void provoke_instruction_page_fault(
    uintptr_t target,
    volatile uintptr_t* fault_resume_pc_slot) {
    __asm__ volatile(
        "la t0, 1f\n"
        "sd t0, 0(%1)\n"
        "jalr ra, 0(%0)\n"
        "1:\n"
        "sd zero, 0(%1)\n"
        :
        : "r"(target), "r"(fault_resume_pc_slot)
        : "t0", "ra", "memory");
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
    const uintptr_t user_exec_page_paddr =
        user_program_value(program, USER_PROGRAM_VALUE_EXEC_PAGE_PADDR);
    const uintptr_t user_exec_vaddr =
        user_program_value(program, USER_PROGRAM_VALUE_EXEC_VADDR);
    const uintptr_t user_stack_vaddr =
        user_program_value(program, USER_PROGRAM_VALUE_STACK_VADDR);
    const uintptr_t user_alias_vaddr =
        user_program_value(program, USER_PROGRAM_VALUE_ALIAS_VADDR);
    const uintptr_t user_anon_tail_vaddr =
        user_program_value(program, USER_PROGRAM_VALUE_ANON_TAIL_VADDR);
    const uintptr_t user_entry_pc =
        user_program_value(program, USER_PROGRAM_VALUE_ENTRY_PC);
    const uintptr_t user_expected_ecall_pc =
        user_program_value(program, USER_PROGRAM_VALUE_EXPECTED_ECALL_PC);
    const uintptr_t user_sp =
        user_program_value(program, USER_PROGRAM_VALUE_USER_SP);

    return program != NULL && user_base == 0 &&
           user_limit == memory_kernel_start() &&
           user_anon_tail_vaddr < user_limit &&
           user_stack_vaddr >= user_base && user_stack_vaddr < user_limit &&
           user_exec_vaddr >= user_base && user_exec_vaddr < user_limit &&
           user_alias_vaddr >= user_base && user_alias_vaddr < user_limit &&
           (user_alias_vaddr & (MEMORY_PAGE_SIZE - 1U)) == 0 &&
           (user_anon_tail_vaddr & (MEMORY_PAGE_SIZE - 1U)) == 0 &&
           (user_stack_vaddr & (MEMORY_PAGE_SIZE - 1U)) == 0 &&
           (user_exec_vaddr & (MEMORY_PAGE_SIZE - 1U)) == 0 &&
           user_sp == user_stack_vaddr + MEMORY_PAGE_SIZE &&
           user_exec_page_paddr >= memory_text_start() &&
           user_exec_page_paddr < memory_text_end() &&
           user_entry_pc >= user_exec_vaddr &&
           user_entry_pc < user_exec_vaddr + MEMORY_PAGE_SIZE &&
           user_expected_ecall_pc >= user_exec_vaddr &&
           user_expected_ecall_pc < user_exec_vaddr + MEMORY_PAGE_SIZE;
}

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

bool user_program_smoke_prepare_standard(user_program_smoke_t* smoke,
                                         user_program_t* program,
                                         const user_program_smoke_prepare_t* prepare) {
    if (smoke == NULL || program == NULL || prepare == NULL ||
        prepare->trap_context == NULL || prepare->backing_page_paddr == 0 ||
        prepare->user_stack_paddr == 0 || prepare->remap_page_paddr == 0 ||
        prepare->fault_skip_vaddr == 0 || prepare->fault_skip_size == 0 ||
        prepare->fault_resume_vaddr == 0 ||
        prepare->fault_resume_size == 0 ||
        prepare->fault_resume_pc_slot == NULL ||
        prepare->trap_stack_base == NULL || prepare->trap_stack_size == 0) {
        return false;
    }

    return user_program_create(program,
                               prepare->backing_page_paddr,
                               prepare->user_stack_paddr) &&
           user_program_smoke_validate_created_program(program) &&
           user_program_smoke_prepare_address_space(smoke,
                                                    program,
                                                    prepare->backing_page_paddr,
                                                    prepare->remap_page_paddr,
                                                    prepare->fault_skip_vaddr,
                                                    prepare->fault_skip_size,
                                                    prepare->fault_resume_vaddr,
                                                    prepare->fault_resume_size,
                                                    prepare->fault_resume_pc_slot) &&
           user_program_smoke_prepare_runtime(
               smoke,
               prepare->trap_context,
               prepare->arg0,
               prepare->trap_stack_base,
               prepare->trap_stack_size,
               prepare->validate,
               prepare->validate_context,
               prepare->supervisor_timer_post_handler,
               prepare->supervisor_timer_post_context,
               prepare->supervisor_external_post_handler,
               prepare->supervisor_external_post_context) &&
           user_program_runtime(program) != NULL;
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
        return false;
    }

    riscv_set_sstatus_bits(RISCV_SSTATUS_SUM);
    return (riscv_read_sstatus() & RISCV_SSTATUS_SUM) != 0;
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
    if (!smoke_ready(smoke) || round == NULL || round->timer_signal_page == NULL ||
        round->timer_delta == 0) {
        return false;
    }

    if (user_program_is_active(smoke->program)) {
        return user_program_smoke_enter_with_interrupt_signals(
            smoke,
            round->timer_signal_page,
            round->timer_signal_index,
            round->timer_signal_value,
            round->external_signal_page,
            round->external_signal_index,
            round->external_signal_value,
            round->timer_delta);
    }

    return user_program_smoke_reactivate_and_enter_with_interrupt_signals(
        smoke,
        round->expected_trap_context,
        round->timer_signal_page,
        round->timer_signal_index,
        round->timer_signal_value,
        round->external_signal_page,
        round->external_signal_index,
        round->external_signal_value,
        round->timer_delta);
}

static bool user_program_smoke_reactivate_and_enter_with_interrupt_signals(
    user_program_smoke_t* smoke,
    trap_context_t* expected_trap_context,
    uint32_t* timer_signal_page,
    size_t timer_signal_index,
    uint32_t timer_signal_value,
    uint32_t* external_signal_page,
    size_t external_signal_index,
    uint32_t external_signal_value,
    uint64_t timer_delta) {
    return smoke_ready(smoke) && !user_program_is_active(smoke->program) &&
           user_program_smoke_activate_supervisor_access(smoke,
                                                         expected_trap_context) &&
           user_program_smoke_enter_with_interrupt_signals(smoke,
                                                           timer_signal_page,
                                                           timer_signal_index,
                                                           timer_signal_value,
                                                           external_signal_page,
                                                           external_signal_index,
                                                           external_signal_value,
                                                           timer_delta);
}

bool user_program_smoke_exercise_active_memory(
    user_program_smoke_t* smoke,
    uint32_t* backing_page,
    uint32_t* remap_page,
    const user_program_smoke_active_phase_t* phase) {
    const uintptr_t alias_vaddr = smoke_ready(smoke)
                                      ? user_program_value(
                                            smoke->program,
                                            USER_PROGRAM_VALUE_ALIAS_VADDR)
                                      : 0;
    const uintptr_t anon_vaddr = smoke_ready(smoke)
                                     ? user_program_value(
                                           smoke->program,
                                           USER_PROGRAM_VALUE_ANON_VADDR)
                                     : 0;
    const uintptr_t anon_tail_vaddr = smoke_ready(smoke)
                                          ? user_program_value(
                                                smoke->program,
                                                USER_PROGRAM_VALUE_ANON_TAIL_VADDR)
                                          : 0;
    uint32_t* alias_page = NULL;
    uint32_t* anon_page = NULL;
    uint32_t* anon_tail_page = NULL;
    const uint32_t backing_word0 = backing_page != NULL ? backing_page[0] : 0;

    if (!smoke_ready(smoke) || !user_program_is_active(smoke->program) ||
        backing_page == NULL || remap_page == NULL || phase == NULL ||
        phase->rodata_marker == NULL || phase->instruction_fault_target == 0 ||
        phase->fault_resume_pc_slot == NULL ||
        (riscv_read_sstatus() & RISCV_SSTATUS_SUM) == 0 ||
        !vm_range_is_user(alias_vaddr, MEMORY_PAGE_SIZE) ||
        !vm_range_is_user(anon_vaddr, MEMORY_PAGE_SIZE) ||
        !vm_range_is_user(anon_tail_vaddr, MEMORY_PAGE_SIZE)) {
        return false;
    }

    alias_page = (uint32_t*)alias_vaddr;
    anon_page = (uint32_t*)anon_vaddr;
    anon_tail_page = (uint32_t*)anon_tail_vaddr;

    if (alias_page[0] != backing_word0) {
        return false;
    }

    alias_page[1] = phase->alias_store_value;
    if (backing_page[1] != phase->alias_store_value) {
        return false;
    }

    backing_page[2] = phase->backing_store_value;
    if (backing_page[2] != phase->backing_store_value ||
        *phase->rodata_marker != phase->rodata_expected ||
        user_program_reset_object(smoke->program, USER_PROGRAM_OBJECT_ANON)) {
        return false;
    }

    if (anon_page[0] != 0U || anon_page[1] != 0U || anon_tail_page[0] != 0U ||
        anon_tail_page[1] != 0U) {
        return false;
    }

    anon_page[0] = phase->anon_value0;
    anon_page[1] = phase->anon_value1;
    anon_tail_page[0] = phase->anon_tail_value0;
    anon_tail_page[1] = phase->anon_tail_value1;
    if (!user_program_unmap_region_base_page(smoke->program,
                                             USER_PROGRAM_REGION_ANON) ||
        !user_program_unmap_region_base_page(smoke->program,
                                             USER_PROGRAM_REGION_ANON_TAIL) ||
        anon_page[0] != phase->anon_value0 ||
        anon_page[1] != phase->anon_value1 ||
        anon_tail_page[0] != phase->anon_tail_value0 ||
        anon_tail_page[1] != phase->anon_tail_value1) {
        return false;
    }

    if (!user_program_smoke_rebind_alias_fault_object(smoke) ||
        alias_page[0] != remap_page[0] || backing_page[0] != backing_word0) {
        return false;
    }

    alias_page[1] = phase->remap_store_value;
    if (remap_page[1] != phase->remap_store_value ||
        backing_page[1] != phase->alias_store_value) {
        return false;
    }

    provoke_rodata_store_fault(phase->rodata_marker);
    if (*phase->rodata_marker != phase->rodata_expected) {
        return false;
    }

    provoke_instruction_page_fault(phase->instruction_fault_target,
                                   phase->fault_resume_pc_slot);
    return *phase->fault_resume_pc_slot == 0;
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
