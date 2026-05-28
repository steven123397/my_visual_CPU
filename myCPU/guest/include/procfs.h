#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "course_fs.h"
#include "course_memory.h"
#include "course_scheduler.h"

typedef struct Procfs {
    const course_scheduler_t* scheduler;
    const course_memory_t* memory;
    const course_fs_t* fs;
} procfs_t;

void procfs_init(procfs_t* procfs,
                 const course_scheduler_t* scheduler,
                 const course_memory_t* memory,
                 const course_fs_t* fs);
bool procfs_read(const procfs_t* procfs,
                 const char* path,
                 char* out,
                 size_t out_size);
bool procfs_write(procfs_t* procfs,
                  const char* path,
                  const char* data,
                  size_t size);
