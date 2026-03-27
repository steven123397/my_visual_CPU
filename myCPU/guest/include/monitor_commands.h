#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "kernel_runtime.h"

void monitor_commands_reset(uint64_t boot_mtime);
bool monitor_execute_line(kernel_runtime_t* runtime, const char* line);
