/* Course OS UART 行输入单测：固定回车、退格、溢出和 prompt 交互边界。 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../guest/include/console_input.h"
#include "../../guest/include/platform.h"

static const uint8_t* g_uart_input = 0;
static size_t g_uart_input_size = 0;
static size_t g_uart_input_offset = 0;
static char g_console_output[512];
static size_t g_console_output_len = 0;

static int fail(const char* message) {
    fprintf(stderr, "%s\n", message);
    return 1;
}

static void reset_stubs(void) {
    g_uart_input = 0;
    g_uart_input_size = 0;
    g_uart_input_offset = 0;
    memset(g_console_output, 0, sizeof(g_console_output));
    g_console_output_len = 0;
}

static void set_uart_input(const char* input) {
    g_uart_input = (const uint8_t*)input;
    g_uart_input_size = input != 0 ? strlen(input) : 0U;
    g_uart_input_offset = 0;
}

void console_putc(char ch) {
    if (g_console_output_len + 1U < sizeof(g_console_output)) {
        g_console_output[g_console_output_len++] = ch;
        g_console_output[g_console_output_len] = '\0';
    }
}

uint64_t platform_uart_rx_ready(void) {
    return g_uart_input != 0 && g_uart_input_offset < g_uart_input_size ? 1U : 0U;
}

uint8_t platform_uart_getc(void) {
    if (g_uart_input == 0 || g_uart_input_offset >= g_uart_input_size) {
        return 0U;
    }
    return g_uart_input[g_uart_input_offset++];
}

static bool push_text(console_input_state_t* state, const char* text) {
    size_t i = 0;

    if (state == 0 || text == 0) {
        return false;
    }
    while (text[i] != '\0') {
        if (!console_input_push_interrupt_byte(state, (uint8_t)text[i])) {
            return false;
        }
        i += 1U;
    }
    return true;
}

static int test_interrupt_buffer_delivers_ready_line(void) {
    console_input_state_t state;

    reset_stubs();
    console_input_init(&state);
    if (!push_text(&state, "echo hi\n") ||
        console_input_poll(&state) != CONSOLE_INPUT_READY ||
        strcmp(state.data, "echo hi") != 0 ||
        g_uart_input_offset != 0U ||
        strcmp(g_console_output, "echo hi\r\n") != 0) {
        return fail("expected interrupt buffer to deliver a ready shell line before polling UART");
    }

    return 0;
}

static int test_polling_fallback_still_delivers_ready_line(void) {
    console_input_state_t state;

    reset_stubs();
    console_input_init(&state);
    set_uart_input("pwd\n");
    if (console_input_poll(&state) != CONSOLE_INPUT_READY ||
        strcmp(state.data, "pwd") != 0 ||
        g_uart_input_offset != 4U ||
        strcmp(g_console_output, "pwd\r\n") != 0) {
        return fail("expected polling fallback to keep existing UART behavior");
    }

    return 0;
}

static int test_interrupt_bytes_share_line_editing_rules(void) {
    console_input_state_t state;

    reset_stubs();
    console_input_init(&state);
    if (!push_text(&state, "ab\b c\x01\n") ||
        console_input_poll(&state) != CONSOLE_INPUT_READY ||
        strcmp(state.data, "a c") != 0) {
        return fail("expected interrupt bytes to share backspace and filtering behavior");
    }

    return 0;
}

static int test_interrupt_overflow_returns_overflow(void) {
    console_input_state_t state;
    size_t i = 0;
    bool overflowed = false;

    reset_stubs();
    console_input_init(&state);
    for (i = 0; i < CONSOLE_INPUT_RX_BUFFER_SIZE + 1U; ++i) {
        if (!console_input_push_interrupt_byte(&state, (uint8_t)'x')) {
            overflowed = true;
            break;
        }
    }
    if (!overflowed ||
        console_input_poll(&state) != CONSOLE_INPUT_OVERFLOW) {
        return fail("expected full interrupt buffer to surface as input overflow");
    }

    return 0;
}

static int test_reset_preserves_queued_interrupt_input(void) {
    console_input_state_t state;

    reset_stubs();
    console_input_init(&state);
    if (!push_text(&state, "one\ntwo\n") ||
        console_input_poll(&state) != CONSOLE_INPUT_READY ||
        strcmp(state.data, "one") != 0) {
        return fail("expected first queued interrupt line");
    }

    console_input_reset(&state);
    if (console_input_poll(&state) != CONSOLE_INPUT_READY ||
        strcmp(state.data, "two") != 0) {
        return fail("expected reset to preserve already queued interrupt bytes");
    }

    return 0;
}

static int test_external_post_handler_filters_uart_source(void) {
    console_input_state_t state;

    reset_stubs();
    console_input_init(&state);
    set_uart_input("rx\n");
    console_input_supervisor_external_post_handler(PLIC_SOURCE_VIRTIO_MMIO,
                                                   &state);
    if (g_uart_input_offset != 0U ||
        console_input_poll(&state) != CONSOLE_INPUT_READY ||
        strcmp(state.data, "rx") != 0) {
        return fail("expected non-UART external source to leave RX bytes for fallback polling");
    }

    reset_stubs();
    console_input_init(&state);
    set_uart_input("irq\n");
    console_input_supervisor_external_post_handler(PLIC_SOURCE_UART_THRE,
                                                   &state);
    if (g_uart_input_offset != 4U ||
        console_input_poll(&state) != CONSOLE_INPUT_READY ||
        strcmp(state.data, "irq") != 0) {
        return fail("expected UART external source to drain RX bytes into interrupt buffer");
    }

    return 0;
}

int main(void) {
    if (test_interrupt_buffer_delivers_ready_line() != 0 ||
        test_polling_fallback_still_delivers_ready_line() != 0 ||
        test_interrupt_bytes_share_line_editing_rules() != 0 ||
        test_interrupt_overflow_returns_overflow() != 0 ||
        test_reset_preserves_queued_interrupt_input() != 0 ||
        test_external_post_handler_filters_uart_source() != 0) {
        return 1;
    }

    return 0;
}
