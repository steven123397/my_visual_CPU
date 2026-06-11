#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "course_shell.h"

bool course_shell_run_linux_command(course_shell_t* shell,
                                    const course_shell_simple_command_t* command,
                                    const char* stdin_text,
                                    char* out,
                                    size_t out_size,
                                    bool* command_success);
bool course_shell_run_linux_fallback_command(
    course_shell_t* shell,
    const course_shell_simple_command_t* command,
    const char* stdin_text,
    char* out,
    size_t out_size,
    bool* command_success);
