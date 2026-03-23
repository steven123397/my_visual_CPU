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

#define TEST_ALIAS_VADDR 0x40000000UL

struct PageFaultContext {
    uintptr_t alias_vaddr;
    uintptr_t rodata_addr;
    uintptr_t exec_addr;
};

static volatile uint64_t timer_irq_seen = 0;
static volatile uint64_t external_irq_seen = 0;
static volatile uint64_t load_page_fault_seen = 0;
static volatile uint64_t store_page_fault_seen = 0;
static volatile uint64_t instruction_page_fault_seen = 0;
static volatile uintptr_t instruction_fault_resume_pc = 0;
static const uint32_t rodata_marker = 0xCAFEBABEU;

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

static trap_page_fault_result_t page_fault_handler(uint64_t cause,
                                                   uint64_t epc,
                                                   uint64_t tval,
                                                   void* context) {
    const struct PageFaultContext* fault_context =
        (const struct PageFaultContext*)context;
    uintptr_t resume_pc = 0;

    if (fault_context == NULL) {
        panic_shutdown();
    }

    if (cause == RISCV_EXC_LOAD_PAGE_FAULT &&
        tval == fault_context->alias_vaddr) {
        load_page_fault_seen = 1;
        return TRAP_PAGE_FAULT_RESULT_UNHANDLED;
    }

    if (cause == RISCV_EXC_STORE_PAGE_FAULT &&
        tval == fault_context->rodata_addr) {
        store_page_fault_seen = 1;
        return TRAP_PAGE_FAULT_RESULT_SKIP_INSTRUCTION;
    }

    if (cause == RISCV_EXC_INSN_PAGE_FAULT &&
        tval == fault_context->exec_addr &&
        instruction_fault_resume_pc != 0) {
        resume_pc = instruction_fault_resume_pc;
        instruction_page_fault_seen = 1;
        instruction_fault_resume_pc = 0;
        return TRAP_PAGE_FAULT_RESULT_RESUME_AT(resume_pc);
    }

    (void)epc;
    return TRAP_PAGE_FAULT_RESULT_UNHANDLED;
}

void kernel_main(void) {
    uintptr_t early_cursor = 0;
    uint32_t* alias_page = NULL;
    uint32_t* backing_page = NULL;
    uint32_t* nx_page = NULL;
    uint32_t* range_probe_page = NULL;
    uint8_t* storage_buffer = NULL;
    uint8_t* storage_page = NULL;
    void* early_allocation = NULL;
    struct PageFaultContext page_fault_context = {0};

    memory_init();
    early_allocation = memory_alloc(96, 64);
    platform_plic_supervisor_init();
    trap_init();

    if (!trap_install_interrupt_handler(RISCV_SUPERVISOR_TIMER_INTERRUPT,
                                        timer_interrupt_handler,
                                        NULL) ||
        !trap_install_interrupt_handler(RISCV_SUPERVISOR_EXTERNAL_INTERRUPT,
                                        external_interrupt_handler,
                                        NULL) ||
        !trap_install_page_fault_handler(page_fault_handler,
                                        &page_fault_context)) {
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
    nx_page = (uint32_t*)pmm_alloc_page();
    range_probe_page = (uint32_t*)pmm_alloc_page();
    if (backing_page == NULL ||
        nx_page == NULL ||
        range_probe_page == NULL ||
        pmm_free_pages() + 3U != pmm_total_pages() ||
        pmm_used_pages() != 3U ||
        !vm_init() ||
        vm_root_table() == 0 ||
        (vm_root_table() & (MEMORY_PAGE_SIZE - 1U)) != 0 ||
        vm_satp_value() !=
            (RISCV_SATP_MODE_SV39 | ((uint64_t)vm_root_table() >> 12)) ||
        !vm_map_identity_1g(0, VM_PAGE_READ | VM_PAGE_WRITE | VM_PAGE_EXEC) ||
        vm_map_kernel_range(TEST_ALIAS_VADDR + MEMORY_PAGE_SIZE,
                            (uintptr_t)range_probe_page,
                            MEMORY_PAGE_SIZE,
                            VM_PAGE_WRITE) ||
        vm_map_user_range(TEST_ALIAS_VADDR + MEMORY_PAGE_SIZE,
                          (uintptr_t)range_probe_page,
                          MEMORY_PAGE_SIZE,
                          VM_PAGE_READ) ||
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
        vm_register_fault_range(TEST_ALIAS_VADDR + 1U,
                                (uintptr_t)backing_page,
                                MEMORY_PAGE_SIZE,
                                VM_PAGE_READ) ||
        !vm_map_page(TEST_ALIAS_VADDR,
                     (uintptr_t)backing_page,
                     VM_PAGE_READ | VM_PAGE_WRITE) ||
        !vm_register_fault_range(TEST_ALIAS_VADDR,
                                 (uintptr_t)backing_page,
                                 MEMORY_PAGE_SIZE,
                                 VM_PAGE_READ | VM_PAGE_WRITE) ||
        vm_register_fault_range(TEST_ALIAS_VADDR,
                                (uintptr_t)backing_page,
                                MEMORY_PAGE_SIZE,
                                VM_PAGE_READ | VM_PAGE_WRITE) ||
        vm_map_range(TEST_ALIAS_VADDR,
                     (uintptr_t)backing_page,
                     MEMORY_PAGE_SIZE * 2U,
                     VM_PAGE_READ | VM_PAGE_WRITE) ||
        !vm_map_page(TEST_ALIAS_VADDR + MEMORY_PAGE_SIZE,
                     (uintptr_t)range_probe_page,
                     VM_PAGE_READ | VM_PAGE_WRITE) ||
        !vm_unmap_page(TEST_ALIAS_VADDR + MEMORY_PAGE_SIZE) ||
        vm_unmap_page(TEST_ALIAS_VADDR + MEMORY_PAGE_SIZE)) {
        panic_shutdown();
    }
    page_fault_context.alias_vaddr = TEST_ALIAS_VADDR;
    page_fault_context.rodata_addr = (uintptr_t)&rodata_marker;
    page_fault_context.exec_addr = (uintptr_t)nx_page;

    backing_page[0] = 0x11223344U;
    nx_page[0] = 0x00008067U;
    if (!vm_enable()) {
        panic_shutdown();
    }

    if (!vm_is_enabled() || riscv_read_satp() != vm_satp_value()) {
        panic_shutdown();
    }
    alias_page = (uint32_t*)TEST_ALIAS_VADDR;
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

    if (!vm_unmap_page(TEST_ALIAS_VADDR)) {
        panic_shutdown();
    }
    vm_flush_tlb();
    if (alias_page[0] != 0x11223344U || !load_page_fault_seen) {
        panic_shutdown();
    }
    provoke_rodata_store_fault();
    if (!store_page_fault_seen || rodata_marker != 0xCAFEBABEU) {
        panic_shutdown();
    }
    provoke_instruction_page_fault((uintptr_t)nx_page);
    if (!instruction_page_fault_seen || instruction_fault_resume_pc != 0) {
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
    timer_schedule_delta(8);
    platform_uart_enable_thre_irq();

    while (!timer_irq_seen || !external_irq_seen) {
    }

    console_puts("KRN");
    platform_shutdown(0);
}
