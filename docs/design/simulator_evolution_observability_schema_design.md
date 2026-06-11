# simulator-evolution observability schema 设计

## 文档定位

本文档定义 `simulator-evolution` 分线的统一 observability schema 边界。

它回答：

- 当前分散在 `ExecutionProfile`、memory observation、`shadow_cache`、JIT / DBT dispatch、
  AI profile 和前端 Evidence Drawer 中的观察信号如何被归类。
- 后续迁移统一 schema 时，最小事件字段、版本策略、非目标和验证口径是什么。
- 哪些现有字段已经是稳定读侧合同，哪些只是人读诊断输出，哪些适合作为首批迁移候选。

本文档不承担实时进度更新。当前状态以
[../status/simulator_evolution_status.md](../status/simulator_evolution_status.md) 为准。

## 关联文档

- 状态文档：
  - [../status/simulator_evolution_status.md](../status/simulator_evolution_status.md)
  - [../status/mainline_status.md](../status/mainline_status.md)
- 历史计划：
  - [../plan/history_plan.md#simulator-evolution-slice2-debug-probe-event-summary-plan](../plan/history_plan.md#simulator-evolution-slice2-debug-probe-event-summary-plan)
  - [../plan/history_plan.md#simulator-evolution-slice1-observability-schema-plan](../plan/history_plan.md#simulator-evolution-slice1-observability-schema-plan)
- 相关路线 / 设计：
  - [../../PROJECT_EVOLUTION_PLAN.md](../../PROJECT_EVOLUTION_PLAN.md)
  - [wave6_jit_dbt_readiness_design.md](wave6_jit_dbt_readiness_design.md)
  - [wave5_cache_memory_system_design.md](wave5_cache_memory_system_design.md)
  - [phase3_ooo_execution_model_design.md](phase3_ooo_execution_model_design.md)
  - [pipeline_speculation_contracts.md](pipeline_speculation_contracts.md)
  - [post_wave7_frontend_lab_product_design.md](post_wave7_frontend_lab_product_design.md)
  - [post_wave7_ai_user_tasks_npu_performance_design.md](post_wave7_ai_user_tasks_npu_performance_design.md)

## 背景与问题

当前仓库已经是一个已可运行的模拟器原型，且已有较强的可观察性资产：
`ExecutionProfile` 聚合 retire / branch / trap / memory signal，`shadow_cache` 给出只读
cache-like memory observation，JIT / DBT dry-run 输出 dispatch summary，AI accelerator
有设备自有 `AiAcceleratorProfileSummary` 和 `ai_profile*` 文本出口，前端 Lab workbench
把这些结果组织成 Evidence Drawer / runtime topic / profile 面板。

问题在于这些观察面仍以各模块局部格式存在：字段命名、事件身份、source、时间顺序、
subject、effect 和 payload 边界没有统一规则。继续扩 Lab protocol、因果切片、
pipeline 参数化或 AI co-sim 前，如果不先固定一层事件 schema，就会让前端、probe、debug
JSON 和 host smoke 各自维护相似但不兼容的事实口径。

统一 schema 的目标不是马上重写所有 producer，而是给后续小切片提供同一套迁移合同。

## 目标

- 定义统一 observation event 的最小字段和版本策略。
- 固定 producer / consumer 分类，避免把 frontend 或 debug 文本输出误当执行语义来源。
- 把现有 observation 面映射到 `stable-contract`、`diagnostic-output` 和
  `candidate-for-schema` 三类。
- 为第一批低耦合迁移候选提供边界和验证命令。
- 保留模块真实语义差异，不用单一表结构抹平 execution、memory、JIT、AI 和 frontend
  evidence 的不同含义。

## 非目标

- 不把统一 schema 变成 guest ABI、MMIO ABI、Linux ABI、syscall ABI 或 debug CLI 写接口。
- 不改变默认 backend，不替换 `functional` reference path，也不改变 `pipeline` 或 JIT / DBT
  的调度语义。
- 不改变 guest 可见 ISA、trap、memory、device 或 interrupt 行为。
- 不要求 `ExecutionProfile`、AI profile、frontend Evidence Drawer 和所有 probe 一次性迁移。
- 不让 frontend 反向成为执行语义来源；浏览器只能消费后端真实响应。
- 不把 AI profile、pipeline 深层统计或 Lab protocol 一次性塞进本切片实现。

## 约束与边界

- `InstructionSemantics + functional backend` 仍是 ISA 真值来源；统一 observation event
  只能描述已经发生或 dry-run 明确声明不会发生的事实。
- `debug/frontend` 只能消费 machine / backend / device 的只读快照。
- `ExecutionProfile`、memory observation、`shadow_cache` 当前只做观测，不改变 guest 可见行为。
- JIT / DBT dispatch 事件当前仍是 opt-in dry-run；事件必须显式携带 no-execution / fallback
  边界，不能暗示默认 JIT backend 已启用。
- AI profile 当前是 host / device profile 合同；它不是任意模型上传或真实 NPU 性能 benchmark。
- schema 迁移必须保留旧字段直到对应 consumer 和 guardrail 明确迁走。

## 事件模型

统一 observation event 的最小字段为：

| 字段 | 作用 |
|------|------|
| `schema_version` | observation event schema 版本，第一版固定为 `1`。 |
| `event_id` | 单次观测事件的稳定身份，用于日志、probe 和前端去重 / 引用。 |
| `source` | producer 来源，例如 `execution-profile`、`debug-probe`、`jit-dbt-dispatch`。 |
| `phase` | 事件阶段，例如 `retire`、`memory-access`、`dry-run`、`profile-summary`、`frontend-evidence`。 |
| `subject` | 被观察对象，例如 guest PC、memory region、JIT block range、AI submission 或 frontend scenario。 |
| `timestamp_or_step` | 周期、retired step、probe order 或 frontend receive order；必须说明口径。 |
| `effect` | 观察到的影响，例如 `retired`、`trap`、`read`、`write`、`cache-hit`、`fallback-required`。 |
| `payload` | 模块自有结构化字段；不得包含会改变执行语义的隐式指令。 |
| `evidence_ref` | 指向原始事实来源的引用，例如 debug JSON path、probe line、host smoke 名称或 source file path。 |

可选字段：

- `severity`：仅用于诊断和 UI tone，例如 `info`、`warning`、`fault`。
- `contract_level`：`stable-contract`、`diagnostic-output` 或 `candidate-for-schema`。
- `producer_version`：保留模块自有 profile schema，例如 `ai_profile_v1`。
- `redaction`：未来用于外部 rootfs / 用户 payload 路径裁剪。

## 版本与兼容策略

- `schema_version=1` 只定义 observation event 包装层，不重命名现有模块内部字段。
- 模块自有版本继续保留，例如 `AI_ACCEL_PROFILE_SCHEMA_VERSION=1`、
  `AI_ACCEL_TIMING_SCHEMA_VERSION=1` 和 `schema=ai_profile_v1`。
- 第一阶段迁移只能新增统一 event 包装或并行只读摘要，不删除现有 debug JSON、probe 文本、
  host smoke 断言字段或 frontend response 字段。
- 当某个 producer 迁移完成后，必须补对应 host / frontend 测试，证明旧 consumer 仍可读，
  新 event 也能被定位到原始 `evidence_ref`。
- event 字段只能向后兼容扩展；删除或重命名稳定字段需要单独计划和 migration note。

## 已落地事件

### `execution-profile / snapshot-summary`

`myCPU/src/debug/debug_protocol_response.cpp` 现在会在 debug snapshot JSON 中保留既有
`profile` 对象，并在同一 snapshot response 顶层新增 `observation_event` 对象，
用于包装 `ExecutionProfileSnapshot` 的核心摘要。

当前事件边界：

- `source=execution-profile`
- `phase=snapshot-summary`
- `effect=observed`
- `subject` 包含 snapshot backend、PC 和 privilege。
- `timestamp_or_step` 包含 cycle 和 instret。
- `payload` 包装既有 total retire / trap / memory observation 计数、各类 profile
  entry 数量、top hot path 和 top PC cost。
- `evidence_ref` 指向 `snapshot.profile`。

这条事件不改变 `ExecutionProfileSnapshot` 结构，不改变 profile 记录逻辑，也不删除或重命名
既有 debug snapshot `profile` JSON 字段。

### `debug-probe / probe-summary`

Slice 2 已在 `myCPU/workloads/run_debug_cli_probe.py` 落地第一条统一 event wrapper：
`emit_probe_summary()` 会在现有 `summary:` 文本之后输出一行 `observation-event: <json>`。

当前事件边界：

- `source=debug-probe`
- `phase=probe-summary`
- `effect=observed`
- `subject` 包含 debug probe target、backend、PC 和 privilege。
- `timestamp_or_step` 包含 cycle 和 instret。
- `payload.load` 来自既有 probe load mode。
- `payload.profile` 只包装已有 snapshot profile 中的 retire / trap / memory observation
  总数、`shadow_cache`、top hot path 和 top memory region。
- `evidence_ref` 指向 `snapshot` 和 `snapshot.profile`。

这条事件不改变 debug CLI command，不改变 backend，不改变 guest state，也不删除既有
`load:`、`summary:`、`profile:`、`shadow-cache:`、`memory-top:`、`jit-dispatch:` 等文本行。

### `jit-dbt-dispatch / dry-run`

`myCPU/src/debug/debug_protocol_response.cpp` 现在会在 debug CLI `jit_dispatch`
JSON response 中新增 `observation_event` 对象；`myCPU/workloads/run_debug_cli_probe.py`
在 `--jit-dispatch` opt-in 时会把该对象作为 `observation-event: <json>` 行转发。

当前事件边界：

- `source=jit-dbt-dispatch`
- `phase=dry-run`
- `effect` 使用既有 dispatch action，例如 `lowered-ready`、`helper-bridge` 或
  `reference-fallback`。
- `subject` 包含 dry-run block start / end PC 和 dispatch source。
- `timestamp_or_step` 使用 candidate executions 和 retired instruction count。
- `payload` 包装既有 ok / cache / planning / translation / lowering / fallback /
  reject / helper metadata。
- `payload.no_execution` 显式携带 `generated_host_code`、
  `requested_executable_memory` 和 `executed_guest_code`。
- `evidence_ref` 指向 `jit_dispatch` debug JSON 和 `jit-dispatch` 文本行。

这条事件不改变默认 backend，不执行 guest code，不申请 executable memory，不生成 host code，
也不删除既有 `type=jit_dispatch` JSON 字段或 `jit-dispatch:` probe 文本。

### `memory-observation / profile-summary`

`myCPU/workloads/run_debug_cli_probe.py` 现在会从既有 snapshot profile 中派生
`source=memory-observation` 的 `observation-event: <json>` 行。

当前事件边界：

- `source=memory-observation`
- `phase=profile-summary`
- `effect=observed`
- `subject` 包含 debug probe target、backend、PC 和 privilege。
- `timestamp_or_step` 包含 cycle 和 instret。
- `payload` 只包装既有 `total_memory_observations`、top memory region 和 top PC cost。
- `evidence_ref` 指向 `snapshot.profile.memory_regions` 和 `snapshot.profile.pc_costs`。

这条事件不改 `ExecutionProfileSnapshot`、memory observation 记录逻辑或既有
`profile:`、`memory-top:`、`pc-cost:` 文本行。

### `cache-shadow / profile-summary`

`myCPU/workloads/run_debug_cli_probe.py` 现在会从既有 `snapshot.profile.shadow_cache`
中派生 `source=cache-shadow` 的 `observation-event: <json>` 行。

当前事件边界：

- `source=cache-shadow`
- `phase=profile-summary`
- `effect=observed`
- `payload` 保留既有 `shadow_cache` 字段，包括 line size、capacity、resident lines、
  line accesses、hits、misses、evictions 和 bypasses。
- `evidence_ref` 指向 `snapshot.profile.shadow_cache`。

这条事件不改变 `shadow_cache` 统计口径，不改 L1D 或 shadow-cache 记录逻辑，也不删除
既有 `shadow-cache:` 文本行。

### `ai-accelerator-profile / profile-summary`

`myCPU/src/main.cpp` 现在会在 `--ai-profile-manifest` 成功或故障完成后，继续保留
既有 `ai_profile`、`ai_profile_aggregate` 和 `ai_profile_op` 文本行，并额外输出
`source=ai-accelerator-profile` 的 `observation-event: <json>` 行。

当前事件边界：

- `source=ai-accelerator-profile`
- `phase=profile-summary`
- `producer_version=ai_profile_v1`
- `effect` 使用既有 profile progress，例如 `completed`、`fault` 或 `timeout`。
- `subject` 包含 profile manifest target、backend、workload name、manifest path 和 graph package path。
- `timestamp_or_step` 包含 ticks、device cycles 和 completion cycles。
- `payload` 包装既有 `ai_profile` timing / outcome / DMA 字段、aggregate summary 和
  per-op summary。
- `evidence_ref` 指向既有 `ai_profile`、`ai_profile_aggregate` 和 `ai_profile_op` 文本行。

这条事件不改变 AI accelerator MMIO ABI、guest demo、设备 profile lifecycle、
`AiAcceleratorProfileSummary` 字段语义或前端 AI tiny model response 字段。

## 现有 producer / consumer 映射

| 分类 | 当前 producer | 当前 consumer | 合同等级 | 迁移判断 |
|------|---------------|----------------|----------|----------|
| `execution-profile` | `myCPU/src/exec/execution_profile.*`，由 functional / pipeline backend 记录 retire、trap、branch、syscall、pc cost。 | debug JSON、`debug_cli_smoke`、`execution_profile_smoke`、DBT hot-path planner。 | `stable-contract` | 已新增 debug snapshot 顶层 `observation_event` wrapper；核心 profile 结构和旧 JSON 字段保留。 |
| `memory-observation` | `ExecutionMemoryObservation`、`record_memory()`、`pc_costs` 和 memory region stats。 | `xv6_boot_smoke`、`execution_profile_smoke`、probe summary、memory-system 文档。 | `stable-contract` | 已新增 probe 读侧 `observation-event` wrapper；核心 profile 字段和文本输出保留。 |
| `cache-shadow` | `ExecutionShadowCacheSnapshot` 和 per-region `shadow_cache_*` 统计。 | debug JSON、probe、Wave 5 memory-system guardrail、frontend runtime docs。 | `stable-contract` | 已新增 probe 读侧 `observation-event` wrapper；`shadow_cache` 字段语义和文本输出保留。 |
| `jit-dbt-dispatch` | `DbtJitDryRunSummary`、`format_dbt_jit_dry_run_summary()`、debug CLI `jit_dispatch` JSON。 | `debug_cli_smoke`、`dbt_jit_engine_smoke`、`run_debug_cli_probe.py --jit-dispatch`、frontend Runtime Labs。 | `candidate-for-schema` | 已新增只读 `observation_event` wrapper；旧 JSON / 文本字段保留。 |
| `ai-accelerator-profile` | `AiAcceleratorProfileSummary`、`mycpu --ai-profile-manifest` 的 `ai_profile` / `ai_profile_aggregate` / `ai_profile_op` 文本行。 | AI profile host smoke、frontend AI tiny model service、AI Lab profile 面板。 | `stable-contract` | 已新增 host CLI 文本出口旁路 `observation-event` wrapper；AI 侧 `ai_profile_v1` 字段语义保留。 |
| `frontend-evidence` | Lab workbench 的 scenario metadata、JIT dispatch panel、AI observed evidence、diagnostics。 | 浏览器 UI、Node tests、产品文档。 | `diagnostic-output` / `candidate-for-schema` | 已落地首个 read-side consumer：Evidence Drawer 优先消费后端 `observation_event`，缺失时回退旧 `snapshot.profile`；后续 Lab protocol 切片再统一更完整的 frontend event view。 |

## 首批迁移候选

### 已落地：ExecutionProfile core snapshot wrapper

范围：

- 在 debug snapshot JSON 顶层新增 `observation_event`，包装既有 `snapshot.profile` 摘要。
- 只读引用 `ExecutionProfileSnapshot`，不改变核心结构、记录逻辑、旧 debug JSON 字段或
  guest 可见行为。

推荐 event：

- `source=execution-profile`
- `phase=snapshot-summary`
- `subject=<backend/pc/privilege>`
- `effect=observed`
- `payload` 引用当前 `profile` 已暴露的 total counters、entry counts、top hot path
  和 top PC cost。

建议验证：

- `cd myCPU && make test-host-execution_profile_smoke`
- `cd myCPU && make test-host-debug_cli_smoke`

### 已落地：debug / probe 只读事件摘要

范围：

- 在 `run_debug_cli_probe.py` 读侧新增统一 event summary。
- 只包装已有 profile / snapshot / probe 事实，不改变 debug command、backend、guest state 或现有 JSON 字段。

推荐 event：

- `source=debug-probe`
- `phase=probe-summary`
- `subject=<workload/backend/probe profile>`
- `effect=observed`
- `payload` 只引用当前 probe 已打印的 summary 字段。

建议验证：

- `cd myCPU && make test-host-debug_cli_smoke test-host-run_debug_cli_probe`

### 已落地：JIT / DBT dispatch summary

范围：

- 为 debug CLI `jit_dispatch` response 增加统一 event 包装。
- 保留当前 `type=jit_dispatch` JSON 和 `jit-dispatch:` 单行文本。
- event 必须携带 `generated_host_code`、`requested_executable_memory`、`executed_guest_code`
  或等价 no-execution flags，继续证明这是 dry-run / opt-in observation。

推荐 event：

- `source=jit-dbt-dispatch`
- `phase=dry-run`
- `subject=<start_pc..end_pc>`
- `effect=<action>`，例如 `lowered-ready`、`helper-bridge`、`reference-fallback`
- `payload` 引用 cache state、candidate evidence、reject/helper metadata 和 no-execution flags。

建议验证：

- `cd myCPU && make test-host-dbt_jit_engine_smoke`
- `cd myCPU && make test-host-debug_cli_smoke test-host-run_debug_cli_probe`

### 已落地：memory observation / `shadow_cache` 读侧映射

范围：

- 在 `run_debug_cli_probe.py` 读侧新增 memory observation 与 `shadow_cache` event summary。
- 只包装已有 snapshot profile 事实，不改变 `ExecutionProfileSnapshot`、debug JSON 字段、
  probe 文本或 guest 可见行为。

推荐 event：

- `source=memory-observation`
- `source=cache-shadow`
- `phase=profile-summary`
- `effect=observed`
- `payload` 只引用当前 probe 已打印的 memory / cache summary 字段。

建议验证：

- `cd myCPU && make test-host-execution_profile_smoke`
- `cd myCPU && make test-host-debug_cli_smoke test-host-run_debug_cli_probe`

### 已落地：AI profile bridge

范围：

- 在 `mycpu --ai-profile-manifest` 的 host-side 文本出口旁新增统一 event wrapper。
- 只包装既有 `ai_profile`、`ai_profile_aggregate` 和 `ai_profile_op` 字段，不改变
  AI accelerator 设备合同、guest demo、profile lifecycle 或前端 response 字段。

推荐 event：

- `source=ai-accelerator-profile`
- `phase=profile-summary`
- `producer_version=ai_profile_v1`
- `effect=<progress>`，例如 `completed`、`fault` 或 `timeout`
- `payload` 引用 timing、outcome、DMA、aggregate 和 per-op summary 字段。

建议验证：

- `cd myCPU && make test-host-ai_accelerator_profile_smoke`

### 已落地：frontend Evidence Drawer read-side consumer

范围：

- 在前端状态归一层读取 debug snapshot 顶层 `observation_event`。
- Evidence Drawer / Lab Workbench 优先展示 event `source`、`phase`、摘要行和
  `evidence_ref`。
- 缺少统一 event 时继续回退到旧 `snapshot.profile`，不破坏现有 snapshot consumer。

推荐 event：

- 消费后端已提供的 `source` 和 `phase`，不在浏览器端改写 producer 身份。
- `effect`、`subject`、`payload` 和 `evidence_ref` 只用于显示既有事实摘要。
- fallback 使用 `contract=legacy-profile` 明确标记兼容路径。

建议验证：

- `cd frontend && node --test`

## 暂不迁移项

- `ExecutionProfileSnapshot` 核心结构：它是 DBT、debug JSON 和 host smoke 的共同输入；当前只做
  debug snapshot read-side wrapper，不改结构、记录逻辑或旧字段名。
- memory observation / `shadow_cache` 核心字段：Wave 5 已固定旧字段语义；当前只做读侧
  event wrapper，不改记录逻辑或字段含义。
- AI user task / device-contract 扩展：当前只做 `--ai-profile-manifest` 读侧 wrapper，不把用户
  AI 任务入口、MMIO ABI、NPU driver 或真实 benchmark 口径混入统一 event wrapper。
- frontend Lab protocol 版本化：当前只做 Evidence Drawer read-side consumer，不把浏览器端升级为
  event producer，也不定义完整 `frontend-simulator protocol` 版本。
- pipeline 深层统计：需要先决定 pipeline 参数化和性能口径，不在本切片纳入。

## 验证策略

文档 / 设计口径切片：

- `git diff --check`
- `git diff --cached --check`

后续实现切片按迁移候选选择：

- debug / probe 只读事件摘要：
  - `cd myCPU && make test-host-debug_cli_smoke test-host-run_debug_cli_probe`
- JIT / DBT dispatch summary：
  - `cd myCPU && make test-host-dbt_jit_engine_smoke`
  - `cd myCPU && make test-host-debug_cli_smoke test-host-run_debug_cli_probe`
- ExecutionProfile core snapshot wrapper：
  - `cd myCPU && make test-host-execution_profile_smoke test-host-debug_cli_smoke`
- memory observation / `shadow_cache` 读侧映射：
  - `cd myCPU && make test-host-execution_profile_smoke`
  - 必要时补 `cd myCPU && make test-host-xv6_boot_smoke`
- AI profile bridge：
  - `cd myCPU && make test-host-ai_accelerator_profile_smoke`
  - 如触及 guest / frontend，再按对应 `AGENTS.md` 扩门。
- frontend Evidence Drawer read-side consumer：
  - `cd frontend && node --test`

## 风险与取舍

- 先做 schema wrapper 而不是重写 producer，会让短期内出现旧字段 + 新 event 并存；这是为了避免破坏现有 guardrail。
- 事件模型如果过早抽象成通用 telemetry，可能掩盖模块真实语义差异；因此 `payload` 必须保留 producer 自有字段。
- AI profile wrapper 只覆盖 host-side profile 文本出口；用户 AI 任务入口和 Lab protocol
  继续保持后续独立切片，避免把产品层协议提前塞进 producer wrapper。
- frontend Evidence Drawer 当前只是 read-side consumer；如果后续把 UI interaction、
  scenario metadata 或 lab state 也包装成 event，必须另行定义 producer 身份和版本边界。
- JIT / DBT event 最容易被误读为可执行 JIT 能力，所以 no-execution flags 必须是首批字段之一。

## 当前有效性说明

- 当前有效：本文档作为 `simulator-evolution` observability schema 的第一版设计入口。
- 当前结果以 [../status/simulator_evolution_status.md](../status/simulator_evolution_status.md) 为准。
- 本文档由
  [../plan/history_plan.md#simulator-evolution-slice1-observability-schema-plan](../plan/history_plan.md#simulator-evolution-slice1-observability-schema-plan)
  对应切片建立；后续小型代码迁移可直接落地后回写状态和相关文档，跨模块或多阶段迁移仍需另开计划。
