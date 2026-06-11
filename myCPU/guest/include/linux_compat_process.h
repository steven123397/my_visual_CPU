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

void linux_compat_process_table_init(linux_compat_process_table_t* table,
                                     const char* cwd);

linux_compat_process_t* linux_compat_process_table_find_mut(
    linux_compat_process_table_t* table,
    uint32_t pid);

const linux_compat_process_t* linux_compat_process_table_find(
    const linux_compat_process_table_t* table,
    uint32_t pid);

bool linux_compat_process_table_set_current_cwd(
    linux_compat_process_table_t* table,
    const char* cwd);

bool linux_compat_process_table_set_current_exec_path(
    linux_compat_process_table_t* table,
    const char* path);

linux_compat_process_t* linux_compat_process_table_spawn_helper(
    linux_compat_process_table_t* table,
    uint32_t ppid,
    const char* path,
    const char* cwd,
    int32_t exit_code);

bool linux_compat_process_table_mark_exited(
    linux_compat_process_table_t* table,
    uint32_t pid,
    int32_t exit_code,
    uint32_t reaper_pid);

int64_t linux_compat_process_table_wait_exited(
    linux_compat_process_table_t* table,
    uint32_t parent_pid,
    int32_t pid,
    int32_t* out_status);
