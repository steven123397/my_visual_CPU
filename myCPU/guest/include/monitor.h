#pragma once

#include <stddef.h>
#include <stdint.h>

#include "kernel_runtime.h"

void monitor_write_char(char ch);
void monitor_write_text(const char* text);
void monitor_write_line(const char* text);
void monitor_write_banner(void);
void monitor_write_prompt(void);
void monitor_write_uint64(uint64_t value);
void monitor_write_hex64(uint64_t value);
void monitor_write_preview_ascii(const uint8_t* data, size_t length);
void monitor_run(kernel_runtime_t* runtime) __attribute__((noreturn));
