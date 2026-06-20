#include "console_input.h"

#include <stdint.h>

#include "console.h"
#include "platform.h"

/* 字节是否为可显示 ASCII。 */
static bool console_input_is_visible_ascii(uint8_t ch) {
    return ch >= 0x20U && ch <= 0x7eU;
}

/* UART RX 就绪时读一个字节到 out。 */
static bool console_input_read_uart_byte(uint8_t* out_ch) {
    if (out_ch == 0 || platform_uart_rx_ready() == 0U) {
        return false;
    }
    *out_ch = platform_uart_getc();
    return true;
}

/* 复位当前行缓冲与溢出标记（不动 RX 环形缓冲）。 */
static void console_input_reset_line(console_input_state_t* state) {
    if (state == NULL) {
        return;
    }

    state->data[0] = '\0';
    state->length = 0;
    state->overflow = false;
    state->rx_overflow = false;
}

/* 复位 RX 环形缓冲的 head/tail/count。 */
static void console_input_reset_rx(console_input_state_t* state) {
    if (state == NULL) {
        return;
    }

    state->rx_head = 0;
    state->rx_tail = 0;
    state->rx_count = 0;
    state->rx_overflow = false;
}

/* 从 RX 环形缓冲弹出一个字节。 */
static bool console_input_pop_interrupt_byte(console_input_state_t* state,
                                             uint8_t* out_ch) {
    if (state == NULL || out_ch == 0 || state->rx_count == 0U) {
        return false;
    }

    *out_ch = state->rx_buffer[state->rx_head];
    state->rx_head = (state->rx_head + 1U) % CONSOLE_INPUT_RX_BUFFER_SIZE;
    state->rx_count -= 1U;
    return true;
}

/* 取下一个字节：优先从环形缓冲弹，否则直接读 UART。 */
static bool console_input_read_next_byte(console_input_state_t* state,
                                         uint8_t* out_ch) {
    return console_input_pop_interrupt_byte(state, out_ch) ||
           console_input_read_uart_byte(out_ch);
}

/* 处理一个字节：回车收行、退格删字符、可显示字符入行，并回显。 */
static console_input_poll_result_t console_input_accept_byte(
    console_input_state_t* state,
    uint8_t ch) {
    if (state == NULL) {
        return CONSOLE_INPUT_NONE;
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
        return CONSOLE_INPUT_NONE;
    }

    if (!console_input_is_visible_ascii(ch)) {
        return CONSOLE_INPUT_NONE;
    }

    if (state->overflow) {
        return CONSOLE_INPUT_NONE;
    }

    if (state->length >= CONSOLE_INPUT_MAX_LINE) {
        state->overflow = true;
        return CONSOLE_INPUT_NONE;
    }

    state->data[state->length++] = (char)ch;
    state->data[state->length] = '\0';
    console_putc((char)ch);
    return CONSOLE_INPUT_NONE;
}

void console_input_init(console_input_state_t* state) {
    console_input_reset_rx(state);
    console_input_reset(state);
}

void console_input_reset(console_input_state_t* state) {
    console_input_reset_line(state);
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

bool console_input_push_interrupt_byte(console_input_state_t* state,
                                       uint8_t ch) {
    if (state == NULL) {
        return false;
    }
    if (state->rx_count >= CONSOLE_INPUT_RX_BUFFER_SIZE) {
        state->rx_overflow = true;
        return false;
    }

    state->rx_buffer[state->rx_tail] = ch;
    state->rx_tail = (state->rx_tail + 1U) % CONSOLE_INPUT_RX_BUFFER_SIZE;
    state->rx_count += 1U;
    return true;
}

void console_input_drain_uart_rx_interrupt(console_input_state_t* state) {
    if (state == NULL) {
        return;
    }

    while (platform_uart_rx_ready() != 0U) {
        const uint8_t ch = platform_uart_getc();

        (void)console_input_push_interrupt_byte(state, ch);
    }
}

void console_input_supervisor_external_post_handler(uint32_t source_id,
                                                    void* context) {
    if (source_id != PLIC_SOURCE_UART_THRE || context == NULL) {
        return;
    }

    console_input_drain_uart_rx_interrupt((console_input_state_t*)context);
}

console_input_poll_result_t console_input_poll(console_input_state_t* state) {
    if (state == NULL) {
        return CONSOLE_INPUT_NONE;
    }
    if (state->rx_overflow) {
        return CONSOLE_INPUT_OVERFLOW;
    }

    while (true) {
        uint8_t ch = 0;

        if (!console_input_read_next_byte(state, &ch)) {
            break;
        }
        const console_input_poll_result_t result =
            console_input_accept_byte(state, ch);

        if (result != CONSOLE_INPUT_NONE) {
            return result;
        }
    }

    return CONSOLE_INPUT_NONE;
}
