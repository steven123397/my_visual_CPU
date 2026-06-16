#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    CONSOLE_INPUT_MAX_LINE = 128U,
    CONSOLE_INPUT_RX_BUFFER_SIZE = 256U,
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
    uint8_t rx_buffer[CONSOLE_INPUT_RX_BUFFER_SIZE];
    size_t rx_head;
    size_t rx_tail;
    size_t rx_count;
    bool rx_overflow;
} console_input_state_t;

void console_input_init(console_input_state_t* state);
void console_input_reset(console_input_state_t* state);
size_t console_input_read_raw(uint8_t* out, size_t out_size);
bool console_input_push_interrupt_byte(console_input_state_t* state,
                                       uint8_t ch);
void console_input_drain_uart_rx_interrupt(console_input_state_t* state);
void console_input_supervisor_external_post_handler(uint32_t source_id,
                                                    void* context);
console_input_poll_result_t console_input_poll(console_input_state_t* state);
