#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "course_process.h"

/* 课程 OS 调度模块包含两种模型：
   1. 离线模型用于稳定复现 FCFS/RR/CFS-lite 的统计结果；
   2. 在线模型由 timer tick 推进，用于展示抢占式调度和上下文切换证据。 */
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
    uint32_t last_switch_cycle_cost;
    uint32_t total_switch_cycle_cost;
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
    /* arrival/burst 用于离线可重复排程，remaining/completion/vruntime 是运行期状态。 */
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

typedef struct CourseOnlineSchedulerTask {
    bool used;
    uint32_t pid;
    /* vruntime 是 CFS-lite 选择依据，run_ticks 是展示用累计运行时间。 */
    uint32_t vruntime;
    uint32_t run_ticks;
} course_online_scheduler_task_t;

typedef struct CourseOnlineSchedulerSummary {
    course_sched_policy_t policy;
    uint32_t ticks;
    uint32_t idle_ticks;
    uint32_t current_pid;
    uint32_t context_switches;
    uint32_t last_switch_cycle_cost;
    uint32_t total_switch_cycle_cost;
    uint32_t time_slice;
    uint32_t preempt_count;
    const char* last_policy_name;
} course_online_scheduler_summary_t;

typedef struct CourseOnlineScheduler {
    /* 在线调度器只引用课程进程表，不拥有进程生命周期。 */
    course_process_table_t* process_table;
    course_online_scheduler_task_t tasks[COURSE_SCHEDULER_MAX_TASKS];
    size_t task_count;
    size_t current_index;
    uint32_t slice_used;
    course_online_scheduler_summary_t summary;
} course_online_scheduler_t;

/* 初始化离线调度器：清空任务表与统计，默认策略记为 FCFS。 */
void course_scheduler_init(course_scheduler_t* scheduler);
/* 向离线调度器追加一个任务（pid / 到达时间 / burst），供后续 run 排程统计。 */
bool course_scheduler_add_task(course_scheduler_t* scheduler,
                               uint32_t pid,
                               uint32_t arrival_time,
                               uint32_t burst_time);
/* 按指定策略把所有任务离线排程一遍，填完成时间与汇总统计；FCFS 不需要时间片。 */
bool course_scheduler_run(course_scheduler_t* scheduler,
                          course_sched_policy_t policy,
                          uint32_t time_slice);
/* 拷贝出当前汇总统计（上下文切换次数、平均等待 / 周转时间等）。 */
bool course_scheduler_summary(const course_scheduler_t* scheduler,
                              course_scheduler_summary_t* out_summary);
/* 拷贝出每个任务的等待 / 周转 / vruntime 明细，供展示层逐行打印。 */
bool course_scheduler_task_stats(const course_scheduler_t* scheduler,
                                 course_scheduler_task_stats_t* out_stats,
                                 size_t max_stats);
/* 把策略枚举转成展示用字符串名（"FCFS" / "RR" / "CFS-lite"）。 */
const char* course_scheduler_policy_name(course_sched_policy_t policy);
/* 初始化在线调度器：清空槽位与统计，默认策略 FCFS，尚未绑定进程表。 */
void course_online_scheduler_init(course_online_scheduler_t* scheduler);
/* 配置在线调度策略与时间片；FCFS 不需要时间片，其它策略要求 time_slice > 0。 */
bool course_online_scheduler_configure(course_online_scheduler_t* scheduler,
                                       course_sched_policy_t policy,
                                       uint32_t time_slice);
/* 绑定课程进程表；在线调度器只引用它，不拥有进程生命周期。 */
bool course_online_scheduler_bind_process_table(
    course_online_scheduler_t* scheduler,
    course_process_table_t* process_table);
/* 把一个活跃课程进程登记进在线调度槽位，供 timer tick 排程。 */
bool course_online_scheduler_add_process(course_online_scheduler_t* scheduler,
                                         uint32_t pid);
/* 推进一个 timer tick：必要时按策略切换当前进程，并累计 run / vruntime 统计。 */
bool course_online_scheduler_tick(course_online_scheduler_t* scheduler);
/* 拷贝出在线调度器运行期统计（ticks、idle、上下文切换、当前 pid 等）。 */
bool course_online_scheduler_summary(
    const course_online_scheduler_t* scheduler,
    course_online_scheduler_summary_t* out_summary);
/* timer 中断 post-handler：校验中断类型后驱动一次 tick，供 trap 层挂接。 */
void course_online_scheduler_timer_post_handler(uint64_t cause, void* context);
