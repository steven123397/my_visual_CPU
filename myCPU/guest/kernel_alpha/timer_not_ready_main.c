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
        kernel_alpha_timer_post_handler_panic,
        kernel_alpha_external_post_handler_emit_ready)) {
        panic_shutdown();
    }

    if (!kernel_alpha_run_interrupt_bringup(
            &runtime,
            UINT64_C(0x54494D4E52445921),
            kernel_runtime_install_interrupt_counter_policies_adapter)) {
        panic_shutdown();
    }

    kernel_alpha_begin_plic_supervisor_phase();
    if (!kernel_alpha_wait_for_first_external_delivery(&runtime, 4096U)) {
        panic_shutdown();
    }

    if (!kernel_alpha_validate_timer_not_ready_contract(4096U, 'T')) {
        panic_shutdown();
    }
    panic_shutdown();
}
