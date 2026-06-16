#include <stdbool.h>
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

static void add_cost_workload(course_scheduler_t* scheduler) {
    course_scheduler_add_task(scheduler, 1U, 0U, 5U);
    course_scheduler_add_task(scheduler, 2U, 1U, 3U);
    course_scheduler_add_task(scheduler, 3U, 2U, 2U);
}

static int test_scheduler_records_switch_cycle_cost(void) {
    course_scheduler_t scheduler;
    course_scheduler_summary_t summary;

    course_scheduler_init(&scheduler);
    add_cost_workload(&scheduler);
    if (!course_scheduler_run(&scheduler, COURSE_SCHED_POLICY_RR, 2U) ||
        !course_scheduler_summary(&scheduler, &summary) ||
        summary.context_switches != 6U ||
        summary.last_switch_cycle_cost != 1U ||
        summary.total_switch_cycle_cost != 10U) {
        return fail("expected RR scheduler to record switch cycle costs");
    }

    return 0;
}

static int test_procfs_schedstat_exposes_switch_cycle_cost(void) {
    course_scheduler_t scheduler;
    course_memory_t memory;
    static course_fs_t fs;
    procfs_t procfs;
    char out[512];

    course_scheduler_init(&scheduler);
    add_cost_workload(&scheduler);
    if (!course_scheduler_run(&scheduler, COURSE_SCHED_POLICY_RR, 2U)) {
        return fail("expected scheduler run before procfs read");
    }

    course_memory_init(&memory, 2U);
    course_fs_init(&fs);
    procfs_init(&procfs, &scheduler, &memory, &fs);
    if (!procfs_read(&procfs, "/proc/schedstat", out, sizeof(out)) ||
        !contains(out, "last_switch_cycle_cost=1") ||
        !contains(out, "total_switch_cycle_cost=10") ||
        contains(out, "switch_time_")) {
        return fail("expected /proc/schedstat to expose cycle-only switch cost");
    }

    return 0;
}

int main(void) {
    if (test_scheduler_records_switch_cycle_cost() != 0 ||
        test_procfs_schedstat_exposes_switch_cycle_cost() != 0) {
        return 1;
    }

    return 0;
}
