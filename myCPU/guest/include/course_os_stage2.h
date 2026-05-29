#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "course_shell.h"
#include "course_syscall.h"

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

void course_os_stage2_init(course_os_stage2_t* stage);
bool course_os_stage2_run(course_os_stage2_t* stage);
bool course_os_stage2_summary(const course_os_stage2_t* stage,
                              char* out,
                              size_t out_size);
