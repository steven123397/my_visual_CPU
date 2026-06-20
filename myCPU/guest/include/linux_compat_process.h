#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef LINUX_COMPAT_MAX_PATH
#define LINUX_COMPAT_MAX_PATH 128U
#endif

#ifndef LINUX_COMPAT_MAX_PROCESSES
#define LINUX_COMPAT_MAX_PROCESSES 4U
#endif

/* Linux compat 小型进程表：记录 exec/wait/clone 需要的旁路状态。 */
typedef struct LinuxCompatProcess {
    bool used;
    uint32_t pid;
    uint32_t ppid;
    bool exited;
    int32_t exit_code;
    char path[LINUX_COMPAT_MAX_PATH];
    char cwd[LINUX_COMPAT_MAX_PATH];
} linux_compat_process_t;

typedef struct LinuxCompatProcessTable {
    linux_compat_process_t processes[LINUX_COMPAT_MAX_PROCESSES];
    uint32_t current_pid;
    uint32_t next_pid;
    uint64_t clone_count;
    uint32_t last_clone_flags;
    uint64_t last_clone_stack;
} linux_compat_process_table_t;

/* 初始化进程表，设当前 cwd。 */
void linux_compat_process_table_init(linux_compat_process_table_t* table,
                                     const char* cwd);

/* 按 pid 查可写进程。 */
linux_compat_process_t* linux_compat_process_table_find_mut(
    linux_compat_process_table_t* table,
    uint32_t pid);

/* 按 pid 查只读进程。 */
const linux_compat_process_t* linux_compat_process_table_find(
    const linux_compat_process_table_t* table,
    uint32_t pid);

/* 设置当前进程的 cwd。 */
bool linux_compat_process_table_set_current_cwd(
    linux_compat_process_table_t* table,
    const char* cwd);

/* 设置当前进程的 exec 路径。 */
bool linux_compat_process_table_set_current_exec_path(
    linux_compat_process_table_t* table,
    const char* path);

/* 生成一个 helper 子进程（clone 旁路），记录路径/cwd/退出码。 */
linux_compat_process_t* linux_compat_process_table_spawn_helper(
    linux_compat_process_table_t* table,
    uint32_t ppid,
    const char* path,
    const char* cwd,
    int32_t exit_code);

/* 标记进程已退出并记退出码与回收者。 */
bool linux_compat_process_table_mark_exited(
    linux_compat_process_table_t* table,
    uint32_t pid,
    int32_t exit_code,
    uint32_t reaper_pid);

/* wait 已退出的子进程，输出状态（pid=-1 等任意子）。 */
int64_t linux_compat_process_table_wait_exited(
    linux_compat_process_table_t* table,
    uint32_t parent_pid,
    int32_t pid,
    int32_t* out_status);
