# 课程 OS 调度 timing 合同

## 文档定位

本文档定义课程 OS scheduler 当前可复验的 cycle 证据合同，对应已归档的
[课程 OS 架构后续增强计划](../plan/history_plan.md#course-os-arch-followup-plan)
中的 context switch cost 后续增强项。

本文档不声明真实 wall-clock 时间、QEMU 耗时、host 耗时或完整在线抢占调度器已经完成。

## 关联文档

- 状态文档：[../status/kernel_alpha_status.md](../status/kernel_alpha_status.md)
- 相关计划归档：[../plan/history_plan.md#course-os-arch-followup-plan](../plan/history_plan.md#course-os-arch-followup-plan)
- 边界设计：[course_os_gap_closure_boundary_design.md](course_os_gap_closure_boundary_design.md)
- 课程 OS 基线设计：[course_os_kernel_alpha_course_os_baseline_design.md](course_os_kernel_alpha_course_os_baseline_design.md)

## 背景与问题

课程 OS 当前 scheduler 是离线统计模型：`course_scheduler_run()` 在 host/unit 和 guest
smoke 中复验 FCFS、RR、CFS-lite 的等待时间、周转时间、preempt 计数和 context switch
计数。它不是由 supervisor timer interrupt 驱动的在线调度器。

因此，context switch cost 不能直接写成 “QEMU < 1ms” 或 host wall-clock 时间。当前默认可复验
证据只能来自 scheduler 模型内部的 cycle delta，并且必须先把 cycle 来源和非目标写清楚。

## 目标

- 为 `course_scheduler_summary_t` 提供最近一次 context switch 观察到的 cycle cost。
- 在 `/proc/schedstat` 输出 cycle 字段，供课程 OS 证据面读取。
- 保持 Stage 1 / Stage 2 / Stage 3 marker、Stage 4 `course-os> ` prompt 和 Linux compat
  旁路边界不变。

## 非目标

- 不实现在线抢占调度器。
- 不接入 trap / timer interrupt path。
- 不读取 RISC-V `mcycle` / `cycle` CSR。
- 不输出 ns / us / ms 等时间字段。
- 不声明 QEMU、host 或真实硬件上的 context switch latency。

## 约束与边界

- 当前 `scheduler cycle` 是 scheduler 模型内部的无量纲 cycle。
- 当前 cycle 来源是离线调度器每次 dispatch 的模拟运行片段：
  - FCFS：本次选中任务的剩余 burst。
  - RR / CFS-lite：`min(remaining_time, time_slice)`。
- `last_switch_cycle_cost` 表示最近一次 context switch 观察到的 scheduler-local cycle delta。
- `total_switch_cycle_cost` 表示本次 `course_scheduler_run()` 内所有 context switch cycle delta
  的累计值。
- `course_scheduler_run()` 每次运行重置本轮 summary 的 switch cycle 字段；`policy_runs[]`
  继续保留既有累计语义。

## 方案

### 结构设计

在 `course_scheduler_summary_t` 中新增 cycle 字段。三个离线策略在每次选择任务并推进模拟时间时，
统一记录本次 dispatch 的 cycle delta。

### 接口 / 数据 / 契约

`/proc/schedstat` 继续输出现有调度字段，并新增：

- `last_switch_cycle_cost=<n>`
- `total_switch_cycle_cost=<n>`

当前不输出时间换算字段。只有后续新增明确的 cycle-to-time 合同时，才允许新增对应时间字段。

### 验证思路

- host unit 验证 RR 固定 workload 的最近一次和累计 switch cycle cost。
- host unit 验证 `/proc/schedstat` 输出 cycle 字段，且不输出时间单位字段。
- 默认 guest smoke 继续验证 Stage marker 不变。

## 风险与取舍

- 当前 cycle 字段证明的是离线 scheduler 模型中的调度片段 cycle delta，不是 CPU save/restore
  overhead。字段名保留 `cost` 是为了对齐课程 OS 后续计划，但文档明确禁止把它解释成 wall-clock
  latency。
- 任务 1 的在线调度器完成后，可以在保持 `/proc/schedstat` 字段名稳定的前提下，把 cycle 来源切换
  为 timer tick 或硬件 cycle counter。

## 当前有效性说明

- 当前有效。
- 本文档对应的当前状态以 [../status/kernel_alpha_status.md](../status/kernel_alpha_status.md) 为准。
