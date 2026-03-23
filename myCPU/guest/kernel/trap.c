#include "trap.h"

#include <stdint.h>

#include "panic.h"
#include "riscv.h"
#include "vm.h"

#define MAX_INTERRUPT_CAUSE 16U
#define MAX_EXCEPTION_CAUSE 16U

struct InterruptHandlerEntry {
    trap_interrupt_handler_t handler;
    void* context;
};

struct ExceptionHandlerEntry {
    trap_exception_handler_t handler;
    void* context;
};

static struct InterruptHandlerEntry interrupt_handlers[MAX_INTERRUPT_CAUSE];
static struct ExceptionHandlerEntry exception_handlers[MAX_EXCEPTION_CAUSE];
static trap_page_fault_handler_t page_fault_handler = 0;
static void* page_fault_handler_context = 0;

static bool is_page_fault_cause(uint64_t cause) {
    return cause == RISCV_EXC_INSN_PAGE_FAULT ||
           cause == RISCV_EXC_LOAD_PAGE_FAULT ||
           cause == RISCV_EXC_STORE_PAGE_FAULT;
}

static bool try_handle_page_fault(uint64_t cause, uint64_t epc, uint64_t tval) {
    trap_page_fault_result_t result = TRAP_PAGE_FAULT_RESULT_UNHANDLED;

    if (!is_page_fault_cause(cause)) {
        return false;
    }

    if (page_fault_handler != 0) {
        result = page_fault_handler(cause, epc, tval, page_fault_handler_context);
        switch (result.action) {
        case TRAP_PAGE_FAULT_ACTION_UNHANDLED:
            break;
        case TRAP_PAGE_FAULT_ACTION_RETRY:
            return true;
        case TRAP_PAGE_FAULT_ACTION_SKIP_INSTRUCTION:
            riscv_write_sepc(epc + 4U);
            return true;
        case TRAP_PAGE_FAULT_ACTION_RESUME_AT:
            riscv_write_sepc(result.resume_pc);
            return true;
        }
    }

    return vm_handle_page_fault(cause, epc, tval);
}

void trap_init(void) {
    uint64_t i = 0;

    for (i = 0; i < MAX_INTERRUPT_CAUSE; ++i) {
        interrupt_handlers[i].handler = 0;
        interrupt_handlers[i].context = 0;
    }

    for (i = 0; i < MAX_EXCEPTION_CAUSE; ++i) {
        exception_handlers[i].handler = 0;
        exception_handlers[i].context = 0;
    }

    page_fault_handler = 0;
    page_fault_handler_context = 0;
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

bool trap_install_page_fault_handler(trap_page_fault_handler_t handler,
                                     void* context) {
    if (handler == 0) {
        return false;
    }

    page_fault_handler = handler;
    page_fault_handler_context = context;
    return true;
}

bool trap_install_exception_handler(uint64_t cause,
                                    trap_exception_handler_t handler,
                                    void* context) {
    if (cause >= MAX_EXCEPTION_CAUSE || handler == 0) {
        return false;
    }
    exception_handlers[cause].handler = handler;
    exception_handlers[cause].context = context;
    return true;
}

void supervisor_trap_dispatch(void) {
    const uint64_t scause = riscv_read_scause();
    const uint64_t cause = scause & ~RISCV_INTERRUPT_BIT;
    const struct InterruptHandlerEntry* entry = 0;
    const struct ExceptionHandlerEntry* exception_entry = 0;

    if ((scause & RISCV_INTERRUPT_BIT) == 0) {
        const uint64_t epc = riscv_read_sepc();
        const uint64_t tval = riscv_read_stval();

        if (cause >= MAX_EXCEPTION_CAUSE) {
            panic_shutdown();
        }

        if (try_handle_page_fault(cause, epc, tval)) {
            return;
        }

        exception_entry = &exception_handlers[cause];
        if (exception_entry->handler == 0) {
            panic_shutdown();
        }

        exception_entry->handler(cause, epc, tval, exception_entry->context);
        return;
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
