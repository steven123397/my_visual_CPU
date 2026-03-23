#include <stdint.h>

#include "console.h"
#include "memory.h"
#include "panic.h"
#include "platform.h"
#include "pmm.h"
#include "riscv.h"
#include "storage.h"
#include "timer.h"
#include "trap.h"
#include "vm.h"

static volatile uint64_t timer_irq_seen = 0;
static volatile uint64_t external_irq_seen = 0;
static volatile uintptr_t instruction_fault_resume_pc = 0;
static volatile uint64_t user_ecall_seen = 0;
static volatile uint64_t user_timer_irq_seen = 0;
static volatile uint64_t user_timer_expected = 0;
static const uint32_t rodata_marker = 0xCAFEBABEU;
static const uint32_t user_data_marker = 0x5A5A1234U;
static const uint32_t user_timer_marker = 1U;
static uint32_t* user_shared_page = NULL;

extern void riscv_enter_user_mode(uintptr_t entry, uintptr_t arg0, uintptr_t user_sp);
extern void supervisor_resume_after_user(void);
extern char user_test_entry[];
extern char user_test_ecall[];

struct UserTrapContext {
    uintptr_t expected_ecall_pc;
    uint32_t* data_backing_page;
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
    (void)cause;
    (void)context;
    timer_handle_interrupt();

    if (user_timer_expected) {
        if (user_shared_page == NULL) {
            panic_shutdown();
        }
        user_shared_page[3] = user_timer_marker;
        user_timer_irq_seen = 1;
        user_timer_expected = 0;
    }

    timer_irq_seen = 1;
}

static void external_interrupt_handler(uint64_t cause, void* context) {
    const uint32_t source_id = platform_plic_supervisor_claim();

    (void)cause;
    (void)context;

    if (source_id != PLIC_SOURCE_UART_THRE) {
        panic_shutdown();
    }

    platform_uart_disable_irq();
    platform_plic_supervisor_complete(source_id);
    external_irq_seen = 1;
}

static void user_ecall_exception_handler(uint64_t cause,
                                         uint64_t epc,
                                         uint64_t tval,
                                         void* context) {
    struct UserTrapContext* user_ctx = (struct UserTrapContext*)context;
    const uint64_t sstatus = riscv_read_sstatus();

    if (cause != RISCV_EXC_ECALL_FROM_U ||
        tval != 0 ||
        user_ctx == NULL ||
        epc != user_ctx->expected_ecall_pc ||
        user_ctx->data_backing_page == NULL ||
        user_ctx->data_backing_page[2] != user_data_marker ||
        user_timer_irq_seen == 0 ||
        user_ctx->data_backing_page[3] != user_timer_marker ||
        (sstatus & RISCV_SSTATUS_SPP) != 0 ||
        (sstatus & RISCV_SSTATUS_SPIE) == 0) {
        panic_shutdown();
    }

    user_ecall_seen = 1;
    riscv_set_sstatus_bits(RISCV_SSTATUS_SPP);
    riscv_write_sepc((uintptr_t)supervisor_resume_after_user);
}

void kernel_main(void) {
    uintptr_t early_cursor = 0;
    uintptr_t user_base = 0;
    uintptr_t user_limit = 0;
    uintptr_t alias_vaddr = 0;
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
    vm_user_region_t alias_region = {0};
    vm_user_region_t remap_region = {0};
    vm_user_region_t user_exec_region = {0};
    vm_user_region_t user_stack_region = {0};
    vm_user_region_t invalid_region = {0};
    struct UserTrapContext user_trap_context = {0};
    uint32_t* alias_page = NULL;
    uint32_t* backing_page = NULL;
    uint32_t* remap_page = NULL;
    uint32_t* nx_page = NULL;
    uint32_t* user_stack_page = NULL;
    uint8_t* storage_buffer = NULL;
    uint8_t* storage_page = NULL;
    void* early_allocation = NULL;

    memory_init();
    early_allocation = memory_alloc(96, 64);
    platform_plic_supervisor_init();
    trap_init();
    user_base = vm_user_base();
    user_limit = vm_user_limit();
    alias_vaddr = user_limit - MEMORY_PAGE_SIZE;
    user_exec_vaddr = alias_vaddr - 2U * MEMORY_PAGE_SIZE;
    user_stack_vaddr = alias_vaddr - 3U * MEMORY_PAGE_SIZE;
    user_exec_pa = align_down_page((uintptr_t)user_test_entry);
    user_entry_va =
        user_exec_vaddr + ((uintptr_t)user_test_entry - user_exec_pa);
    user_ecall_va =
        user_exec_vaddr + ((uintptr_t)user_test_ecall - user_exec_pa);
    user_stack_top = user_stack_vaddr + MEMORY_PAGE_SIZE;
    user_trap_context.expected_ecall_pc = user_ecall_va;

    if (!trap_install_interrupt_handler(RISCV_SUPERVISOR_TIMER_INTERRUPT,
                                        timer_interrupt_handler,
                                        NULL) ||
        !trap_install_interrupt_handler(RISCV_SUPERVISOR_EXTERNAL_INTERRUPT,
                                        external_interrupt_handler,
                                        NULL) ||
        !trap_install_exception_handler(RISCV_EXC_ECALL_FROM_U,
                                        user_ecall_exception_handler,
                                        &user_trap_context)) {
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

    backing_page = (uint32_t*)pmm_alloc_page();
    remap_page = (uint32_t*)pmm_alloc_page();
    nx_page = (uint32_t*)pmm_alloc_page();
    user_stack_page = (uint32_t*)pmm_alloc_page();
    user_trap_context.data_backing_page = remap_page;
    user_shared_page = remap_page;
    if (backing_page == NULL ||
        remap_page == NULL ||
        nx_page == NULL ||
        user_stack_page == NULL ||
        pmm_free_pages() + 4U != pmm_total_pages() ||
        pmm_used_pages() != 4U ||
        !vm_init() ||
        vm_root_table() == 0 ||
        (vm_root_table() & (MEMORY_PAGE_SIZE - 1U)) != 0 ||
        vm_satp_value() !=
            (RISCV_SATP_MODE_SV39 | ((uint64_t)vm_root_table() >> 12)) ||
        vm_kernel_base() != memory_kernel_start() ||
        vm_kernel_limit() != memory_heap_limit() ||
        !vm_range_is_user(alias_vaddr, MEMORY_PAGE_SIZE) ||
        vm_range_is_user(memory_text_start(), MEMORY_PAGE_SIZE) ||
        !vm_range_is_kernel(memory_text_start(),
                            memory_text_end() - memory_text_start()) ||
        vm_range_is_kernel(alias_vaddr, MEMORY_PAGE_SIZE) ||
        !vm_map_identity_1g(0, VM_PAGE_READ | VM_PAGE_WRITE | VM_PAGE_EXEC) ||
        !vm_user_region_init(&alias_region,
                             alias_vaddr,
                             MEMORY_PAGE_SIZE,
                             user_rw_flags) ||
        !vm_user_region_init(&user_exec_region,
                             user_exec_vaddr,
                             MEMORY_PAGE_SIZE,
                             user_rx_flags) ||
        !vm_user_region_init(&user_stack_region,
                             user_stack_vaddr,
                             MEMORY_PAGE_SIZE,
                             user_rw_flags) ||
        !vm_user_region_map(&user_exec_region, user_exec_pa) ||
        !vm_user_region_map(&user_stack_region, (uintptr_t)user_stack_page) ||
        !vm_user_region_contains(&user_stack_region, user_stack_top - 16U, 16U) ||
        !vm_user_region_contains(&alias_region, alias_vaddr, MEMORY_PAGE_SIZE) ||
        vm_user_region_contains(&alias_region,
                                alias_vaddr - MEMORY_PAGE_SIZE,
                                MEMORY_PAGE_SIZE) ||
        vm_user_region_init(&invalid_region,
                            0,
                            MEMORY_PAGE_SIZE,
                            user_rw_flags) ||
        vm_map_kernel_range(alias_vaddr,
                            (uintptr_t)remap_page,
                            MEMORY_PAGE_SIZE,
                            VM_PAGE_WRITE) ||
        vm_map_user_range(alias_vaddr,
                          (uintptr_t)remap_page,
                          MEMORY_PAGE_SIZE,
                          VM_PAGE_READ) ||
        vm_map_user_range(memory_text_start(),
                          memory_text_start(),
                          memory_text_end() - memory_text_start(),
                          VM_PAGE_READ | VM_PAGE_EXEC | VM_PAGE_USER) ||
        !vm_map_kernel_range(memory_text_start(),
                             memory_text_start(),
                             memory_text_end() - memory_text_start(),
                             VM_PAGE_READ | VM_PAGE_EXEC) ||
        !vm_map_kernel_range(memory_rodata_start(),
                             memory_rodata_start(),
                             memory_rodata_end() - memory_rodata_start(),
                             VM_PAGE_READ) ||
        !vm_map_kernel_range(memory_data_start(),
                             memory_data_start(),
                             pmm_managed_start() - memory_data_start(),
                             VM_PAGE_READ | VM_PAGE_WRITE) ||
        !vm_map_kernel_range(pmm_managed_start(),
                             pmm_managed_start(),
                             pmm_managed_end() - pmm_managed_start(),
                             VM_PAGE_READ | VM_PAGE_WRITE) ||
        vm_register_fault_range(alias_vaddr + 1U,
                                (uintptr_t)backing_page,
                                MEMORY_PAGE_SIZE,
                                VM_PAGE_READ) ||
        vm_unmap_page(memory_text_start()) ||
        !vm_user_region_map(&alias_region, (uintptr_t)backing_page) ||
        vm_register_fault_range(alias_vaddr,
                                (uintptr_t)remap_page,
                                MEMORY_PAGE_SIZE,
                                VM_PAGE_READ | VM_PAGE_WRITE) ||
        !vm_user_region_set_fault_backing(&alias_region, (uintptr_t)remap_page) ||
        vm_user_region_set_fault_backing(&alias_region, (uintptr_t)remap_page) ||
        vm_register_user_fault_range(alias_vaddr,
                                     (uintptr_t)remap_page,
                                     MEMORY_PAGE_SIZE,
                                     VM_PAGE_READ | VM_PAGE_WRITE) ||
        !vm_register_fault_skip(RISCV_EXC_STORE_PAGE_FAULT,
                                (uintptr_t)&rodata_marker,
                                sizeof(rodata_marker)) ||
        !vm_register_fault_resume_slot(RISCV_EXC_INSN_PAGE_FAULT,
                                       (uintptr_t)nx_page,
                                       MEMORY_PAGE_SIZE,
                                       &instruction_fault_resume_pc) ||
        !vm_user_region_init(&remap_region,
                             alias_vaddr - MEMORY_PAGE_SIZE,
                             MEMORY_PAGE_SIZE,
                             user_rw_flags) ||
        vm_user_region_init(&invalid_region,
                            alias_vaddr,
                            MEMORY_PAGE_SIZE,
                            user_rw_flags) ||
        !vm_user_region_map(&remap_region, (uintptr_t)remap_page) ||
        !vm_user_region_unmap_page(&remap_region, alias_vaddr - MEMORY_PAGE_SIZE) ||
        vm_user_region_unmap_page(&remap_region, alias_vaddr - MEMORY_PAGE_SIZE)) {
        panic_shutdown();
    }

    backing_page[0] = 0x11223344U;
    remap_page[0] = 0xA1B2C3D4U;
    remap_page[1] = 0x01020304U;
    remap_page[2] = 0U;
    remap_page[3] = 0U;
    nx_page[0] = 0x00008067U;
    if (!vm_enable()) {
        panic_shutdown();
    }

    if (!vm_is_enabled() || riscv_read_satp() != vm_satp_value()) {
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

    if (!vm_user_region_unmap_page(&alias_region, alias_vaddr)) {
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
    timer_irq_seen = 0;
    user_timer_irq_seen = 0;
    user_timer_expected = 1;
    timer_schedule_delta(8);
    user_ecall_seen = 0;
    riscv_enter_user_mode(user_entry_va, alias_vaddr, user_stack_top);
    if (!user_ecall_seen || remap_page[2] != user_data_marker ||
        remap_page[3] != user_timer_marker || !user_timer_irq_seen ||
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
    timer_irq_seen = 0;
    timer_schedule_delta(8);
    platform_uart_enable_thre_irq();

    while (!timer_irq_seen || !external_irq_seen) {
    }

    console_puts("KRN");
    platform_shutdown(0);
}
