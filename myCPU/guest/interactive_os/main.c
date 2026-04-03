#include "kernel_runtime.h"
#include "monitor.h"
#include "panic.h"

void kernel_main(void) {
    kernel_runtime_t runtime;

    kernel_runtime_init(&runtime);
    if (!kernel_runtime_run_identity_superpage_bringup(&runtime)) {
        panic_shutdown();
    }

    monitor_run(&runtime);
}
