#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "course_shell.h"

/* Stage3 固定 marker：确认 ELF/libc、三策略调度、同步、COW、脚本和扩展 procfs。 */
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

/* 初始化 Stage3：shell + 各能力标记。 */
void course_os_stage3_init(course_os_stage3_t* stage);
/* 预置 Stage3 demo 文件并准备好常驻 shell 状态（供 course_os_shell 入口复用）。 */
bool course_os_stage3_prepare_shell(course_os_stage3_t* stage);
/* 跑 Stage3 smoke：ELF/libc、三策略调度、同步、Sv39 COW、FS/shell 脚本、扩展 procfs。 */
bool course_os_stage3_run(course_os_stage3_t* stage);
/* 输出 Stage3 固定 marker summary。 */
bool course_os_stage3_summary(const course_os_stage3_t* stage,
                              char* out,
                              size_t out_size);
