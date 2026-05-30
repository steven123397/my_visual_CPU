#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "course_shell.h"

#define COURSE_OS_STAGE3_MARKER \
    "course-os-stage3 elf=5 libc=ok sched=fcfs/rr/cfs sync=sem/mutex vm=sv39-cow fs=seek/mkfs shell=script proc=cpuinfo/uptime/pid"

typedef struct CourseOsStage3 {
    course_shell_t shell;
    bool elf_libc_ok;
    bool sched_sync_ok;
    bool vm_cow_ok;
    bool fs_shell_ok;
    bool proc_ok;
} course_os_stage3_t;

void course_os_stage3_init(course_os_stage3_t* stage);
bool course_os_stage3_run(course_os_stage3_t* stage);
bool course_os_stage3_summary(const course_os_stage3_t* stage,
                              char* out,
                              size_t out_size);
