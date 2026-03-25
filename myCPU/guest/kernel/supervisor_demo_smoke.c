#include "supervisor_demo_smoke.h"

#include <stddef.h>
#include <stdint.h>

#include "memory.h"
#include "panic.h"
#include "platform.h"
#include "pmm.h"
#include "riscv.h"
#include "runtime_context.h"
#include "storage.h"
#include "supervisor_runtime.h"
#include "user_program.h"
#include "user_program_smoke.h"
#include "vm.h"

typedef struct SupervisorDemoSmokePages {
    uint32_t* backing_page;
    uint32_t* remap_page;
    uint32_t* nx_page;
    uint32_t* user_stack_page;
    uint8_t* user_trap_stack_page;
} supervisor_demo_smoke_pages_t;

typedef struct SupervisorDemoSmokeState {
    supervisor_runtime_interrupt_state_t interrupts;
    volatile uint64_t user_ecall_seen;
    uint32_t* user_data_page;
    uint32_t expected_user_data;
    uint32_t expected_user_timer;
    uint32_t expected_user_external;
} supervisor_demo_smoke_state_t;

enum {
    SUPERVISOR_DEMO_SMOKE_EARLY_ALLOC_SIZE = 96U,
    SUPERVISOR_DEMO_SMOKE_EARLY_ALLOC_ALIGN = 64U,
    SUPERVISOR_DEMO_SMOKE_TIMER_DELTA = 8U,
    SUPERVISOR_DEMO_SMOKE_INTERRUPT_TIMEOUT = 4096U,
    SUPERVISOR_DEMO_SMOKE_TIMER_SIGNAL_INDEX = 3U,
    SUPERVISOR_DEMO_SMOKE_EXTERNAL_SIGNAL_INDEX = 4U,
};

static const uint32_t supervisor_demo_rodata_marker = 0xCAFEBABEU;
static const uint32_t supervisor_demo_user_data_marker = 0x5A5A1234U;
static const uint32_t supervisor_demo_user_timer_marker = 1U;
static const uint32_t supervisor_demo_user_external_marker = 2U;

static void supervisor_demo_smoke_init(supervisor_demo_smoke_state_t* state,
                                       uint32_t* user_data_page,
                                       uint32_t expected_user_data,
                                       uint32_t expected_user_timer,
                                       uint32_t expected_user_external);
static void supervisor_demo_smoke_init_active_phase(
    user_program_smoke_active_phase_t* phase,
    const volatile uint32_t* rodata_marker,
    volatile uintptr_t* fault_resume_pc_slot);
static bool supervisor_demo_smoke_validate_memory_layout(
    void* early_allocation,
    const volatile uint32_t* rodata_marker);
static bool supervisor_demo_smoke_validate_pmm_setup(uintptr_t early_cursor);
static bool supervisor_demo_smoke_probe_storage_page(void);
static bool supervisor_demo_smoke_alloc_pages(supervisor_demo_smoke_pages_t* pages);
static bool supervisor_demo_smoke_prime_active_pages(
    supervisor_demo_smoke_pages_t* pages,
    user_program_smoke_active_phase_t* phase);
static bool supervisor_demo_smoke_validate_user_program_lifecycle(
    trap_context_t* trap_context,
    uintptr_t exec_symbol,
    uintptr_t ecall_symbol,
    const supervisor_demo_smoke_pages_t* pages);
static bool supervisor_demo_smoke_prepare_bootstrap(
    supervisor_demo_smoke_state_t* state,
    supervisor_demo_smoke_pages_t* pages,
    trap_context_t* trap_context,
    user_program_t* program,
    void* early_allocation,
    const volatile uint32_t* rodata_marker,
    uintptr_t exec_symbol,
    uintptr_t ecall_symbol,
    uint32_t expected_user_data,
    uint32_t expected_user_timer,
    uint32_t expected_user_external);
static bool supervisor_demo_smoke_prepare_user_program(
    supervisor_demo_smoke_state_t* state,
    user_program_t* program,
    user_program_smoke_t* smoke,
    trap_context_t* trap_context,
    const supervisor_demo_smoke_pages_t* pages,
    const volatile uint32_t* rodata_marker,
    volatile uintptr_t* fault_resume_pc_slot);
static bool supervisor_demo_smoke_prepare_user_entry(
    supervisor_demo_smoke_state_t* state);
static bool supervisor_demo_smoke_run_user_round(
    supervisor_demo_smoke_state_t* state,
    user_program_smoke_t* smoke,
    trap_context_t* expected_trap_context,
    trap_user_runtime_t* user_runtime,
    uint32_t* timer_signal_page,
    size_t timer_signal_index,
    uint32_t timer_signal_value,
    uint32_t* external_signal_page,
    size_t external_signal_index,
    uint32_t external_signal_value,
    uint64_t timer_delta);
static bool supervisor_demo_smoke_run_user_program(
    supervisor_demo_smoke_state_t* state,
    user_program_smoke_t* smoke,
    trap_context_t* expected_trap_context,
    supervisor_demo_smoke_pages_t* pages,
    user_program_smoke_active_phase_t* active_phase,
    uint64_t timer_delta);
static bool supervisor_demo_smoke_read_storage_signature(uint8_t* storage_buffer);
static bool supervisor_demo_smoke_wait_platform_interrupts(
    supervisor_demo_smoke_state_t* state,
    uint64_t timer_delta);
static bool supervisor_demo_smoke_run_platform_tail(
    supervisor_demo_smoke_state_t* state,
    uint64_t timer_delta);
static bool supervisor_demo_smoke_run_demo_session(
    supervisor_demo_smoke_state_t* state,
    user_program_smoke_t* smoke,
    trap_context_t* expected_trap_context,
    supervisor_demo_smoke_pages_t* pages,
    user_program_smoke_active_phase_t* active_phase,
    uint64_t timer_delta);
static void supervisor_demo_smoke_external_post_handler(uint32_t source_id,
                                                        void* context);
static bool supervisor_demo_smoke_user_ecall_validate(
    const trap_user_runtime_t* user_runtime,
    uint64_t epc,
    uint64_t tval,
    void* context);
static bool supervisor_demo_smoke_verify_user_return(
    const supervisor_demo_smoke_state_t* state,
    const trap_user_runtime_t* user_runtime);
static bool active_user_program_state_ok(user_program_smoke_t* smoke,
                                         trap_context_t* expected_trap_context);

static void supervisor_demo_smoke_init(supervisor_demo_smoke_state_t* state,
                                       uint32_t* user_data_page,
                                       uint32_t expected_user_data,
                                       uint32_t expected_user_timer,
                                       uint32_t expected_user_external) {
    if (state == NULL) {
        return;
    }

    supervisor_runtime_interrupt_state_init(&state->interrupts);
    state->interrupts.expected_external_source_id = PLIC_SOURCE_UART_THRE;
    state->interrupts.external_post_handler =
        supervisor_demo_smoke_external_post_handler;
    state->interrupts.external_post_context = state;
    state->user_ecall_seen = 0;
    state->user_data_page = user_data_page;
    state->expected_user_data = expected_user_data;
    state->expected_user_timer = expected_user_timer;
    state->expected_user_external = expected_user_external;
}

static void supervisor_demo_smoke_init_active_phase(
    user_program_smoke_active_phase_t* phase,
    const volatile uint32_t* rodata_marker,
    volatile uintptr_t* fault_resume_pc_slot) {
    if (phase == NULL) {
        return;
    }

    phase->alias_store_value = 0x55667788U;
    phase->backing_store_value = 0x99AABBCCU;
    phase->anon_value0 = 0x13579BDFU;
    phase->anon_value1 = 0x02468ACEU;
    phase->anon_tail_value0 = 0x2468ACE0U;
    phase->anon_tail_value1 = 0x10203040U;
    phase->remap_store_value = 0x77665544U;
    phase->rodata_marker = rodata_marker;
    phase->rodata_expected = 0xCAFEBABEU;
    phase->instruction_fault_target = 0;
    phase->fault_resume_pc_slot = fault_resume_pc_slot;
}

static bool supervisor_demo_smoke_validate_memory_layout(
    void* early_allocation,
    const volatile uint32_t* rodata_marker) {
    return early_allocation != NULL && rodata_marker != NULL &&
           memory_kernel_start() == MEM_BASE &&
           memory_text_start() == memory_kernel_start() &&
           memory_text_start() < memory_text_end() &&
           memory_text_end() <= memory_rodata_start() &&
           memory_rodata_start() <= memory_rodata_end() &&
           memory_rodata_end() <= memory_data_start() &&
           memory_data_start() <= memory_data_end() &&
           memory_data_end() <= memory_bss_start() &&
           memory_bss_start() <= memory_bss_end() &&
           memory_bss_end() <= memory_heap_start() &&
           memory_heap_start() >= memory_kernel_end() &&
           memory_heap_limit() == MEM_BASE + MEM_SIZE &&
           ((uintptr_t)rodata_marker) >= memory_rodata_start() &&
           ((uintptr_t)rodata_marker) < memory_rodata_end() &&
           (((uintptr_t)early_allocation) & 63U) == 0 &&
           (memory_text_start() & (MEMORY_PAGE_SIZE - 1U)) == 0 &&
           (memory_text_end() & (MEMORY_PAGE_SIZE - 1U)) == 0 &&
           (memory_rodata_start() & (MEMORY_PAGE_SIZE - 1U)) == 0 &&
           (memory_rodata_end() & (MEMORY_PAGE_SIZE - 1U)) == 0 &&
           (memory_data_start() & (MEMORY_PAGE_SIZE - 1U)) == 0 &&
           (memory_bss_end() & (MEMORY_PAGE_SIZE - 1U)) == 0 &&
           (memory_heap_start() & (MEMORY_PAGE_SIZE - 1U)) == 0;
}

static bool supervisor_demo_smoke_validate_pmm_setup(uintptr_t early_cursor) {
    return pmm_managed_start() == memory_heap_current() &&
           pmm_managed_start() >= early_cursor &&
           pmm_managed_start() < pmm_managed_end() &&
           pmm_managed_end() == memory_heap_limit() &&
           pmm_total_pages() != 0 &&
           pmm_total_pages() ==
               (size_t)((pmm_managed_end() - pmm_managed_start()) /
                        MEMORY_PAGE_SIZE) &&
           pmm_free_pages() == pmm_total_pages() && pmm_used_pages() == 0 &&
           memory_alloc(16, 8) == NULL &&
           !pmm_free_page((void*)memory_kernel_start());
}

static bool supervisor_demo_smoke_probe_storage_page(void) {
    uint8_t* storage_page = (uint8_t*)pmm_alloc_page();

    return storage_page != NULL &&
           (((uintptr_t)storage_page) & (MEMORY_PAGE_SIZE - 1U)) == 0 &&
           pmm_free_pages() + 1U == pmm_total_pages() &&
           pmm_used_pages() == 1U && pmm_free_page(storage_page) &&
           !pmm_free_page(storage_page) &&
           pmm_free_pages() == pmm_total_pages() && pmm_used_pages() == 0U;
}

static bool supervisor_demo_smoke_alloc_pages(supervisor_demo_smoke_pages_t* pages) {
    if (pages == NULL) {
        return false;
    }

    pages->backing_page = (uint32_t*)pmm_alloc_page();
    pages->remap_page = (uint32_t*)pmm_alloc_page();
    pages->nx_page = (uint32_t*)pmm_alloc_page();
    pages->user_stack_page = (uint32_t*)pmm_alloc_page();
    pages->user_trap_stack_page = (uint8_t*)pmm_alloc_page();

    return pages->backing_page != NULL && pages->remap_page != NULL &&
           pages->nx_page != NULL && pages->user_stack_page != NULL &&
           pages->user_trap_stack_page != NULL &&
           (((uintptr_t)pages->user_trap_stack_page) &
            (TRAP_USER_RUNTIME_STACK_ALIGNMENT - 1U)) == 0 &&
           pmm_free_pages() + 5U == pmm_total_pages() &&
           pmm_used_pages() == 5U;
}

static bool supervisor_demo_smoke_prime_active_pages(
    supervisor_demo_smoke_pages_t* pages,
    user_program_smoke_active_phase_t* phase) {
    if (pages == NULL || phase == NULL || pages->backing_page == NULL ||
        pages->remap_page == NULL || pages->nx_page == NULL) {
        return false;
    }

    pages->backing_page[0] = 0x11223344U;
    pages->remap_page[0] = 0xA1B2C3D4U;
    pages->remap_page[1] = 0x01020304U;
    pages->remap_page[2] = 0U;
    pages->remap_page[3] = 0U;
    pages->remap_page[4] = 0U;
    pages->nx_page[0] = 0x00008067U;
    phase->instruction_fault_target = (uintptr_t)pages->nx_page;
    return true;
}

static bool supervisor_demo_smoke_validate_user_program_lifecycle(
    trap_context_t* trap_context,
    uintptr_t exec_symbol,
    uintptr_t ecall_symbol,
    const supervisor_demo_smoke_pages_t* pages) {
    return trap_context != NULL && pages != NULL &&
           pages->backing_page != NULL && pages->user_stack_page != NULL &&
           pages->user_trap_stack_page != NULL &&
           user_program_smoke_validate_lifecycle(
               trap_context,
               exec_symbol,
               ecall_symbol,
               (uintptr_t)pages->backing_page,
               (uintptr_t)pages->user_stack_page,
               1U,
               pages->user_trap_stack_page,
               MEMORY_PAGE_SIZE);
}

static bool supervisor_demo_smoke_prepare_bootstrap(
    supervisor_demo_smoke_state_t* state,
    supervisor_demo_smoke_pages_t* pages,
    trap_context_t* trap_context,
    user_program_t* program,
    void* early_allocation,
    const volatile uint32_t* rodata_marker,
    uintptr_t exec_symbol,
    uintptr_t ecall_symbol,
    uint32_t expected_user_data,
    uint32_t expected_user_timer,
    uint32_t expected_user_external) {
    uintptr_t user_anon_vaddr = 0;
    uintptr_t early_cursor = 0;
    size_t lifecycle_free_before = 0;

    if (state == NULL || pages == NULL || trap_context == NULL ||
        program == NULL || rodata_marker == NULL) {
        return false;
    }

    if (!supervisor_demo_smoke_validate_memory_layout(early_allocation,
                                                      rodata_marker) ||
        !user_program_smoke_plan_standard(program, exec_symbol, ecall_symbol) ||
        !user_program_smoke_validate_standard_plan(program,
                                                   vm_user_base(),
                                                   vm_user_limit())) {
        return false;
    }

    user_anon_vaddr =
        user_program_value(program, USER_PROGRAM_VALUE_ANON_VADDR);
    early_cursor = memory_heap_current();
    pmm_init();
    if (!supervisor_demo_smoke_validate_pmm_setup(early_cursor) ||
        !supervisor_demo_smoke_probe_storage_page()) {
        return false;
    }

    lifecycle_free_before = pmm_free_pages();
    if (!user_program_smoke_validate_vm_lifecycle(user_anon_vaddr,
                                                  lifecycle_free_before) ||
        !supervisor_demo_smoke_alloc_pages(pages)) {
        return false;
    }

    supervisor_demo_smoke_init(state,
                               pages->remap_page,
                               expected_user_data,
                               expected_user_timer,
                               expected_user_external);
    return supervisor_demo_smoke_validate_user_program_lifecycle(
        trap_context, exec_symbol, ecall_symbol, pages);
}

static bool supervisor_demo_smoke_prepare_user_program(
    supervisor_demo_smoke_state_t* state,
    user_program_t* program,
    user_program_smoke_t* smoke,
    trap_context_t* trap_context,
    const supervisor_demo_smoke_pages_t* pages,
    const volatile uint32_t* rodata_marker,
    volatile uintptr_t* fault_resume_pc_slot) {
    const user_program_smoke_prepare_t prepare = {
        .trap_context = trap_context,
        .backing_page_paddr = (uintptr_t)pages->backing_page,
        .user_stack_paddr = (uintptr_t)pages->user_stack_page,
        .remap_page_paddr = (uintptr_t)pages->remap_page,
        .fault_skip_vaddr = (uintptr_t)rodata_marker,
        .fault_skip_size = sizeof(*rodata_marker),
        .fault_resume_vaddr = (uintptr_t)pages->nx_page,
        .fault_resume_size = MEMORY_PAGE_SIZE,
        .fault_resume_pc_slot = fault_resume_pc_slot,
        .arg0 = user_program_value(program, USER_PROGRAM_VALUE_ALIAS_VADDR),
        .trap_stack_base = pages->user_trap_stack_page,
        .trap_stack_size = MEMORY_PAGE_SIZE,
        .validate = supervisor_demo_smoke_user_ecall_validate,
        .validate_context = state,
        .supervisor_timer_post_handler =
            supervisor_runtime_timer_counter_post_handler,
        .supervisor_timer_post_context = &state->interrupts,
        .supervisor_external_post_handler =
            supervisor_runtime_external_counter_post_handler,
        .supervisor_external_post_context = &state->interrupts,
    };

    if (state == NULL || program == NULL || smoke == NULL ||
        trap_context == NULL || pages == NULL || pages->backing_page == NULL ||
        pages->remap_page == NULL || pages->nx_page == NULL ||
        pages->user_stack_page == NULL || pages->user_trap_stack_page == NULL ||
        rodata_marker == NULL || fault_resume_pc_slot == NULL) {
        return false;
    }

    return user_program_smoke_prepare_standard(smoke, program, &prepare);
}

static bool supervisor_demo_smoke_prepare_user_entry(
    supervisor_demo_smoke_state_t* state) {
    if (state == NULL || state->user_data_page == NULL) {
        return false;
    }

    supervisor_runtime_interrupt_state_reset_counters(&state->interrupts);
    state->user_ecall_seen = 0;
    state->user_data_page[2] = 0;
    state->user_data_page[3] = 0;
    state->user_data_page[4] = 0;
    return true;
}

static bool supervisor_demo_smoke_run_user_round(
    supervisor_demo_smoke_state_t* state,
    user_program_smoke_t* smoke,
    trap_context_t* expected_trap_context,
    trap_user_runtime_t* user_runtime,
    uint32_t* timer_signal_page,
    size_t timer_signal_index,
    uint32_t timer_signal_value,
    uint32_t* external_signal_page,
    size_t external_signal_index,
    uint32_t external_signal_value,
    uint64_t timer_delta) {
    const user_program_smoke_round_t round = {
        .expected_trap_context = expected_trap_context,
        .timer_signal_page = timer_signal_page,
        .timer_signal_index = timer_signal_index,
        .timer_signal_value = timer_signal_value,
        .external_signal_page = external_signal_page,
        .external_signal_index = external_signal_index,
        .external_signal_value = external_signal_value,
        .timer_delta = timer_delta,
    };

    if (state == NULL || smoke == NULL || expected_trap_context == NULL ||
        user_runtime == NULL || timer_signal_page == NULL || timer_delta == 0 ||
        !supervisor_demo_smoke_prepare_user_entry(state)) {
        return false;
    }

    if (!user_program_smoke_enter_round(smoke, &round)) {
        return false;
    }

    return supervisor_demo_smoke_verify_user_return(state, user_runtime) &&
           user_program_smoke_deactivate_supervisor_only(smoke,
                                                         expected_trap_context);
}

static bool supervisor_demo_smoke_run_user_program(
    supervisor_demo_smoke_state_t* state,
    user_program_smoke_t* smoke,
    trap_context_t* expected_trap_context,
    supervisor_demo_smoke_pages_t* pages,
    user_program_smoke_active_phase_t* active_phase,
    uint64_t timer_delta) {
    trap_user_runtime_t* user_runtime = NULL;

    if (state == NULL || smoke == NULL || expected_trap_context == NULL ||
        pages == NULL || pages->backing_page == NULL ||
        pages->remap_page == NULL || active_phase == NULL ||
        timer_delta == 0) {
        return false;
    }

    user_runtime = smoke->program != NULL ? user_program_runtime(smoke->program)
                                          : NULL;
    return user_runtime != NULL &&
           supervisor_demo_smoke_prime_active_pages(pages, active_phase) &&
           user_program_smoke_activate_supervisor_access(smoke,
                                                         expected_trap_context) &&
           active_user_program_state_ok(smoke, expected_trap_context) &&
           user_program_smoke_exercise_active_memory(smoke,
                                                     pages->backing_page,
                                                     pages->remap_page,
                                                     active_phase) &&
           supervisor_demo_smoke_run_user_round(
               state,
               smoke,
               expected_trap_context,
               user_runtime,
               pages->remap_page,
               SUPERVISOR_DEMO_SMOKE_TIMER_SIGNAL_INDEX,
               state->expected_user_timer,
               pages->remap_page,
               SUPERVISOR_DEMO_SMOKE_EXTERNAL_SIGNAL_INDEX,
               state->expected_user_external,
               timer_delta) &&
           supervisor_demo_smoke_run_user_round(
               state,
               smoke,
               expected_trap_context,
               user_runtime,
               pages->remap_page,
               SUPERVISOR_DEMO_SMOKE_TIMER_SIGNAL_INDEX,
               state->expected_user_timer,
               pages->remap_page,
               SUPERVISOR_DEMO_SMOKE_EXTERNAL_SIGNAL_INDEX,
               state->expected_user_external,
               timer_delta);
}

static bool supervisor_demo_smoke_read_storage_signature(uint8_t* storage_buffer) {
    return storage_buffer != NULL && storage_read_block(0, storage_buffer) == 0 &&
           storage_buffer[0] == 'S' && storage_buffer[1] == 't' &&
           storage_buffer[2] == 'o' && storage_buffer[3] == 'r';
}

static bool supervisor_demo_smoke_wait_platform_interrupts(
    supervisor_demo_smoke_state_t* state,
    uint64_t timer_delta) {
    return state != NULL &&
           supervisor_runtime_schedule_platform_interrupts_and_wait(
               &state->interrupts,
               timer_delta,
               SUPERVISOR_DEMO_SMOKE_INTERRUPT_TIMEOUT);
}

static bool supervisor_demo_smoke_run_platform_tail(
    supervisor_demo_smoke_state_t* state,
    uint64_t timer_delta) {
    uint8_t* storage_buffer = (uint8_t*)pmm_alloc_page();

    return storage_buffer != NULL && pmm_used_pages() >= 6U &&
           supervisor_demo_smoke_read_storage_signature(storage_buffer) &&
           supervisor_demo_smoke_wait_platform_interrupts(state, timer_delta);
}

static bool supervisor_demo_smoke_run_demo_session(
    supervisor_demo_smoke_state_t* state,
    user_program_smoke_t* smoke,
    trap_context_t* expected_trap_context,
    supervisor_demo_smoke_pages_t* pages,
    user_program_smoke_active_phase_t* active_phase,
    uint64_t timer_delta) {
    return supervisor_demo_smoke_run_user_program(state,
                                                  smoke,
                                                  expected_trap_context,
                                                  pages,
                                                  active_phase,
                                                  timer_delta) &&
           supervisor_demo_smoke_run_platform_tail(state, timer_delta);
}

static void supervisor_demo_smoke_external_post_handler(uint32_t source_id,
                                                        void* context) {
    supervisor_demo_smoke_state_t* state =
        (supervisor_demo_smoke_state_t*)context;

    if (state == NULL || source_id != PLIC_SOURCE_UART_THRE) {
        panic_shutdown();
    }

    platform_uart_disable_irq();
}

static bool supervisor_demo_smoke_user_ecall_validate(
    const trap_user_runtime_t* user_runtime,
    uint64_t epc,
    uint64_t tval,
    void* context) {
    supervisor_demo_smoke_state_t* state =
        (supervisor_demo_smoke_state_t*)context;

    (void)epc;
    if (tval != 0 || user_runtime == NULL || state == NULL ||
        state->user_data_page == NULL ||
        state->user_data_page[2] != state->expected_user_data ||
        !trap_user_runtime_timer_signal_delivered(user_runtime) ||
        state->user_data_page[3] != state->expected_user_timer ||
        !trap_user_runtime_external_signal_delivered(user_runtime) ||
        state->user_data_page[4] != state->expected_user_external) {
        return false;
    }

    state->user_ecall_seen = 1;
    return true;
}

static bool supervisor_demo_smoke_verify_user_return(
    const supervisor_demo_smoke_state_t* state,
    const trap_user_runtime_t* user_runtime) {
    return state != NULL && user_runtime != NULL &&
           state->user_data_page != NULL && state->user_ecall_seen &&
           state->interrupts.timer_interrupts != 0U &&
           state->user_data_page[2] == state->expected_user_data &&
           state->user_data_page[3] == state->expected_user_timer &&
           state->user_data_page[4] == state->expected_user_external &&
           trap_user_runtime_timer_signal_delivered(user_runtime) &&
           trap_user_runtime_external_signal_delivered(user_runtime) &&
           state->interrupts.external_interrupts != 0U &&
           (riscv_read_sstatus() & RISCV_SSTATUS_SPP) == 0;
}

static bool active_user_program_state_ok(user_program_smoke_t* smoke,
                                         trap_context_t* expected_trap_context) {
    user_program_t* program = NULL;

    if (smoke == NULL || expected_trap_context == NULL) {
        return false;
    }

    program = smoke->program;
    return program != NULL && user_program_is_active(program) &&
           runtime_context_active_process() == user_program_process(program) &&
           runtime_context_active_address_space() ==
               user_program_address_space(program) &&
           trap_active_context() == expected_trap_context;
}

bool supervisor_demo_smoke_run(trap_context_t* trap_context,
                               uintptr_t exec_symbol,
                               uintptr_t ecall_symbol) {
    user_program_t user_program;
    user_program_smoke_t user_program_smoke;
    user_program_smoke_active_phase_t active_phase;
    supervisor_demo_smoke_state_t demo_trap_state;
    supervisor_demo_smoke_pages_t demo_pages = {0};
    volatile uintptr_t instruction_fault_resume_pc = 0;
    void* early_allocation = NULL;

    if (trap_context == NULL || !trap_context_is_active(trap_context) ||
        trap_active_context() != trap_context) {
        return false;
    }

    early_allocation = memory_alloc(SUPERVISOR_DEMO_SMOKE_EARLY_ALLOC_SIZE,
                                    SUPERVISOR_DEMO_SMOKE_EARLY_ALLOC_ALIGN);
    user_program_init(&user_program);
    user_program_smoke_init(&user_program_smoke);
    supervisor_demo_smoke_init_active_phase(&active_phase,
                                            &supervisor_demo_rodata_marker,
                                            &instruction_fault_resume_pc);

    return supervisor_demo_smoke_prepare_bootstrap(
               &demo_trap_state,
               &demo_pages,
               trap_context,
               &user_program,
               early_allocation,
               &supervisor_demo_rodata_marker,
               exec_symbol,
               ecall_symbol,
               supervisor_demo_user_data_marker,
               supervisor_demo_user_timer_marker,
               supervisor_demo_user_external_marker) &&
           supervisor_demo_smoke_prepare_user_program(
               &demo_trap_state,
               &user_program,
               &user_program_smoke,
               trap_context,
               &demo_pages,
               &supervisor_demo_rodata_marker,
               &instruction_fault_resume_pc) &&
           supervisor_demo_smoke_run_demo_session(&demo_trap_state,
                                                  &user_program_smoke,
                                                  trap_context,
                                                  &demo_pages,
                                                  &active_phase,
                                                  SUPERVISOR_DEMO_SMOKE_TIMER_DELTA);
}
