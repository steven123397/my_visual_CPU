#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../../guest/include/course_fs.h"
#include "../../guest/include/course_memory.h"
#include "../../guest/include/course_scheduler.h"
#include "../../guest/include/procfs.h"

static int fail(const char* message) {
    fprintf(stderr, "%s\n", message);
    return 1;
}

static bool contains(const char* haystack, const char* needle) {
    return strstr(haystack, needle) != NULL;
}

static int test_scheduler_policies_and_stats(void) {
    course_scheduler_t scheduler;
    course_scheduler_task_stats_t stats[3];
    course_scheduler_summary_t summary;

    course_scheduler_init(&scheduler);
    if (!course_scheduler_add_task(&scheduler, 1U, 0U, 5U) ||
        !course_scheduler_add_task(&scheduler, 2U, 1U, 3U) ||
        !course_scheduler_add_task(&scheduler, 3U, 2U, 1U)) {
        return fail("expected scheduler to accept the stage1 workload");
    }

    if (!course_scheduler_run(&scheduler, COURSE_SCHED_POLICY_FCFS, 2U) ||
        !course_scheduler_summary(&scheduler, &summary) ||
        summary.policy != COURSE_SCHED_POLICY_FCFS ||
        summary.context_switches != 3U ||
        summary.total_wait_time != 10U ||
        summary.total_turnaround_time != 19U) {
        return fail("expected FCFS stats to expose wait/turnaround/switches");
    }

    if (!course_scheduler_task_stats(&scheduler, stats, 3U) ||
        stats[0].waiting_time != 0U || stats[0].turnaround_time != 5U ||
        stats[1].waiting_time != 4U || stats[1].turnaround_time != 7U ||
        stats[2].waiting_time != 6U || stats[2].turnaround_time != 7U) {
        return fail("expected per-task FCFS stats to be stable");
    }

    if (!course_scheduler_run(&scheduler, COURSE_SCHED_POLICY_RR, 2U) ||
        !course_scheduler_summary(&scheduler, &summary) ||
        summary.policy != COURSE_SCHED_POLICY_RR ||
        summary.context_switches != 6U ||
        summary.total_wait_time != 10U ||
        summary.total_turnaround_time != 19U) {
        fprintf(stderr,
                "actual RR policy=%d ctx=%u wait=%u turnaround=%u\n",
                (int)summary.policy,
                summary.context_switches,
                summary.total_wait_time,
                summary.total_turnaround_time);
        return fail("expected RR stats to use the configured time slice");
    }

    if (!course_scheduler_run(&scheduler, COURSE_SCHED_POLICY_CFS_LITE, 1U) ||
        !course_scheduler_summary(&scheduler, &summary) ||
        summary.policy != COURSE_SCHED_POLICY_CFS_LITE ||
        summary.context_switches != 9U ||
        summary.total_wait_time != 7U ||
        summary.total_turnaround_time != 16U ||
        summary.policy_runs[COURSE_SCHED_POLICY_FCFS] != 1U ||
        summary.policy_runs[COURSE_SCHED_POLICY_RR] != 1U ||
        summary.policy_runs[COURSE_SCHED_POLICY_CFS_LITE] != 1U) {
        return fail("expected CFS-lite and per-policy run stats");
    }

    return 0;
}

static int test_memory_demand_clock_and_kmalloc(void) {
    course_memory_t memory;
    course_memory_stats_t stats;
    void* first = NULL;
    void* second = NULL;
    void* reused = NULL;

    course_memory_init(&memory, 3U);
    if (!course_memory_touch(&memory, 0U, true) ||
        !course_memory_touch(&memory, 1U, true) ||
        !course_memory_touch(&memory, 2U, false) ||
        !course_memory_touch(&memory, 3U, true) ||
        !course_memory_stats(&memory, &stats) ||
        stats.total_pages != 3U || stats.free_pages != 0U ||
        stats.used_pages != 3U || stats.page_faults != 4U ||
        stats.page_reclaims != 1U) {
        return fail("expected demand paging and Clock reclaim stats");
    }

    first = course_kmalloc(&memory, 24U);
    second = course_kmalloc(&memory, 16U);
    course_kfree(&memory, first);
    reused = course_kmalloc(&memory, 12U);
    if (first == NULL || second == NULL || reused != first ||
        !course_memory_stats(&memory, &stats) ||
        stats.kmalloc_allocs != 3U || stats.kfree_calls != 1U ||
        stats.kmalloc_reuses != 1U) {
        return fail("expected kmalloc/kfree to reuse a freed block");
    }

    return 0;
}

static int test_course_fs_crud_seek_btree_stats(void) {
    static course_fs_t fs;
    course_fs_stats_t before_lookup;
    course_fs_stats_t after_lookup;
    course_fs_stats_t stats;
    char data[16];

    course_fs_init(&fs);
    if (!course_fs_mkdir(&fs, "/home") ||
        !course_fs_mkdir(&fs, "/home/course") ||
        !course_fs_create(&fs, "/home/course/a.txt", false) ||
        !course_fs_create(&fs, "/home/course/m.txt", false) ||
        !course_fs_create(&fs, "/home/course/n.txt", false) ||
        !course_fs_create(&fs, "/home/course/o.txt", false) ||
        !course_fs_create(&fs, "/home/course/p.txt", false) ||
        !course_fs_create(&fs, "/home/course/q.txt", false) ||
        !course_fs_create(&fs, "/home/course/r.txt", false) ||
        !course_fs_create(&fs, "/home/course/s.txt", false) ||
        !course_fs_create(&fs, "/home/course/t.txt", false) ||
        !course_fs_create(&fs, "/home/course/u.txt", false) ||
        !course_fs_create(&fs, "/home/course/z.txt", false) ||
        !course_fs_write(&fs, "/home/course/m.txt", 0U, "hello", 5U) ||
        !course_fs_write(&fs, "/home/course/m.txt", 8U, "os", 2U) ||
        !course_fs_read(&fs, "/home/course/m.txt", 8U, data, 2U)) {
        return fail("expected course fs CRUD and seek workload to succeed");
    }
    data[2] = '\0';
    if (strcmp(data, "os") != 0) {
        return fail("expected seek read to return data written at offset");
    }

    if (!course_fs_stats(&fs, &before_lookup) ||
        !course_fs_lookup(&fs, "/home/course/z.txt") ||
        !course_fs_stats(&fs, &after_lookup) ||
        !course_fs_unlink(&fs, "/home/course/a.txt") ||
        !course_fs_stats(&fs, &stats) ||
        stats.file_creates != 11U || stats.dir_creates != 2U ||
        stats.file_writes != 2U || stats.file_reads != 1U ||
        stats.path_resolves < 8U || stats.dir_index_lookups == 0U ||
        stats.btree_compare_steps == 0U ||
        stats.btree_internal_nodes == 0U || stats.btree_leaf_nodes < 2U ||
        after_lookup.btree_compare_steps - before_lookup.btree_compare_steps >
            8U ||
        stats.file_deletes != 1U) {
        return fail("expected fs stats to expose CRUD/path/B-tree evidence");
    }

    return 0;
}

static int test_procfs_readonly_outputs(void) {
    course_scheduler_t scheduler;
    course_memory_t memory;
    static course_fs_t fs;
    procfs_t procfs;
    char out[512];

    course_scheduler_init(&scheduler);
    course_scheduler_add_task(&scheduler, 1U, 0U, 4U);
    course_scheduler_add_task(&scheduler, 2U, 1U, 2U);
    course_scheduler_run(&scheduler, COURSE_SCHED_POLICY_CFS_LITE, 1U);

    course_memory_init(&memory, 2U);
    course_memory_touch(&memory, 0U, true);
    course_memory_touch(&memory, 1U, true);
    course_memory_touch(&memory, 2U, false);

    course_fs_init(&fs);
    course_fs_mkdir(&fs, "/tmp");
    course_fs_create(&fs, "/tmp/log", false);
    course_fs_write(&fs, "/tmp/log", 0U, "x", 1U);
    course_fs_lookup(&fs, "/tmp/log");

    procfs_init(&procfs, &scheduler, &memory, &fs);
    if (!procfs_read(&procfs, "/proc/ps", out, sizeof(out)) ||
        !contains(out, "policy=CFS-lite") || !contains(out, "pid=1") ||
        !contains(out, "wait=") || !contains(out, "turnaround=")) {
        return fail("expected /proc/ps to expose task stats");
    }

    if (!procfs_read(&procfs, "/proc/meminfo", out, sizeof(out)) ||
        !contains(out, "total=") || !contains(out, "free=") ||
        !contains(out, "page_fault=") || !contains(out, "page_reclaim=")) {
        return fail("expected /proc/meminfo to expose memory stats");
    }

    if (!procfs_read(&procfs, "/proc/schedstat", out, sizeof(out)) ||
        !contains(out, "context_switches=") ||
        !contains(out, "fcfs_runs=") || !contains(out, "rr_runs=") ||
        !contains(out, "cfs_lite_runs=")) {
        return fail("expected /proc/schedstat to expose scheduler stats");
    }

    if (!procfs_read(&procfs, "/proc/fsstat", out, sizeof(out)) ||
        !contains(out, "file_creates=") ||
        !contains(out, "path_resolves=") ||
        !contains(out, "btree_internal_nodes=") ||
        !contains(out, "btree_leaf_nodes=") ||
        !contains(out, "btree_compare_steps=")) {
        return fail("expected /proc/fsstat to expose fs stats");
    }

    if (procfs_write(&procfs, "/proc/schedstat", "policy=RR", 9U)) {
        return fail("expected procfs to be read-only in stage1");
    }

    return 0;
}

int main(void) {
    if (test_scheduler_policies_and_stats() != 0 ||
        test_memory_demand_clock_and_kmalloc() != 0 ||
        test_course_fs_crud_seek_btree_stats() != 0 ||
        test_procfs_readonly_outputs() != 0) {
        return 1;
    }

    return 0;
}
