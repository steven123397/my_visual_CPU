#pragma once

#include "kernel_runtime.h"

void monitor_write_text(const char* text);
void monitor_write_line(const char* text);
void monitor_write_banner(void);
void monitor_write_prompt(void);
void monitor_run(kernel_runtime_t* runtime) __attribute__((noreturn));
