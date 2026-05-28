#include "course_scheduler.h"

#include <stddef.h>

static void reset_runtime_fields(course_scheduler_t* scheduler) {
    size_t i = 0;

    for (i = 0; i < scheduler->task_count; ++i) {
        scheduler->tasks[i].remaining_time = scheduler->tasks[i].burst_time;
        scheduler->tasks[i].completion_time = 0;
        scheduler->tasks[i].vruntime = 0;
    }
}

static bool all_tasks_complete(const course_scheduler_t* scheduler) {
    size_t i = 0;

    for (i = 0; i < scheduler->task_count; ++i) {
        if (scheduler->tasks[i].remaining_time != 0) {
            return false;
        }
    }

    return true;
}

static uint32_t next_arrival_time(const course_scheduler_t* scheduler,
                                  uint32_t now) {
    size_t i = 0;
    uint32_t next = UINT32_MAX;

    for (i = 0; i < scheduler->task_count; ++i) {
        const course_scheduler_task_t* task = &scheduler->tasks[i];

        if (task->remaining_time != 0 && task->arrival_time > now &&
            task->arrival_time < next) {
            next = task->arrival_time;
        }
    }

    return next;
}

static size_t pick_fcfs(const course_scheduler_t* scheduler, uint32_t now) {
    size_t i = 0;
    size_t best = COURSE_SCHEDULER_MAX_TASKS;

    for (i = 0; i < scheduler->task_count; ++i) {
        const course_scheduler_task_t* task = &scheduler->tasks[i];

        if (task->remaining_time == 0 || task->arrival_time > now) {
            continue;
        }
        if (best == COURSE_SCHEDULER_MAX_TASKS ||
            task->arrival_time < scheduler->tasks[best].arrival_time ||
            (task->arrival_time == scheduler->tasks[best].arrival_time &&
             task->pid < scheduler->tasks[best].pid)) {
            best = i;
        }
    }

    return best;
}

static size_t pick_cfs_lite(const course_scheduler_t* scheduler, uint32_t now) {
    size_t i = 0;
    size_t best = COURSE_SCHEDULER_MAX_TASKS;

    for (i = 0; i < scheduler->task_count; ++i) {
        const course_scheduler_task_t* task = &scheduler->tasks[i];

        if (task->remaining_time == 0 || task->arrival_time > now) {
            continue;
        }
        if (best == COURSE_SCHEDULER_MAX_TASKS ||
            task->vruntime < scheduler->tasks[best].vruntime ||
            (task->vruntime == scheduler->tasks[best].vruntime &&
             task->arrival_time < scheduler->tasks[best].arrival_time) ||
            (task->vruntime == scheduler->tasks[best].vruntime &&
             task->arrival_time == scheduler->tasks[best].arrival_time &&
             task->pid < scheduler->tasks[best].pid)) {
            best = i;
        }
    }

    return best;
}

static bool task_ready(const course_scheduler_task_t* task, uint32_t now) {
    return task != NULL && task->remaining_time != 0 && task->arrival_time <= now;
}

static bool rr_queue_contains(const size_t* queue,
                              size_t queue_count,
                              size_t task_index) {
    size_t i = 0;

    for (i = 0; i < queue_count; ++i) {
        if (queue[i] == task_index) {
            return true;
        }
    }

    return false;
}

static void rr_enqueue_arrivals(const course_scheduler_t* scheduler,
                                size_t* queue,
                                size_t* queue_count,
                                bool* discovered,
                                uint32_t now) {
    size_t i = 0;

    for (i = 0; i < scheduler->task_count; ++i) {
        if (!discovered[i] && task_ready(&scheduler->tasks[i], now) &&
            !rr_queue_contains(queue, *queue_count, i)) {
            queue[*queue_count] = i;
            *queue_count += 1U;
            discovered[i] = true;
        }
    }
}

static size_t rr_pop(size_t* queue, size_t* queue_count) {
    size_t i = 0;
    const size_t task_index = queue[0];

    for (i = 1; i < *queue_count; ++i) {
        queue[i - 1U] = queue[i];
    }
    *queue_count -= 1U;
    return task_index;
}

static bool run_fcfs(course_scheduler_t* scheduler) {
    uint32_t now = 0;

    while (!all_tasks_complete(scheduler)) {
        const size_t task_index = pick_fcfs(scheduler, now);
        course_scheduler_task_t* task = NULL;

        if (task_index == COURSE_SCHEDULER_MAX_TASKS) {
            const uint32_t next = next_arrival_time(scheduler, now);

            if (next == UINT32_MAX) {
                return false;
            }
            now = next;
            continue;
        }

        task = &scheduler->tasks[task_index];
        scheduler->summary.context_switches += 1U;
        now += task->remaining_time;
        task->remaining_time = 0;
        task->completion_time = now;
    }

    return true;
}

static bool run_rr(course_scheduler_t* scheduler, uint32_t time_slice) {
    size_t queue[COURSE_SCHEDULER_MAX_TASKS];
    bool discovered[COURSE_SCHEDULER_MAX_TASKS];
    size_t queue_count = 0;
    size_t i = 0;
    uint32_t now = 0;

    if (time_slice == 0) {
        return false;
    }

    for (i = 0; i < COURSE_SCHEDULER_MAX_TASKS; ++i) {
        queue[i] = 0;
        discovered[i] = false;
    }

    while (!all_tasks_complete(scheduler)) {
        size_t task_index = 0;
        course_scheduler_task_t* task = NULL;
        uint32_t run_for = 0;

        rr_enqueue_arrivals(scheduler, queue, &queue_count, discovered, now);
        if (queue_count == 0) {
            const uint32_t next = next_arrival_time(scheduler, now);

            if (next == UINT32_MAX) {
                return false;
            }
            now = next;
            rr_enqueue_arrivals(scheduler, queue, &queue_count, discovered, now);
        }
        if (queue_count == 0) {
            return false;
        }

        task_index = rr_pop(queue, &queue_count);
        task = &scheduler->tasks[task_index];
        run_for = task->remaining_time < time_slice ? task->remaining_time
                                                    : time_slice;
        scheduler->summary.context_switches += 1U;
        now += run_for;
        task->remaining_time -= run_for;
        rr_enqueue_arrivals(scheduler, queue, &queue_count, discovered, now);
        if (task->remaining_time == 0) {
            task->completion_time = now;
        } else {
            queue[queue_count] = task_index;
            queue_count += 1U;
        }
    }

    return true;
}

static bool run_cfs_lite(course_scheduler_t* scheduler, uint32_t time_slice) {
    uint32_t now = 0;

    if (time_slice == 0) {
        return false;
    }

    while (!all_tasks_complete(scheduler)) {
        const size_t task_index = pick_cfs_lite(scheduler, now);
        course_scheduler_task_t* task = NULL;
        uint32_t run_for = 0;

        if (task_index == COURSE_SCHEDULER_MAX_TASKS) {
            const uint32_t next = next_arrival_time(scheduler, now);

            if (next == UINT32_MAX) {
                return false;
            }
            now = next;
            continue;
        }

        task = &scheduler->tasks[task_index];
        run_for = task->remaining_time < time_slice ? task->remaining_time
                                                    : time_slice;
        scheduler->summary.context_switches += 1U;
        now += run_for;
        task->remaining_time -= run_for;
        task->vruntime += run_for;
        if (task->remaining_time == 0) {
            task->completion_time = now;
        }
    }

    return true;
}

static void update_wait_turnaround(course_scheduler_t* scheduler) {
    size_t i = 0;

    scheduler->summary.total_wait_time = 0;
    scheduler->summary.total_turnaround_time = 0;
    for (i = 0; i < scheduler->task_count; ++i) {
        const course_scheduler_task_t* task = &scheduler->tasks[i];
        const uint32_t turnaround = task->completion_time - task->arrival_time;
        const uint32_t wait = turnaround - task->burst_time;

        scheduler->summary.total_wait_time += wait;
        scheduler->summary.total_turnaround_time += turnaround;
    }
}

void course_scheduler_init(course_scheduler_t* scheduler) {
    size_t i = 0;

    if (scheduler == NULL) {
        return;
    }

    scheduler->task_count = 0;
    scheduler->summary.policy = COURSE_SCHED_POLICY_FCFS;
    scheduler->summary.context_switches = 0;
    scheduler->summary.total_wait_time = 0;
    scheduler->summary.total_turnaround_time = 0;
    for (i = 0; i < COURSE_SCHED_POLICY_COUNT; ++i) {
        scheduler->summary.policy_runs[i] = 0;
    }
    for (i = 0; i < COURSE_SCHEDULER_MAX_TASKS; ++i) {
        scheduler->tasks[i].used = false;
        scheduler->tasks[i].pid = 0;
        scheduler->tasks[i].arrival_time = 0;
        scheduler->tasks[i].burst_time = 0;
        scheduler->tasks[i].remaining_time = 0;
        scheduler->tasks[i].completion_time = 0;
        scheduler->tasks[i].vruntime = 0;
    }
}

bool course_scheduler_add_task(course_scheduler_t* scheduler,
                               uint32_t pid,
                               uint32_t arrival_time,
                               uint32_t burst_time) {
    course_scheduler_task_t* task = NULL;

    if (scheduler == NULL || pid == 0 || burst_time == 0 ||
        scheduler->task_count >= COURSE_SCHEDULER_MAX_TASKS) {
        return false;
    }

    task = &scheduler->tasks[scheduler->task_count];
    task->used = true;
    task->pid = pid;
    task->arrival_time = arrival_time;
    task->burst_time = burst_time;
    task->remaining_time = burst_time;
    task->completion_time = 0;
    task->vruntime = 0;
    scheduler->task_count += 1U;
    return true;
}

bool course_scheduler_run(course_scheduler_t* scheduler,
                          course_sched_policy_t policy,
                          uint32_t time_slice) {
    bool ok = false;

    if (scheduler == NULL || scheduler->task_count == 0 ||
        policy >= COURSE_SCHED_POLICY_COUNT) {
        return false;
    }

    reset_runtime_fields(scheduler);
    scheduler->summary.policy = policy;
    scheduler->summary.context_switches = 0;
    scheduler->summary.total_wait_time = 0;
    scheduler->summary.total_turnaround_time = 0;

    switch (policy) {
    case COURSE_SCHED_POLICY_FCFS:
        ok = run_fcfs(scheduler);
        break;
    case COURSE_SCHED_POLICY_RR:
        ok = run_rr(scheduler, time_slice);
        break;
    case COURSE_SCHED_POLICY_CFS_LITE:
        ok = run_cfs_lite(scheduler, time_slice);
        break;
    case COURSE_SCHED_POLICY_COUNT:
        ok = false;
        break;
    }

    if (!ok) {
        return false;
    }

    scheduler->summary.policy_runs[policy] += 1U;
    update_wait_turnaround(scheduler);
    return true;
}

bool course_scheduler_summary(const course_scheduler_t* scheduler,
                              course_scheduler_summary_t* out_summary) {
    if (scheduler == NULL || out_summary == NULL) {
        return false;
    }

    *out_summary = scheduler->summary;
    return true;
}

bool course_scheduler_task_stats(const course_scheduler_t* scheduler,
                                 course_scheduler_task_stats_t* out_stats,
                                 size_t max_stats) {
    size_t i = 0;

    if (scheduler == NULL || out_stats == NULL ||
        max_stats < scheduler->task_count) {
        return false;
    }

    for (i = 0; i < scheduler->task_count; ++i) {
        const course_scheduler_task_t* task = &scheduler->tasks[i];
        const uint32_t turnaround = task->completion_time - task->arrival_time;

        out_stats[i].pid = task->pid;
        out_stats[i].arrival_time = task->arrival_time;
        out_stats[i].burst_time = task->burst_time;
        out_stats[i].completion_time = task->completion_time;
        out_stats[i].waiting_time = turnaround - task->burst_time;
        out_stats[i].turnaround_time = turnaround;
        out_stats[i].vruntime = task->vruntime;
    }

    return true;
}

const char* course_scheduler_policy_name(course_sched_policy_t policy) {
    switch (policy) {
    case COURSE_SCHED_POLICY_FCFS:
        return "FCFS";
    case COURSE_SCHED_POLICY_RR:
        return "RR";
    case COURSE_SCHED_POLICY_CFS_LITE:
        return "CFS-lite";
    case COURSE_SCHED_POLICY_COUNT:
        break;
    }

    return "unknown";
}
