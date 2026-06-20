#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "course_shell.h"

/* Shell 到 Linux compat 的桥接接口：显式 linux 命令和 PATH fallback 都走这里。 */
/* 执行显式 `linux <path-or-command> [args]`：fork 子进程、建 VM、转 linux_compat_run。 */
bool course_shell_run_linux_command(course_shell_t* shell,
                                    const course_shell_simple_command_t* command,
                                    const char* stdin_text,
                                    char* out,
                                    size_t out_size,
                                    bool* command_success);
/* PATH fallback：把无 `/` 的命令解析成绝对路径后改写成显式 linux 命令再执行。 */
bool course_shell_run_linux_fallback_command(
    course_shell_t* shell,
    const course_shell_simple_command_t* command,
    const char* stdin_text,
    char* out,
    size_t out_size,
    bool* command_success);
