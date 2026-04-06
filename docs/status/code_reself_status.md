# 代码复查状态

## 文档定位

本文档用于集中记录代码审查 / 复查任务中发现的问题、当前处理状态和下一步。

它不记录完整修复过程；具体执行步骤应进入对应 `plan` 文档，已完成事项统一归档到 [plan/history_plan.md](../plan/history_plan.md)。

## 关联文档

- 相关状态：
  - [mainline_status.md](mainline_status.md)
  - [project_priority_roadmap.md](project_priority_roadmap.md)
- 当前计划：
  - 当前无活跃计划。
- 已完成计划归档：
  - [../plan/history_plan.md#spike-external-differential-validation-plan](../plan/history_plan.md#spike-external-differential-validation-plan)

## 当前状态

- `2026-04-06` 对当前 `spike` 外部差分验证工作区完成的实现级复查已关闭。
- 关闭结论：
  - `make test-host-spike_differential` 显式入口已经补齐，本地验证可直接跑通，不再与 `tests/host/spike_differential/` 目录名冲突。
  - `spike_differential_smoke` 已经串起真实正向的 `myCPU vs Spike` final-state differential；当前 `alu_mem_csr`、`control_flow`、`predictable_branch_loop`、`trap_return`、`illegal_trap` 与 `delegated_user_ecall_to_supervisor` 6 条场景都能在真实 Spike 环境下匹配通过。
  - Spike adapter 当前已经支持最小初始 GPR / CSR / memory、非 M-mode 起始态、`trap_program` 与 final privilege 明确读取；输出解析也已改成精确字段计数、未知行 fail-closed 的严格策略。
  - 当前仍保留的设计边界，已转入实现已知限制而非活跃缺陷：V1 仍是 final-state differential，不覆盖 `configure hook`、平台 fixture、设备 side effect 与 `Sv39 / page fault` 子集；对执行 `mret/sret` 的 returning trap handler，当前也还不比较“第一现场” trap summary，而只比较最终可恢复状态。
- `2026-04-05` 对 `8403a563c3578291990220f56010488d37e18dd4`（`fix(phase3): 收窄 decode 级 blocked-by-unresolved-store 边界`）完成一轮提交级复查。
- 代码路径本身未发现新的 `LSQ` / `pipeline` correctness 回归；相关 `host smoke` 与 `make test-pipeline` 已通过。
- 本轮复查暴露的两条文档同步问题已关闭：
  - [docs/design/blocked_by_unresolved_store_boundary.md](../design/blocked_by_unresolved_store_boundary.md) 已改为指向 [plan/history_plan.md#phase3-blocked-by-unresolved-store-boundary-plan](../plan/history_plan.md#phase3-blocked-by-unresolved-store-boundary-plan)，不再保留已删除的活跃 plan 死链。
  - [AGENTS.md](../../AGENTS.md) 已同步最新 `Phase 3` 口径：decode 级 `BlockedByUnresolvedStore` 边界专项已完成，当前下一步改为评估是否继续扩 issue / replay / speculation。
- 当前无活跃复查问题。

## 记录规则

1. 问题按严重级别和影响面排序。
2. 每条问题至少写清影响范围、风险、建议动作和当前状态。
3. 如果问题进入修复，应补充对应 `plan` 或相关提交 / 分支说明。
4. 问题关闭后只保留简短结论，不在本文件堆积完整执行流水账。
