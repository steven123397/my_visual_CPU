#include <stdint.h>

#include "console.h"
#include "memory.h"
#include "panic.h"
#include "platform.h"
#include "pmm.h"
#include "runtime_context.h"
#include "supervisor_demo_smoke.h"
#include "trap.h"
#include "user_program.h"
#include "user_program_smoke.h"
#include "vm.h"

static volatile uintptr_t instruction_fault_resume_pc = 0;
static const uint32_t rodata_marker = 0xCAFEBABEU;
static const uint32_t user_data_marker = 0x5A5A1234U;
static const uint32_t user_timer_marker = 1U;
static const uint32_t user_external_marker = 2U;

extern char user_test_entry[];
extern char user_test_ecall[];

void kernel_main(void) {
    uintptr_t early_cursor = 0;
    trap_context_t supervisor_trap_context;
    trap_user_runtime_t* user_runtime = NULL;
    user_program_t user_program;
    user_program_smoke_t user_program_smoke;
    uintptr_t user_anon_vaddr = 0;
    user_program_smoke_active_phase_t active_phase;
    supervisor_demo_smoke_state_t demo_trap_state;
    supervisor_demo_smoke_pages_t demo_pages = {0};
    size_t lifecycle_free_before = 0;
    void* early_allocation = NULL;

    memory_init();
    early_allocation = memory_alloc(96, 64);
    platform_plic_supervisor_init();
    runtime_context_reset();
    trap_context_init(&supervisor_trap_context);
    user_program_init(&user_program);
    user_program_smoke_init(&user_program_smoke);
    supervisor_demo_smoke_init_active_phase(&active_phase,
                                            &rodata_marker,
                                            &instruction_fault_resume_pc);
    if (!trap_context_activate(&supervisor_trap_context) ||
        !trap_context_is_active(&supervisor_trap_context) ||
        trap_active_context() != &supervisor_trap_context) {
        panic_shutdown();
    }
    if (!supervisor_demo_smoke_validate_memory_layout(early_allocation,
                                                      &rodata_marker) ||
        !user_program_smoke_plan_standard(&user_program,
                                          (uintptr_t)user_test_entry,
                                          (uintptr_t)user_test_ecall) ||
        !user_program_smoke_validate_standard_plan(&user_program,
                                                   vm_user_base(),
                                                   vm_user_limit())) {
        panic_shutdown();
    }
    user_anon_vaddr =
        user_program_value(&user_program, USER_PROGRAM_VALUE_ANON_VADDR);
    early_cursor = memory_heap_current();
    pmm_init();

    if (!supervisor_demo_smoke_validate_pmm_setup(early_cursor) ||
        !supervisor_demo_smoke_probe_storage_page()) {
        panic_shutdown();
    }
    lifecycle_free_before = pmm_free_pages();
    if (!user_program_smoke_validate_vm_lifecycle(user_anon_vaddr,
                                                  lifecycle_free_before)) {
        panic_shutdown();
    }

    if (!supervisor_demo_smoke_alloc_pages(&demo_pages)) {
        panic_shutdown();
    }
    supervisor_demo_smoke_init(&demo_trap_state,
                               demo_pages.remap_page,
                               user_data_marker,
                               user_timer_marker,
                               user_external_marker);
    if (!user_program_smoke_validate_runtime_reprepare(
            (uintptr_t)user_test_entry,
            (uintptr_t)user_test_ecall,
            (uintptr_t)demo_pages.backing_page,
            (uintptr_t)demo_pages.user_stack_page,
            1U,
            demo_pages.user_trap_stack_page,
            MEMORY_PAGE_SIZE)) {
        panic_shutdown();
    }
    if (!user_program_create(&user_program,
                             (uintptr_t)demo_pages.backing_page,
                             (uintptr_t)demo_pages.user_stack_page) ||
        !user_program_smoke_validate_created_program(&user_program)) {
        panic_shutdown();
    }
    user_runtime = user_program_runtime(&user_program);

    if (user_runtime == NULL ||
        !user_program_smoke_prepare_address_space(
            &user_program_smoke,
            &user_program,
            (uintptr_t)demo_pages.backing_page,
            (uintptr_t)demo_pages.remap_page,
            (uintptr_t)&rodata_marker,
            sizeof(rodata_marker),
            (uintptr_t)demo_pages.nx_page,
            MEMORY_PAGE_SIZE,
            &instruction_fault_resume_pc) ||
        !user_program_smoke_prepare_runtime(
            &user_program_smoke,
            &supervisor_trap_context,
            user_program_value(&user_program, USER_PROGRAM_VALUE_ALIAS_VADDR),
            demo_pages.user_trap_stack_page,
            MEMORY_PAGE_SIZE,
            supervisor_demo_smoke_user_ecall_validate,
            &demo_trap_state,
            supervisor_demo_smoke_timer_interrupt_handler,
            &demo_trap_state,
            supervisor_demo_smoke_external_interrupt_handler,
            &demo_trap_state)) {
        panic_shutdown();
    }

    if (!supervisor_demo_smoke_prime_active_pages(&demo_pages, &active_phase) ||
        !user_program_smoke_activate_supervisor_access(
            &user_program_smoke, &supervisor_trap_context) ||
        !user_program_is_active(&user_program) ||
        runtime_context_active_process() != user_program_process(&user_program) ||
        runtime_context_active_address_space() !=
            user_program_address_space(&user_program)) {
        panic_shutdown();
    }
    if (!user_program_smoke_exercise_active_memory(&user_program_smoke,
                                                   demo_pages.backing_page,
                                                   demo_pages.remap_page,
                                                   &active_phase)) {
        panic_shutdown();
    }
    if (!supervisor_demo_smoke_prepare_user_entry(&demo_trap_state) ||
        !user_program_smoke_enter_with_interrupt_signals(
            &user_program_smoke,
            demo_pages.remap_page,
            3U,
            user_timer_marker,
            demo_pages.remap_page,
            4U,
            user_external_marker,
            8U)) {
        panic_shutdown();
    }
    if (!supervisor_demo_smoke_verify_user_return(&demo_trap_state,
                                                  user_runtime)) {
        panic_shutdown();
    }
    if (!user_program_smoke_deactivate_supervisor_only(&user_program_smoke,
                                                       &supervisor_trap_context)) {
        panic_shutdown();
    }
    if (!supervisor_demo_smoke_run_platform_tail(&demo_trap_state, 8U)) {
        panic_shutdown();
    }

    console_puts("KRN");
    platform_shutdown(0);
}
