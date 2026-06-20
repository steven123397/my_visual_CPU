#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define COURSE_SYSCALL_IO_BUFFER_SIZE 256U

/* 课程 syscall ABI：面向课程用户程序的固定小号表。
   Linux 兼容 syscall 走 linux_compat_*，这里保持教学语义和清晰的错误返回。 */
struct CourseFdTable;
struct CourseProcessTable;

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
    /* user_base/user_size 是简化的用户指针沙箱，所有入参指针先经过范围检查。 */
    uintptr_t user_base;
    size_t user_size;
    bool exited;
    int32_t exit_code;
    char stdout_buffer[COURSE_SYSCALL_IO_BUFFER_SIZE];
    size_t stdout_size;
    char stderr_buffer[COURSE_SYSCALL_IO_BUFFER_SIZE];
    size_t stderr_size;
    course_syscall_stats_t stats;
    /* FD 表和进程表由外部注入，syscall 层只做分发和错误码转换。 */
    struct CourseFdTable* fd_table;
    struct CourseProcessTable* process_table;
} course_syscall_t;

/* 初始化 syscall 上下文：记录 pid 与用户指针沙箱范围，清空缓冲与统计。 */
void course_syscall_init(course_syscall_t* syscalls,
                         uint32_t pid,
                         uintptr_t user_base,
                         size_t user_size);
/* 按 syscall 号分发到对应 handler，统一做指针检查与错误码/统计记录。 */
int64_t course_syscall_dispatch(course_syscall_t* syscalls,
                                uint32_t number,
                                uint64_t arg0,
                                uint64_t arg1,
                                uint64_t arg2,
                                uint64_t arg3);
/* 判断 user_ptr..+size 是否完全落在用户沙箱范围内。 */
bool course_syscall_user_range_valid(const course_syscall_t* syscalls,
                                     uintptr_t user_ptr,
                                     size_t size);
/* 拷贝出 syscall 调用统计。 */
bool course_syscall_stats(const course_syscall_t* syscalls,
                          course_syscall_stats_t* out_stats);
/* 注入 FD 表，供 read/write/open 等转发。 */
bool course_syscall_attach_fd_table(course_syscall_t* syscalls,
                                    struct CourseFdTable* fd_table);
/* 注入进程表，供 fork/exec/wait 等转发。 */
bool course_syscall_attach_process_table(
    course_syscall_t* syscalls,
    struct CourseProcessTable* process_table);
/* 判断已捕获的 stdout 是否等于 expected。 */
bool course_syscall_stdout_equals(const course_syscall_t* syscalls,
                                  const char* expected);
/* 判断已捕获的 stderr 是否等于 expected。 */
bool course_syscall_stderr_equals(const course_syscall_t* syscalls,
                                  const char* expected);
/* 进程是否已通过 exit syscall 退出。 */
bool course_syscall_exited(const course_syscall_t* syscalls);
/* 返回 exit syscall 记录的退出码。 */
int32_t course_syscall_exit_code(const course_syscall_t* syscalls);
/* 把 syscall 号转成展示用字符串名。 */
const char* course_syscall_name(uint32_t number);
