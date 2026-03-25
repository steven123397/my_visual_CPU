#include <stdint.h>

#include "console.h"
#include "kernel_alpha.h"
#include "panic.h"
#include "platform.h"

static void kernel_alpha_timer_post_handler(void* context);
static void kernel_alpha_external_post_handler(uint32_t source_id,
                                               void* context);

static void kernel_alpha_timer_post_handler(void* context) {
    if (context == NULL) {
        panic_shutdown();
    }

    console_putc('T');
}

static void kernel_alpha_external_post_handler(uint32_t source_id,
                                               void* context) {
    if (context == NULL || source_id != PLIC_SOURCE_UART_THRE) {
        panic_shutdown();
    }

    platform_uart_disable_irq();
    console_putc('E');
}

void kernel_main(void) {
    kernel_runtime_t runtime;
    const kernel_alpha_bringup_options_t options = {
        .mmio_mask = KERNEL_ALPHA_MMIO_UART | KERNEL_ALPHA_MMIO_CLINT |
                     KERNEL_ALPHA_MMIO_PLIC | KERNEL_ALPHA_MMIO_STORAGE,
        .pmm_probe_marker = UINT64_C(0x4B41504D4D56414C),
        .pre_vm_setup = kernel_runtime_install_interrupt_counter_policies_adapter,
        .pre_vm_context = &runtime,
    };

    kernel_runtime_init(&runtime);
    if (!kernel_runtime_bind_self_interrupt_handlers(
            &runtime,
        PLIC_SOURCE_UART_THRE,
        kernel_alpha_timer_post_handler,
        kernel_alpha_external_post_handler)) {
        panic_shutdown();
    }

    if (!kernel_runtime_run_common_bringup(&runtime, &options)) {
        panic_shutdown();
    }

    kernel_alpha_begin_plic_supervisor_phase();
    if (!kernel_alpha_wait_for_first_external_delivery(&runtime, 4096U) ||
        !kernel_alpha_wait_for_first_timer_delivery(&runtime, 64U, 4096U) ||
        !kernel_alpha_complete_storage_probe() ||
        !kernel_alpha_complete_storage_signature_check()) {
        panic_shutdown();
    }

    platform_shutdown(0);
}
