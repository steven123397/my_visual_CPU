# 主线统一路线图与排期设计

## 文档定位

本文档不再记录“未来候选路线菜单”。

自 `2026-04-27` 起，本文档的职责调整为：

- 定义主线长期排期
- 明确各条 workstream 的依赖关系和激活门槛
- 给出从已完成基线到更后续 `Linux / observation / AI accelerator / cache / JIT / multicore` 的统一波次安排

与当前 design / status 体系的分工如下：

- 当前模块边界、当前实现方式、当前正式 contract
  - 以对应模块 design 文档为准。
- 当前主线状态、当前风险、当前默认下一步
  - 以 `docs/status/` 为准。
- 本文档
  - 只回答“主线长期怎么排、哪些工作已经纳入主线、各波次之间什么先后关系和依赖门槛”。

因此，本文档现在是主线长期排期设计，而不是“做不做都行的未来菜单”；但它仍然不替代 `status` 对实时状态和当前下一步的记录。

## 关联文档

- 状态文档：
  - [../status/mainline_status.md](../status/mainline_status.md)
  - [../status/project_priority_roadmap.md](../status/project_priority_roadmap.md)
  - [../status/kernel_alpha_status.md](../status/kernel_alpha_status.md)
  - [../status/xv6_linux_jit_status.md](../status/xv6_linux_jit_status.md)
- 相关设计：
  - [debug_frontend_integration.md](debug_frontend_integration.md)
  - [minimal_interactive_os_design.md](minimal_interactive_os_design.md)
  - [phase3_ooo_execution_model_design.md](phase3_ooo_execution_model_design.md)
  - [pipeline_speculation_contracts.md](pipeline_speculation_contracts.md)
  - [vector_ml_workload_direction_design.md](vector_ml_workload_direction_design.md)
  - [npu_tpu_accelerator_direction_design.md](npu_tpu_accelerator_direction_design.md)
  - [phase4_preparation_design.md](phase4_preparation_design.md)
  - [platform_mmio_contract.md](platform_mmio_contract.md)
  - [spike_differential_validation_design.md](spike_differential_validation_design.md)
  - [xv6_linux_jit_mainline_design.md](xv6_linux_jit_mainline_design.md)
- 已完成计划归档：
  - [../plan/history_plan.md#phase4-prep2-memory-observation-shadow-cache-plan](../plan/history_plan.md#phase4-prep2-memory-observation-shadow-cache-plan)
  - [../plan/history_plan.md#xv6-linux-jit-wave1-plan](../plan/history_plan.md#xv6-linux-jit-wave1-plan)

## 背景与问题

当前仓库已经是一个已可运行的模拟器原型。此前这份文档把 `V4 / observation`、`xv6 / Linux`、`AI accelerator`、`JIT / cache / multicore` 分成“默认延续线 / 候选切换线 / 远期激进线”，这在切主线前是合理的；但在 `xv6 / Linux / JIT` 主线已经激活、Linux checkpoint 已连续推进、AI accelerator 也已完成多轮 wave 之后，继续把这些内容表述成“未来候选”已经会制造事实来源冲突。

当前真正需要回答的问题不再是“哪些方向可供未来考虑”，而是：

- 它们在主线里排到哪一波
- 当前哪些波次已经完成
- 当前 active wave 的完成定义是什么
- 哪些后续波次只能在前一波次给出稳定证据后再激活

当前主线已经完成或基本完成的基线包括：

- `Phase 1` 冻结：RV64IM reference path、M/S/U 特权级、Sv39、最小平台设备、`kernel_alpha` bring-up
- `Phase 2` 收口：`functional / pipeline` 双后端、验证补洞两轮完成
- `Phase 3` 首轮：最小 predictor、`rename + ROB + LSQ +` 最小 `OoO execute`、decode 边界收窄与当前后续取舍判断
- `Phase 4` 准备：`P4-prep-1`（`bus / memory region` 合同）与 `P4-prep-2` 第一刀 `C1 / memory observation / shadow cache` 已完成
- 向量扩展：`V-lite` `V0 ~ V4` + 当前前端教学可视化已接通
- 验证体系：现有 `make test` / `make test-pipeline`、guest 正负回归与 Spike 外部差分
- `NPU / TPU-like`：Wave 1~3 已归档，设备基座、bounded dynamic shape 和 profile lifecycle 已接通

在这个基础上，真正需要回答的问题已经变成：主线在未来几个 wave 里要如何继续推进，以及每一波应该由什么门槛决定能否进入下一波。

## 目标

- 给出从当前稳定基线继续往后的主线长期排期，覆盖 ISA、平台、workload、微架构、系统级跃迁 5 个维度。
- 明确各 workstream 的依赖链、激活门槛和波次关系，避免前置条件未满足时抢跑。
- 为每个方向提供当前所处 wave、下一波目标和进入条件。
- 与当前 `reference-first`、`workload-driven`、`small-wave` 的仓库方法论保持一致。

## 非目标

- 不在本文档中维护逐项执行 checklist；具体执行仍应落到 `docs/plan/`。
- 不改写当前 `Phase 1 ~ 3` 已完成的正式判断。
- 不用日历日期承诺每个 wave 的完成时间；这里定义的是相对顺序、门槛和完成定义。
- 不自动覆盖 `docs/status/` 里对“当前下一步”的即时判断。

## 主线组成

当前主线由 6 条长期 workstream 组成：

1. `A`：reference correctness / hardening 常态维护
2. `B`：标准 OS bring-up（当前近端以 `xv6 -> Linux` 为核心）
3. `C`：observation / profile / `V4` / `P4-prep-*` guardrail
4. `D`：独立 `NPU / TPU-like` AI accelerator
5. `E`：cache / memory-system 路线
6. `F`：`JIT / DBT` 与更重系统级跃迁（multicore / coherence）

这些 workstream 全部已经纳入主线排期；差别只在于它们位于不同 wave，且激活门槛不同。

## 排期原则

1. `A` 常开，任何 wave 都不能暂停 correctness / hardening。
2. `B` 当前是主推进线，但不能吞掉 `C` 的 guardrail 预算。
3. `D` 已不再是“未来候选方向”，而是主线的后续 wave；当前只是不在近端 blocker 上。
4. `E` 必须等 `C1 / shadow_cache` 和更稳定 workload 证据足够后再激活。
5. `F` 必须等 `Linux` 与 hot-path/profile 证据都更稳定后再激活。
6. 任何时点最多只允许 1 条重推进线 + 1~2 条并行 guardrail / hardening 线。

## 总体依赖关系

```text
已完成基线
  -> Wave 1：标准 OS foundation + xv6 shell + Linux block-rootfs 多阶段基线
  -> Wave 2：Linux 当前 checkpoint 收口 + 主线排期统一
  -> Wave 3：Linux 更后 userland checkpoint + observation/pipeline gap 判断
  -> Wave 4：AI accelerator 下一轮扩展 + 向量/observation 继续深化
  -> Wave 5：cache / memory-system 第一刀（以 workload 证据触发）
  -> Wave 6：JIT / DBT 与 multicore / coherence（以前置证据触发）
```

重点不是所有线同时推进，而是所有线都已经被排入主线，只是按依赖顺序逐波激活。

## 主线波次

### Wave 0：已完成基线

Wave 0 指当前已经完成、并被后续所有波次依赖的基线：

- `Phase 1` 冻结与 `phase1-stable`
- `P1` 结构收口、`P2` 首轮验证补洞
- `Phase 3` 当前克制边界
- `V-lite V0~V4`
- `P4-prep-1`
- `C1 / P4-prep-2 shadow_cache` 第一刀
- `NPU / TPU-like` Wave 1~3

Wave 0 的含义不是“以后不再碰”，而是“这些基线已完成首轮收口，后续只做 bug-driven hardening 或按新 wave 的前置条件演进”。

### Wave 1：已完成的标准 OS bring-up foundation

Wave 1 已完成，完成定义包括：

- `RV64A + CSR / privilege` foundation
- `virtio-mmio + virtio-blk` platform foundation
- 真实 `virtio-blk` board path 下的 `xv6` shell guardrail
- Linux-facing `flat image + payload + set_gpr + dtb/chosen/cmdline` foundation
- repo-generated initramfs / block-rootfs fallback
- Linux block-rootfs 的 multi-stage post-init baseline
- `functional`/probe 侧 observation guardrail 首轮收口

Wave 1 现在已经不是待选路线，而是主线既成事实。

### Wave 2：已完成的排期统一与当前 checkpoint 收口

Wave 2 已完成，完成内容包括：

1. 把 Linux 当前最小 block-rootfs post-init checkpoint 线收口到自然停顿点
2. 把长期路线图从“未来菜单”改成“主线排期设计”

Wave 2 的完成结果：

- 在既有 fourth-stage baseline 之后，再冻结一处窄、稳定、真实 `Image` 可验证的 userland contract：`unlinkat-open-fd-survives`
- `future_expansion_roadmap_design.md` 的“未来 / 候选”语义全部移除
- `status / AGENTS / 相关 design` 不再把 AI accelerator、`V4`、`cache`、`JIT` 表述成“主线外待定菜单”

### Wave 3：当前 active wave

Wave 3 是当前 active wave，计划目标：

- 继续沿真实 Linux `Image + rootfs.ext4` 路径冻结下一处更后 userland 或 platform checkpoint
- 在不扩大 Linux 功能面的前提下，判断当前 `functional` observation guardrail 是否已经足够，还是需要单独收口 `xv6 / Linux` pipeline gap

Wave 3 的激活门槛：

- Wave 2 的 Linux 当前 checkpoint 线已经形成自然停顿点
- 当前 `xv6` shell 与 probe guardrail 稳定

### Wave 4：AI accelerator 与向量 / observation 深化

Wave 4 不是可选方向，而是已排进主线的后续波次，目标包括：

- 沿 `NPU / TPU-like` 当前 Wave 3 完成态继续扩下一刀
- 继续围绕 `V4` 与 workload observation 做更窄 hardening
- 只在有明确收益时再补更像 cache 评估前置的 workload 分析

Wave 4 的激活门槛：

- Linux 当前 checkpoint 线不再是近端 blocker
- `shadow_cache` 和代表性 workload 已能提供足够稳定的读侧信号

### Wave 5：cache / memory-system 第一刀

Wave 5 仍然属于主线，但只在证据充分时激活。目标是：

- 基于现有 `memory_region` 与 `shadow_cache` 证据，决定是否落最小 L1 cache 模型
- 在不破坏 reference-first 的前提下，把 cache 从“只读观测”推进到“真实可执行模型”

Wave 5 的激活门槛：

- `C1` 观测已经在代表性 workload 上形成稳定判断
- `xv6 / Linux` 至少有一条更可信的 pipeline-side memory signal 路径

### Wave 6：JIT / DBT 与 multicore / coherence

Wave 6 也是主线内排期，但属于高门槛波次。目标是：

- 基于 Linux / workload / profile 证据决定 `JIT / DBT` 是否值得正式启动
- 基于 cache 路线与 memory-order 验证进展决定 multicore / coherence 是否可启动

Wave 6 的激活门槛：

- 有足够明确的 hot-path/profile 证据
- `cache / DMA` 路线已不再处于准备态
- 当前平台与 Linux bring-up 不再频繁暴露基础 contract 缺口

## 各方向当前安排

### A：ISA 补全与平台成熟

- `RV64A`、`virtio-blk`、首轮 `CSR / privilege` 已进入已完成基线。
- `F / D`、更后续 `pmp* / menvcfg / stimecmp` 等 contract 已纳入主线，但继续按 workload 暴露顺序按需激活。
- 当前这条线的策略不是“大一统补齐”，而是服务 `Linux / cache / JIT` 后续波次的 bug-driven hardening。

### B：标准 OS bring-up

- `xv6` shell 已从激活理由变成稳定 guardrail。
- `Linux` 当前是 Wave 2 / Wave 3 的核心推进面。
- 这条线直到更完整 userland checkpoint 稳定前，都会持续占据主线近端优先级。

### C：向量 / observation / `Phase 4` 准备

- `V4`、`P4-prep-1`、`C1` 已都是已完成基线。
- 当前继续承担主线 guardrail、workload corpus 和证据收集职责。
- `cache` 不会跳过这条线直接激活。

### D：独立 `NPU / TPU-like` AI accelerator

- 该方向已经完成 Wave 1~3，不再是“未来候选”。
- 当前被排到 Linux 当前 checkpoint 之后的主线后续 wave，先进入维护态和门槛等待态。
- 一旦 Wave 4 激活，就继续沿独立 device 路线推进，而不是回退成 CPU 向量扩展的附属项。

### E：cache / memory-system

- 当前明确属于主线后续 wave，而不是主线外议题。
- 但它必须后置到 `shadow_cache` 证据、workload baseline 和 Linux / pipeline gap 判断之后。

### F：JIT / DBT / multicore / coherence

- 当前也已纳入主线长期排期。
- 但它们只在 Linux 更稳定、profile 更明确、cache 路线更实之后才允许激活。

## 风险与取舍

- 把所有方向都纳入主线排期，能避免“未来候选”歧义，但也更要求严格的波次门槛；否则容易重新回到所有方向同时抢跑。
- Linux 当前 checkpoint 线如果继续无限细分，会拖住后续 wave；因此 Wave 2 完成定义必须强调“自然停顿点”，而不是追求无限 marker。
- AI accelerator 被纳入主线后，不代表它立刻变成近端 blocker；它当前仍应服从 Linux 波次让路。
- `cache / JIT / multicore` 都已被明确排期，但激活条件必须依赖 workload 证据，而不是排期一到就机械开工。

## 当前有效性说明

- 当前有效：本文档作为主线长期排期设计。
- 当前实时状态、当前风险和“这一轮先做什么”，仍以 [../status/mainline_status.md](../status/mainline_status.md)、[../status/project_priority_roadmap.md](../status/project_priority_roadmap.md) 与各专项状态文档为准。
- 当前 active wave 的具体执行结果，应继续回写到对应 `status` 文档和活跃 `plan`，而不是在本文档里堆实时进度。
