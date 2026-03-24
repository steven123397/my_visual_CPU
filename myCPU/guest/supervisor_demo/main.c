#include <stdint.h>

#include "console.h"
#include "memory.h"
#include "panic.h"
#include "platform.h"
#include "runtime_context.h"
#include "supervisor_demo_smoke.h"
#include "trap.h"

extern char user_test_entry[];
extern char user_test_ecall[];

void kernel_main(void) {
    trap_context_t supervisor_trap_context;

    memory_init();
    platform_plic_supervisor_init();
    runtime_context_reset();
    trap_context_init(&supervisor_trap_context);
    if (!trap_context_activate(&supervisor_trap_context) ||
        !trap_context_is_active(&supervisor_trap_context) ||
        trap_active_context() != &supervisor_trap_context) {
        panic_shutdown();
    }
    if (!supervisor_demo_smoke_run(&supervisor_trap_context,
                                   (uintptr_t)user_test_entry,
                                   (uintptr_t)user_test_ecall)) {
        panic_shutdown();
    }

    console_puts("KRN");
    platform_shutdown(0);
}
