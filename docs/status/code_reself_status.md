# 代码复查状态

## 文档定位

本文档用于集中记录代码审查 / 复查任务中发现的问题、当前处理状态和下一步。

它不记录完整修复过程；具体执行步骤应进入对应 `plan` 文档，已完成事项统一归档到 [../plan/history_plan.md](../plan/history_plan.md)。

## 关联文档

- 相关状态：
  - [mainline_status.md](mainline_status.md)
  - [project_priority_roadmap.md](project_priority_roadmap.md)
- 当前计划：
  - 当前无活跃计划。

## 当前状态

- 当前无活跃问题。
- `2026-04-22` 已关闭 `xv6 / Linux / JIT` 第一轮整合后的 2 条集中复查 findings：
  - 普通 `store` 现在会在 functional / commit-boundary 路径上正确打破 `LR/SC` reservation，`lr -> overlapping sw -> sc` 已补 host 回归。
  - `execution_profile` 现在会把 translation-fault memory access 记成 `unmapped` fault observation，并由 `execution_profile_smoke` 守住 `total_memory_observations` / `memory_regions[].faults`。
- 本轮关闭验证已覆盖：
  - `cd myCPU && make test-host-atomic_semantics_smoke`
  - `cd myCPU && make test-host-execution_profile_smoke`
  - `cd myCPU && make test-host-debug_cli_smoke`
  - `cd myCPU && make test`
  - `cd myCPU && make test-pipeline`

## 下一步

1. 当前 review 线回到维护态；后续只在新问题出现时再补最窄 findings。
2. 主线优先级回到 B 类平台 follow-up，再到 C 类 `xv6` board profile / boot checkpoint 推进。
3. A / D 继续保持 bug-driven hardening，不主动扩大无关实现面。

## 记录规则

1. 问题按严重级别和影响面排序。
2. 每条问题至少写清影响范围、风险、建议动作和当前状态。
3. 如果问题进入修复，应补充对应 `plan` 或相关提交 / 分支说明。
4. 问题关闭后只保留简短结论，不在本文件堆积完整执行流水账。
