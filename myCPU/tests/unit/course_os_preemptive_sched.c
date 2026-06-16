#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "../../guest/include/course_fs.h"
#include "../../guest/include/course_memory.h"
#include "../../guest/include/course_process.h"
#include "../../guest/include/course_scheduler.h"
#include "../../guest/include/procfs.h"
#include "../../guest/include/riscv.h"

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

static bool prepare_online_scheduler(course_process_table_t* table,
                                     course_online_scheduler_t* scheduler,
                                     course_sched_policy_t policy,
                                     uint32_t time_slice) {
    course_process_t* first = NULL;
    course_process_t* second = NULL;
    course_process_t* third = NULL;

    course_process_table_init(table);
    first = course_process_spawn(table, 0U, "first");
    second = course_process_spawn(table, 0U, "second");
    third = course_process_spawn(table, 0U, "third");
    course_online_scheduler_init(scheduler);
    return first != NULL && second != NULL && third != NULL &&
           course_online_scheduler_bind_process_table(scheduler, table) &&
           course_online_scheduler_configure(scheduler, policy, time_slice) &&
           course_online_scheduler_add_process(scheduler, first->pid) &&
           course_online_scheduler_add_process(scheduler, second->pid) &&
           course_online_scheduler_add_process(scheduler, third->pid);
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

static int test_online_rr_preempts_after_time_slice(void) {
    course_process_table_t table;
    course_online_scheduler_t scheduler;
    course_online_scheduler_summary_t summary;
    course_process_t* first = NULL;
    course_process_t* second = NULL;

    if (!prepare_online_scheduler(&table,
                                  &scheduler,
                                  COURSE_SCHED_POLICY_RR,
                                  2U)) {
        return fail("expected online RR scheduler setup");
    }
    first = course_process_find(&table, 1U);
    second = course_process_find(&table, 2U);
    if (first == NULL || second == NULL ||
        !course_online_scheduler_tick(&scheduler) ||
        !course_online_scheduler_tick(&scheduler) ||
        first->state != COURSE_PROCESS_RUNNING ||
        second->state != COURSE_PROCESS_READY ||
        !course_online_scheduler_tick(&scheduler) ||
        !course_online_scheduler_summary(&scheduler, &summary) ||
        first->state != COURSE_PROCESS_READY ||
        second->state != COURSE_PROCESS_RUNNING ||
        summary.current_pid != second->pid ||
        summary.context_switches != 2U ||
        summary.preempt_count != 1U ||
        summary.last_switch_cycle_cost != 1U ||
        summary.total_switch_cycle_cost != 2U) {
        return fail("expected online RR to preempt on time-slice expiry");
    }

    return 0;
}

static int test_online_fcfs_does_not_preempt_running_process(void) {
    course_process_table_t table;
    course_online_scheduler_t scheduler;
    course_online_scheduler_summary_t summary;
    course_process_t* first = NULL;
    course_process_t* second = NULL;

    if (!prepare_online_scheduler(&table,
                                  &scheduler,
                                  COURSE_SCHED_POLICY_FCFS,
                                  0U)) {
        return fail("expected online FCFS scheduler setup");
    }
    first = course_process_find(&table, 1U);
    second = course_process_find(&table, 2U);
    if (first == NULL || second == NULL ||
        !course_online_scheduler_tick(&scheduler) ||
        !course_online_scheduler_tick(&scheduler) ||
        !course_online_scheduler_tick(&scheduler) ||
        !course_online_scheduler_summary(&scheduler, &summary) ||
        first->state != COURSE_PROCESS_RUNNING ||
        second->state != COURSE_PROCESS_READY ||
        summary.current_pid != first->pid ||
        summary.context_switches != 1U ||
        summary.preempt_count != 0U) {
        return fail("expected online FCFS to keep the running process");
    }

    return 0;
}

static int test_online_cfs_lite_picks_lowest_vruntime(void) {
    course_process_table_t table;
    course_online_scheduler_t scheduler;
    course_online_scheduler_summary_t summary;
    course_process_t* first = NULL;
    course_process_t* second = NULL;

    if (!prepare_online_scheduler(&table,
                                  &scheduler,
                                  COURSE_SCHED_POLICY_CFS_LITE,
                                  1U)) {
        return fail("expected online CFS-lite scheduler setup");
    }
    first = course_process_find(&table, 1U);
    second = course_process_find(&table, 2U);
    if (first == NULL || second == NULL ||
        !course_online_scheduler_tick(&scheduler) ||
        first->state != COURSE_PROCESS_RUNNING ||
        !course_online_scheduler_tick(&scheduler) ||
        !course_online_scheduler_summary(&scheduler, &summary) ||
        first->state != COURSE_PROCESS_READY ||
        second->state != COURSE_PROCESS_RUNNING ||
        summary.current_pid != second->pid ||
        summary.context_switches != 2U) {
        return fail("expected online CFS-lite to pick the lowest vruntime");
    }

    return 0;
}

static int test_online_scheduler_skips_blocked_zombie_and_timer_drives_tick(void) {
    course_process_table_t table;
    course_online_scheduler_t scheduler;
    course_online_scheduler_summary_t summary;
    course_process_t* first = NULL;
    course_process_t* second = NULL;

    if (!prepare_online_scheduler(&table,
                                  &scheduler,
                                  COURSE_SCHED_POLICY_RR,
                                  1U)) {
        return fail("expected online scheduler setup for blocked/zombie test");
    }
    first = course_process_find(&table, 1U);
    second = course_process_find(&table, 2U);
    if (first == NULL || second == NULL ||
        !course_process_set_state(&table, first->pid, COURSE_PROCESS_BLOCKED) ||
        !course_process_set_state(&table, second->pid, COURSE_PROCESS_ZOMBIE) ||
        !course_online_scheduler_tick(&scheduler) ||
        !course_online_scheduler_summary(&scheduler, &summary) ||
        summary.current_pid != 3U ||
        summary.idle_ticks != 0U ||
        !course_process_set_state(&table, 3U, COURSE_PROCESS_BLOCKED)) {
        return fail("expected online scheduler to skip blocked and zombie processes");
    }

    course_online_scheduler_timer_post_handler(RISCV_SUPERVISOR_TIMER_INTERRUPT,
                                               &scheduler);
    if (!course_online_scheduler_summary(&scheduler, &summary) ||
        summary.current_pid != 0U ||
        summary.idle_ticks != 1U ||
        summary.context_switches != 1U) {
        return fail("expected timer post handler to drive an idle online tick");
    }

    return 0;
}

int main(void) {
    if (test_scheduler_records_switch_cycle_cost() != 0 ||
        test_procfs_schedstat_exposes_switch_cycle_cost() != 0 ||
        test_online_rr_preempts_after_time_slice() != 0 ||
        test_online_fcfs_does_not_preempt_running_process() != 0 ||
        test_online_cfs_lite_picks_lowest_vruntime() != 0 ||
        test_online_scheduler_skips_blocked_zombie_and_timer_drives_tick() != 0) {
        return 1;
    }

    return 0;
}
