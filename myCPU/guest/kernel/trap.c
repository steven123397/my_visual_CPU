#include "trap.h"

#include <stdint.h>

#include "panic.h"
#include "platform.h"
#include "riscv.h"
#include "timer.h"

static volatile uint64_t timer_irq_seen = 0;
static volatile uint64_t external_irq_seen = 0;

void trap_reset_state(void) {
    timer_irq_seen = 0;
    external_irq_seen = 0;
}

bool trap_timer_irq_seen(void) {
    return timer_irq_seen != 0;
}

bool trap_external_irq_seen(void) {
    return external_irq_seen != 0;
}

void supervisor_trap_dispatch(void) {
    const uint64_t scause = riscv_read_scause();
    const uint64_t cause = scause & ~RISCV_INTERRUPT_BIT;

    if ((scause & RISCV_INTERRUPT_BIT) == 0) {
        panic_shutdown();
    }

    if (cause == RISCV_SUPERVISOR_TIMER_INTERRUPT) {
        timer_handle_interrupt();
        timer_irq_seen = 1;
        return;
    }

    if (cause == RISCV_SUPERVISOR_EXTERNAL_INTERRUPT) {
        const uint32_t source_id = platform_plic_supervisor_claim();
        if (source_id != PLIC_SOURCE_UART_THRE) {
            panic_shutdown();
        }

        platform_uart_disable_irq();
        platform_plic_supervisor_complete(source_id);
        external_irq_seen = 1;
        return;
    }

    panic_shutdown();
}
