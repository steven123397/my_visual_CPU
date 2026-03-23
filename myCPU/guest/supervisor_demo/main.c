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
#include "vm.h"

static volatile uintptr_t instruction_fault_resume_pc = 0;
static const uint32_t rodata_marker = 0xCAFEBABEU;
static const uint32_t user_data_marker = 0x5A5A1234U;
static const uint32_t user_timer_marker = 1U;

extern void riscv_enter_user_mode(uintptr_t entry, uintptr_t arg0, uintptr_t user_sp);
extern void supervisor_resume_after_user(void);
extern char user_test_entry[];
extern char user_test_ecall[];

struct DemoTrapState {
    volatile uint64_t timer_irq_seen;
    volatile uint64_t external_irq_seen;
    volatile uint64_t user_ecall_seen;
    uint32_t* user_data_page;
};

static uintptr_t align_down_page(uintptr_t value) {
    return value & ~((uintptr_t)MEMORY_PAGE_SIZE - 1U);
}

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
    trap_user_runtime_t user_runtime;
    vm_address_space_t* lifecycle_space = NULL;
    vm_address_space_t* recycled_space = NULL;
    vm_address_space_t* user_address_space = NULL;
    vm_process_t lifecycle_process = {0};
    vm_process_t user_process = {0};
    uintptr_t user_base = 0;
    uintptr_t user_limit = 0;
    uintptr_t alias_vaddr = 0;
    uintptr_t lifecycle_vaddr = 0;
    uintptr_t user_exec_vaddr = 0;
    uintptr_t user_stack_vaddr = 0;
    uintptr_t user_exec_pa = 0;
    uintptr_t user_entry_va = 0;
    uintptr_t user_ecall_va = 0;
    uintptr_t user_stack_top = 0;
    const uint64_t user_rw_flags =
        VM_PAGE_READ | VM_PAGE_WRITE | VM_PAGE_USER;
    const uint64_t user_rx_flags =
        VM_PAGE_READ | VM_PAGE_EXEC | VM_PAGE_USER;
    vm_user_region_t lifecycle_region = {0};
    vm_user_region_t alias_region = {0};
    vm_user_region_t remap_region = {0};
    vm_user_region_t user_exec_region = {0};
    vm_user_region_t user_stack_region = {0};
    vm_user_region_t invalid_region = {0};
    vm_object_t lifecycle_object = {0};
    vm_object_t alias_object = {0};
    vm_object_t remap_object = {0};
    vm_object_t user_exec_object = {0};
    vm_object_t user_stack_object = {0};
    struct DemoTrapState demo_trap_state = {0};
    uint32_t* alias_page = NULL;
    uint32_t* backing_page = NULL;
    uint32_t* remap_page = NULL;
    uint32_t* nx_page = NULL;
    uint32_t* user_stack_page = NULL;
    size_t lifecycle_free_before = 0;
    uint8_t* storage_buffer = NULL;
    uint8_t* storage_page = NULL;
    void* early_allocation = NULL;

    memory_init();
    early_allocation = memory_alloc(96, 64);
    platform_plic_supervisor_init();
    runtime_context_reset();
    trap_context_init(&supervisor_trap_context);
    trap_user_runtime_init(&user_runtime);
    if (!trap_context_activate(&supervisor_trap_context) ||
        !trap_context_is_active(&supervisor_trap_context) ||
        trap_active_context() != &supervisor_trap_context) {
        panic_shutdown();
    }
    user_base = vm_user_base();
    user_limit = vm_user_limit();
    alias_vaddr = user_limit - MEMORY_PAGE_SIZE;
    lifecycle_vaddr = alias_vaddr - 4U * MEMORY_PAGE_SIZE;
    user_exec_vaddr = alias_vaddr - 2U * MEMORY_PAGE_SIZE;
    user_stack_vaddr = alias_vaddr - 3U * MEMORY_PAGE_SIZE;
    user_exec_pa = align_down_page((uintptr_t)user_test_entry);
    user_entry_va =
        user_exec_vaddr + ((uintptr_t)user_test_entry - user_exec_pa);
    user_ecall_va =
        user_exec_vaddr + ((uintptr_t)user_test_ecall - user_exec_pa);
    user_stack_top = user_stack_vaddr + MEMORY_PAGE_SIZE;

    if (!trap_context_install_supervisor_timer_policy(
            &supervisor_trap_context,
            timer_interrupt_handler,
            &demo_trap_state) ||
        !trap_context_install_supervisor_external_policy(
            &supervisor_trap_context,
            external_interrupt_handler,
            &demo_trap_state)) {
        panic_shutdown();
    }

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
        user_stack_vaddr < user_base ||
        user_stack_vaddr >= user_limit ||
        user_exec_vaddr < user_base ||
        user_exec_vaddr >= user_limit ||
        alias_vaddr < user_base ||
        alias_vaddr >= user_limit ||
        !vm_range_is_user(user_stack_vaddr, MEMORY_PAGE_SIZE) ||
        !vm_range_is_user(user_exec_vaddr, MEMORY_PAGE_SIZE) ||
        (alias_vaddr & (MEMORY_PAGE_SIZE - 1U)) != 0 ||
        (user_stack_vaddr & (MEMORY_PAGE_SIZE - 1U)) != 0 ||
        (user_exec_vaddr & (MEMORY_PAGE_SIZE - 1U)) != 0 ||
        user_stack_top != user_stack_vaddr + MEMORY_PAGE_SIZE ||
        user_exec_pa < memory_text_start() ||
        user_exec_pa >= memory_text_end() ||
        user_entry_va < user_exec_vaddr ||
        user_entry_va >= user_exec_vaddr + MEMORY_PAGE_SIZE ||
        user_ecall_va < user_exec_vaddr ||
        user_ecall_va >= user_exec_vaddr + MEMORY_PAGE_SIZE ||
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
    if (!vm_address_space_create(&lifecycle_space) ||
        lifecycle_space == NULL ||
        !vm_process_create(&lifecycle_process, lifecycle_space) ||
        !vm_address_space_activate(lifecycle_space) ||
        !vm_address_space_is_active(lifecycle_space) ||
        runtime_context_active_address_space() != lifecycle_space ||
        !vm_address_space_disable(lifecycle_space) ||
        vm_address_space_is_active(lifecycle_space) ||
        runtime_context_active_address_space() != NULL ||
        !vm_address_space_map_identity_1g(lifecycle_space,
                                          0,
                                          VM_PAGE_READ | VM_PAGE_WRITE |
                                              VM_PAGE_EXEC) ||
        !vm_process_user_region_init(&lifecycle_process,
                                     &lifecycle_region,
                                     lifecycle_vaddr,
                                     MEMORY_PAGE_SIZE,
                                     user_rw_flags) ||
        vm_process_set_user_context(&lifecycle_process,
                                    lifecycle_vaddr,
                                    lifecycle_vaddr + MEMORY_PAGE_SIZE) ||
        !vm_object_init_physical(&lifecycle_object,
                                 memory_data_start(),
                                 MEMORY_PAGE_SIZE) ||
        vm_object_init_physical(&lifecycle_object,
                                memory_data_start(),
                                MEMORY_PAGE_SIZE) ||
        !vm_user_region_map_object(&lifecycle_region, &lifecycle_object) ||
        vm_user_region_map_object(&lifecycle_region, &lifecycle_object) ||
        vm_user_region_set_fault_object(&lifecycle_region, &lifecycle_object) ||
        !vm_user_region_clear_object(&lifecycle_region) ||
        !vm_user_region_set_fault_object(&lifecycle_region, &lifecycle_object) ||
        vm_user_region_map_object(&lifecycle_region, &lifecycle_object) ||
        !vm_user_region_clear_object(&lifecycle_region) ||
        vm_address_space_destroy(lifecycle_space) ||
        !vm_process_remove_user_region(&lifecycle_process, &lifecycle_region) ||
        lifecycle_region.registered ||
        lifecycle_region.address_space != NULL ||
        lifecycle_region.object != NULL ||
        vm_process_create(&lifecycle_process, lifecycle_space)) {
        panic_shutdown();
    }
    if (!vm_process_reset(&lifecycle_process)) {
        panic_shutdown();
    }
    vm_object_reset(&lifecycle_object);
    if (lifecycle_process.address_space != NULL ||
        lifecycle_process.entry_pc != 0 ||
        lifecycle_process.user_sp != 0 ||
        lifecycle_process.user_regions[0] != NULL ||
        lifecycle_object.initialized ||
        !vm_object_init_physical(&lifecycle_object,
                                 memory_data_start(),
                                 MEMORY_PAGE_SIZE) ||
        !vm_address_space_destroy(lifecycle_space) ||
        !vm_address_space_create(&lifecycle_space) ||
        !vm_address_space_create(&recycled_space) ||
        !vm_address_space_destroy(lifecycle_space) ||
        !vm_address_space_destroy(recycled_space) ||
        pmm_free_pages() != lifecycle_free_before) {
        panic_shutdown();
    }
    vm_object_reset(&lifecycle_object);

    backing_page = (uint32_t*)pmm_alloc_page();
    remap_page = (uint32_t*)pmm_alloc_page();
    nx_page = (uint32_t*)pmm_alloc_page();
    user_stack_page = (uint32_t*)pmm_alloc_page();
    demo_trap_state.user_data_page = remap_page;
    if (backing_page == NULL ||
        remap_page == NULL ||
        nx_page == NULL ||
        user_stack_page == NULL ||
        pmm_free_pages() + 4U != pmm_total_pages() ||
        pmm_used_pages() != 4U ||
        !vm_address_space_create(&user_address_space) ||
        user_address_space == NULL ||
        !vm_process_create(&user_process, user_address_space) ||
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
        !vm_range_is_user(alias_vaddr, MEMORY_PAGE_SIZE) ||
        vm_range_is_user(memory_text_start(), MEMORY_PAGE_SIZE) ||
        !vm_range_is_kernel(memory_text_start(),
                            memory_text_end() - memory_text_start()) ||
        vm_range_is_kernel(alias_vaddr, MEMORY_PAGE_SIZE) ||
        !vm_address_space_map_identity_1g(user_address_space,
                                          0,
                                          VM_PAGE_READ | VM_PAGE_WRITE |
                                              VM_PAGE_EXEC) ||
        !vm_process_user_region_init(&user_process,
                                     &alias_region,
                                     alias_vaddr,
                                     MEMORY_PAGE_SIZE,
                                     user_rw_flags) ||
        !vm_process_user_region_init(&user_process,
                                     &user_exec_region,
                                     user_exec_vaddr,
                                     MEMORY_PAGE_SIZE,
                                     user_rx_flags) ||
        !vm_process_user_region_init(&user_process,
                                     &user_stack_region,
                                     user_stack_vaddr,
                                     MEMORY_PAGE_SIZE,
                                     user_rw_flags) ||
        !vm_object_init_physical(&user_exec_object,
                                 user_exec_pa,
                                 MEMORY_PAGE_SIZE) ||
        !vm_object_init_physical(&user_stack_object,
                                 (uintptr_t)user_stack_page,
                                 MEMORY_PAGE_SIZE) ||
        !vm_object_init_physical(&alias_object,
                                 (uintptr_t)backing_page,
                                 MEMORY_PAGE_SIZE) ||
        !vm_object_init_physical(&remap_object,
                                 (uintptr_t)remap_page,
                                 MEMORY_PAGE_SIZE) ||
        !vm_user_region_map_object(&user_exec_region, &user_exec_object) ||
        !vm_user_region_map_object(&user_stack_region, &user_stack_object) ||
        !vm_user_region_contains(&user_stack_region, user_stack_top - 16U, 16U) ||
        !vm_user_region_contains(&alias_region, alias_vaddr, MEMORY_PAGE_SIZE) ||
        vm_user_region_contains(&alias_region,
                                alias_vaddr - MEMORY_PAGE_SIZE,
                                MEMORY_PAGE_SIZE) ||
        vm_process_user_region_init(&user_process,
                                    &invalid_region,
                                    0,
                                    MEMORY_PAGE_SIZE,
                                    user_rw_flags) ||
        vm_address_space_map_kernel_range(user_address_space,
                                          alias_vaddr,
                                          (uintptr_t)remap_page,
                                          MEMORY_PAGE_SIZE,
                                          VM_PAGE_WRITE) ||
        vm_address_space_map_kernel_range(user_address_space,
                                          memory_text_start(),
                                          memory_text_start(),
                                          memory_text_end() -
                                              memory_text_start(),
                                          VM_PAGE_READ | VM_PAGE_EXEC |
                                              VM_PAGE_USER) ||
        vm_process_user_region_init(&user_process,
                                    &invalid_region,
                                    memory_text_start(),
                                    memory_text_end() - memory_text_start(),
                                    VM_PAGE_READ | VM_PAGE_EXEC |
                                        VM_PAGE_USER) ||
        !vm_address_space_map_kernel_range(user_address_space,
                                           memory_text_start(),
                                           memory_text_start(),
                                           memory_text_end() -
                                               memory_text_start(),
                                           VM_PAGE_READ | VM_PAGE_EXEC) ||
        !vm_address_space_map_kernel_range(user_address_space,
                                           memory_rodata_start(),
                                           memory_rodata_start(),
                                           memory_rodata_end() -
                                               memory_rodata_start(),
                                           VM_PAGE_READ) ||
        !vm_address_space_map_kernel_range(user_address_space,
                                           memory_data_start(),
                                           memory_data_start(),
                                           pmm_managed_start() -
                                               memory_data_start(),
                                           VM_PAGE_READ | VM_PAGE_WRITE) ||
        !vm_address_space_map_kernel_range(user_address_space,
                                           pmm_managed_start(),
                                           pmm_managed_start(),
                                           pmm_managed_end() -
                                               pmm_managed_start(),
                                           VM_PAGE_READ | VM_PAGE_WRITE) ||
        vm_address_space_register_fault_range(user_address_space,
                                              alias_vaddr + 1U,
                                              (uintptr_t)backing_page,
                                              MEMORY_PAGE_SIZE,
                                              VM_PAGE_READ) ||
        vm_user_region_unmap_page(&alias_region, memory_text_start()) ||
        !vm_user_region_map_object(&alias_region, &alias_object) ||
        vm_address_space_register_fault_range(user_address_space,
                                              alias_vaddr,
                                              (uintptr_t)remap_page,
                                              MEMORY_PAGE_SIZE,
                                              VM_PAGE_READ |
                                                  VM_PAGE_WRITE) ||
        vm_user_region_set_fault_object(&alias_region, &remap_object) ||
        vm_user_region_set_fault_object(&invalid_region, &remap_object) ||
        !vm_address_space_register_fault_skip(user_address_space,
                                              RISCV_EXC_STORE_PAGE_FAULT,
                                              (uintptr_t)&rodata_marker,
                                              sizeof(rodata_marker)) ||
        !vm_address_space_register_fault_resume_slot(
            user_address_space,
            RISCV_EXC_INSN_PAGE_FAULT,
            (uintptr_t)nx_page,
            MEMORY_PAGE_SIZE,
            &instruction_fault_resume_pc) ||
        !vm_process_user_region_init(&user_process,
                                     &remap_region,
                                     alias_vaddr - MEMORY_PAGE_SIZE,
                                     MEMORY_PAGE_SIZE,
                                     user_rw_flags) ||
        vm_process_user_region_init(&user_process,
                                    &invalid_region,
                                    alias_vaddr,
                                    MEMORY_PAGE_SIZE,
                                    user_rw_flags) ||
        vm_process_is_active(&user_process) ||
        !vm_user_region_map_object(&remap_region, &remap_object) ||
        !vm_user_region_unmap_page(&remap_region,
                                   alias_vaddr - MEMORY_PAGE_SIZE) ||
        vm_process_is_runnable(&user_process) ||
        !vm_process_set_user_context(&user_process,
                                     user_entry_va,
                                     user_stack_top) ||
        !trap_user_runtime_bind(&user_runtime,
                                &supervisor_trap_context,
                                &user_process,
                                alias_vaddr) ||
        !trap_user_runtime_configure_ecall_resume(
            &user_runtime,
            user_ecall_va,
            (uintptr_t)supervisor_resume_after_user,
            user_ecall_validate,
            &demo_trap_state) ||
        !trap_context_bind_supervisor_timer_user_runtime(
            &supervisor_trap_context,
            &user_runtime) ||
        !trap_context_install_user_runtime_resume_policy(
            &supervisor_trap_context,
            &user_runtime) ||
        !vm_process_is_runnable(&user_process) ||
        vm_user_region_unmap_page(&remap_region, alias_vaddr - MEMORY_PAGE_SIZE)) {
        panic_shutdown();
    }

    backing_page[0] = 0x11223344U;
    remap_page[0] = 0xA1B2C3D4U;
    remap_page[1] = 0x01020304U;
    remap_page[2] = 0U;
    remap_page[3] = 0U;
    nx_page[0] = 0x00008067U;
    if (!trap_user_runtime_activate(&user_runtime)) {
        panic_shutdown();
    }

    if (!trap_user_runtime_is_active(&user_runtime) ||
        !vm_process_is_active(&user_process) ||
        runtime_context_active_process() != &user_process ||
        runtime_context_active_address_space() != user_address_space ||
        trap_active_context() != &supervisor_trap_context ||
        !vm_address_space_is_active(user_address_space) ||
        !vm_address_space_is_enabled(user_address_space) ||
        riscv_read_satp() != vm_address_space_satp_value(user_address_space)) {
        panic_shutdown();
    }
    if ((riscv_read_sstatus() & RISCV_SSTATUS_SUM) != 0) {
        panic_shutdown();
    }
    riscv_set_sstatus_bits(RISCV_SSTATUS_SUM);
    if ((riscv_read_sstatus() & RISCV_SSTATUS_SUM) == 0) {
        panic_shutdown();
    }
    alias_page = (uint32_t*)alias_vaddr;
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

    if (!vm_user_region_clear_object(&alias_region) ||
        !vm_user_region_set_fault_object(&alias_region, &remap_object)) {
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
    riscv_clear_sstatus_bits(RISCV_SSTATUS_SUM);
    if ((riscv_read_sstatus() & RISCV_SSTATUS_SUM) != 0) {
        panic_shutdown();
    }
    if (!vm_user_region_unmap_page(&alias_region, alias_vaddr)) {
        panic_shutdown();
    }
    demo_trap_state.timer_irq_seen = 0;
    if (!trap_user_runtime_arm_timer_signal(&user_runtime,
                                            remap_page,
                                            3U,
                                            user_timer_marker)) {
        panic_shutdown();
    }
    timer_schedule_delta(8);
    demo_trap_state.user_ecall_seen = 0;
    if (!trap_user_runtime_enter(&user_runtime, riscv_enter_user_mode)) {
        panic_shutdown();
    }
    if (!demo_trap_state.user_ecall_seen ||
        remap_page[2] != user_data_marker ||
        remap_page[3] != user_timer_marker ||
        !trap_user_runtime_timer_signal_delivered(&user_runtime) ||
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
