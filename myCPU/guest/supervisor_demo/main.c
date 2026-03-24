#include <stdint.h>

#include "console.h"
#include "memory.h"
#include "panic.h"
#include "platform.h"
#include "pmm.h"
#include "riscv.h"
#include "runtime_context.h"
#include "storage.h"
#include "timer.h"
#include "trap.h"
#include "user_program.h"
#include "user_program_smoke.h"
#include "vm.h"

static volatile uintptr_t instruction_fault_resume_pc = 0;
static const uint32_t rodata_marker = 0xCAFEBABEU;
static const uint32_t user_data_marker = 0x5A5A1234U;
static const uint32_t user_timer_marker = 1U;

extern char user_test_entry[];
extern char user_test_ecall[];

struct DemoTrapState {
    volatile uint64_t timer_irq_seen;
    volatile uint64_t external_irq_seen;
    volatile uint64_t user_ecall_seen;
    uint32_t* user_data_page;
};

__attribute__((noinline)) static void provoke_rodata_store_fault(void) {
    volatile uint32_t* marker_ptr = (volatile uint32_t*)(uintptr_t)&rodata_marker;
    *marker_ptr = 0xDEADBEEFU;
}

__attribute__((noinline)) static void provoke_instruction_page_fault(uintptr_t target) {
    __asm__ volatile(
        "la t0, 1f\n"
        "la t1, instruction_fault_resume_pc\n"
        "sd t0, 0(t1)\n"
        "jalr ra, 0(%0)\n"
        "1:\n"
        "la t1, instruction_fault_resume_pc\n"
        "sd zero, 0(t1)\n"
        :
        : "r"(target)
        : "t0", "t1", "ra", "memory");
}

static void timer_interrupt_handler(uint64_t cause, void* context) {
    struct DemoTrapState* demo_state = (struct DemoTrapState*)context;

    if (cause != RISCV_SUPERVISOR_TIMER_INTERRUPT || demo_state == NULL) {
        panic_shutdown();
    }

    demo_state->timer_irq_seen = 1;
}

static void external_interrupt_handler(uint64_t cause,
                                       uint32_t source_id,
                                       void* context) {
    struct DemoTrapState* demo_state = (struct DemoTrapState*)context;

    if (cause != RISCV_SUPERVISOR_EXTERNAL_INTERRUPT || demo_state == NULL ||
        source_id != PLIC_SOURCE_UART_THRE) {
        panic_shutdown();
    }

    platform_uart_disable_irq();
    demo_state->external_irq_seen = 1;
}

static bool user_ecall_validate(const trap_user_runtime_t* user_runtime,
                                uint64_t epc,
                                uint64_t tval,
                                void* context) {
    struct DemoTrapState* demo_state = (struct DemoTrapState*)context;

    (void)epc;
    if (tval != 0 || user_runtime == NULL || demo_state == NULL ||
        demo_state->user_data_page == NULL ||
        demo_state->user_data_page[2] != user_data_marker ||
        !trap_user_runtime_timer_signal_delivered(user_runtime) ||
        demo_state->user_data_page[3] != user_timer_marker) {
        return false;
    }

    demo_state->user_ecall_seen = 1;
    return true;
}

void kernel_main(void) {
    uintptr_t early_cursor = 0;
    trap_context_t supervisor_trap_context;
    vm_address_space_t* user_address_space = NULL;
    vm_process_t* user_process = NULL;
    trap_user_runtime_t* user_runtime = NULL;
    user_program_t user_program;
    user_program_smoke_t user_program_smoke;
    uintptr_t user_base = 0;
    uintptr_t user_limit = 0;
    uintptr_t user_exec_page_paddr = 0;
    uintptr_t user_exec_vaddr = 0;
    uintptr_t user_stack_vaddr = 0;
    uintptr_t user_alias_vaddr = 0;
    uintptr_t user_anon_vaddr = 0;
    uintptr_t user_anon_tail_vaddr = 0;
    uintptr_t user_entry_pc = 0;
    uintptr_t user_expected_ecall_pc = 0;
    uintptr_t user_sp = 0;
    struct DemoTrapState demo_trap_state = {0};
    uint32_t* alias_page = NULL;
    uint32_t* anon_page = NULL;
    uint32_t* anon_tail_page = NULL;
    uint32_t* backing_page = NULL;
    uint32_t* remap_page = NULL;
    uint32_t* nx_page = NULL;
    uint32_t* user_stack_page = NULL;
    uint8_t* user_trap_stack_page = NULL;
    size_t lifecycle_free_before = 0;
    uint8_t* storage_buffer = NULL;
    uint8_t* storage_page = NULL;
    void* early_allocation = NULL;

    memory_init();
    early_allocation = memory_alloc(96, 64);
    platform_plic_supervisor_init();
    runtime_context_reset();
    trap_context_init(&supervisor_trap_context);
    user_program_init(&user_program);
    user_program_smoke_init(&user_program_smoke);
    if (!trap_context_activate(&supervisor_trap_context) ||
        !trap_context_is_active(&supervisor_trap_context) ||
        trap_active_context() != &supervisor_trap_context) {
        panic_shutdown();
    }
    user_base = vm_user_base();
    user_limit = vm_user_limit();
    if (!user_program_plan_standard(&user_program,
                                    (uintptr_t)user_test_entry,
                                    (uintptr_t)user_test_ecall)) {
        panic_shutdown();
    }
    user_exec_page_paddr =
        user_program_value(&user_program, USER_PROGRAM_VALUE_EXEC_PAGE_PADDR);
    user_exec_vaddr =
        user_program_value(&user_program, USER_PROGRAM_VALUE_EXEC_VADDR);
    user_stack_vaddr =
        user_program_value(&user_program, USER_PROGRAM_VALUE_STACK_VADDR);
    user_alias_vaddr =
        user_program_value(&user_program, USER_PROGRAM_VALUE_ALIAS_VADDR);
    user_anon_vaddr =
        user_program_value(&user_program, USER_PROGRAM_VALUE_ANON_VADDR);
    user_anon_tail_vaddr =
        user_program_value(&user_program, USER_PROGRAM_VALUE_ANON_TAIL_VADDR);
    user_entry_pc =
        user_program_value(&user_program, USER_PROGRAM_VALUE_ENTRY_PC);
    user_expected_ecall_pc = user_program_value(
        &user_program, USER_PROGRAM_VALUE_EXPECTED_ECALL_PC);
    user_sp = user_program_value(&user_program, USER_PROGRAM_VALUE_USER_SP);

    if (memory_kernel_start() != MEM_BASE ||
        memory_text_start() != memory_kernel_start() ||
        memory_text_start() >= memory_text_end() ||
        memory_text_end() > memory_rodata_start() ||
        memory_rodata_start() > memory_rodata_end() ||
        memory_rodata_end() > memory_data_start() ||
        memory_data_start() > memory_data_end() ||
        memory_data_end() > memory_bss_start() ||
        memory_bss_start() > memory_bss_end() ||
        memory_bss_end() > memory_heap_start() ||
        memory_heap_start() < memory_kernel_end() ||
        memory_heap_limit() != MEM_BASE + MEM_SIZE ||
        ((uintptr_t)&rodata_marker) < memory_rodata_start() ||
        ((uintptr_t)&rodata_marker) >= memory_rodata_end() ||
        early_allocation == NULL ||
        user_base != 0 ||
        user_limit != memory_kernel_start() ||
        user_anon_tail_vaddr < user_base ||
        user_anon_tail_vaddr >= user_limit ||
        user_stack_vaddr < user_base ||
        user_stack_vaddr >= user_limit ||
        user_exec_vaddr < user_base ||
        user_exec_vaddr >= user_limit ||
        user_alias_vaddr < user_base ||
        user_alias_vaddr >= user_limit ||
        (user_alias_vaddr & (MEMORY_PAGE_SIZE - 1U)) != 0 ||
        (user_anon_tail_vaddr & (MEMORY_PAGE_SIZE - 1U)) != 0 ||
        (user_stack_vaddr & (MEMORY_PAGE_SIZE - 1U)) != 0 ||
        (user_exec_vaddr & (MEMORY_PAGE_SIZE - 1U)) != 0 ||
        user_sp != user_stack_vaddr + MEMORY_PAGE_SIZE ||
        user_exec_page_paddr < memory_text_start() ||
        user_exec_page_paddr >= memory_text_end() ||
        user_entry_pc < user_exec_vaddr ||
        user_entry_pc >= user_exec_vaddr + MEMORY_PAGE_SIZE ||
        user_expected_ecall_pc < user_exec_vaddr ||
        user_expected_ecall_pc >= user_exec_vaddr + MEMORY_PAGE_SIZE ||
        (((uintptr_t)early_allocation) & 63U) != 0 ||
        (memory_text_start() & (MEMORY_PAGE_SIZE - 1U)) != 0 ||
        (memory_text_end() & (MEMORY_PAGE_SIZE - 1U)) != 0 ||
        (memory_rodata_start() & (MEMORY_PAGE_SIZE - 1U)) != 0 ||
        (memory_rodata_end() & (MEMORY_PAGE_SIZE - 1U)) != 0 ||
        (memory_data_start() & (MEMORY_PAGE_SIZE - 1U)) != 0 ||
        (memory_bss_end() & (MEMORY_PAGE_SIZE - 1U)) != 0 ||
        (memory_heap_start() & (MEMORY_PAGE_SIZE - 1U)) != 0) {
        panic_shutdown();
    }
    early_cursor = memory_heap_current();
    pmm_init();

    if (pmm_managed_start() != memory_heap_current() ||
        pmm_managed_start() < early_cursor ||
        pmm_managed_start() >= pmm_managed_end() ||
        pmm_managed_end() != memory_heap_limit() ||
        pmm_total_pages() == 0 ||
        pmm_total_pages() !=
            (size_t)((pmm_managed_end() - pmm_managed_start()) /
                     MEMORY_PAGE_SIZE) ||
        pmm_free_pages() != pmm_total_pages() ||
        pmm_used_pages() != 0 ||
        memory_alloc(16, 8) != NULL ||
        pmm_free_page((void*)memory_kernel_start())) {
        panic_shutdown();
    }
    storage_page = (uint8_t*)pmm_alloc_page();
    storage_buffer = storage_page;
    if (storage_page == NULL ||
        (((uintptr_t)storage_page) & (MEMORY_PAGE_SIZE - 1U)) != 0 ||
        pmm_free_pages() + 1U != pmm_total_pages() ||
        pmm_used_pages() != 1U ||
        !pmm_free_page(storage_page) ||
        pmm_free_page(storage_page) ||
        pmm_free_pages() != pmm_total_pages() ||
        pmm_used_pages() != 0U) {
        panic_shutdown();
    }
    lifecycle_free_before = pmm_free_pages();
    if (!user_program_smoke_validate_vm_lifecycle(user_anon_vaddr,
                                                  lifecycle_free_before)) {
        panic_shutdown();
    }

    backing_page = (uint32_t*)pmm_alloc_page();
    remap_page = (uint32_t*)pmm_alloc_page();
    nx_page = (uint32_t*)pmm_alloc_page();
    user_stack_page = (uint32_t*)pmm_alloc_page();
    user_trap_stack_page = (uint8_t*)pmm_alloc_page();
    demo_trap_state.user_data_page = remap_page;
    if (backing_page == NULL ||
        remap_page == NULL ||
        nx_page == NULL ||
        user_stack_page == NULL ||
        user_trap_stack_page == NULL ||
        (((uintptr_t)user_trap_stack_page) &
         (TRAP_USER_RUNTIME_STACK_ALIGNMENT - 1U)) != 0 ||
        pmm_free_pages() + 5U != pmm_total_pages() ||
        pmm_used_pages() != 5U) {
        panic_shutdown();
    }

    if (!user_program_create(&user_program,
                             (uintptr_t)backing_page,
                             (uintptr_t)user_stack_page)) {
        panic_shutdown();
    }
    user_address_space = user_program_address_space(&user_program);
    user_process = user_program_process(&user_program);
    user_runtime = user_program_runtime(&user_program);

    if (user_address_space == NULL ||
        user_process == NULL ||
        user_runtime == NULL ||
        user_program_region(&user_program, USER_PROGRAM_REGION_STACK) == NULL ||
        user_program_region(&user_program, USER_PROGRAM_REGION_ALIAS) == NULL ||
        user_program_region(&user_program, USER_PROGRAM_REGION_ANON) == NULL ||
        user_program_region(&user_program,
                            USER_PROGRAM_REGION_ANON_TAIL) == NULL ||
        user_program_object(&user_program, USER_PROGRAM_OBJECT_ANON) == NULL ||
        vm_address_space_root_table(user_address_space) == 0 ||
        (vm_address_space_root_table(user_address_space) &
         (MEMORY_PAGE_SIZE - 1U)) != 0 ||
        vm_address_space_satp_value(user_address_space) !=
            (RISCV_SATP_MODE_SV39 |
             ((uint64_t)vm_address_space_root_table(user_address_space) >>
              12)) ||
        vm_kernel_base() != memory_kernel_start() ||
        vm_kernel_limit() != memory_heap_limit() ||
        vm_address_space_is_active(user_address_space) ||
        !vm_range_is_user(user_alias_vaddr, MEMORY_PAGE_SIZE) ||
        vm_range_is_user(memory_text_start(), MEMORY_PAGE_SIZE) ||
        !vm_range_is_kernel(memory_text_start(),
                            memory_text_end() - memory_text_start()) ||
        vm_range_is_kernel(user_alias_vaddr, MEMORY_PAGE_SIZE) ||
        !user_program_region_contains(&user_program,
                                      USER_PROGRAM_REGION_STACK,
                                      user_sp - 16U,
                                      16U) ||
        !user_program_region_contains(&user_program,
                                      USER_PROGRAM_REGION_ALIAS,
                                      user_alias_vaddr,
                                      MEMORY_PAGE_SIZE) ||
        user_program_region_contains(&user_program,
                                     USER_PROGRAM_REGION_ALIAS,
                                     user_alias_vaddr - MEMORY_PAGE_SIZE,
                                     MEMORY_PAGE_SIZE) ||
        !user_program_smoke_prepare_address_space(
            &user_program_smoke,
            &user_program,
            (uintptr_t)backing_page,
            (uintptr_t)remap_page,
            (uintptr_t)&rodata_marker,
            sizeof(rodata_marker),
            (uintptr_t)nx_page,
            MEMORY_PAGE_SIZE,
            &instruction_fault_resume_pc) ||
        !user_program_smoke_prepare_runtime(
            &user_program_smoke,
            &supervisor_trap_context,
            user_alias_vaddr,
            user_trap_stack_page,
            MEMORY_PAGE_SIZE,
            user_ecall_validate,
            &demo_trap_state,
            timer_interrupt_handler,
            &demo_trap_state,
            external_interrupt_handler,
            &demo_trap_state)) {
        panic_shutdown();
    }

    backing_page[0] = 0x11223344U;
    remap_page[0] = 0xA1B2C3D4U;
    remap_page[1] = 0x01020304U;
    remap_page[2] = 0U;
    remap_page[3] = 0U;
    nx_page[0] = 0x00008067U;
    if (!user_program_smoke_activate_supervisor_access(
            &user_program_smoke, &supervisor_trap_context) ||
        !user_program_is_active(&user_program) ||
        runtime_context_active_process() != user_process ||
        runtime_context_active_address_space() != user_address_space) {
        panic_shutdown();
    }
    alias_page = (uint32_t*)user_alias_vaddr;
    if (alias_page[0] != 0x11223344U) {
        panic_shutdown();
    }
    alias_page[1] = 0x55667788U;
    if (backing_page[1] != 0x55667788U) {
        panic_shutdown();
    }
    backing_page[2] = 0x99AABBCCU;
    if (backing_page[2] != 0x99AABBCCU || rodata_marker != 0xCAFEBABEU) {
        panic_shutdown();
    }
    if (user_program_reset_object(&user_program, USER_PROGRAM_OBJECT_ANON)) {
        panic_shutdown();
    }
    anon_page = (uint32_t*)user_anon_vaddr;
    anon_tail_page = (uint32_t*)user_anon_tail_vaddr;
    if (anon_page[0] != 0U || anon_page[1] != 0U ||
        anon_tail_page[0] != 0U || anon_tail_page[1] != 0U) {
        panic_shutdown();
    }
    anon_page[0] = 0x13579BDFU;
    anon_page[1] = 0x02468ACEU;
    anon_tail_page[0] = 0x2468ACE0U;
    anon_tail_page[1] = 0x10203040U;
    if (!user_program_unmap_region_base_page(&user_program,
                                             USER_PROGRAM_REGION_ANON) ||
        !user_program_unmap_region_base_page(&user_program,
                                             USER_PROGRAM_REGION_ANON_TAIL) ||
        anon_page[0] != 0x13579BDFU ||
        anon_page[1] != 0x02468ACEU ||
        anon_tail_page[0] != 0x2468ACE0U ||
        anon_tail_page[1] != 0x10203040U) {
        panic_shutdown();
    }

    if (!user_program_smoke_rebind_alias_fault_object(&user_program_smoke)) {
        panic_shutdown();
    }
    if (alias_page[0] != 0xA1B2C3D4U) {
        panic_shutdown();
    }
    if (backing_page[0] != 0x11223344U) {
        panic_shutdown();
    }
    alias_page[1] = 0x77665544U;
    if (remap_page[1] != 0x77665544U || backing_page[1] != 0x55667788U) {
        panic_shutdown();
    }
    provoke_rodata_store_fault();
    if (rodata_marker != 0xCAFEBABEU) {
        panic_shutdown();
    }
    provoke_instruction_page_fault((uintptr_t)nx_page);
    if (instruction_fault_resume_pc != 0) {
        panic_shutdown();
    }
    demo_trap_state.timer_irq_seen = 0;
    demo_trap_state.user_ecall_seen = 0;
    if (!user_program_smoke_enter_with_timer_signal(&user_program_smoke,
                                                    remap_page,
                                                    3U,
                                                    user_timer_marker,
                                                    8U)) {
        panic_shutdown();
    }
    if (!demo_trap_state.user_ecall_seen ||
        remap_page[2] != user_data_marker ||
        remap_page[3] != user_timer_marker ||
        !trap_user_runtime_timer_signal_delivered(user_runtime) ||
        (riscv_read_sstatus() & RISCV_SSTATUS_SPP) != 0) {
        panic_shutdown();
    }
    storage_page = (uint8_t*)pmm_alloc_page();
    storage_buffer = storage_page;
    if (storage_page == NULL || pmm_used_pages() < 6U) {
        panic_shutdown();
    }

    if (storage_read_block(0, storage_buffer) != 0) {
        panic_shutdown();
    }

    if (storage_buffer[0] != 'S' ||
        storage_buffer[1] != 't' ||
        storage_buffer[2] != 'o' ||
        storage_buffer[3] != 'r') {
        panic_shutdown();
    }
    demo_trap_state.timer_irq_seen = 0;
    timer_schedule_delta(8);
    platform_uart_enable_thre_irq();

    while (!demo_trap_state.timer_irq_seen ||
           !demo_trap_state.external_irq_seen) {
    }

    console_puts("KRN");
    platform_shutdown(0);
}
