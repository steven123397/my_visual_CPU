#include "console.h"
#include "console_input.h"
#include "course_os_stage3.h"
#include "kernel_runtime.h"
#include "panic.h"
#include "platform.h"
#include "supervisor_runtime.h"

/*
 * course_os_stage3_t carries the full shell/process/fs state and is far larger
 * than the 8 KiB supervisor boot stack. Keep persistent shell state in .bss so
 * the shell command path cannot corrupt adjacent VM globals.
 */
static kernel_runtime_t g_runtime;
static course_os_stage3_t g_stage;
static console_input_state_t g_input;
static char g_out[COURSE_SHELL_COMMAND_OUTPUT_SIZE];

static void print_prompt(void) {
    console_puts("course-os> ");
}

static void print_shell_output(const char* out) {
    if (out == 0 || out[0] == '\0') {
        return;
    }
    console_puts(out);
}

static void course_os_shell_external_post_handler(uint32_t source_id,
                                                  void* context) {
    if (context == 0) {
        return;
    }

    console_input_supervisor_external_post_handler(source_id, &g_input);
}

static bool install_interrupt_input(void) {
    trap_context_t* trap_context = kernel_runtime_trap_context(&g_runtime);
    supervisor_runtime_interrupt_state_t* interrupts =
        kernel_runtime_interrupt_state(&g_runtime);

    if (trap_context == 0 || interrupts == 0) {
        return false;
    }
    if (!kernel_runtime_bind_self_interrupt_handlers(
            &g_runtime,
            PLIC_SOURCE_UART_THRE,
            0,
            course_os_shell_external_post_handler) ||
        !supervisor_runtime_install_external_counter_policy(trap_context,
                                                            interrupts)) {
        return false;
    }

    platform_plic_supervisor_init();
    platform_plic_supervisor_enable_source(PLIC_SOURCE_UART_THRE);
    platform_uart_enable_rx_irq();
    return true;
}

void kernel_main(void) {
    kernel_runtime_init(&g_runtime);
    if (!kernel_runtime_run_identity_superpage_bringup(&g_runtime)) {
        panic_shutdown();
    }

    course_os_stage3_init(&g_stage);
    if (!course_os_stage3_prepare_shell(&g_stage)) {
        panic_shutdown();
    }

    console_input_init(&g_input);
    if (!install_interrupt_input()) {
        panic_shutdown();
    }
    console_puts("course-os shell ready\r\n");
    print_prompt();

    for (;;) {
        const console_input_poll_result_t result = console_input_poll(&g_input);

        if (result == CONSOLE_INPUT_NONE) {
            continue;
        }
        if (result == CONSOLE_INPUT_OVERFLOW) {
            console_puts("line too long\r\n");
            console_input_reset(&g_input);
            print_prompt();
            continue;
        }

        if (g_input.data[0] != '\0') {
            g_out[0] = '\0';
            if (course_shell_run_line(&g_stage.shell,
                                      g_input.data,
                                      g_out,
                                      sizeof(g_out))) {
                print_shell_output(g_out);
            } else {
                console_puts("error\r\n");
            }
        }
        console_input_reset(&g_input);
        print_prompt();
    }
}
