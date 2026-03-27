#pragma once

#include <stdbool.h>
#include <stddef.h>

enum {
    CONSOLE_INPUT_MAX_LINE = 128U,
};

typedef enum ConsoleInputPollResult {
    CONSOLE_INPUT_NONE = 0,
    CONSOLE_INPUT_READY,
    CONSOLE_INPUT_OVERFLOW,
} console_input_poll_result_t;

typedef struct ConsoleInputState {
    char data[CONSOLE_INPUT_MAX_LINE + 1U];
    size_t length;
    bool overflow;
} console_input_state_t;

void console_input_init(console_input_state_t* state);
void console_input_reset(console_input_state_t* state);
console_input_poll_result_t console_input_poll(console_input_state_t* state);
