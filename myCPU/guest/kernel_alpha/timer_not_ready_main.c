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
    trap_context_t supervisor_trap_context;
    vm_address_space_t* kernel_address_space = NULL;
    supervisor_runtime_interrupt_state_t interrupts;
    const kernel_alpha_bringup_options_t options = {
        .mmio_mask = KERNEL_ALPHA_MMIO_UART | KERNEL_ALPHA_MMIO_CLINT |
                     KERNEL_ALPHA_MMIO_PLIC,
        .pmm_probe_marker = UINT64_C(0x54494D4E52445921),
        .pre_vm_setup =
            supervisor_runtime_install_interrupt_counter_policies_adapter,
        .pre_vm_context = &interrupts,
    };

    supervisor_runtime_interrupt_state_init(&interrupts);
    supervisor_runtime_interrupt_state_bind_self_handlers(
        &interrupts,
        PLIC_SOURCE_UART_THRE,
        kernel_alpha_timer_not_ready_timer_post_handler,
        kernel_alpha_timer_not_ready_external_post_handler);

    if (!kernel_alpha_run_common_bringup(&supervisor_trap_context,
                                         &kernel_address_space,
                                         &options)) {
        panic_shutdown();
    }

    platform_plic_supervisor_init();
    console_putc('P');

    if (!supervisor_runtime_enable_uart_thre_and_wait(
            &interrupts.external_interrupts,
            4096U)) {
        panic_shutdown();
    }

    supervisor_runtime_cancel_timer_delivery();

    if (!supervisor_runtime_wait_timeout(4096U)) {
        panic_shutdown();
    }

    console_putc('T');
    panic_shutdown();
}
