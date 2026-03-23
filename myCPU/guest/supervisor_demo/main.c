#include <stdint.h>

#include "console.h"
#include "memory.h"
#include "panic.h"
#include "platform.h"
#include "riscv.h"
#include "storage.h"
#include "timer.h"
#include "trap.h"

static volatile uint64_t timer_irq_seen = 0;
static volatile uint64_t external_irq_seen = 0;

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

void kernel_main(void) {
    uint8_t* storage_buffer = NULL;
    uint8_t* storage_page = NULL;

    memory_init();
    platform_plic_supervisor_init();
    trap_init();

    if (!trap_install_interrupt_handler(RISCV_SUPERVISOR_TIMER_INTERRUPT,
                                        timer_interrupt_handler,
                                        NULL) ||
        !trap_install_interrupt_handler(RISCV_SUPERVISOR_EXTERNAL_INTERRUPT,
                                        external_interrupt_handler,
                                        NULL)) {
        panic_shutdown();
    }

    if (memory_kernel_start() != MEM_BASE ||
        memory_heap_start() < memory_kernel_end() ||
        memory_heap_limit() != MEM_BASE + MEM_SIZE) {
        panic_shutdown();
    }

    storage_page = (uint8_t*)memory_alloc_pages(1);
    storage_buffer = storage_page;
    if (storage_page == NULL ||
        (((uintptr_t)storage_page) & (MEMORY_PAGE_SIZE - 1U)) != 0 ||
        memory_heap_current() - (uintptr_t)storage_page < MEMORY_PAGE_SIZE) {
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
