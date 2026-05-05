# Wave 5 Cache / Memory-system 设计

## 文档定位

本文档记录主线 `Wave 5 / cache / memory-system` 的当前有效设计边界。

它回答：

- `Wave 5` 已经沉淀出哪些 cache / memory-system 合同。
- 当前最小 L1 data cache 模型的功能边界是什么。
- 哪些更重 memory-system 能力仍未启用。

本文档不记录执行 checklist。当前进度以
[../status/mainline_status.md](../status/mainline_status.md)、计划归档和后续活跃计划为准。

## 关联文档

- 状态文档：
  - [../status/mainline_status.md](../status/mainline_status.md)
- 已完成计划归档：
  - [../plan/history_plan.md#mainline-wave6-jit-dbt-hot-path-evidence-slice-a-plan](../plan/history_plan.md#mainline-wave6-jit-dbt-hot-path-evidence-slice-a-plan)
  - [../plan/history_plan.md#mainline-wave5-closeout-wave6-readiness-plan](../plan/history_plan.md#mainline-wave5-closeout-wave6-readiness-plan)
  - [../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-f-l1d-lifecycle-guardrail-plan](../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-f-l1d-lifecycle-guardrail-plan)
  - [../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-e-l1d-frontend-observation-plan](../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-e-l1d-frontend-observation-plan)
  - [../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-d-l1d-hardening-plan](../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-d-l1d-hardening-plan)
  - [../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-c-l1d-observation-guardrail-plan](../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-c-l1d-observation-guardrail-plan)
  - [../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-b-minimal-l1d-plan](../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-b-minimal-l1d-plan)
  - [../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-a-signal-contract-plan](../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-a-signal-contract-plan)
- 相关设计：
  - [phase4_preparation_design.md](phase4_preparation_design.md)
  - [platform_mmio_contract.md](platform_mmio_contract.md)
  - [pipeline_speculation_contracts.md](pipeline_speculation_contracts.md)

## 背景与问题

当前仓库已经是一个已可运行的模拟器原型。`P4-prep-1` 已把
`memory_region` 合同收口到 `Bus`，`C1 / P4-prep-2` 已把
`shadow_cache` 观测接入 `ExecutionProfile`、debug JSON 和 probe 摘要。
这些工作已经被 `Wave 5` 消费并形成当前完成态：一条只读 `shadow_cache` 观测面，
一个默认关闭、RAM-only、write-through、no dirty write-back 的最小 L1 data cache，
以及围绕 bypass、fault、frontend 只读展示和 load/reset lifecycle 的 guardrail。

`shadow_cache` 仍只做观测，不改变 guest 可见行为。`SimpleL1DataCache` 只在显式启用时
参与 data load/store，仍不承担 write-back、DMA coherence、multicore、JIT、I-cache 或
cache maintenance instruction。

## 目标

- 记录主线 `Wave 5` 已完成的 `memory signal + cache contract`。
- 复用 `Bus::describe_region()/describe_span()` 作为 cacheability / side effect /
  DMA visibility 的唯一事实来源。
- 固定当前最小 L1 data cache 模型的保守策略：只覆盖 cacheable RAM，不改变 guest
  架构结果。
- 明确 `Wave 5` 与 AI accelerator 后续专项、JIT、multicore / coherence 的边界。

## 非目标

- 不实现 I-cache、write-back dirty state、cache maintenance instruction、
  multicore coherence 或 memory consistency 新模型。
- 不把 AI accelerator 的 `real DMA overlap`、multi outstanding queue 或 Linux-facing
  NPU driver 并入主线 `Wave 5`。
- 不基于宿主机 wall-clock 宣称性能提升。
- 不把真实 Linux `Image` 变成默认仓库依赖。

## 约束与边界

- `InstructionSemantics + functional backend` 继续是 ISA 真值来源；cache 模型不能复制
  或改写 ISA 语义。
- `CPU -> AddressSpace -> Bus -> Ram/Device` 的访存边界继续成立。
- `memory_region` 的 `cacheable / dma_visible / has_side_effect / supports_burst /
  label` 是 cache 与 DMA 判断的唯一事实来源。
- MMIO、unmapped、faulting access 和 side-effect region 必须 bypass cache 路径。
- 任何真实 cache 行为必须先证明不会破坏现有 `make test`、`make test-pipeline`、
  debug snapshot 和 workload guardrail。

## 当前实现合同

### Memory Signal 与 `shadow_cache`

当前 pipeline-side workload memory signal 已固定为
`RunDebugCliProbeTest.test_real_xv6_probe_emits_pipeline_memory_signal`：

- 该 guardrail 通过 `debug-cli` 以 `--backend pipeline` 加载真实 `xv6` kernel 和
  `fs.img`，在 5000 cycle probe 窗口固定 `profile.memory`、
  `shadow-cache` 和 RAM `memory-top` 摘要。
- 这条 guardrail 是 pipeline-side memory observation 信号，不是 “pipeline 已完整
  boot xv6” 的证明。当前 pipeline xv6 probe 仍会暴露 trap-heavy 的早期窗口。
- Linux 真实 `Image` runtime 仍保持 opt-in；默认仓库只固定 dummy/probe 输出合同，
  不把 `timerfd-one-shot-readback-ok` 写成默认已证明 runtime 结果。

### L1D 执行模型

当前最小 L1 data cache 模型满足：

- 只对 `cacheable && !has_side_effect` 的 RAM region 建模。
- MMIO、unmapped、fault 和 non-cacheable region 继续 bypass，并保持现有 fault /
  side effect 可见性。
- stores 保持 guest 可见结果与 reference path 一致，采用 write-through，当前没有 dirty
  line 生命周期。
- atomic / fence 按保守路径处理：bypass 或 serialize，但不重排到会改变 architected result
  的程度。
- DMA interaction 不承诺透明 coherence。
- debug/profile 输出可以新增 cache counters，但不能移除或改写既有
  `shadow_cache` 字段语义。
- L1D 默认关闭；打开后以 functional/reference 结果等价作为验收，不能用宿主机
  wall-clock 或 cache counter 变化宣称性能结论。

- `SimpleL1DataCache` 只缓存 `cacheable && !has_side_effect` 的 RAM line。
- stores 采用 write-through；当前没有 dirty line 生命周期，也没有 write-back /
  flush / cache maintenance instruction。
- `AddressSpace` 只在 L1D 绑定且启用时，让 data load/store 经过 L1D。
- instruction fetch、page walk、atomic、MMIO、unmapped、fault access 和 side-effect
  region 继续 bypass L1D。
- `Machine` 默认不启用 L1D；只有显式调用 `set_l1_data_cache_enabled(true)` 时才打开。
- `Slice B` 不新增 debug/profile cache counters，不声明性能提升。

### L1D 观察面

当前 L1D 观察面已经收口为只读 debug/probe/frontend 字段：

- 为默认关闭的 `SimpleL1DataCache` 暴露顶层 `l1_data_cache` debug snapshot
  只读 counters。
- 为 `run_debug_cli_probe.py --l1d` 增加显式 opt-in L1D 开关和 `l1d-cache:`
  文本摘要；默认 probe 路径仍不打开 L1D。
- 用最小 flat workload guardrail 证明打开 L1D 后 guest 结果等价，并能观察到 hit /
  miss / write-through 信号。
- 既有 `shadow_cache` 字段语义不变；本轮只提供观察与 guardrail，不声明性能提升。
- 跨 cache line store 继续 bypass L1D，但成功写入 backing bus 后会失效所有重叠
  cached line，避免后续 load 读到陈旧 cache bytes。
- store miss 固定为 write-through + no-allocate，并作为 miss 进入 L1D counters。
- non-cacheable、side-effect、unmapped 和 refill fault 路径不污染 cache state。
- instruction fetch、page walk 和 atomic memory operation 继续绕过 L1D。
- 默认 `make test` / `make test-pipeline` 不携带 `--l1d`；L1D 仍只在显式
  debug/probe opt-in 路径打开。
- frontend 平台组新增只读 `L1 data cache` 面板。
- 面板展示已有顶层 `l1_data_cache` debug snapshot counters：enabled、line size、
  capacity、loads、stores、hits、misses、evictions、bypasses 和 write-through
  stores。
- 无 `l1_data_cache` 字段或默认关闭 snapshot 时保持稳定 fallback。
- 不扩 debug ABI，不改变 `shadow_cache` 字段语义，不改变 L1D 默认关闭和
  debug/probe 显式 opt-in 边界。

### Lifecycle Guardrail

当前 lifecycle guardrail 固定 opt-in L1D 在 machine / debug load lifecycle 下的清理和失效：

- `BinaryLoader::load()` 返回写入 byte count，供调用方按覆盖范围处理后续状态。
- `Machine::load_binary_payload()` 写入 RAM payload 后，会按 payload 覆盖范围失效
  L1D line，避免启用中的 L1D 在 payload 覆盖后继续返回旧 cached bytes。
- primary load / `cpu_init()` / debug reset 继续清空 L1D line state 与 counters。
- debug reset 保留显式 opt-in 的 `enabled=true` 状态，但 counters 回到 0。

### 完成态边界

当前 `Wave 5 closeout / Wave 6 readiness` 已完成，结果归档见
[../plan/history_plan.md#mainline-wave5-closeout-wave6-readiness-plan](../plan/history_plan.md#mainline-wave5-closeout-wave6-readiness-plan)。
完成态结论是：

- `Wave 5` `Slice A ~ F` 已足够作为 cache / memory-system 首轮收口。
- 当前完成态仍是保守 L1D：默认关闭、RAM-only、write-through、no dirty
  write-back、显式 opt-in 观察与边界 hardening。
- `Wave 5` 不继续扩成 write-back、DMA coherence、multicore、JIT、I-cache 或
  cache maintenance instruction。
- 后续若重新打开更重 memory-system 能力，必须先重新定义新的设计 / 计划边界。

## 风险与取舍

- `Wave 5` 先做证据与合同收口，降低了真实 cache 模型过早污染 reference path 的风险。
- 第一版 L1D 倾向 write-through，性能模型不够真实；但它显著降低 dirty eviction、DMA
  coherence 和 fault recovery 的状态空间。
- pipeline-side `xv6 / Linux` memory signal 仍不能被误读为完整 pipeline Linux runtime。

## 当前有效性说明

- 当前有效：本文档作为主线 `Wave 5 / cache / memory-system` 的设计入口。
- `Slice A / signal + contract` 已完成，结果归档见
  [../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-a-signal-contract-plan](../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-a-signal-contract-plan)。
- `Slice B / minimal executable L1D` 已完成，结果归档见
  [../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-b-minimal-l1d-plan](../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-b-minimal-l1d-plan)。
- `Slice C / L1D opt-in observation + guardrail` 已完成，结果归档见
  [../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-c-l1d-observation-guardrail-plan](../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-c-l1d-observation-guardrail-plan)。
- `Slice D / L1D hardening` 已完成，结果归档见
  [../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-d-l1d-hardening-plan](../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-d-l1d-hardening-plan)。
- `Slice E / L1D frontend observation` 已完成，结果归档见
  [../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-e-l1d-frontend-observation-plan](../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-e-l1d-frontend-observation-plan)。
- `Slice F / L1D lifecycle guardrail` 已完成，结果归档见
  [../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-f-l1d-lifecycle-guardrail-plan](../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-f-l1d-lifecycle-guardrail-plan)。
- `Wave 5 closeout / Wave 6 readiness` 已完成，结果归档见
  [../plan/history_plan.md#mainline-wave5-closeout-wave6-readiness-plan](../plan/history_plan.md#mainline-wave5-closeout-wave6-readiness-plan)。
- 如果继续推进 cache 大功能，仍必须遵守 `write-through + no dirty write-back`、RAM-only、
  MMIO/unmapped/side-effect bypass、atomic/fence conservative serialize/bypass，以及 DMA
  不透明 coherence 的边界，除非先另开设计/计划并补足验证。
