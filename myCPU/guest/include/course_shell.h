#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "course_fd.h"
#include "linux_compat.h"
#include "course_memory.h"
#include "course_process.h"
#include "course_scheduler.h"
#include "course_sync.h"
#include "trap.h"
#include "vm.h"

#define COURSE_SHELL_MAX_ARGS 16U
#define COURSE_SHELL_MAX_ARG_LEN 32U
#define COURSE_SHELL_MAX_PIPELINE_STAGES 8U
#define COURSE_SHELL_MAX_TRANSCRIPT 2048U
#define COURSE_SHELL_COMMAND_OUTPUT_SIZE 16384U
#define COURSE_SHELL_LINE_OUTPUT_SIZE COURSE_SHELL_COMMAND_OUTPUT_SIZE
#define COURSE_SHELL_LINUX_COMPAT_TRAP_STACK_SIZE 16384U
#define COURSE_SHELL_EXTERNAL_ELF_MAX 4096U
#define COURSE_SHELL_COMMAND_SCRATCH_DEPTH 4U

typedef struct CourseShellSimpleCommand {
    char argv[COURSE_SHELL_MAX_ARGS][COURSE_SHELL_MAX_ARG_LEN];
    size_t argc;
} course_shell_simple_command_t;

typedef struct CourseShellCommand {
    course_shell_simple_command_t pipeline[COURSE_SHELL_MAX_PIPELINE_STAGES];
    size_t pipeline_len;
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
    linux_compat_runtime_t linux_compat_runtime;
    linux_compat_trace_t linux_trace;
    vm_process_t linux_compat_process;
    trap_user_runtime_t linux_compat_user_runtime;
    course_semaphore_t semaphore;
    course_mutex_t mutex;
    bool semaphore_initialized;
    bool mutex_initialized;
    uint8_t linux_compat_trap_stack[COURSE_SHELL_LINUX_COMPAT_TRAP_STACK_SIZE]
        __attribute__((aligned(TRAP_USER_RUNTIME_STACK_ALIGNMENT)));
    course_shell_command_t command_scratch[COURSE_SHELL_COMMAND_SCRATCH_DEPTH];
    size_t command_scratch_depth;
    uint8_t external_elf_scratch[COURSE_SHELL_EXTERNAL_ELF_MAX];
    char command_output_scratch[COURSE_SHELL_COMMAND_OUTPUT_SIZE];
    char line_output_scratch[COURSE_SHELL_LINE_OUTPUT_SIZE];
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
