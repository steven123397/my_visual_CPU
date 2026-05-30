#include "console.h"
#include "console_input.h"
#include "course_os_stage3.h"
#include "kernel_runtime.h"
#include "panic.h"

static void print_prompt(void) {
    console_puts("course-os> ");
}

static void print_shell_output(const char* out) {
    if (out == 0 || out[0] == '\0') {
        return;
    }
    console_puts(out);
}

void kernel_main(void) {
    kernel_runtime_t runtime;
    course_os_stage3_t stage;
    console_input_state_t input;
    char out[2048];

    kernel_runtime_init(&runtime);
    if (!kernel_runtime_run_identity_superpage_bringup(&runtime)) {
        panic_shutdown();
    }

    course_os_stage3_init(&stage);
    if (!course_os_stage3_prepare_shell(&stage)) {
        panic_shutdown();
    }

    console_input_init(&input);
    console_puts("course-os shell ready\r\n");
    print_prompt();

    for (;;) {
        const console_input_poll_result_t result = console_input_poll(&input);

        if (result == CONSOLE_INPUT_NONE) {
            continue;
        }
        if (result == CONSOLE_INPUT_OVERFLOW) {
            console_puts("line too long\r\n");
            console_input_reset(&input);
            print_prompt();
            continue;
        }

        if (input.data[0] != '\0') {
            out[0] = '\0';
            if (course_shell_run_line(&stage.shell,
                                      input.data,
                                      out,
                                      sizeof(out))) {
                print_shell_output(out);
            } else {
                console_puts("error\r\n");
            }
        }
        console_input_reset(&input);
        print_prompt();
    }
}
