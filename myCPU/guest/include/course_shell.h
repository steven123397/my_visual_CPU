#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "course_fd.h"
#include "linux_compat.h"
#include "course_memory.h"
#include "course_process.h"
#include "course_scheduler.h"

#define COURSE_SHELL_MAX_ARGS 8U
#define COURSE_SHELL_MAX_ARG_LEN 32U
#define COURSE_SHELL_MAX_TRANSCRIPT 2048U

typedef struct CourseShellSimpleCommand {
    char argv[COURSE_SHELL_MAX_ARGS][COURSE_SHELL_MAX_ARG_LEN];
    size_t argc;
} course_shell_simple_command_t;

typedef struct CourseShellCommand {
    course_shell_simple_command_t left;
    course_shell_simple_command_t right;
    bool has_pipe;
    bool has_output_redirect;
    bool has_input_redirect;
    char output_path[COURSE_FD_MAX_PATH];
    char input_path[COURSE_FD_MAX_PATH];
} course_shell_command_t;

typedef struct CourseShell {
    course_fs_t fs;
    course_scheduler_t scheduler;
    course_memory_t memory;
    course_process_table_t processes;
    procfs_t procfs;
    course_fd_table_t fds;
    course_syscall_t syscalls;
    linux_compat_trace_t linux_trace;
    char transcript[COURSE_SHELL_MAX_TRANSCRIPT];
    size_t transcript_size;
    uint32_t shell_pid;
} course_shell_t;

bool course_shell_parse(const char* line, course_shell_command_t* out_command);
void course_shell_init(course_shell_t* shell);
bool course_shell_run_line(course_shell_t* shell,
                           const char* line,
                           char* out,
                           size_t out_size);
bool course_shell_transcript(const course_shell_t* shell,
                             char* out,
                             size_t out_size);
