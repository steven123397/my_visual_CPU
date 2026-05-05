# Pipeline / Phase 3 执行模型统一设计

## 文档定位

本文档作为当前 `pipeline` 微架构的统一设计来源，集中描述当前已经落地的执行模型、关键边界和后续仍然成立的取舍判断。

它吸收并取代此前分散维护的几类专项文档：

- `pipeline core` 正式接入主线时的结构边界
- `Phase 3-A` 分支预测增强的当前结果
- `Phase 3-B/C` `rename + ROB + LSQ +` 最小 `OoO execute` 主路径
- decode 级 `BlockedByUnresolvedStore` 串行化边界收窄
- 是否继续扩大更激进 `issue / replay / speculation` 的主线判断

本文档不承担实时进度更新。当前状态、当前优先级和后续是否重开更激进专项，以 `docs/status/` 为准。

## 关联文档

- 状态文档：
  - [../status/mainline_status.md](../status/mainline_status.md)
- 相关设计：
  - [pipeline_speculation_contracts.md](pipeline_speculation_contracts.md)
- 已完成计划归档：
  - [../plan/history_plan.md#pipeline-core-integration-plan](../plan/history_plan.md#pipeline-core-integration-plan)
  - [../plan/history_plan.md#phase3-branch-prediction-plan](../plan/history_plan.md#phase3-branch-prediction-plan)
  - [../plan/history_plan.md#phase3-ooo-readiness-plan](../plan/history_plan.md#phase3-ooo-readiness-plan)
  - [../plan/history_plan.md#phase3-ooo-execution-plan](../plan/history_plan.md#phase3-ooo-execution-plan)
  - [../plan/history_plan.md#phase3-blocked-by-unresolved-store-boundary-plan](../plan/history_plan.md#phase3-blocked-by-unresolved-store-boundary-plan)

## 背景与问题

当前仓库已经是一个已可运行的模拟器原型。`functional` reference path、`pipeline` backend、`debug/frontend` 教学演示链路，以及 guest / `kernel_alpha` 基线都已经稳定接入主线。

因此，`pipeline` 相关设计文档不再适合继续按“某一次集成”“某一轮专项收窄”“某一个阶段判断”分散维护。对当前仓库而言，更有价值的是保留一份可以直接回答下面问题的统一参考资料：

- 当前 `pipeline` 到底已经做到什么程度。
- 当前分支预测、`rename / ROB / LSQ`、最小 `OoO execute` 的正式边界是什么。
- 当前 load / store 分类、replay 和 stall 观测如何解释。
- 当前为什么不主动继续扩大更激进的 `issue / replay / speculation`。

## 目标

- 给出当前 `pipeline` 微架构的单一设计来源。
- 明确已经落地的结构成果、当前仍然成立的约束，以及后续判断口径。
- 让 `status`、`history_plan`、`AGENTS.md` 和读者都能围绕同一份当前设计理解 `pipeline`。

## 非目标

- 不把当前文档写成新的实现计划。
- 不重复维护逐次提交历史；已完成过程统一放在 `history_plan`。
- 不把尚未启动的更重 `Phase 4`、cache、DMA、multicore 混入当前 `pipeline` 模型描述。

## 当前统一设计边界

### 1. 统一 ISA 真值与 backend 边界

当前 `pipeline` 继续严格依赖共享 `InstructionSemantics`，不拥有第二套 ISA 语义解释器：

- `functional` 继续是 architected 真值来源。
- `pipeline` 只负责调度、暂存、提交、flush、rollback 与可观察性。
- 任何 ISA / privilege / MMIO / trap 语义修复，优先落在共享语义层与公共 simulator 边界，而不是分别修多条 backend 路径。

### 2. 当前前端与分支预测边界

当前 `pipeline` 仍是单发射模型，但已经接入第一轮最小分支预测增强：

- `jal` 走静态 predict-taken。
- 条件分支使用最小 `2-bit` bimodal counter + target 记忆。
- `jalr` 仍不预测。
- mispredict 继续复用现有 flush / redirect 恢复路径。
- predictor 当前属于 `pipeline` 内部可观察状态，不构成新的 architected 语义面。

这意味着：当前前端不再是“永远顺序取指 + redirect”，但也没有进入更复杂的 BTB / RAS / TAGE 组合模型。

### 3. 当前 `rename + ROB + LSQ +` 最小 `OoO execute` 边界

当前 `Phase 3-B/C` 已经落地的正式边界如下：

- decode 侧会完成 `rename` 与 `ROB` 分配。
- backend 仍保持单发射、顺序退休。
- non-memory 指令可以先完成 execute，再等待 `ROB head` 退休。
- RAM load / store 进入最小 `LSQ` 管理。
- store 只在 commit boundary 真正落到 RAM / MMIO。
- MMIO load 继续维持 non-speculative；MMIO store 也不允许在投机阶段泄漏副作用。
- 当前 automatic replay 仍是保守的 coarse rollback + flush，而不是 selective replay。

因此，当前 `pipeline` 已经具备“最小真实 `OoO execute`”而不再是纯 in-order 五级流水；但它仍是克制形态，不追求 superscalar、深 issue queue 或激进 memory speculation。

### 4. 当前 load / store 分类与 `LSQ` 状态语义

当前 `LSQ` 最关键的对外解释边界如下：

- `BlockedByUnresolvedStore`
  - 只表示更老 store 地址未知，当前无法判断是否冲突。
- `BlockedByOverlappingStore`
  - 表示更老 store 地址已知，且与年轻 load 明确重叠，因此当前仍不能放行。
- `ReplayRequired`
  - 表示年轻 load 已经放行，但后续被确认需要按当前保守 replay 路径重放。

当前 decode 级串行化边界已经明确收窄到“仅 unknown-address older store 才阻塞”。地址已知但 data 未 ready 的 older store，不再全局阻塞非重叠 younger load。

### 5. 当前可观察性边界

当前 `pipeline` 的观测面已经不是附属信息，而是正式设计的一部分。当前主线至少稳定暴露：

- `sequence_id` / retire trace
- `stall_reason`
- 最小 `ROB / LSQ` 摘要
- predictor 最小状态与命中信息
- commit-boundary 下的 trap / interrupt / rollback 结果

这组观测面当前服务 3 件事：

- host smoke / differential 门禁
- `debug_cli` 与浏览器前端的只读观察
- 后续是否值得继续重开更激进微架构专项的证据收集

### 6. 当前明确不主动继续扩的方向

当前这份统一设计保留如下主线判断：

- 不主动继续扩大更激进的 `issue / replay / speculation`。
- 不直接放宽 unknown-address load speculation。
- 不把当前 `pipeline` 顺势扩成 superscalar、复杂 memory disambiguation 或更重的 replay 框架。

原因并不是这些方向永远不做，而是当前已知 stall 热点仍主要落在 `memory_path_busy` 与 `source_operands_not_ready`，而不是 decode 级 load/store 串行化本身。

如果未来真实 workload 再次证明值得重开，这一轮最小切片应优先评估 `issue decoupling`，而不是直接放大 unknown-address speculation。

## 演进摘要

当前统一文档只保留仍然有效的台阶结论：

- `pipeline core` 已正式接入主线，并与 `functional` 共享同一套 ISA 语义层。
- `Phase 3-A` 已为当前 `pipeline` 接上最小 predictor 与对应观测面。
- `Phase 3-B/C` 已完成 `rename + ROB + LSQ +` 最小真实 `OoO execute` 主路径。
- decode 级 `BlockedByUnresolvedStore` 已收窄到“仅 older store 地址未知才阻塞”。
- decode 边界收窄之后，当前主线已完成后续取舍判断：不主动继续扩大更激进的 `issue / replay / speculation`。

这些结果已经从“阶段性设计提案”转成当前实现边界，因此统一收口到本文档维护。

## 验证思路

当前与本设计直接相关的正式基线至少包括：

- `cd myCPU && make test-pipeline`
- `cd myCPU && make test-host-predictor_smoke`
- `cd myCPU && make test-host-pipeline_backend_smoke`
- `cd myCPU && make test-host-pipeline_commit_trace_smoke`
- `cd myCPU && make test-host-pipeline_rename_commit_smoke`
- `cd myCPU && make test-host-pipeline_speculation_contracts_smoke`
- `cd myCPU && make test-host-backend_differential_smoke`
- `cd myCPU && make test-host-debug_cli_smoke`

如果未来继续调整 `LSQ` 边界、replay 行为或 predictor 观测，应优先补最窄 host-side 回归，而不是新增更宽的大一统 smoke。

## 风险与取舍

- 当前把多份阶段性 pipeline 文档收口成一份统一设计，会减少专题细节的分散叙事，但能显著降低旧阶段文档过期后的误导风险。
- 当前继续维持“最小真实 `OoO execute` + 保守 replay”的克制形态，会牺牲一部分可见并行度；但这符合当前 reference-first、可调试、可回归的主线方法。
- 当前把“是否继续推进更激进 speculation”的判断保留在本文档和 `status`，意味着后续若要重开专项，必须基于新的 workload 证据，而不能只沿旧文档机械推进。

## 当前有效性说明

- 当前有效：本文档作为当前 `pipeline / Phase 3` 微架构边界的统一设计来源。
- 当前实时状态和近期风险，以 [../status/mainline_status.md](../status/mainline_status.md) 为准。
