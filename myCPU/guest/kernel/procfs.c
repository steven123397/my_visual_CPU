#include "procfs.h"

#include "course_fd.h"

static bool str_eq(const char* a, const char* b) {
    size_t i = 0;

    if (a == NULL || b == NULL) {
        return false;
    }
    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i]) {
            return false;
        }
        i += 1U;
    }
    return a[i] == b[i];
}

static bool append_char(char* out, size_t out_size, size_t* used, char ch) {
    if (out == NULL || used == NULL || *used + 1U >= out_size) {
        return false;
    }

    out[*used] = ch;
    *used += 1U;
    out[*used] = '\0';
    return true;
}

static bool append_str(char* out,
                       size_t out_size,
                       size_t* used,
                       const char* value) {
    size_t i = 0;

    if (value == NULL) {
        return false;
    }

    while (value[i] != '\0') {
        if (!append_char(out, out_size, used, value[i])) {
            return false;
        }
        i += 1U;
    }
    return true;
}

static bool append_u32(char* out,
                       size_t out_size,
                       size_t* used,
                       uint32_t value) {
    char digits[10];
    size_t count = 0;

    if (value == 0) {
        return append_char(out, out_size, used, '0');
    }

    while (value != 0 && count < sizeof(digits)) {
        digits[count] = (char)('0' + (value % 10U));
        value /= 10U;
        count += 1U;
    }
    while (count > 0) {
        count -= 1U;
        if (!append_char(out, out_size, used, digits[count])) {
            return false;
        }
    }
    return true;
}

static bool append_i32(char* out,
                       size_t out_size,
                       size_t* used,
                       int32_t value) {
    uint32_t magnitude = 0;

    if (value < 0) {
        if (!append_char(out, out_size, used, '-')) {
            return false;
        }
        magnitude = (uint32_t)(-(value + 1)) + 1U;
    } else {
        magnitude = (uint32_t)value;
    }
    return append_u32(out, out_size, used, magnitude);
}

static bool append_key_value_u32(char* out,
                                 size_t out_size,
                                 size_t* used,
                                 const char* key,
                                 uint32_t value) {
    return append_str(out, out_size, used, key) &&
           append_char(out, out_size, used, '=') &&
           append_u32(out, out_size, used, value) &&
           append_char(out, out_size, used, '\n');
}

static bool append_key_value_i32(char* out,
                                 size_t out_size,
                                 size_t* used,
                                 const char* key,
                                 int32_t value) {
    return append_str(out, out_size, used, key) &&
           append_char(out, out_size, used, '=') &&
           append_i32(out, out_size, used, value) &&
           append_char(out, out_size, used, '\n');
}

static bool parse_u32_component(const char* text,
                                size_t len,
                                uint32_t* out_value) {
    uint32_t value = 0;
    size_t i = 0;

    if (text == NULL || out_value == NULL || len == 0) {
        return false;
    }
    for (i = 0; i < len; ++i) {
        if (text[i] < '0' || text[i] > '9') {
            return false;
        }
        value = (value * 10U) + (uint32_t)(text[i] - '0');
    }
    *out_value = value;
    return true;
}

static const course_process_t* find_const_process(
    const course_process_table_t* table,
    uint32_t pid) {
    size_t i = 0;

    if (table == NULL || pid == 0) {
        return NULL;
    }
    for (i = 0; i < COURSE_PROCESS_MAX_PROCESSES; ++i) {
        const course_process_t* process = course_process_at(table, i);

        if (process != NULL && process->pid == pid &&
            process->state != COURSE_PROCESS_DEAD &&
            process->state != COURSE_PROCESS_UNUSED) {
            return process;
        }
    }
    return NULL;
}

static bool read_ps(const procfs_t* procfs, char* out, size_t out_size) {
    course_scheduler_summary_t summary;
    course_scheduler_task_stats_t stats[COURSE_SCHEDULER_MAX_TASKS];
    size_t used = 0;
    size_t i = 0;

    if (procfs->processes != NULL) {
        for (i = 0; i < COURSE_PROCESS_MAX_PROCESSES; ++i) {
            const course_process_t* process =
                course_process_at(procfs->processes, i);

            if (process == NULL || process->state == COURSE_PROCESS_DEAD ||
                process->state == COURSE_PROCESS_UNUSED) {
                continue;
            }
            if (!append_str(out, out_size, &used, "pid=") ||
                !append_u32(out, out_size, &used, process->pid) ||
                !append_str(out, out_size, &used, " ppid=") ||
                !append_u32(out, out_size, &used, process->ppid) ||
                !append_str(out, out_size, &used, " state=") ||
                !append_str(out,
                            out_size,
                            &used,
                            course_process_state_name(process->state)) ||
                !append_str(out, out_size, &used, " name=") ||
                !append_str(out, out_size, &used, process->name) ||
                !append_str(out, out_size, &used, " exit=") ||
                !append_i32(out, out_size, &used, process->exit_code) ||
                !append_char(out, out_size, &used, '\n')) {
                return false;
            }
        }
        return true;
    }

    if (!course_scheduler_summary(procfs->scheduler, &summary) ||
        !course_scheduler_task_stats(procfs->scheduler,
                                     stats,
                                     COURSE_SCHEDULER_MAX_TASKS) ||
        !append_str(out, out_size, &used, "policy=") ||
        !append_str(out,
                    out_size,
                    &used,
                    course_scheduler_policy_name(summary.policy)) ||
        !append_char(out, out_size, &used, '\n')) {
        return false;
    }

    for (i = 0; i < procfs->scheduler->task_count; ++i) {
        if (!append_str(out, out_size, &used, "pid=") ||
            !append_u32(out, out_size, &used, stats[i].pid) ||
            !append_str(out, out_size, &used, " wait=") ||
            !append_u32(out, out_size, &used, stats[i].waiting_time) ||
            !append_str(out, out_size, &used, " turnaround=") ||
            !append_u32(out, out_size, &used, stats[i].turnaround_time) ||
            !append_str(out, out_size, &used, " vruntime=") ||
            !append_u32(out, out_size, &used, stats[i].vruntime) ||
            !append_char(out, out_size, &used, '\n')) {
            return false;
        }
    }

    return true;
}

static bool read_meminfo(const procfs_t* procfs, char* out, size_t out_size) {
    course_memory_stats_t stats;
    size_t used = 0;

    return course_memory_stats(procfs->memory, &stats) &&
           append_key_value_u32(out, out_size, &used, "total", stats.total_pages) &&
           append_key_value_u32(out, out_size, &used, "free", stats.free_pages) &&
           append_key_value_u32(out, out_size, &used, "used", stats.used_pages) &&
           append_key_value_u32(out,
                                out_size,
                                &used,
                                "page_fault",
                                stats.page_faults) &&
           append_key_value_u32(out,
                                out_size,
                                &used,
                                "page_reclaim",
                                stats.page_reclaims);
}

static bool read_schedstat(const procfs_t* procfs, char* out, size_t out_size) {
    course_scheduler_summary_t summary;
    size_t used = 0;

    return course_scheduler_summary(procfs->scheduler, &summary) &&
           append_str(out, out_size, &used, "policy=") &&
           append_str(out,
                      out_size,
                      &used,
                      course_scheduler_policy_name(summary.policy)) &&
           append_char(out, out_size, &used, '\n') &&
           append_key_value_u32(out,
                                out_size,
                                &used,
                                "context_switches",
                                summary.context_switches) &&
           append_key_value_u32(out,
                                out_size,
                                &used,
                                "time_slice",
                                summary.time_slice) &&
           append_key_value_u32(out,
                                out_size,
                                &used,
                                "preempts",
                                summary.preempt_count) &&
           append_key_value_u32(out,
                                out_size,
                                &used,
                                "avg_wait",
                                summary.average_wait_time) &&
           append_key_value_u32(out,
                                out_size,
                                &used,
                                "avg_turnaround",
                                summary.average_turnaround_time) &&
           append_key_value_u32(out,
                                out_size,
                                &used,
                                "fcfs_runs",
                                summary.policy_runs[COURSE_SCHED_POLICY_FCFS]) &&
           append_key_value_u32(out,
                                out_size,
                                &used,
                                "rr_runs",
                                summary.policy_runs[COURSE_SCHED_POLICY_RR]) &&
           append_key_value_u32(
               out,
               out_size,
               &used,
               "cfs_lite_runs",
               summary.policy_runs[COURSE_SCHED_POLICY_CFS_LITE]);
}

static bool read_fsstat(const procfs_t* procfs, char* out, size_t out_size) {
    course_fs_stats_t stats;
    size_t used = 0;

    return course_fs_stats(procfs->fs, &stats) &&
           append_key_value_u32(out,
                                out_size,
                                &used,
                                "file_creates",
                                stats.file_creates) &&
           append_key_value_u32(out, out_size, &used, "dir_creates", stats.dir_creates) &&
           append_key_value_u32(out, out_size, &used, "file_reads", stats.file_reads) &&
           append_key_value_u32(out,
                                out_size,
                                &used,
                                "file_writes",
                                stats.file_writes) &&
           append_key_value_u32(out,
                                out_size,
                                &used,
                                "file_deletes",
                                stats.file_deletes) &&
           append_key_value_u32(out, out_size, &used, "dir_deletes", stats.dir_deletes) &&
           append_key_value_u32(out,
                                out_size,
                                &used,
                                "path_resolves",
                                stats.path_resolves) &&
           append_key_value_u32(out,
                                out_size,
                                &used,
                                "dir_index_lookups",
                                stats.dir_index_lookups) &&
           append_key_value_u32(out,
                                out_size,
                                &used,
                                "btree_internal_nodes",
                                stats.btree_internal_nodes) &&
           append_key_value_u32(out,
                                out_size,
                                &used,
                                "btree_leaf_nodes",
                                stats.btree_leaf_nodes) &&
           append_key_value_u32(out,
                                out_size,
                                &used,
                                "btree_compare_steps",
                                stats.btree_compare_steps) &&
           append_key_value_u32(out,
                                out_size,
                                &used,
                                "open_calls",
                                stats.open_calls) &&
           append_key_value_u32(out,
                                out_size,
                                &used,
                                "close_calls",
                                stats.close_calls) &&
           append_key_value_u32(out,
                                out_size,
                                &used,
                                "seek_calls",
                                stats.seek_calls) &&
           append_key_value_u32(out,
                                out_size,
                                &used,
                                "max_files",
                                stats.max_files) &&
           append_key_value_u32(out,
                                out_size,
                                &used,
                                "max_file_size",
                                stats.max_file_size) &&
           append_key_value_u32(out,
                                out_size,
                                &used,
                                "max_depth",
                                stats.max_depth);
}

static bool read_syscalls(const procfs_t* procfs, char* out, size_t out_size) {
    course_syscall_stats_t stats;
    size_t used = 0;
    uint32_t i = 0;

    if (procfs->syscalls == NULL ||
        !course_syscall_stats(procfs->syscalls, &stats) ||
        !append_key_value_u32(out,
                              out_size,
                              &used,
                              "total_calls",
                              stats.total_calls) ||
        !append_key_value_u32(out,
                              out_size,
                              &used,
                              "failures",
                              stats.failures) ||
        !append_key_value_i32(out,
                              out_size,
                              &used,
                              "last_error",
                              stats.last_error)) {
        return false;
    }

    for (i = 0; i < COURSE_SYSCALL_COUNT; ++i) {
        if (!append_key_value_u32(out,
                                  out_size,
                                  &used,
                                  course_syscall_name(i),
                                  stats.calls[i])) {
            return false;
        }
    }
    return true;
}

static bool read_cow(const procfs_t* procfs, char* out, size_t out_size) {
    course_process_cow_stats_t stats;
    size_t used = 0;

    if (procfs->processes == NULL ||
        !course_process_cow_stats(procfs->processes, &stats)) {
        return false;
    }

    return append_key_value_u32(out,
                                out_size,
                                &used,
                                "mapped_pages",
                                stats.mapped_pages) &&
           append_key_value_u32(out,
                                out_size,
                                &used,
                                "shared_pages",
                                stats.shared_pages) &&
           append_key_value_u32(out,
                                out_size,
                                &used,
                                "cow_faults",
                                stats.cow_faults) &&
           append_key_value_u32(out,
                                out_size,
                                &used,
                                "saved_pages",
                                stats.saved_pages) &&
           append_key_value_u32(out,
                                out_size,
                                &used,
                                "copied_pages",
                                stats.copied_pages) &&
           append_key_value_u32(out,
                                out_size,
                                &used,
                                "refcount_peak",
                                stats.refcount_peak) &&
           append_key_value_u32(out,
                                out_size,
                                &used,
                                "released_pages",
                                stats.released_pages) &&
           append_str(out, out_size, &used, "leak_free=") &&
           append_str(out, out_size, &used, stats.leak_free ? "yes" : "no") &&
           append_char(out, out_size, &used, '\n');
}

static bool read_crashlog(const procfs_t* procfs, char* out, size_t out_size) {
    size_t i = 0;
    size_t used = 0;
    const course_process_t* latest = NULL;

    if (procfs->processes == NULL) {
        return false;
    }

    for (i = 0; i < COURSE_PROCESS_MAX_PROCESSES; ++i) {
        const course_process_t* process =
            course_process_at(procfs->processes, i);

        if (process == NULL ||
            process->exit_code != COURSE_PROCESS_EXIT_CRASH ||
            process->crash_reason[0] == '\0') {
            continue;
        }
        if (latest == NULL || process->pid >= latest->pid) {
            latest = process;
        }
    }

    if (latest == NULL) {
        return append_str(out, out_size, &used, "none\n");
    }

    return append_str(out, out_size, &used, "pid=") &&
           append_u32(out, out_size, &used, latest->pid) &&
           append_str(out, out_size, &used, " name=") &&
           append_str(out, out_size, &used, latest->name) &&
           append_str(out, out_size, &used, " sepc=") &&
           append_u32(out, out_size, &used, (uint32_t)latest->crash_sepc) &&
           append_str(out, out_size, &used, " scause=") &&
           append_u32(out, out_size, &used, (uint32_t)latest->crash_scause) &&
           append_str(out, out_size, &used, " stval=") &&
           append_u32(out, out_size, &used, (uint32_t)latest->crash_stval) &&
           append_str(out, out_size, &used, " reason=") &&
           append_str(out, out_size, &used, latest->crash_reason) &&
           append_char(out, out_size, &used, '\n');
}

static bool read_cpuinfo(const procfs_t* procfs, char* out, size_t out_size) {
    size_t used = 0;

    (void)procfs;
    return append_str(out, out_size, &used, "isa=rv64im\n") &&
           append_str(out, out_size, &used, "backend=myCPU\n") &&
           append_str(out, out_size, &used, "stage=kernel_alpha_stage3\n");
}

static bool read_uptime(const procfs_t* procfs, char* out, size_t out_size) {
    size_t used = 0;
    uint32_t ticks = 1U;

    if (procfs != NULL && procfs->scheduler != NULL) {
        course_scheduler_summary_t summary;

        if (course_scheduler_summary(procfs->scheduler, &summary)) {
            ticks += summary.context_switches + summary.total_turnaround_time;
        }
    }
    return append_key_value_u32(out, out_size, &used, "ticks", ticks);
}

static bool read_pid_status(const procfs_t* procfs,
                            uint32_t pid,
                            char* out,
                            size_t out_size) {
    const course_process_t* process =
        procfs != NULL ? find_const_process(procfs->processes, pid) : NULL;
    size_t used = 0;

    if (process == NULL) {
        return false;
    }
    return append_key_value_u32(out, out_size, &used, "pid", process->pid) &&
           append_key_value_u32(out, out_size, &used, "ppid", process->ppid) &&
           append_str(out, out_size, &used, "state=") &&
           append_str(out,
                      out_size,
                      &used,
                      course_process_state_name(process->state)) &&
           append_char(out, out_size, &used, '\n') &&
           append_str(out, out_size, &used, "name=") &&
           append_str(out, out_size, &used, process->name) &&
           append_char(out, out_size, &used, '\n') &&
           append_key_value_i32(out,
                                out_size,
                                &used,
                                "exit_code",
                                process->exit_code) &&
           append_str(out, out_size, &used, "crash=") &&
           append_str(out,
                      out_size,
                      &used,
                      process->crash_reason[0] != '\0' ? "yes" : "no") &&
           append_char(out, out_size, &used, '\n');
}

static const char* fd_kind_name(course_fd_kind_t kind) {
    switch (kind) {
    case COURSE_FD_KIND_STDIO:
        return "stdio";
    case COURSE_FD_KIND_FILE:
        return "file";
    case COURSE_FD_KIND_PROC:
        return "proc";
    case COURSE_FD_KIND_UNUSED:
    default:
        return "unused";
    }
}

static bool read_pid_fd(const procfs_t* procfs,
                        uint32_t pid,
                        char* out,
                        size_t out_size) {
    size_t used = 0;
    size_t i = 0;

    if (procfs == NULL || procfs->processes == NULL ||
        find_const_process(procfs->processes, pid) == NULL ||
        procfs->fd_table == NULL || procfs->fd_owner_pid != pid) {
        return false;
    }
    for (i = 0; i < COURSE_FD_MAX_OPEN; ++i) {
        const course_fd_entry_t* entry = &procfs->fd_table->entries[i];

        if (entry->kind == COURSE_FD_KIND_UNUSED) {
            continue;
        }
        if (!append_str(out, out_size, &used, "fd=") ||
            !append_u32(out, out_size, &used, (uint32_t)i) ||
            !append_str(out, out_size, &used, " kind=") ||
            !append_str(out, out_size, &used, fd_kind_name(entry->kind))) {
            return false;
        }
        if (entry->path[0] != '\0') {
            if (!append_str(out, out_size, &used, " path=") ||
                !append_str(out, out_size, &used, entry->path)) {
                return false;
            }
        }
        if (!append_char(out, out_size, &used, '\n')) {
            return false;
        }
    }
    return true;
}

static bool read_pid_maps(const procfs_t* procfs,
                          uint32_t pid,
                          char* out,
                          size_t out_size) {
    const course_process_t* process =
        procfs != NULL ? find_const_process(procfs->processes, pid) : NULL;
    size_t used = 0;
    size_t i = 0;

    if (process == NULL) {
        return false;
    }
    for (i = 0; i < process->map_count; ++i) {
        const course_elf_map_t* map = &process->maps[i];

        if (!append_str(out, out_size, &used, map->name) ||
            !append_str(out, out_size, &used, " start=") ||
            !append_u32(out, out_size, &used, (uint32_t)map->start) ||
            !append_str(out, out_size, &used, " end=") ||
            !append_u32(out, out_size, &used, (uint32_t)map->end) ||
            !append_str(out, out_size, &used, " cow=") ||
            !append_str(out, out_size, &used, map->cow ? "yes" : "no") ||
            !append_char(out, out_size, &used, '\n')) {
            return false;
        }
    }
    return process->map_count > 0U;
}

static bool read_pid_node(const procfs_t* procfs,
                          const char* path,
                          char* out,
                          size_t out_size) {
    const char* pid_start = path + 6U;
    const char* slash = pid_start;
    uint32_t pid = 0;
    size_t pid_len = 0;

    while (*slash != '\0' && *slash != '/') {
        slash += 1;
    }
    if (*slash != '/') {
        return false;
    }
    pid_len = (size_t)(slash - pid_start);
    if (!parse_u32_component(pid_start, pid_len, &pid)) {
        return false;
    }
    slash += 1;
    if (str_eq(slash, "status")) {
        return read_pid_status(procfs, pid, out, out_size);
    }
    if (str_eq(slash, "fd")) {
        return read_pid_fd(procfs, pid, out, out_size);
    }
    if (str_eq(slash, "maps")) {
        return read_pid_maps(procfs, pid, out, out_size);
    }
    return false;
}

void procfs_init(procfs_t* procfs,
                 const course_scheduler_t* scheduler,
                 const course_memory_t* memory,
                 const course_fs_t* fs) {
    if (procfs == NULL) {
        return;
    }

    procfs->scheduler = scheduler;
    procfs->memory = memory;
    procfs->fs = fs;
    procfs->syscalls = NULL;
    procfs->processes = NULL;
    procfs->fd_table = NULL;
    procfs->fd_owner_pid = 0;
}

bool procfs_read(const procfs_t* procfs,
                 const char* path,
                 char* out,
                 size_t out_size) {
    if (procfs == NULL || procfs->scheduler == NULL || procfs->memory == NULL ||
        procfs->fs == NULL || path == NULL || out == NULL || out_size == 0) {
        return false;
    }

    out[0] = '\0';
    if (str_eq(path, "/proc/ps")) {
        return read_ps(procfs, out, out_size);
    }
    if (str_eq(path, "/proc/meminfo")) {
        return read_meminfo(procfs, out, out_size);
    }
    if (str_eq(path, "/proc/schedstat")) {
        return read_schedstat(procfs, out, out_size);
    }
    if (str_eq(path, "/proc/fsstat")) {
        return read_fsstat(procfs, out, out_size);
    }
    if (str_eq(path, "/proc/syscalls")) {
        return read_syscalls(procfs, out, out_size);
    }
    if (str_eq(path, "/proc/cow")) {
        return read_cow(procfs, out, out_size);
    }
    if (str_eq(path, "/proc/crashlog")) {
        return read_crashlog(procfs, out, out_size);
    }
    if (str_eq(path, "/proc/cpuinfo")) {
        return read_cpuinfo(procfs, out, out_size);
    }
    if (str_eq(path, "/proc/uptime")) {
        return read_uptime(procfs, out, out_size);
    }
    if (path[0] == '/' && path[1] == 'p' && path[2] == 'r' &&
        path[3] == 'o' && path[4] == 'c' && path[5] == '/') {
        return read_pid_node(procfs, path, out, out_size);
    }

    return false;
}

bool procfs_write(procfs_t* procfs,
                  const char* path,
                  const char* data,
                  size_t size) {
    (void)procfs;
    (void)path;
    (void)data;
    (void)size;
    return false;
}

bool procfs_attach_syscalls(procfs_t* procfs,
                            const course_syscall_t* syscalls) {
    if (procfs == NULL || syscalls == NULL) {
        return false;
    }

    procfs->syscalls = syscalls;
    return true;
}

bool procfs_attach_processes(procfs_t* procfs,
                             const course_process_table_t* processes) {
    if (procfs == NULL || processes == NULL) {
        return false;
    }

    procfs->processes = processes;
    return true;
}

bool procfs_attach_fd_table(procfs_t* procfs,
                            uint32_t owner_pid,
                            const struct CourseFdTable* fd_table) {
    if (procfs == NULL || fd_table == NULL || owner_pid == 0U) {
        return false;
    }
    procfs->fd_table = fd_table;
    procfs->fd_owner_pid = owner_pid;
    return true;
}
