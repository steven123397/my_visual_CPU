# Phase 4 准备性设计

## 文档定位

本文档记录当前仓库面向 `Phase 4` 的统一准备性边界，重点说明下面 3 件事：

- 为什么当前仍不直接启动完整 `cache / DMA / multicore / coherence`
- 已完成的 `P4-prep-1` 到底沉淀成了什么正式 contract
- 如果未来继续往前走，`P4-prep-2 / P4-prep-3` 应如何理解

本文档只保留当前仍然有效的准备性设计，不再把“某一轮要不要开做”写成过程性实施提案。

## 关联文档

- 状态文档：
  - [../status/mainline_status.md](../status/mainline_status.md)
- 相关设计：
  - [wave5_cache_memory_system_design.md](wave5_cache_memory_system_design.md)
  - [platform_mmio_contract.md](platform_mmio_contract.md)
  - [vector_ml_workload_direction_design.md](vector_ml_workload_direction_design.md)
  - [debug_frontend_integration.md](debug_frontend_integration.md)
- 当前计划：
  - 暂无主线活跃计划；继续推进 `Wave 6` 下一刀前先新建 `docs/plan/` 计划。
- 已完成计划归档：
  - [../plan/history_plan.md#mainline-wave6-jit-dbt-hot-path-evidence-slice-a-plan](../plan/history_plan.md#mainline-wave6-jit-dbt-hot-path-evidence-slice-a-plan)
  - [../plan/history_plan.md#mainline-wave5-closeout-wave6-readiness-plan](../plan/history_plan.md#mainline-wave5-closeout-wave6-readiness-plan)
  - [../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-f-l1d-lifecycle-guardrail-plan](../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-f-l1d-lifecycle-guardrail-plan)
  - [../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-e-l1d-frontend-observation-plan](../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-e-l1d-frontend-observation-plan)
  - [../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-d-l1d-hardening-plan](../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-d-l1d-hardening-plan)
  - [../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-c-l1d-observation-guardrail-plan](../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-c-l1d-observation-guardrail-plan)
  - [../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-b-minimal-l1d-plan](../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-b-minimal-l1d-plan)
  - [../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-a-signal-contract-plan](../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-a-signal-contract-plan)
  - [../plan/history_plan.md#phase4-prep2-memory-observation-shadow-cache-plan](../plan/history_plan.md#phase4-prep2-memory-observation-shadow-cache-plan)
  - [../plan/history_plan.md#phase4-prep1-bus-memory-region-plan](../plan/history_plan.md#phase4-prep1-bus-memory-region-plan)

## 背景与问题

当前仓库已经是一个已可运行的模拟器原型。`Phase 1` bring-up、`Phase 3` 的最小 `OoO execute`、`debug/frontend` 教学演示链路，以及 `向量扩展 + ML workload` 的 `V-lite` `V0 ~ V4` 都已经接通。现阶段主线方法论仍然是：reference-first、小步推进、由真实 workload 或真实 bug 驱动更重结构。

在这个语境下，`Phase 4` 不能简单理解成“下一步立刻做 cache / DMA / multicore”。当前还缺少足够稳定的 memory-level workload 信号，也还没有证据说明哪一刀最值得先做；如果过早把完整 `Phase 4` 打开，只会提前放大状态空间与验证成本。

但这并不意味着 `Phase 4` 当前完全不能动。更健康的做法，是先把那些本身就有独立结构收益、又不会改变 guest 可见语义的准备性工作收口成正式边界。`P4-prep-1` 就属于这类工作。

## 目标

- 把当前 `Phase 4` 的正式入口限定为一组准备性切片，而不是完整大专项。
- 说明 `P4-prep-1` 已完成后留下的统一 `bus / memory region` contract。
- 给未来是否继续 `P4-prep-2 / P4-prep-3` 提供清晰依赖关系和判断口径。
- 保持当前 guest 可见语义、reference correctness 与已有验证基线不被顺手扩大或污染。

## 非目标

- 不在本文档中定义真实 `D-cache / I-cache`、DMA engine、multicore 或 coherence 的实施计划。
- 不把 `P4-prep-1` 夸大成性能结论或阶段性“已经进入 Phase 4 实施”。
- 不改写 [platform_mmio_contract.md](platform_mmio_contract.md) 里当前对 guest 暴露的平台设备合同。
- 不把未来所有 `Phase 4` 候选方向都自动升级成当前默认下一步。

## 当前统一设计边界

### 1. 当前对 `Phase 4` 的正式理解

当前仓库把 `Phase 4` 理解为“需要先做准备项，再决定是否值得继续展开”的远期阶段，而不是一个已经进入完整实施的活跃专项。

因此，当前正式保留的准备性切片只有 3 类：

1. `P4-prep-1`
   - `bus / memory region` 合同收口
2. `P4-prep-2`
   - workload 驱动的 `memory observation / shadow cache`
3. `P4-prep-3`
   - DMA-ready 的 initiator / transaction 合同

其中当前已经落地的是 `P4-prep-1`，以及 `P4-prep-2` 的第一刀 `C1 / memory observation / shadow cache`；`NPU / TPU-like` AI accelerator Wave 1 也已经消费并落地了一条更窄的 `DMA-ready` initiator / transaction 合同。更通用的 DMA 设备模型，以及完整 `cache / multicore / coherence` 仍然只是候选后继项。

### 2. `P4-prep-1` 已沉淀的正式 contract

`P4-prep-1` 的核心不是新功能面，而是统一物理 memory region 的事实来源。当前这层 contract 已经明确收口到 `Bus` 侧，至少稳定表达：

- `kind`
  - `ram`
  - `mmio`
  - `unmapped`
- `cacheable`
- `dma_visible`
- `has_side_effect`
- `supports_burst`
- `label`

这意味着当前仓库里对下面这些问题的正式答案，已经不应该散落在不同执行路径里各写一套：

- 这个物理地址是不是 RAM
- 这段 span 是否跨进 live MMIO
- 这块区域是否允许 cache / DMA / burst
- 这里是不是会产生设备 side effect

### 3. 与现有路径的关系

当前 `P4-prep-1` 只收口内部事实来源，不改变 guest 可见语义。它与现有路径的职责分工如下：

- `AddressSpace`
  - 继续负责虚实地址翻译、权限与 fault 语义
- `Bus`
  - 继续负责物理地址路由，并成为 region 属性的统一查询入口
- 执行路径
  - 在拿到物理地址后，统一复用 `Bus` 提供的 region contract，而不是继续硬编码 `MEM_BASE / MEM_SIZE`

对当前仓库而言，这个收口已经直接服务于：

- 向量 `vle.v / vse.v` 的整段 span 预校验
- `pipeline` / `LSQ` 的 RAM、MMIO 与 side-effect 判断
- 更窄的 memory hardening 与 debug observation

### 4. 当前为什么不直接做真实 `cache / DMA / multicore`

当前不直接跳完整 `Phase 4`，原因不是这些方向不重要，而是它们都需要更稳定的前置信号：

- `cache`
  - 需要更持续的 memory-level workload 证据，才能判断模型复杂度是否值得
- `DMA`
  - 已有 AI accelerator 消费的窄 initiator / transaction contract，但更通用的设备 DMA、buffer ownership 与 cache 交界面仍未展开
- `multicore / coherence`
  - 会显著放大同步、原子性、一致性和平台状态空间，不能在当前准备层不足时抢跑

因此，当前最健康的顺序仍然是：先用准备项把结构边界收口清楚，再决定是否有必要继续往下展开。

### 5. `P4-prep-2 / P4-prep-3` 的正式定位

当前这些方向的后续定位如下：

- `P4-prep-2`
  - 第一刀 `C1` 已经落地为 `execution_profile` 下的地址级 memory observation / shadow cache
  - `ExecutionMemoryObservation` 会携带可选物理地址，`ExecutionProfile` 会聚合全局和 region 级 shadow cache 统计，debug JSON 与 `run_debug_cli_probe` 文本摘要只读展示这些结果
  - 当前第一组稳定 baseline 已经收口为：
    - pipeline `vector_cnn` 的 RAM shadow-cache 信号
    - `xv6` 的 functional 5000-step observation baseline
    - 不依赖外部 Linux `Image` 的 `linux_proto` dummy-payload functional observation baseline
  - 重点仍是收集 workload 证据，而不是立刻引入真实 cache 行为
- `P4-prep-3`
  - 首个窄切片已经由 `NPU / TPU-like` AI accelerator Wave 1 落地，形成 `dma_transaction`、`Bus::dma_read()/dma_write()` 与设备侧 fail-closed DMA 语义
  - 后续重点仍是继续定义更通用的 DMA / buffer ownership 边界，而不是把异步设备模型、cache coherence 或多设备 DMA 一次性做完

后续这些方向仍不自动进入默认主线，是否值得继续，应以 `docs/status/` 中的即时判断和 workload 信号为准。

## 验证思路

当前与这条设计直接相关的正式基线至少包括：

- `cd myCPU && make test`
- `cd myCPU && make test-pipeline`

如果改动集中在 memory / bus / vector memory boundary，至少额外关注：

- `cd myCPU && make test-host-execution_profile_smoke`
- `cd myCPU && make test-host-vector_vlite_smoke`
- `cd myCPU && make test-host-vector_cnn_smoke`
- `cd myCPU && make test-host-vector_pipeline_smoke`
- `cd myCPU && make test-host-debug_cli_smoke`
- `cd myCPU && make test-host-run_debug_cli_probe`

验证重点不是“性能有没有提升”，而是：

- RAM / MMIO / unmapped 的 fault 与 side effect 口径不回退
- 现有 debug snapshot / frontend 观察面不被顺手污染
- region contract 真的成为多个路径可复用的统一事实来源

## 风险与取舍

- 当前先收口 region contract，而不直接上真实 cache，会让 `Phase 4` 看起来不够“有功能感”；但这正是当前仓库需要的克制推进方式。
- 当前把 `cacheable / dma_visible / supports_burst` 这类属性提前收口，会让接口略宽于眼下立即需求；但它们都直接服务未来已知候选方向，不属于无约束预留。
- 如果后续 workload 证据仍不足，`P4-prep-1` 也依然有独立价值，因为它已经减少了 memory boundary 判断的重复实现和口径分叉。

## 当前有效性说明

- 当前有效：本文档作为 `Phase 4` 准备性入口的统一设计来源。
- 当前已完成的正式结果是 `P4-prep-1`、`P4-prep-2` 的 `C1 / memory observation / shadow cache` 第一刀及其首轮 workload baseline 收口，以及 AI accelerator Wave 1 消费的一条窄 `DMA-ready` contract。
- 主线 `Wave 5` 已开始消费这些准备性边界，`Slice A / signal + contract` 已由
  [wave5_cache_memory_system_design.md](wave5_cache_memory_system_design.md) 与
  [../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-a-signal-contract-plan](../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-a-signal-contract-plan)
  收口；它只证明 memory signal 与 cache contract 已进入后续最小 L1D 的入口条件，
  不代表完整 cache / DMA / multicore / coherence 已经实现。
- `Slice B / minimal executable L1D` 已由
  [../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-b-minimal-l1d-plan](../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-b-minimal-l1d-plan)
  收口；它只落地默认关闭、RAM-only、write-through 的最小 data cache 模型，
  不代表完整 cache / DMA / multicore / coherence 已经实现。
- `Slice C / L1D opt-in observation + guardrail` 已由
  [../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-c-l1d-observation-guardrail-plan](../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-c-l1d-observation-guardrail-plan)
  收口；它只补显式 opt-in 的 L1D debug/probe 观察面和行为等价 guardrail。
- `Slice D / L1D hardening` 已由
  [../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-d-l1d-hardening-plan](../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-d-l1d-hardening-plan)
  收口；它只固定 L1D 边界合同，不扩成新大功能。
- `Slice E / L1D frontend observation` 已由
  [../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-e-l1d-frontend-observation-plan](../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-e-l1d-frontend-observation-plan)
  收口；它只把已有 L1D counters 接入 frontend 只读观察面，不扩 debug ABI 或
  cache 功能面。
- `Slice F / L1D lifecycle guardrail` 已由
  [../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-f-l1d-lifecycle-guardrail-plan](../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-f-l1d-lifecycle-guardrail-plan)
  收口；它只固定 L1D lifecycle guardrail，不扩成新大功能。
- `Wave 5 closeout / Wave 6 readiness` 已由
  [../plan/history_plan.md#mainline-wave5-closeout-wave6-readiness-plan](../plan/history_plan.md#mainline-wave5-closeout-wave6-readiness-plan)
  收口；`Wave 5` 首轮完成，主线 active wave 转入 `Wave 6`。
- `Wave 6` `Slice A / JIT DBT hot-path evidence` 已由
  [../plan/history_plan.md#mainline-wave6-jit-dbt-hot-path-evidence-slice-a-plan](../plan/history_plan.md#mainline-wave6-jit-dbt-hot-path-evidence-slice-a-plan)
  收口；当前暂无主线活跃计划。
