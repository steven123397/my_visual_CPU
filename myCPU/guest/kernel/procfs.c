#include "procfs.h"

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

static bool read_ps(const procfs_t* procfs, char* out, size_t out_size) {
    course_scheduler_summary_t summary;
    course_scheduler_task_stats_t stats[COURSE_SCHEDULER_MAX_TASKS];
    size_t used = 0;
    size_t i = 0;

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
                                stats.btree_compare_steps);
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
