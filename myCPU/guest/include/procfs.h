#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "course_fs.h"
#include "course_memory.h"
#include "course_process.h"
#include "course_scheduler.h"
#include "course_syscall.h"

struct CourseFdTable;

typedef struct Procfs {
    const course_scheduler_t* scheduler;
    const course_memory_t* memory;
    const course_fs_t* fs;
    const course_syscall_t* syscalls;
    const course_process_table_t* processes;
    const struct CourseFdTable* fd_table;
    uint32_t fd_owner_pid;
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
bool procfs_attach_syscalls(procfs_t* procfs,
                            const course_syscall_t* syscalls);
bool procfs_attach_processes(procfs_t* procfs,
                             const course_process_table_t* processes);
bool procfs_attach_fd_table(procfs_t* procfs,
                            uint32_t owner_pid,
                            const struct CourseFdTable* fd_table);
