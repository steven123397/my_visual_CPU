#include <stdint.h>

#include "console.h"
#include "kernel_alpha.h"
#include "panic.h"
#include "platform.h"

void kernel_main(void) {
    kernel_runtime_t runtime;

    kernel_runtime_init(&runtime);
    if (!kernel_runtime_bind_self_interrupt_handlers(
            &runtime,
        PLIC_SOURCE_UART_THRE,
        NULL,
        kernel_alpha_external_post_handler_panic_on_delivery)) {
        panic_shutdown();
    }

    if (!kernel_alpha_run_interrupt_bringup(
            &runtime,
            UINT64_C(0x504C49434E524459),
            kernel_runtime_install_external_counter_policy_adapter) ||
        !kernel_alpha_validate_plic_not_ready_contract(&runtime, 4096U, 'P')) {
        panic_shutdown();
    }

    panic_shutdown();
}
