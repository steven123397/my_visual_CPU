#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "../../guest/include/course_fs.h"
#include "../../guest/include/course_memory.h"
#include "../../guest/include/course_process.h"
#include "../../guest/include/course_scheduler.h"
#include "../../guest/include/course_sync.h"
#include "../../guest/include/procfs.h"

static int fail(const char* message) {
    fprintf(stderr, "%s\n", message);
    return 1;
}

static bool contains(const char* haystack, const char* needle) {
    return strstr(haystack, needle) != NULL;
}

static void add_fixed_workload(course_scheduler_t* scheduler) {
    course_scheduler_add_task(scheduler, 1U, 0U, 5U);
    course_scheduler_add_task(scheduler, 2U, 1U, 3U);
    course_scheduler_add_task(scheduler, 3U, 2U, 2U);
}

static int test_scheduler_stage3_metrics(void) {
    course_scheduler_t scheduler;
    course_scheduler_summary_t summary;
    static course_fs_t fs;
    course_memory_t memory;
    procfs_t procfs;
    char out[512];

    course_scheduler_init(&scheduler);
    add_fixed_workload(&scheduler);
    if (!course_scheduler_run(&scheduler, COURSE_SCHED_POLICY_FCFS, 0U) ||
        !course_scheduler_summary(&scheduler, &summary) ||
        summary.context_switches != 3U ||
        summary.time_slice != 0U ||
        summary.preempt_count != 0U ||
        summary.average_wait_time != 3U ||
        summary.average_turnaround_time != 6U ||
        strcmp(summary.last_policy_name, "FCFS") != 0) {
        return fail("expected FCFS average wait/turnaround metrics");
    }

    if (!course_scheduler_run(&scheduler, COURSE_SCHED_POLICY_RR, 2U) ||
        !course_scheduler_summary(&scheduler, &summary) ||
        summary.context_switches != 6U ||
        summary.time_slice != 2U ||
        summary.preempt_count != 3U ||
        summary.average_wait_time != 4U ||
        summary.average_turnaround_time != 7U ||
        strcmp(summary.last_policy_name, "RR") != 0) {
        return fail("expected RR time slice and preempt metrics");
    }

    if (!course_scheduler_run(&scheduler, COURSE_SCHED_POLICY_CFS_LITE, 2U) ||
        !course_scheduler_summary(&scheduler, &summary) ||
        strcmp(summary.last_policy_name, "CFS-lite") != 0) {
        return fail("expected CFS-lite policy name to remain stable");
    }

    course_fs_mkfs(&fs);
    course_memory_init(&memory, 2U);
    procfs_init(&procfs, &scheduler, &memory, &fs);
    if (!procfs_read(&procfs, "/proc/schedstat", out, sizeof(out)) ||
        !contains(out, "policy=CFS-lite") ||
        !contains(out, "time_slice=2") ||
        !contains(out, "context_switches=") ||
        !contains(out, "preempts=") ||
        !contains(out, "avg_wait=") ||
        !contains(out, "avg_turnaround=")) {
        return fail("expected /proc/schedstat to expose Stage 3 metrics");
    }

    return 0;
}

static int test_semaphore_and_mutex_state_transitions(void) {
    course_process_table_t table;
    course_process_t* owner = NULL;
    course_process_t* waiter = NULL;
    course_semaphore_t semaphore;
    course_mutex_t mutex;

    course_process_table_init(&table);
    owner = course_process_spawn(&table, 0U, "owner");
    waiter = course_process_spawn(&table, 0U, "waiter");
    if (owner == NULL || waiter == NULL) {
        return fail("expected test processes");
    }

    course_semaphore_init(&semaphore, &table, 0);
    if (course_semaphore_wait(&semaphore, waiter->pid) != COURSE_SYNC_BLOCKED ||
        waiter->state != COURSE_PROCESS_BLOCKED ||
        course_semaphore_post(&semaphore) != COURSE_SYNC_OK ||
        waiter->state != COURSE_PROCESS_READY) {
        return fail("expected semaphore wait to block and post to wake");
    }

    course_mutex_init(&mutex, &table);
    if (course_mutex_lock(&mutex, owner->pid) != COURSE_SYNC_OK ||
        mutex.owner_pid != owner->pid ||
        course_mutex_lock(&mutex, waiter->pid) != COURSE_SYNC_BLOCKED ||
        waiter->state != COURSE_PROCESS_BLOCKED ||
        course_mutex_unlock(&mutex, waiter->pid) != COURSE_SYNC_ERR_NOT_OWNER ||
        course_mutex_unlock(&mutex, owner->pid) != COURSE_SYNC_OK ||
        waiter->state != COURSE_PROCESS_READY ||
        mutex.owner_pid != waiter->pid) {
        return fail("expected mutex owner, blocked waiter and misuse guard");
    }

    return 0;
}

int main(void) {
    if (test_scheduler_stage3_metrics() != 0 ||
        test_semaphore_and_mutex_state_transitions() != 0) {
        return 1;
    }

    return 0;
}
