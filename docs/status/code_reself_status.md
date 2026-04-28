# 代码复查状态

## 文档定位

本文档用于集中记录代码审查 / 复查任务中发现的问题、当前处理状态和下一步。

它不记录完整修复过程；具体执行步骤应进入对应 `plan` 文档，已完成事项统一归档到 [../plan/history_plan.md](../plan/history_plan.md)。

## 关联文档

- 相关状态：
  - [mainline_status.md](mainline_status.md)
- 当前计划：
  - 当前无活跃计划。

## 当前状态

- 当前无活跃问题。
- `2026-04-22` 已完成一轮针对主分支顶部向量 workload 合并的复查，当前未发现新的 active finding：
  - `fix(向量): 显式物化 ReLU zero vector` 这轮改动已把 `vector_demo` / `vector_cnn_demo` 的 ReLU 路径从“隐式依赖 reset-zero `v0`”收口成“显式加载 zero vector”，并同步更新了对应 host smoke。
  - 复查已确认 guest demo 与 host smoke 的 functional / pipeline 两条入口都保持通过；当前没有发现新的行为回归、contract 破坏或遗漏的 workload 入口。
  - 本轮确认验证已覆盖：
    - `cd myCPU && make test-host-vector_operator_smoke test-host-vector_cnn_smoke`
    - `cd myCPU && make test-host-vector_backend_smoke test-host-vector_vlite_smoke`
    - `cd myCPU && make test-guest-vector_demo test-pipeline-guest-vector_demo test-guest-vector_cnn_demo test-pipeline-guest-vector_cnn_demo`
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
2. 主线优先级继续保持在 `xv6 / Linux` bring-up foundation；向量 workload 线当前回到 bug-driven maintenance，不主动扩大实现面。
3. A / D 继续保持 bug-driven hardening，不主动扩大无关实现面。

## 记录规则

1. 问题按严重级别和影响面排序。
2. 每条问题至少写清影响范围、风险、建议动作和当前状态。
3. 如果问题进入修复，应补充对应 `plan` 或相关提交 / 分支说明。
4. 问题关闭后只保留简短结论，不在本文件堆积完整执行流水账。
