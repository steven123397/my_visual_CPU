#include <stdint.h>

#include "console.h"
#include "kernel_alpha.h"
#include "panic.h"
#include "platform.h"
#include "supervisor_runtime.h"

static void kernel_alpha_timer_not_ready_external_post_handler(
    uint32_t source_id,
    void* context) {
    if (context == NULL || source_id != PLIC_SOURCE_UART_THRE) {
        panic_shutdown();
    }

    platform_uart_disable_irq();
    console_putc('E');
}

static void kernel_alpha_timer_not_ready_timer_post_handler(void* context) {
    if (context == NULL) {
        panic_shutdown();
    }

    panic_shutdown();
}

void kernel_main(void) {
    kernel_runtime_t runtime;
    const kernel_alpha_bringup_options_t options = {
        .mmio_mask = KERNEL_ALPHA_MMIO_UART | KERNEL_ALPHA_MMIO_CLINT |
                     KERNEL_ALPHA_MMIO_PLIC,
        .pmm_probe_marker = UINT64_C(0x54494D4E52445921),
        .pre_vm_setup = kernel_runtime_install_interrupt_counter_policies_adapter,
        .pre_vm_context = &runtime,
    };

    kernel_runtime_init(&runtime);
    if (!kernel_runtime_bind_self_interrupt_handlers(
            &runtime,
        PLIC_SOURCE_UART_THRE,
        kernel_alpha_timer_not_ready_timer_post_handler,
        kernel_alpha_timer_not_ready_external_post_handler)) {
        panic_shutdown();
    }

    if (!kernel_runtime_run_common_bringup(&runtime, &options)) {
        panic_shutdown();
    }

    kernel_alpha_begin_plic_supervisor_phase();
    if (!kernel_alpha_wait_for_first_external_delivery(&runtime, 4096U)) {
        panic_shutdown();
    }

    supervisor_runtime_cancel_timer_delivery();

    if (!supervisor_runtime_wait_timeout(4096U)) {
        panic_shutdown();
    }

    console_putc('T');
    panic_shutdown();
}
