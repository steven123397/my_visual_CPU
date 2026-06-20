#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "course_fs.h"
#include "course_memory.h"
#include "course_scheduler.h"
#include "procfs.h"

/* Stage1 smoke 状态：基础调度、内存、文件系统和 procfs 证据的最小组合。 */
typedef struct CourseOsStage1 {
    course_scheduler_t scheduler;
    course_memory_t memory;
    course_fs_t fs;
    procfs_t procfs;
} course_os_stage1_t;

/* 初始化 Stage1：调度/内存/FS/procfs 实例并互相关联。 */
void course_os_stage1_init(course_os_stage1_t* stage);
/* 跑 Stage1 smoke：调度三种策略、demand paging、FS CRUD、procfs 证据。 */
bool course_os_stage1_run(course_os_stage1_t* stage);
/* 输出 Stage1 固定 marker summary（调度/内存/FS/proc 统计）。 */
bool course_os_stage1_summary(const course_os_stage1_t* stage,
                              char* out,
                              size_t out_size);
