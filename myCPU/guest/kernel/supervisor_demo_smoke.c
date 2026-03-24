#include "supervisor_demo_smoke.h"

#include "memory.h"
#include "panic.h"
#include "platform.h"
#include "pmm.h"
#include "riscv.h"
#include "storage.h"
#include "timer.h"

void supervisor_demo_smoke_init(supervisor_demo_smoke_state_t* state,
                                uint32_t* user_data_page,
                                uint32_t expected_user_data,
                                uint32_t expected_user_timer,
                                uint32_t expected_user_external) {
    if (state == NULL) {
        return;
    }

    state->timer_irq_seen = 0;
    state->external_irq_seen = 0;
    state->user_ecall_seen = 0;
    state->user_data_page = user_data_page;
    state->expected_user_data = expected_user_data;
    state->expected_user_timer = expected_user_timer;
    state->expected_user_external = expected_user_external;
}

void supervisor_demo_smoke_init_active_phase(
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

bool supervisor_demo_smoke_validate_memory_layout(
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

bool supervisor_demo_smoke_validate_pmm_setup(uintptr_t early_cursor) {
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

bool supervisor_demo_smoke_probe_storage_page(void) {
    uint8_t* storage_page = (uint8_t*)pmm_alloc_page();

    return storage_page != NULL &&
           (((uintptr_t)storage_page) & (MEMORY_PAGE_SIZE - 1U)) == 0 &&
           pmm_free_pages() + 1U == pmm_total_pages() &&
           pmm_used_pages() == 1U && pmm_free_page(storage_page) &&
           !pmm_free_page(storage_page) &&
           pmm_free_pages() == pmm_total_pages() && pmm_used_pages() == 0U;
}

bool supervisor_demo_smoke_alloc_pages(supervisor_demo_smoke_pages_t* pages) {
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

bool supervisor_demo_smoke_prime_active_pages(
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

bool supervisor_demo_smoke_prepare_user_entry(
    supervisor_demo_smoke_state_t* state) {
    if (state == NULL) {
        return false;
    }

    state->timer_irq_seen = 0;
    state->external_irq_seen = 0;
    state->user_ecall_seen = 0;
    return true;
}

void supervisor_demo_smoke_timer_interrupt_handler(uint64_t cause,
                                                   void* context) {
    supervisor_demo_smoke_state_t* state =
        (supervisor_demo_smoke_state_t*)context;

    if (cause != RISCV_SUPERVISOR_TIMER_INTERRUPT || state == NULL) {
        panic_shutdown();
    }

    state->timer_irq_seen = 1;
}

void supervisor_demo_smoke_external_interrupt_handler(uint64_t cause,
                                                      uint32_t source_id,
                                                      void* context) {
    supervisor_demo_smoke_state_t* state =
        (supervisor_demo_smoke_state_t*)context;

    if (cause != RISCV_SUPERVISOR_EXTERNAL_INTERRUPT || state == NULL ||
        source_id != PLIC_SOURCE_UART_THRE) {
        panic_shutdown();
    }

    platform_uart_disable_irq();
    state->external_irq_seen = 1;
}

bool supervisor_demo_smoke_user_ecall_validate(
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

bool supervisor_demo_smoke_verify_user_return(
    const supervisor_demo_smoke_state_t* state,
    const trap_user_runtime_t* user_runtime) {
    return state != NULL && user_runtime != NULL &&
           state->user_data_page != NULL && state->user_ecall_seen &&
           state->user_data_page[2] == state->expected_user_data &&
           state->user_data_page[3] == state->expected_user_timer &&
           state->user_data_page[4] == state->expected_user_external &&
           trap_user_runtime_timer_signal_delivered(user_runtime) &&
           trap_user_runtime_external_signal_delivered(user_runtime) &&
           state->external_irq_seen &&
           (riscv_read_sstatus() & RISCV_SSTATUS_SPP) == 0;
}

bool supervisor_demo_smoke_read_storage_signature(uint8_t* storage_buffer) {
    return storage_buffer != NULL && storage_read_block(0, storage_buffer) == 0 &&
           storage_buffer[0] == 'S' && storage_buffer[1] == 't' &&
           storage_buffer[2] == 'o' && storage_buffer[3] == 'r';
}

bool supervisor_demo_smoke_wait_platform_interrupts(
    supervisor_demo_smoke_state_t* state,
    uint64_t timer_delta) {
    if (state == NULL || timer_delta == 0) {
        return false;
    }

    state->timer_irq_seen = 0;
    state->external_irq_seen = 0;
    timer_schedule_delta(timer_delta);
    platform_uart_enable_thre_irq();

    while (!state->timer_irq_seen || !state->external_irq_seen) {
    }

    return true;
}

bool supervisor_demo_smoke_run_platform_tail(
    supervisor_demo_smoke_state_t* state,
    uint64_t timer_delta) {
    uint8_t* storage_buffer = (uint8_t*)pmm_alloc_page();

    return storage_buffer != NULL && pmm_used_pages() >= 6U &&
           supervisor_demo_smoke_read_storage_signature(storage_buffer) &&
           supervisor_demo_smoke_wait_platform_interrupts(state, timer_delta);
}
