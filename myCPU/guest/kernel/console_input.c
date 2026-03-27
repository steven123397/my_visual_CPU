#include "console_input.h"

#include <stdint.h>

#include "console.h"
#include "platform.h"

static bool console_input_is_visible_ascii(uint8_t ch) {
    return ch >= 0x20U && ch <= 0x7eU;
}

void console_input_init(console_input_state_t* state) {
    console_input_reset(state);
}

void console_input_reset(console_input_state_t* state) {
    if (state == NULL) {
        return;
    }

    state->data[0] = '\0';
    state->length = 0;
    state->overflow = false;
}

console_input_poll_result_t console_input_poll(console_input_state_t* state) {
    if (state == NULL) {
        return CONSOLE_INPUT_NONE;
    }

    while (platform_uart_rx_ready() != 0U) {
        const uint8_t ch = platform_uart_getc();

        if (ch == '\r' || ch == '\n') {
            console_putc('\r');
            console_putc('\n');
            if (state->overflow) {
                return CONSOLE_INPUT_OVERFLOW;
            }
            state->data[state->length] = '\0';
            return CONSOLE_INPUT_READY;
        }

        if (ch == '\b' || ch == 0x7fU) {
            if (!state->overflow && state->length != 0U) {
                state->length -= 1U;
                state->data[state->length] = '\0';
                console_putc('\b');
                console_putc(' ');
                console_putc('\b');
            }
            continue;
        }

        if (!console_input_is_visible_ascii(ch)) {
            continue;
        }

        if (state->overflow) {
            continue;
        }

        if (state->length >= CONSOLE_INPUT_MAX_LINE) {
            state->overflow = true;
            continue;
        }

        state->data[state->length++] = (char)ch;
        state->data[state->length] = '\0';
        console_putc((char)ch);
    }

    return CONSOLE_INPUT_NONE;
}
