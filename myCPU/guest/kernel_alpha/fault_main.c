#include <stddef.h>
#include <stdint.h>

#include "console.h"
#include "kernel_alpha.h"
#include "kernel_runtime.h"
#include "panic.h"
#include "platform.h"

void kernel_main(void) {
    kernel_runtime_t runtime;
    const kernel_alpha_bringup_options_t options = {
        .mmio_mask = KERNEL_ALPHA_MMIO_UART,
        .pmm_probe_marker = 0,
        .pre_vm_setup = NULL,
        .pre_vm_context = NULL,
    };

    kernel_runtime_init(&runtime);
    if (!kernel_runtime_run_common_bringup(&runtime, &options)) {
        panic_shutdown();
    }

    /* CLINT is intentionally left unmapped to force a kernel MMIO fault. */
    (void)platform_clint_read_mtime();

    panic_shutdown();
}
