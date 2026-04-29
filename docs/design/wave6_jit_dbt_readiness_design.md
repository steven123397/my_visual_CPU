# Wave 6 JIT / DBT Readiness 设计

## 文档定位

本文档记录主线 `Wave 6 / JIT / DBT 与 multicore / coherence` 的当前有效入口边界。

它回答：

- 为什么 `Wave 6` 可以在 `Wave 5` 首轮收口后激活。
- 为什么第一刀选择 `JIT / DBT hot-path evidence`，而不是直接实现 JIT engine 或 multicore / coherence。
- `Wave 6 Slice A` 允许推进到哪里，哪些内容仍必须留在后续切片。

本文档不记录执行 checklist。当前进度以
[../status/mainline_status.md](../status/mainline_status.md)、活跃计划和计划归档为准。

## 关联文档

- 状态文档：
  - [../status/mainline_status.md](../status/mainline_status.md)
- 当前计划：
  - [../plan/mainline_wave6_jit_dbt_hot_path_evidence_slice_a_plan.md](../plan/mainline_wave6_jit_dbt_hot_path_evidence_slice_a_plan.md)
- 已完成计划归档：
  - [../plan/history_plan.md#mainline-wave5-closeout-wave6-readiness-plan](../plan/history_plan.md#mainline-wave5-closeout-wave6-readiness-plan)
  - [../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-f-l1d-lifecycle-guardrail-plan](../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-f-l1d-lifecycle-guardrail-plan)
- 相关设计：
  - [future_expansion_roadmap_design.md](future_expansion_roadmap_design.md)
  - [xv6_linux_jit_mainline_design.md](xv6_linux_jit_mainline_design.md)
  - [phase3_ooo_execution_model_design.md](phase3_ooo_execution_model_design.md)
  - [pipeline_speculation_contracts.md](pipeline_speculation_contracts.md)
  - [wave5_cache_memory_system_design.md](wave5_cache_memory_system_design.md)

## 背景与问题

当前仓库已经是一个已可运行的模拟器原型。`Wave 5 / cache / memory-system`
已经完成 `Slice A ~ F`：从 memory signal、最小可执行 L1D、显式 opt-in
观察面，到 L1D 边界和 lifecycle guardrail，都已经形成首轮可维护合同。

这给 `Wave 6` 提供了入口，但不能把它理解为“现在可以直接写 JIT 或多核”。`JIT /
DBT` 会把 shared `InstructionSemantics`、profile、debug snapshot、host 执行策略、
fault / trap 语义和未来代码缓存串起来；multicore / coherence 则会同时放大 cache、
atomic、DMA、memory-order 和平台状态空间。如果第一刀直接改执行语义，风险会明显高于当前证据能支撑的范围。

因此 `Wave 6` 的第一刀必须继续 `reference-first`：先固定可重复的 hot-path /
translation candidate 证据，只做观察和候选区间定义，不生成宿主机器码，不引入 block
cache，不改变 guest 可见语义。

## 目标

- 激活主线 `Wave 6`，但把第一刀限定为 `JIT / DBT hot-path evidence`。
- 复用现有 `ExecutionProfile`、debug snapshot、probe 和代表性 workload，形成可验证的
  hot-path / translation candidate 观察合同。
- 为后续是否实现 JIT / DBT engine 提供证据，而不是用“预期会快”作为动机。
- 保持 `functional` reference path、`pipeline` 和当前 workload guardrail 的行为不变。

## 非目标

- 不在 `Slice A` 实现 JIT engine、DBT translator、IR、block cache 或 host code emission。
- 不申请可执行内存，不引入宿主平台相关代码生成。
- 不改变 `InstructionSemantics` 的 ISA 真值来源定位。
- 不改变 guest 可见 fault / trap / CSR / memory 语义。
- 不启动 multicore、coherence、memory consistency 新模型、write-back cache、I-cache 或
  cache maintenance instruction。
- 不把 AI accelerator 后续专项并入 `Wave 6`。

## 激活判断

`Wave 6` 当前满足“可以启动第一刀 evidence slice”的条件：

- `Linux` checkpoint 已冻结在 `timerfd-one-shot-readback-ok`，不再作为近端无限扩展 blocker。
- `xv6` shell、Linux dummy/probe、pipeline `vector_cnn`、debug CLI 和现有 workload
  已经形成稳定 guardrail。
- `Wave 5` 已把 cache / memory-system 从纯观察推进到默认关闭、可显式启用、可观察和已
  hardening 的最小 L1D 模型。
- 现有 profile / debug / probe 基础足以承载一刀不改语义的 hot-path evidence。

但这些条件只允许 `Wave 6 Slice A` 做证据收集和候选区间合同；后续 JIT engine 或
multicore / coherence 仍需要新的设计和计划。

## 方案

### Slice A：JIT / DBT hot-path evidence

第一刀只回答“哪些代码区间值得未来翻译”，不回答“如何翻译”。

允许范围：

- 复用现有 execution/profile 数据，固定可重复的 hot-path candidate 口径。
- 在 debug/probe 或 host smoke 中输出只读 candidate 摘要。
- 以代表性 workload 验证 candidate 输出稳定、默认路径行为不变。
- 保持输出可降级：没有足够证据时应输出空候选或低置信度候选，而不是制造假热点。

禁止范围：

- 不创建 JIT block cache。
- 不生成或执行宿主代码。
- 不把 hot-path candidate 写成 guest ABI 或 debug ABI 的破坏性变更。
- 不把 pipeline speculation、L1D counters 或 AI accelerator timing 混成同一个性能结论。

### 后续切片候选

后续只有在 `Slice A` 给出稳定证据后，才允许继续拆分：

- `Slice B / translation contract design`
  - 只定义 translator 输入、helper 边界、fault / trap 回退和 invalidation 口径。
- `Slice C / interpreter-assisted DBT prototype`
  - 只在 opt-in 路径做最小 prototype，仍以 functional result 等价为验收。
- `multicore / coherence readiness`
  - 必须另开设计，先补 atomic、memory-order、DMA / cache 交界和 verification matrix。

## 验证思路

`Slice A` 的验证重点是“不改变语义 + 输出可重复证据”：

- 先补最窄 host / probe 回归，固定 candidate 摘要格式和空候选 fallback。
- 覆盖至少一个代表性 workload 的 hot-path signal。
- 守住默认门禁：
  - `cd myCPU && make test`
  - `cd myCPU && make test-pipeline`
- 若触达 frontend，再补：
  - `cd frontend && node --test`

## 风险与取舍

- 第一刀只做 evidence，看起来不像“真正 JIT”；但这能防止在没有热点证据和回退合同前过早引入执行语义分叉。
- hot-path 统计如果过早设计成长期 ABI，会约束后续实现；因此 `Slice A` 输出应定位为 debug/probe 观察合同，不是 guest ABI。
- multicore / coherence 与 JIT 同属 `Wave 6`，但不应同刀启动；它需要更重的 memory-order 和 DMA / cache 交界验证。

## 当前有效性说明

- 当前有效：本文档作为主线 `Wave 6 / JIT / DBT` 首轮 readiness 与 Slice A 设计入口。
- 当前活跃计划是
  [../plan/mainline_wave6_jit_dbt_hot_path_evidence_slice_a_plan.md](../plan/mainline_wave6_jit_dbt_hot_path_evidence_slice_a_plan.md)。
- `Wave 6` 已激活，但当前只允许推进 `JIT / DBT hot-path evidence`；JIT engine、
  multicore / coherence、write-back cache、I-cache 和 cache maintenance instruction
  仍未启动。
