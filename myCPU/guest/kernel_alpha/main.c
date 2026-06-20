#include <stdint.h>

#include "console.h"
#include "course_os_stage1.h"
#include "course_os_stage2.h"
#include "course_os_stage3.h"
#include "kernel_alpha.h"
#include "panic.h"
#include "platform.h"

/* kernel_alpha_demo 入口：bring-up 后依次跑 Stage1/2/3 smoke，输出三段 marker。 */
void kernel_main(void) {
    kernel_runtime_t runtime;
    static course_os_stage1_t stage1;
    static course_os_stage2_t stage2;
    static course_os_stage3_t stage3;
    char stage1_summary[192];
    char stage2_summary[192];
    char stage3_summary[192];

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

    course_os_stage1_init(&stage1);
    if (!course_os_stage1_run(&stage1) ||
        !course_os_stage1_summary(&stage1,
                                  stage1_summary,
                                  sizeof(stage1_summary))) {
        panic_shutdown();
    }

    course_os_stage2_init(&stage2);
    if (!course_os_stage2_run(&stage2) ||
        !course_os_stage2_summary(&stage2,
                                  stage2_summary,
                                  sizeof(stage2_summary))) {
        panic_shutdown();
    }

    course_os_stage3_init(&stage3);
    if (!course_os_stage3_run(&stage3) ||
        !course_os_stage3_summary(&stage3,
                                  stage3_summary,
                                  sizeof(stage3_summary))) {
        panic_shutdown();
    }

    console_putc('|');
    console_puts(stage1_summary);
    console_putc('|');
    console_puts(stage2_summary);
    console_putc('|');
    console_puts(stage3_summary);
    platform_shutdown(0);
}
