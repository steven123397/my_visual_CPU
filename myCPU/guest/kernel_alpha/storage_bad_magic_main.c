#include "console.h"
#include "kernel_alpha.h"
#include "panic.h"

void kernel_main(void) {
    kernel_runtime_t runtime;

    kernel_runtime_init(&runtime);
    if (!kernel_alpha_run_storage_bringup(&runtime) ||
        !kernel_alpha_validate_storage_bad_magic_contract()) {
        panic_shutdown();
    }

    console_putc('G');
    panic_shutdown();
}
