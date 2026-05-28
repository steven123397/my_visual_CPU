#include <stdint.h>

#include "console.h"
#include "course_os_stage1.h"
#include "kernel_alpha.h"
#include "panic.h"
#include "platform.h"

void kernel_main(void) {
    kernel_runtime_t runtime;
    course_os_stage1_t stage;
    char summary[192];

    kernel_runtime_init(&runtime);
    if (!kernel_runtime_bind_self_interrupt_handlers(
            &runtime,
        PLIC_SOURCE_UART_THRE,
        kernel_alpha_timer_post_handler_emit_ready,
        kernel_alpha_external_post_handler_emit_ready)) {
        panic_shutdown();
    }

    if (!kernel_runtime_run_bringup(
            &runtime,
            KERNEL_ALPHA_MMIO_UART | KERNEL_ALPHA_MMIO_CLINT |
                KERNEL_ALPHA_MMIO_PLIC | KERNEL_ALPHA_MMIO_STORAGE,
            UINT64_C(0x4B41504D4D56414C),
            kernel_runtime_install_interrupt_counter_policies_adapter)) {
        panic_shutdown();
    }

    if (!kernel_alpha_complete_platform_interrupt_readiness(&runtime,
                                                            64U,
                                                            4096U)) {
        panic_shutdown();
    }

    course_os_stage1_init(&stage);
    if (!course_os_stage1_run(&stage) ||
        !course_os_stage1_summary(&stage, summary, sizeof(summary))) {
        panic_shutdown();
    }

    console_putc('|');
    console_puts(summary);
    platform_shutdown(0);
}
