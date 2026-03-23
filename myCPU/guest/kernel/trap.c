#include "trap.h"

#include <stdint.h>

#include "panic.h"
#include "riscv.h"

#define MAX_INTERRUPT_CAUSE 16U

struct InterruptHandlerEntry {
    trap_interrupt_handler_t handler;
    void* context;
};

static struct InterruptHandlerEntry interrupt_handlers[MAX_INTERRUPT_CAUSE];

void trap_init(void) {
    uint64_t i = 0;

    for (i = 0; i < MAX_INTERRUPT_CAUSE; ++i) {
        interrupt_handlers[i].handler = 0;
        interrupt_handlers[i].context = 0;
    }
}

bool trap_install_interrupt_handler(uint64_t cause,
                                    trap_interrupt_handler_t handler,
                                    void* context) {
    if (cause >= MAX_INTERRUPT_CAUSE || handler == 0) {
        return false;
    }
    interrupt_handlers[cause].handler = handler;
    interrupt_handlers[cause].context = context;
    return true;
}

void supervisor_trap_dispatch(void) {
    const uint64_t scause = riscv_read_scause();
    const uint64_t cause = scause & ~RISCV_INTERRUPT_BIT;
    const struct InterruptHandlerEntry* entry = 0;

    if ((scause & RISCV_INTERRUPT_BIT) == 0) {
        panic_shutdown();
    }

    if (cause >= MAX_INTERRUPT_CAUSE) {
        panic_shutdown();
    }

    entry = &interrupt_handlers[cause];
    if (entry->handler == 0) {
        panic_shutdown();
    }

    entry->handler(cause, entry->context);
}
