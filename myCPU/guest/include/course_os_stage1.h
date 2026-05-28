#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "course_fs.h"
#include "course_memory.h"
#include "course_scheduler.h"
#include "procfs.h"

typedef struct CourseOsStage1 {
    course_scheduler_t scheduler;
    course_memory_t memory;
    course_fs_t fs;
    procfs_t procfs;
} course_os_stage1_t;

void course_os_stage1_init(course_os_stage1_t* stage);
bool course_os_stage1_run(course_os_stage1_t* stage);
bool course_os_stage1_summary(const course_os_stage1_t* stage,
                              char* out,
                              size_t out_size);
