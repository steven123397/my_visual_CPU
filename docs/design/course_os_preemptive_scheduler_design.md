# 课程 OS 在线抢占调度设计

## 文档定位

本文档定义课程 OS 在线抢占调度器的职责边界、timer tick 输入、进程状态转换、
context switch 统计和 Stage marker 兼容策略，对应已归档的
[课程 OS 架构后续增强计划](../plan/history_plan.md#course-os-arch-followup-plan)
任务 1。

本文档不替代 [course_os_scheduler_timing_contract.md](course_os_scheduler_timing_contract.md)
中已经固定的离线 scheduler-local cycle 合同，也不声明完整 CPU 寄存器上下文切换已经完成。

## 关联文档

- 状态文档：[../status/kernel_alpha_status.md](../status/kernel_alpha_status.md)
- 相关计划归档：[../plan/history_plan.md#course-os-arch-followup-plan](../plan/history_plan.md#course-os-arch-followup-plan)
- 边界设计：[course_os_gap_closure_boundary_design.md](course_os_gap_closure_boundary_design.md)
- 调度 timing 合同：[course_os_scheduler_timing_contract.md](course_os_scheduler_timing_contract.md)
- 课程 OS 基线设计：[course_os_kernel_alpha_course_os_baseline_design.md](course_os_kernel_alpha_course_os_baseline_design.md)

## 背景与问题

课程 OS 当前 `course_scheduler_run()` 是离线统计调度器：host/unit 和 guest smoke
用它复验 FCFS、RR、CFS-lite 的等待时间、周转时间、preempt 计数、context switch 计数
和 scheduler-local cycle cost。它一次性消费任务列表并产生统计结果，不由 supervisor
timer interrupt 驱动，也不直接维护 `course_process_table_t` 中进程的 READY / RUNNING
状态。

架构后续任务需要补一条在线抢占调度路径。该路径必须能由 timer tick 驱动、能观察进程状态
转换，并保留现有 Stage 1 / Stage 2 / Stage 3 summary 与 Stage 4 `course-os> ` prompt
合同。

## 目标

- 新增独立在线调度器状态，不改变 `course_scheduler_run()` 的离线统计语义。
- 用 timer tick 作为唯一推进单位，支持 FCFS、RR 和 CFS-lite 三种课程策略。
- 复用 `course_process_state_t`，把 READY / RUNNING / BLOCKED / ZOMBIE / DEAD 作为唯一
进程状态来源。
- 记录在线 context switch、preempt、idle tick 和 scheduler-local cycle cost 证据。
- 通过 supervisor timer post handler opt-in 接入 trap / timer path，默认不改变现有 guest
marker。

## 非目标

- 不实现真实寄存器 save / restore 或完整 U-mode 多进程并发执行。
- 不把 Linux compat 的 process / syscall / ELF 语义倒灌到课程 `course_*` 模块。
- 不改变 Stage 1 / Stage 2 / Stage 3 marker、Stage 4 shell prompt 或 Linux compat Plus
旁路边界。
- 不引入 ns / us / ms 时间换算，也不声明 QEMU、host 或真实硬件 context switch latency。
- 不让默认 supervisor timer handler 无条件驱动课程在线调度器。

## 约束与边界

- 在线调度器与离线调度器共享策略枚举 `course_sched_policy_t`，但状态对象彼此独立。
- `course_scheduler_t` / `course_scheduler_run()` 继续服务既有 `/proc/schedstat` 离线证据；
  在线调度器不得重置或覆盖它的 summary。
- 在线调度器只持有 `course_process_table_t*` 和最多 `COURSE_SCHEDULER_MAX_TASKS` 个 pid
  观察项，不拥有进程生命周期。
- 可运行进程只包括 `COURSE_PROCESS_READY` 和当前在线调度器持有的 `COURSE_PROCESS_RUNNING`；
  `BLOCKED`、`ZOMBIE`、`DEAD` 和 `UNUSED` 不得被选中。
- timer post handler 只接收 `course_online_scheduler_t*` context；未显式安装时，trap / timer
  行为保持原样。

## 方案

### 结构设计

新增 `course_online_scheduler_t`，内部记录：

- 绑定的 `course_process_table_t`。
- policy、time slice、tick 计数、idle tick、current pid / index。
- 每个被纳入在线调度的 pid 及其 online `vruntime` / run tick。
- online summary：context switch、preempt、last / total switch cycle cost。

离线调度器继续保留 `course_scheduler_t` 和 `course_scheduler_summary_t`。在线 summary 使用
独立 `course_online_scheduler_summary_t`，避免调用方把两种证据面混在一起。

### 接口 / 数据 / 契约

对外接口：

- `course_online_scheduler_init()`：清空在线状态，默认 FCFS、未绑定进程表。
- `course_online_scheduler_configure()`：设置 policy 和 time slice；FCFS 忽略 time slice，
  RR / CFS-lite 要求非零 time slice。
- `course_online_scheduler_bind_process_table()`：绑定课程进程表。
- `course_online_scheduler_add_process()`：把已有、未结束的 pid 纳入在线调度观察集合。
- `course_online_scheduler_tick()`：推进一个 online tick，执行选择、状态转换和统计更新。
- `course_online_scheduler_summary()`：只读复制 online summary。
- `course_online_scheduler_timer_post_handler()`：供 supervisor timer post handler opt-in 调用。

调度语义：

- FCFS：一旦选中 RUNNING 进程，只要它仍处于 RUNNING 就不因 timer tick 抢占；当前进程
  退出、阻塞或消失后，再按加入顺序选下一个 READY 进程。
- RR：当前 RUNNING 进程累计到 time slice 后，在 tick 边界改回 READY，`preempt_count`
  加一，再从下一项开始选择 READY 进程。
- CFS-lite：每个 tick 边界在 READY 进程和当前 RUNNING 进程之间选择 online `vruntime`
  最小者；当前进程如不再是最小值，先改回 READY，再切换到新的最小 `vruntime` 进程。

统计语义：

- 选择的 running pid 发生变化时记录一次 context switch；首次从 idle 进入进程也计为一次
  context switch。
- 每次 context switch 的 scheduler-local cycle cost 固定为 1 个 online tick。
- 没有 READY 进程时记录 idle tick，不增加 context switch 或 preempt。

### 验证思路

- host unit 验证 RR 在 time slice 到期后切换到下一个 READY 进程，并增加 preempt。
- host unit 验证 FCFS 不因 timer tick 抢占。
- host unit 验证 CFS-lite 按 online `vruntime` 选择更少运行的 READY 进程。
- host unit 验证 BLOCKED / ZOMBIE 进程不会被选中，空 ready 集只记录 idle tick。
- guest smoke 验证 `kernel_alpha_demo` 的 Stage marker 不被默认在线调度路径改变。

## 风险与取舍

- 当前 online scheduler 是课程级可观察调度模型，不是完整 CPU execution scheduler。该取舍让
  host unit 能稳定复验进程状态转换和 preempt 统计，同时不扰动 Linux compat / Stage marker。
- CFS-lite 当前仍是最小 `vruntime` 模型，不引入权重表或 nice 值；后续如果要展示权重，可在
  online task entry 增加 weight，但不能改变本轮默认合同。
- timer post handler opt-in 复用现有 trap policy，避免在默认 supervisor timer path 中硬编码
  课程调度器依赖。

## 当前有效性说明

- 当前有效。
- 本文档对应的当前状态以 [../status/kernel_alpha_status.md](../status/kernel_alpha_status.md) 为准。
