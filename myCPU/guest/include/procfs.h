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

/* 课程 procfs 是只读证据面：把调度、内存、文件系统、syscall、COW 和进程状态
   格式化成文本节点，供 shell、FD 层和展示截图读取。 */
typedef const struct CourseFdTable* (*procfs_fd_table_resolver_t)(
    const void* context,
    uint32_t pid);

typedef struct Procfs {
    /* 这些指针都由课程 OS 编排层注入；procfs 不拥有任何子系统。 */
    const course_scheduler_t* scheduler;
    const course_memory_t* memory;
    const course_fs_t* fs;
    const course_syscall_t* syscalls;
    const course_process_table_t* processes;
    const struct CourseFdTable* fd_table;
    uint32_t fd_owner_pid;
    procfs_fd_table_resolver_t fd_table_resolver;
    const void* fd_table_resolver_context;
} procfs_t;

/* 初始化 procfs，注入调度/内存/FS 三个核心子系统指针（其余后续按需 attach）。 */
void procfs_init(procfs_t* procfs,
                 const course_scheduler_t* scheduler,
                 const course_memory_t* memory,
                 const course_fs_t* fs);
/* 按路径读取对应 /proc 节点文本到 out；未知路径返回 false。 */
bool procfs_read(const procfs_t* procfs,
                 const char* path,
                 char* out,
                 size_t out_size);
/* procfs 是只读面，写入恒返回 false。 */
bool procfs_write(procfs_t* procfs,
                  const char* path,
                  const char* data,
                  size_t size);
/* 注入 syscall 统计来源，供 /proc/syscalls 输出。 */
bool procfs_attach_syscalls(procfs_t* procfs,
                            const course_syscall_t* syscalls);
/* 注入进程表，供 /proc/ps、/proc/<pid> 等节点读取。 */
bool procfs_attach_processes(procfs_t* procfs,
                             const course_process_table_t* processes);
/* 注入某 pid 的 FD 表，供 /proc/<pid>/fd 输出。 */
bool procfs_attach_fd_table(procfs_t* procfs,
                            uint32_t owner_pid,
                            const struct CourseFdTable* fd_table);
/* 注入按 pid 解析 FD 表的回调，支持多进程 FD 查询。 */
bool procfs_attach_fd_table_resolver(procfs_t* procfs,
                                     procfs_fd_table_resolver_t resolver,
                                     const void* context);
