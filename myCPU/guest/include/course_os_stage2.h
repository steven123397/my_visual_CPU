#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "course_shell.h"
#include "course_syscall.h"

/* Stage2 固定 marker：确认 syscall、进程/FD/FS/shell、COW 和崩溃隔离 guardrail。 */
#define COURSE_OS_STAGE2_MARKER \
    "course-os-stage2 syscall=ok shell=ok procs=ok fd=ok fs=128/64K/3 pipe=ok cow=ok crash=isolated proc=ps/meminfo/schedstat/fsstat/syscalls/cow/crashlog"

typedef struct CourseOsStage2 {
    course_shell_t shell;
    course_syscall_t syscalls;
    bool bad_syscall_guarded;
    bool bad_user_pointer_guarded;
    bool bad_fd_guarded;
    bool proc_write_guarded;
    bool user_crash_guarded;
    bool cow_write_guarded;
    bool pipe_misuse_guarded;
} course_os_stage2_t;

/* 初始化 Stage2：shell/syscall + 各类 guardrail 标记。 */
void course_os_stage2_init(course_os_stage2_t* stage);
/* 跑 Stage2 smoke：syscall/进程/FD/FS/shell/管道/COW/崩溃隔离 + 负向 guardrail。 */
bool course_os_stage2_run(course_os_stage2_t* stage);
/* 输出 Stage2 固定 marker summary。 */
bool course_os_stage2_summary(const course_os_stage2_t* stage,
                              char* out,
                              size_t out_size);
