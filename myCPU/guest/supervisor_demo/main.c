#include <stdint.h>

#include "console.h"
#include "kernel_runtime.h"
#include "panic.h"
#include "platform.h"
#include "supervisor_demo_smoke.h"

extern char user_test_entry[];
extern char user_test_ecall[];

void kernel_main(void) {
    kernel_runtime_t runtime;

    kernel_runtime_init(&runtime);
    platform_plic_supervisor_init();
    if (!kernel_runtime_run_entry_bringup(&runtime)) {
        panic_shutdown();
    }
    if (!supervisor_demo_smoke_run(kernel_runtime_trap_context(&runtime),
                                   (uintptr_t)user_test_entry,
                                   (uintptr_t)user_test_ecall)) {
        panic_shutdown();
    }

    console_puts("KRN");
    platform_shutdown(0);
}
