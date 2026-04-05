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
  - 当前无专项归档条目。

## 当前状态

- `2026-04-05` 对 `8403a563c3578291990220f56010488d37e18dd4`（`fix(phase3): 收窄 decode 级 blocked-by-unresolved-store 边界`）完成一轮提交级复查。
- 代码路径本身未发现新的 `LSQ` / `pipeline` correctness 回归；相关 `host smoke` 与 `make test-pipeline` 已通过。
- 本轮复查暴露的两条文档同步问题已关闭：
  - [docs/design/blocked_by_unresolved_store_boundary.md](../design/blocked_by_unresolved_store_boundary.md) 已改为指向 [plan/history_plan.md#phase3-blocked-by-unresolved-store-boundary-plan](../plan/history_plan.md#phase3-blocked-by-unresolved-store-boundary-plan)，不再保留已删除的活跃 plan 死链。
  - [AGENTS.md](../../AGENTS.md) 已同步最新 `Phase 3` 口径：decode 级 `BlockedByUnresolvedStore` 边界专项已完成，当前下一步改为评估是否继续扩 issue / replay / speculation。
- 当前无活跃代码审查 / 复查问题。

## 记录规则

1. 问题按严重级别和影响面排序。
2. 每条问题至少写清影响范围、风险、建议动作和当前状态。
3. 如果问题进入修复，应补充对应 `plan` 或相关提交 / 分支说明。
4. 问题关闭后只保留简短结论，不在本文件堆积完整执行流水账。
