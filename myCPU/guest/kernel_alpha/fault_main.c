#include <stddef.h>
#include <stdint.h>

#include "console.h"
#include "kernel_alpha.h"
#include "kernel_runtime.h"
#include "panic.h"
#include "platform.h"

void kernel_main(void) {
    kernel_runtime_t runtime;

    kernel_runtime_init(&runtime);
    if (!kernel_alpha_run_fault_bringup(&runtime)) {
        panic_shutdown();
    }

    /* CLINT is intentionally left unmapped to force a kernel MMIO fault. */
    (void)platform_clint_read_mtime();

    panic_shutdown();
}
