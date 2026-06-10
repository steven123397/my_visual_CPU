#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define COURSE_SCHEDULER_MAX_TASKS 8U

typedef enum CourseSchedPolicy {
    COURSE_SCHED_POLICY_FCFS = 0,
    COURSE_SCHED_POLICY_RR,
    COURSE_SCHED_POLICY_CFS_LITE,
    COURSE_SCHED_POLICY_COUNT,
} course_sched_policy_t;

typedef struct CourseSchedulerTaskStats {
    uint32_t pid;
    uint32_t arrival_time;
    uint32_t burst_time;
    uint32_t completion_time;
    uint32_t waiting_time;
    uint32_t turnaround_time;
    uint32_t vruntime;
} course_scheduler_task_stats_t;

typedef struct CourseSchedulerSummary {
    course_sched_policy_t policy;
    uint32_t context_switches;
    uint32_t time_slice;
    uint32_t preempt_count;
    uint32_t total_wait_time;
    uint32_t total_turnaround_time;
    uint32_t average_wait_time;
    uint32_t average_turnaround_time;
    uint32_t policy_runs[COURSE_SCHED_POLICY_COUNT];
    const char* last_policy_name;
} course_scheduler_summary_t;

typedef struct CourseSchedulerTask {
    bool used;
    uint32_t pid;
    uint32_t arrival_time;
    uint32_t burst_time;
    uint32_t remaining_time;
    uint32_t completion_time;
    uint32_t vruntime;
} course_scheduler_task_t;

typedef struct CourseScheduler {
    course_scheduler_task_t tasks[COURSE_SCHEDULER_MAX_TASKS];
    size_t task_count;
    course_scheduler_summary_t summary;
} course_scheduler_t;

void course_scheduler_init(course_scheduler_t* scheduler);
bool course_scheduler_add_task(course_scheduler_t* scheduler,
                               uint32_t pid,
                               uint32_t arrival_time,
                               uint32_t burst_time);
bool course_scheduler_run(course_scheduler_t* scheduler,
                          course_sched_policy_t policy,
                          uint32_t time_slice);
bool course_scheduler_summary(const course_scheduler_t* scheduler,
                              course_scheduler_summary_t* out_summary);
bool course_scheduler_task_stats(const course_scheduler_t* scheduler,
                                 course_scheduler_task_stats_t* out_stats,
                                 size_t max_stats);
const char* course_scheduler_policy_name(course_sched_policy_t policy);
