# Wave 5 Cache / Memory-system 设计

## 文档定位

本文档记录主线 `Wave 5 / cache / memory-system` 的当前有效设计边界。

它回答：

- 为什么 `Wave 5` 先从 memory signal 与合同收口开始。
- `cache / memory-system` 第一刀允许推进到哪里。
- 后续最小可执行 L1 data cache 模型需要满足哪些前置条件。

本文档不记录执行 checklist。当前进度以
[../status/mainline_status.md](../status/mainline_status.md)、计划归档和后续活跃计划为准。

## 关联文档

- 状态文档：
  - [../status/mainline_status.md](../status/mainline_status.md)
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
- 相关设计：
  - [future_expansion_roadmap_design.md](future_expansion_roadmap_design.md)
  - [phase4_preparation_design.md](phase4_preparation_design.md)
  - [platform_mmio_contract.md](platform_mmio_contract.md)
  - [pipeline_speculation_contracts.md](pipeline_speculation_contracts.md)

## 背景与问题

当前仓库已经是一个已可运行的模拟器原型。`P4-prep-1` 已把
`memory_region` 合同收口到 `Bus`，`C1 / P4-prep-2` 已把
`shadow_cache` 观测接入 `ExecutionProfile`、debug JSON 和 probe 摘要。
这些工作给 `Wave 5` 提供了入口，但还不等于真实 cache 行为已经可以直接打开。

`shadow_cache` 当前只做读侧观测，不改变 guest 可见行为。真正的 cache /
memory-system 模型会触及 load/store 可见性、MMIO side effect、atomic、DMA buffer
ownership、pipeline side effect 顺序和后续 coherence 边界。这里如果跳太大，会把
reference-first 的正确性基线暴露在过多新状态空间里。

因此 `Wave 5` 的第一刀不是直接上完整 `D-cache / I-cache / DMA coherence`，而是先
把两件事收口清楚：一条更可信的 pipeline-side memory signal，以及后续最小 L1 data
cache 模型必须遵守的合同。

## 目标

- 把主线 `Wave 5` 的第一段执行范围限定为 `memory signal + cache contract`。
- 复用 `Bus::describe_region()/describe_span()` 作为 cacheability / side effect /
  DMA visibility 的唯一事实来源。
- 固定后续最小 L1 data cache 模型的保守策略：只覆盖 cacheable RAM，不改变 guest
  架构结果。
- 明确 `Wave 5` 与 AI accelerator 后续专项、JIT、multicore / coherence 的边界。

## 非目标

- 不在 Slice A 里实现完整 L1 data cache。
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

## 方案

### 结构设计

`Wave 5` 按小切片推进：

1. `Slice A / signal + contract`
   - 固定 pipeline-side workload memory signal。
   - 明确最小 L1 data cache 合同。
   - 不改变 guest 可见行为。
2. `Slice B / minimal executable L1D`
   - 仅在 Slice A 绿灯后启动。
   - 初始模型应是可关闭、可验证、保守的 L1 data cache。
   - 默认策略倾向 `write-through + no dirty write-back`，避免第一刀引入 flush /
     eviction / DMA coherence 语义。
3. `Slice C / L1D opt-in observation + guardrail`
   - 仅在 Slice B 默认关闭、行为等价的 L1D 执行模型稳定后启动。
   - 只增加只读 debug/probe counters 和显式 opt-in workload guardrail。
   - 不改写既有 `shadow_cache` 字段语义，不声明性能提升。
4. `Slice D / L1D hardening`
   - 仅固定已接入 L1D 的边界合同，不扩新大功能。
   - 重点覆盖跨 line、store miss、fault refill、bypass 路径和默认门禁。
5. `Slice E / L1D frontend observation`
   - 只把已有 `l1_data_cache` counters 接入 frontend 只读观察面。
   - 不扩 debug ABI、后端 cache 功能或默认 L1D 开关。
6. `Slice F / L1D lifecycle guardrail`
   - 只固定显式 opt-in L1D 在 machine / debug load lifecycle 下的清理和失效合同。
   - 覆盖 payload load 覆盖 cached RAM line、reset 清空 counters 等生命周期边界。
   - 不扩 write-back、DMA coherence、multicore、JIT、I-cache 或 cache maintenance
     instruction。

### Slice A 收口结果

当前 `Slice A / signal + contract` 已完成，完成结果只证明 cache / memory-system
第一刀的入口条件，不代表真实 cache 行为已经接入：

- pipeline-side guardrail 固定为
  `RunDebugCliProbeTest.test_real_xv6_probe_emits_pipeline_memory_signal`。
  该 guardrail 通过 `debug-cli` 以 `--backend pipeline` 加载真实 `xv6` kernel 和
  `fs.img`，在 5000 cycle probe 窗口固定 `profile.memory`、
  `shadow-cache` 和 RAM `memory-top` 摘要。
- 这条 guardrail 是 pipeline-side memory observation 信号，不是 “pipeline 已完整
  boot xv6” 的证明。当前 pipeline xv6 probe 仍会暴露 trap-heavy 的早期窗口。
- Linux 真实 `Image` runtime 仍保持 opt-in；默认仓库只固定 dummy/probe 输出合同，
  不把 `timerfd-one-shot-readback-ok` 写成默认已证明 runtime 结果。
- 基于上述信号与现有 `functional xv6`、`functional linux_proto dummy-payload`、
  pipeline `vector_cnn` RAM shadow-cache baseline，允许进入后续
  `Slice B / minimal executable L1D`，但只允许按下方保守合同实现。

### 接口 / 数据 / 契约

后续最小 L1 data cache 模型应满足：

- 只对 `cacheable && !has_side_effect` 的 RAM region 建模。
- MMIO、unmapped、fault 和 non-cacheable region 继续 bypass，并保持现有 fault /
  side effect 可见性。
- stores 必须保持 guest 可见结果与 reference path 一致；第一刀优先采用
  write-through，避免 dirty line 生命周期。
- atomic / fence 先按保守路径处理：允许 bypass 或 serialize，但不能重排到会改变
  architectural result 的程度。
- DMA interaction 第一刀不承诺透明 coherence；如果后续 cache 模型可能看到 DMA 写入，
  必须先通过 explicit bypass / invalidate / opt-in 开关固定合同。
- debug/profile 输出可以新增 cache counters，但不能移除或改写既有
  `shadow_cache` 字段语义。
- 第一版 L1D 必须默认可关闭；打开后仍应以 functional/reference 结果等价作为验收，
  不能用宿主机 wall-clock 或 cache counter 变化宣称性能结论。

### 验证思路

Slice A 的验证重点是证据与合同：

- pipeline-side workload 至少给出一条稳定 memory observation guardrail。
- `shadow_cache` 的 RAM / MMIO / bypass 计数不回退。
- 现有 debug JSON、probe 文本和 frontend 消费不被破坏。

后续 Slice B 的验证重点才是行为等价：

- 最窄 unit test 覆盖 cache hit / miss / bypass / write-through。
- host smoke 覆盖 functional / pipeline 结果等价。
- `make test`、`make test-pipeline` 作为默认总门禁。

### Slice B 收口结果

当前 `Slice B / minimal executable L1D` 已完成，完成结果是一个保守、默认关闭的
最小 data cache 执行模型：

- `SimpleL1DataCache` 只缓存 `cacheable && !has_side_effect` 的 RAM line。
- stores 采用 write-through；当前没有 dirty line 生命周期，也没有 write-back /
  flush / cache maintenance instruction。
- `AddressSpace` 只在 L1D 绑定且启用时，让 data load/store 经过 L1D。
- instruction fetch、page walk、atomic、MMIO、unmapped、fault access 和 side-effect
  region 继续 bypass L1D。
- `Machine` 默认不启用 L1D；只有显式调用 `set_l1_data_cache_enabled(true)` 时才打开。
- `Slice B` 不新增 debug/profile cache counters，不声明性能提升。

### Slice C 收口结果

当前 `Slice C / L1D opt-in observation + guardrail` 已完成，结果归档见
[../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-c-l1d-observation-guardrail-plan](../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-c-l1d-observation-guardrail-plan)。
本轮完成结果是：

- 为默认关闭的 `SimpleL1DataCache` 暴露顶层 `l1_data_cache` debug snapshot
  只读 counters。
- 为 `run_debug_cli_probe.py --l1d` 增加显式 opt-in L1D 开关和 `l1d-cache:`
  文本摘要；默认 probe 路径仍不打开 L1D。
- 用最小 flat workload guardrail 证明打开 L1D 后 guest 结果等价，并能观察到 hit /
  miss / write-through 信号。
- 既有 `shadow_cache` 字段语义不变；本轮只提供观察与 guardrail，不声明性能提升。

后续仍不允许直接把 L1D 扩成 write-back、DMA coherence、multicore、JIT、I-cache 或
cache maintenance instruction。

### Slice D 收口结果

当前 `Slice D / L1D hardening` 已完成，结果归档见
[../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-d-l1d-hardening-plan](../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-d-l1d-hardening-plan)。
本轮完成结果是：

- 跨 cache line store 继续 bypass L1D，但成功写入 backing bus 后会失效所有重叠
  cached line，避免后续 load 读到陈旧 cache bytes。
- store miss 固定为 write-through + no-allocate，并作为 miss 进入 L1D counters。
- non-cacheable、side-effect、unmapped 和 refill fault 路径不污染 cache state。
- instruction fetch、page walk 和 atomic memory operation 继续绕过 L1D。
- 默认 `make test` / `make test-pipeline` 不携带 `--l1d`；L1D 仍只在显式
  debug/probe opt-in 路径打开。

本轮仍不实现 write-back、DMA coherence、multicore、JIT、I-cache 或 cache
maintenance instruction。

### Slice E 收口结果

当前 `Slice E / L1D frontend observation` 已完成，结果归档见
[../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-e-l1d-frontend-observation-plan](../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-e-l1d-frontend-observation-plan)。
本轮完成结果是：

- frontend 平台组新增只读 `L1 data cache` 面板。
- 面板展示已有顶层 `l1_data_cache` debug snapshot counters：enabled、line size、
  capacity、loads、stores、hits、misses、evictions、bypasses 和 write-through
  stores。
- 无 `l1_data_cache` 字段或默认关闭 snapshot 时保持稳定 fallback。
- 不扩 debug ABI，不改变 `shadow_cache` 字段语义，不改变 L1D 默认关闭和
  debug/probe 显式 opt-in 边界。

本轮仍不实现 write-back、DMA coherence、multicore、JIT、I-cache 或 cache
maintenance instruction。

### Slice F 收口结果

当前 `Slice F / L1D lifecycle guardrail` 已完成，结果归档见
[../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-f-l1d-lifecycle-guardrail-plan](../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-f-l1d-lifecycle-guardrail-plan)。
本轮完成结果是：

- `BinaryLoader::load()` 返回写入 byte count，供调用方按覆盖范围处理后续状态。
- `Machine::load_binary_payload()` 写入 RAM payload 后，会按 payload 覆盖范围失效
  L1D line，避免启用中的 L1D 在 payload 覆盖后继续返回旧 cached bytes。
- primary load / `cpu_init()` / debug reset 继续清空 L1D line state 与 counters。
- debug reset 保留显式 opt-in 的 `enabled=true` 状态，但 counters 回到 0。

本轮仍不实现 write-back、DMA coherence、multicore、JIT、I-cache 或 cache
maintenance instruction。

### Wave 5 Closeout 结果

当前 `Wave 5 closeout / Wave 6 readiness` 已完成，结果归档见
[../plan/history_plan.md#mainline-wave5-closeout-wave6-readiness-plan](../plan/history_plan.md#mainline-wave5-closeout-wave6-readiness-plan)。
本轮结论是：

- `Wave 5` `Slice A ~ F` 已足够作为 cache / memory-system 首轮收口。
- 当前完成态仍是保守 L1D：默认关闭、RAM-only、write-through、no dirty
  write-back、显式 opt-in 观察与边界 hardening。
- `Wave 5` 不继续扩成 write-back、DMA coherence、multicore、JIT、I-cache 或
  cache maintenance instruction。
- 主线 active wave 转入 `Wave 6`，第一刀只做 `JIT / DBT hot-path evidence`。

## 风险与取舍

- 先做 Slice A 会让 `Wave 5` 启动看起来偏“证据化”，但它可以防止真实 cache 模型过早污染
  reference path。
- 第一版 L1D 倾向 write-through，性能模型不够真实；但它显著降低 dirty eviction、DMA
  coherence 和 fault recovery 的状态空间。
- 如果 pipeline-side `xv6 / Linux` memory signal 仍不稳定，`Wave 5` 应继续收口证据，
  而不是用不稳定 workload 推动 cache 行为上线。

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
- `Wave 6` `Slice A / JIT DBT hot-path evidence` 已完成，结果归档见
  [../plan/history_plan.md#mainline-wave6-jit-dbt-hot-path-evidence-slice-a-plan](../plan/history_plan.md#mainline-wave6-jit-dbt-hot-path-evidence-slice-a-plan)。
  当前暂无主线活跃计划。
  后续如果继续推进 cache 大功能，仍必须遵守 `write-through + no dirty write-back`、RAM-only、
  MMIO/unmapped/side-effect bypass、atomic/fence conservative serialize/bypass，以及 DMA
  不透明 coherence 的边界，除非先另开设计/计划并补足验证。
