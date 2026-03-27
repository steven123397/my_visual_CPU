#include "monitor.h"

#include "console_input.h"
#include "monitor_commands.h"
#include "platform.h"

void monitor_run(kernel_runtime_t* runtime) {
    console_input_state_t input;

    console_input_init(&input);
    monitor_commands_reset(platform_clint_read_mtime());
    monitor_write_banner();
    monitor_write_prompt();

    for (;;) {
        switch (console_input_poll(&input)) {
        case CONSOLE_INPUT_READY:
            (void)monitor_execute_line(runtime, input.data);
            console_input_reset(&input);
            monitor_write_prompt();
            break;
        case CONSOLE_INPUT_OVERFLOW:
            monitor_write_line("line too long");
            console_input_reset(&input);
            monitor_write_prompt();
            break;
        case CONSOLE_INPUT_NONE:
        default:
            break;
        }
    }
}
