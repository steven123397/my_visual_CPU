#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define COURSE_SYSCALL_IO_BUFFER_SIZE 256U

typedef enum CourseSyscallNumber {
    COURSE_SYSCALL_READ = 0,
    COURSE_SYSCALL_WRITE = 1,
    COURSE_SYSCALL_OPEN = 2,
    COURSE_SYSCALL_CLOSE = 3,
    COURSE_SYSCALL_SEEK = 4,
    COURSE_SYSCALL_EXIT = 5,
    COURSE_SYSCALL_FORK = 6,
    COURSE_SYSCALL_EXEC = 7,
    COURSE_SYSCALL_WAIT = 8,
    COURSE_SYSCALL_WAITPID = 9,
    COURSE_SYSCALL_GETPID = 10,
    COURSE_SYSCALL_PS = 11,
    COURSE_SYSCALL_KILL = 12,
    COURSE_SYSCALL_COUNT = 13,
} course_syscall_number_t;

typedef enum CourseSyscallError {
    COURSE_SYSCALL_OK = 0,
    COURSE_SYSCALL_ERR_INVALID_SYSCALL = -1,
    COURSE_SYSCALL_ERR_BAD_USER_POINTER = -2,
    COURSE_SYSCALL_ERR_BAD_FD = -3,
    COURSE_SYSCALL_ERR_NO_SUCH_FILE = -4,
    COURSE_SYSCALL_ERR_NO_CHILD = -5,
    COURSE_SYSCALL_ERR_NO_MEMORY = -6,
    COURSE_SYSCALL_ERR_PERMISSION_DENIED = -7,
} course_syscall_error_t;

typedef struct CourseSyscallStats {
    uint32_t total_calls;
    uint32_t calls[COURSE_SYSCALL_COUNT];
    uint32_t failures;
    int32_t last_error;
} course_syscall_stats_t;

typedef struct CourseSyscall {
    uint32_t pid;
    uintptr_t user_base;
    size_t user_size;
    bool exited;
    int32_t exit_code;
    char stdout_buffer[COURSE_SYSCALL_IO_BUFFER_SIZE];
    size_t stdout_size;
    char stderr_buffer[COURSE_SYSCALL_IO_BUFFER_SIZE];
    size_t stderr_size;
    course_syscall_stats_t stats;
} course_syscall_t;

void course_syscall_init(course_syscall_t* syscalls,
                         uint32_t pid,
                         uintptr_t user_base,
                         size_t user_size);
int64_t course_syscall_dispatch(course_syscall_t* syscalls,
                                uint32_t number,
                                uint64_t arg0,
                                uint64_t arg1,
                                uint64_t arg2,
                                uint64_t arg3);
bool course_syscall_user_range_valid(const course_syscall_t* syscalls,
                                     uintptr_t user_ptr,
                                     size_t size);
bool course_syscall_stats(const course_syscall_t* syscalls,
                          course_syscall_stats_t* out_stats);
bool course_syscall_stdout_equals(const course_syscall_t* syscalls,
                                  const char* expected);
bool course_syscall_stderr_equals(const course_syscall_t* syscalls,
                                  const char* expected);
bool course_syscall_exited(const course_syscall_t* syscalls);
int32_t course_syscall_exit_code(const course_syscall_t* syscalls);
const char* course_syscall_name(uint32_t number);
