#include "course_scheduler.h"

#include <stddef.h>

#include "riscv.h"

#define COURSE_ONLINE_SCHEDULER_NO_INDEX COURSE_SCHEDULER_MAX_TASKS
#define COURSE_ONLINE_SWITCH_CYCLE_COST 1U

static void reset_runtime_fields(course_scheduler_t* scheduler) {
    size_t i = 0;

    for (i = 0; i < scheduler->task_count; ++i) {
        scheduler->tasks[i].remaining_time = scheduler->tasks[i].burst_time;
        scheduler->tasks[i].completion_time = 0;
        scheduler->tasks[i].vruntime = 0;
    }
}

static void record_context_switch(course_scheduler_t* scheduler,
                                  uint32_t cycle_cost) {
    scheduler->summary.context_switches += 1U;
    scheduler->summary.last_switch_cycle_cost = cycle_cost;
    scheduler->summary.total_switch_cycle_cost += cycle_cost;
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
        record_context_switch(scheduler, task->remaining_time);
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
        record_context_switch(scheduler, run_for);
        now += run_for;
        task->remaining_time -= run_for;
        rr_enqueue_arrivals(scheduler, queue, &queue_count, discovered, now);
        if (task->remaining_time == 0) {
            task->completion_time = now;
        } else {
            scheduler->summary.preempt_count += 1U;
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
        record_context_switch(scheduler, run_for);
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
    if (scheduler->task_count != 0U) {
        scheduler->summary.average_wait_time =
            scheduler->summary.total_wait_time / (uint32_t)scheduler->task_count;
        scheduler->summary.average_turnaround_time =
            scheduler->summary.total_turnaround_time /
            (uint32_t)scheduler->task_count;
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
    scheduler->summary.last_switch_cycle_cost = 0;
    scheduler->summary.total_switch_cycle_cost = 0;
    scheduler->summary.time_slice = 0;
    scheduler->summary.preempt_count = 0;
    scheduler->summary.total_wait_time = 0;
    scheduler->summary.total_turnaround_time = 0;
    scheduler->summary.average_wait_time = 0;
    scheduler->summary.average_turnaround_time = 0;
    scheduler->summary.last_policy_name = "FCFS";
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
    scheduler->summary.last_switch_cycle_cost = 0;
    scheduler->summary.total_switch_cycle_cost = 0;
    scheduler->summary.time_slice =
        policy == COURSE_SCHED_POLICY_FCFS ? 0U : time_slice;
    scheduler->summary.preempt_count = 0;
    scheduler->summary.total_wait_time = 0;
    scheduler->summary.total_turnaround_time = 0;
    scheduler->summary.average_wait_time = 0;
    scheduler->summary.average_turnaround_time = 0;
    scheduler->summary.last_policy_name = course_scheduler_policy_name(policy);

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

static bool online_index_valid(const course_online_scheduler_t* scheduler,
                               size_t index) {
    return scheduler != NULL && index < scheduler->task_count &&
           scheduler->tasks[index].used;
}

static course_process_t* online_process_at(course_online_scheduler_t* scheduler,
                                           size_t index) {
    if (!online_index_valid(scheduler, index) ||
        scheduler->process_table == NULL) {
        return NULL;
    }

    return course_process_find(scheduler->process_table,
                               scheduler->tasks[index].pid);
}

static bool online_process_runnable(const course_process_t* process) {
    return process != NULL && process->used &&
           process->state == COURSE_PROCESS_READY;
}

static bool online_process_cfs_candidate(
    const course_online_scheduler_t* scheduler,
    size_t index,
    const course_process_t* process) {
    if (process == NULL || !process->used) {
        return false;
    }
    if (process->state == COURSE_PROCESS_READY) {
        return true;
    }
    return scheduler != NULL && index == scheduler->current_index &&
           process->state == COURSE_PROCESS_RUNNING;
}

static bool online_current_running(course_online_scheduler_t* scheduler) {
    course_process_t* process = NULL;

    if (!online_index_valid(scheduler, scheduler->current_index)) {
        return false;
    }

    process = online_process_at(scheduler, scheduler->current_index);
    return process != NULL && process->state == COURSE_PROCESS_RUNNING;
}

static void online_record_context_switch(course_online_scheduler_t* scheduler,
                                         uint32_t next_pid) {
    if (scheduler == NULL || scheduler->summary.current_pid == next_pid) {
        return;
    }

    scheduler->summary.context_switches += 1U;
    scheduler->summary.last_switch_cycle_cost =
        COURSE_ONLINE_SWITCH_CYCLE_COST;
    scheduler->summary.total_switch_cycle_cost +=
        COURSE_ONLINE_SWITCH_CYCLE_COST;
}

static void online_release_current(course_online_scheduler_t* scheduler) {
    course_process_t* process = NULL;

    if (!online_index_valid(scheduler, scheduler->current_index)) {
        scheduler->current_index = COURSE_ONLINE_SCHEDULER_NO_INDEX;
        scheduler->summary.current_pid = 0;
        scheduler->slice_used = 0;
        return;
    }

    process = online_process_at(scheduler, scheduler->current_index);
    if (process != NULL && process->state == COURSE_PROCESS_RUNNING) {
        course_process_set_state(scheduler->process_table,
                                 process->pid,
                                 COURSE_PROCESS_READY);
    }
    scheduler->current_index = COURSE_ONLINE_SCHEDULER_NO_INDEX;
    scheduler->summary.current_pid = 0;
    scheduler->slice_used = 0;
}

static size_t online_pick_fcfs(course_online_scheduler_t* scheduler) {
    size_t i = 0;

    for (i = 0; i < scheduler->task_count; ++i) {
        if (online_process_runnable(online_process_at(scheduler, i))) {
            return i;
        }
    }

    return COURSE_ONLINE_SCHEDULER_NO_INDEX;
}

static size_t online_pick_rr(course_online_scheduler_t* scheduler,
                             size_t previous_index) {
    size_t step = 0;
    size_t start = 0;

    if (scheduler->task_count == 0) {
        return COURSE_ONLINE_SCHEDULER_NO_INDEX;
    }

    start = previous_index == COURSE_ONLINE_SCHEDULER_NO_INDEX
                ? 0U
                : (previous_index + 1U) % scheduler->task_count;
    for (step = 0; step < scheduler->task_count; ++step) {
        const size_t index = (start + step) % scheduler->task_count;

        if (online_process_runnable(online_process_at(scheduler, index))) {
            return index;
        }
    }

    return COURSE_ONLINE_SCHEDULER_NO_INDEX;
}

static size_t online_pick_cfs_lite(course_online_scheduler_t* scheduler) {
    size_t i = 0;
    size_t best = COURSE_ONLINE_SCHEDULER_NO_INDEX;

    for (i = 0; i < scheduler->task_count; ++i) {
        const course_online_scheduler_task_t* task = &scheduler->tasks[i];
        const course_process_t* process = online_process_at(scheduler, i);

        if (!online_process_cfs_candidate(scheduler, i, process)) {
            continue;
        }
        if (best == COURSE_ONLINE_SCHEDULER_NO_INDEX ||
            task->vruntime < scheduler->tasks[best].vruntime ||
            (task->vruntime == scheduler->tasks[best].vruntime &&
             task->pid < scheduler->tasks[best].pid)) {
            best = i;
        }
    }

    return best;
}

static bool online_select(course_online_scheduler_t* scheduler,
                          size_t next_index) {
    course_process_t* next_process = NULL;
    course_process_t* previous_process = NULL;
    const size_t previous_index = scheduler != NULL ? scheduler->current_index
                                                    : COURSE_ONLINE_SCHEDULER_NO_INDEX;

    if (!online_index_valid(scheduler, next_index)) {
        scheduler->summary.current_pid = 0;
        scheduler->current_index = COURSE_ONLINE_SCHEDULER_NO_INDEX;
        scheduler->slice_used = 0;
        return true;
    }

    next_process = online_process_at(scheduler, next_index);
    if (!online_process_cfs_candidate(scheduler, next_index, next_process)) {
        return false;
    }

    if (next_process == NULL || !course_process_set_state(scheduler->process_table,
                                                          next_process->pid,
                                                          COURSE_PROCESS_RUNNING)) {
        return false;
    }

    if (previous_index < scheduler->task_count) {
        previous_process = online_process_at(scheduler, previous_index);
    }
    if (previous_process != NULL && previous_process->pid != next_process->pid &&
        previous_process->state == COURSE_PROCESS_RUNNING) {
        (void)course_process_set_state(scheduler->process_table,
                                       previous_process->pid,
                                       COURSE_PROCESS_READY);
    }
    online_record_context_switch(scheduler, next_process->pid);
    scheduler->current_index = next_index;
    scheduler->summary.current_pid = next_process->pid;
    scheduler->slice_used = 0;
    return true;
}

static bool online_select_next(course_online_scheduler_t* scheduler,
                               size_t previous_index) {
    size_t next_index = COURSE_ONLINE_SCHEDULER_NO_INDEX;

    switch (scheduler->summary.policy) {
    case COURSE_SCHED_POLICY_FCFS:
        next_index = online_pick_fcfs(scheduler);
        break;
    case COURSE_SCHED_POLICY_RR:
        next_index = online_pick_rr(scheduler, previous_index);
        break;
    case COURSE_SCHED_POLICY_CFS_LITE:
        next_index = online_pick_cfs_lite(scheduler);
        break;
    case COURSE_SCHED_POLICY_COUNT:
        return false;
    }

    return online_select(scheduler, next_index);
}

static bool online_prepare_current(course_online_scheduler_t* scheduler) {
    const size_t previous_index = scheduler->current_index;

    if (!online_current_running(scheduler)) {
        online_release_current(scheduler);
        return online_select_next(scheduler, previous_index);
    }

    switch (scheduler->summary.policy) {
    case COURSE_SCHED_POLICY_FCFS:
        return true;
    case COURSE_SCHED_POLICY_RR:
        if (scheduler->slice_used >= scheduler->summary.time_slice) {
            scheduler->summary.preempt_count += 1U;
            return online_select_next(scheduler, previous_index);
        }
        return true;
    case COURSE_SCHED_POLICY_CFS_LITE: {
        const size_t next_index = online_pick_cfs_lite(scheduler);

        if (next_index != COURSE_ONLINE_SCHEDULER_NO_INDEX &&
            next_index != scheduler->current_index) {
            return online_select(scheduler, next_index);
        }
        return true;
    }
    case COURSE_SCHED_POLICY_COUNT:
        break;
    }

    return false;
}

void course_online_scheduler_init(course_online_scheduler_t* scheduler) {
    size_t i = 0;

    if (scheduler == NULL) {
        return;
    }

    scheduler->process_table = NULL;
    scheduler->task_count = 0;
    scheduler->current_index = COURSE_ONLINE_SCHEDULER_NO_INDEX;
    scheduler->slice_used = 0;
    scheduler->summary.policy = COURSE_SCHED_POLICY_FCFS;
    scheduler->summary.ticks = 0;
    scheduler->summary.idle_ticks = 0;
    scheduler->summary.current_pid = 0;
    scheduler->summary.context_switches = 0;
    scheduler->summary.last_switch_cycle_cost = 0;
    scheduler->summary.total_switch_cycle_cost = 0;
    scheduler->summary.time_slice = 0;
    scheduler->summary.preempt_count = 0;
    scheduler->summary.last_policy_name = "FCFS";
    for (i = 0; i < COURSE_SCHEDULER_MAX_TASKS; ++i) {
        scheduler->tasks[i].used = false;
        scheduler->tasks[i].pid = 0;
        scheduler->tasks[i].vruntime = 0;
        scheduler->tasks[i].run_ticks = 0;
    }
}

bool course_online_scheduler_configure(course_online_scheduler_t* scheduler,
                                       course_sched_policy_t policy,
                                       uint32_t time_slice) {
    if (scheduler == NULL || policy >= COURSE_SCHED_POLICY_COUNT ||
        (policy != COURSE_SCHED_POLICY_FCFS && time_slice == 0)) {
        return false;
    }

    scheduler->summary.policy = policy;
    scheduler->summary.time_slice =
        policy == COURSE_SCHED_POLICY_FCFS ? 0U : time_slice;
    scheduler->summary.last_policy_name = course_scheduler_policy_name(policy);
    scheduler->current_index = COURSE_ONLINE_SCHEDULER_NO_INDEX;
    scheduler->summary.current_pid = 0;
    scheduler->slice_used = 0;
    return true;
}

bool course_online_scheduler_bind_process_table(
    course_online_scheduler_t* scheduler,
    course_process_table_t* process_table) {
    if (scheduler == NULL || process_table == NULL) {
        return false;
    }

    scheduler->process_table = process_table;
    scheduler->current_index = COURSE_ONLINE_SCHEDULER_NO_INDEX;
    scheduler->summary.current_pid = 0;
    scheduler->slice_used = 0;
    return true;
}

bool course_online_scheduler_add_process(course_online_scheduler_t* scheduler,
                                         uint32_t pid) {
    course_process_t* process = NULL;
    size_t i = 0;

    if (scheduler == NULL || scheduler->process_table == NULL ||
        pid == 0 || scheduler->task_count >= COURSE_SCHEDULER_MAX_TASKS) {
        return false;
    }

    process = course_process_find(scheduler->process_table, pid);
    if (process == NULL || !process->used ||
        process->state == COURSE_PROCESS_UNUSED ||
        process->state == COURSE_PROCESS_ZOMBIE ||
        process->state == COURSE_PROCESS_DEAD) {
        return false;
    }

    for (i = 0; i < scheduler->task_count; ++i) {
        if (scheduler->tasks[i].used && scheduler->tasks[i].pid == pid) {
            return false;
        }
    }

    scheduler->tasks[scheduler->task_count].used = true;
    scheduler->tasks[scheduler->task_count].pid = pid;
    scheduler->tasks[scheduler->task_count].vruntime = 0;
    scheduler->tasks[scheduler->task_count].run_ticks = 0;
    scheduler->task_count += 1U;
    return true;
}

bool course_online_scheduler_tick(course_online_scheduler_t* scheduler) {
    if (scheduler == NULL || scheduler->process_table == NULL ||
        scheduler->summary.policy >= COURSE_SCHED_POLICY_COUNT ||
        (scheduler->summary.policy != COURSE_SCHED_POLICY_FCFS &&
         scheduler->summary.time_slice == 0)) {
        return false;
    }

    scheduler->summary.ticks += 1U;
    if (!online_prepare_current(scheduler)) {
        return false;
    }

    if (!online_current_running(scheduler)) {
        scheduler->summary.current_pid = 0;
        scheduler->summary.idle_ticks += 1U;
        return true;
    }

    scheduler->tasks[scheduler->current_index].run_ticks += 1U;
    scheduler->tasks[scheduler->current_index].vruntime += 1U;
    scheduler->slice_used += 1U;
    return true;
}

bool course_online_scheduler_summary(
    const course_online_scheduler_t* scheduler,
    course_online_scheduler_summary_t* out_summary) {
    if (scheduler == NULL || out_summary == NULL) {
        return false;
    }

    *out_summary = scheduler->summary;
    return true;
}

void course_online_scheduler_timer_post_handler(uint64_t cause, void* context) {
    if (cause != RISCV_SUPERVISOR_TIMER_INTERRUPT || context == NULL) {
        return;
    }

    (void)course_online_scheduler_tick((course_online_scheduler_t*)context);
}
