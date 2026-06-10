#include "console_input.h"

#include <stdint.h>

#include "console.h"
#include "platform.h"

static bool console_input_is_visible_ascii(uint8_t ch) {
    return ch >= 0x20U && ch <= 0x7eU;
}

static bool console_input_read_uart_byte(uint8_t* out_ch) {
    if (out_ch == 0 || platform_uart_rx_ready() == 0U) {
        return false;
    }
    *out_ch = platform_uart_getc();
    return true;
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

size_t console_input_read_raw(uint8_t* out, size_t out_size) {
    size_t used = 0;

    if (out == 0 || out_size == 0U) {
        return 0U;
    }

    while (used < out_size && console_input_read_uart_byte(&out[used])) {
        used += 1U;
    }
    return used;
}

console_input_poll_result_t console_input_poll(console_input_state_t* state) {
    if (state == NULL) {
        return CONSOLE_INPUT_NONE;
    }

    while (true) {
        uint8_t ch = 0;

        if (!console_input_read_uart_byte(&ch)) {
            break;
        }

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
