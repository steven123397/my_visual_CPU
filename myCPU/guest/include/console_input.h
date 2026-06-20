#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    CONSOLE_INPUT_MAX_LINE = 128U,
    CONSOLE_INPUT_RX_BUFFER_SIZE = 256U,
};

/* 轮询式 UART 行输入：环形缓冲 + 最小行编辑，供常驻 shell 读取完整命令行。 */
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

/* 初始化输入状态：清空 RX 环形缓冲与当前行。 */
void console_input_init(console_input_state_t* state);
/* 复位当前行（保留 RX 缓冲），用于行处理完毕或溢出后清空。 */
void console_input_reset(console_input_state_t* state);
/* 直接从 UART 读 raw 字节到 out，返回读到的字节数（不进环形缓冲）。 */
size_t console_input_read_raw(uint8_t* out, size_t out_size);
/* 把一个中断收到的字节压入 RX 环形缓冲，满则置溢出标记。 */
bool console_input_push_interrupt_byte(console_input_state_t* state,
                                       uint8_t ch);
/* 轮询 UART RX，把就绪字节全部压入环形缓冲。 */
void console_input_drain_uart_rx_interrupt(console_input_state_t* state);
/* supervisor external 中断 post-handler：校验 UART 源后 drain RX。 */
void console_input_supervisor_external_post_handler(uint32_t source_id,
                                                    void* context);
/* 推进行编辑：消费缓冲/UART 字节，遇到回车返回完整行，溢出返回 OVERFLOW。 */
console_input_poll_result_t console_input_poll(console_input_state_t* state);
