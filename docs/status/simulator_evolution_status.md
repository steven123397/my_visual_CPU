# simulator-evolution 状态

## 文档定位

本文档记录 `simulator-evolution` 分线的当前定位、活跃切片、风险和下一步。

这条线只跟踪模拟器自身的模型、架构、协议和可观察性升级；不承接 `kernel_alpha`
课程 OS 后续阶段，也不把 Linux 用户态兼容层继续扩到 `rustc`、Stage 12 或 Stage 13。

## 关联文档

- 相关路线 / 设计：
  - [../../PROJECT_EVOLUTION_PLAN.md](../../PROJECT_EVOLUTION_PLAN.md)
  - [../design/simulator_evolution_observability_schema_design.md](../design/simulator_evolution_observability_schema_design.md)
  - [../design/wave6_jit_dbt_readiness_design.md](../design/wave6_jit_dbt_readiness_design.md)
  - [../design/wave5_cache_memory_system_design.md](../design/wave5_cache_memory_system_design.md)
  - [../design/phase3_ooo_execution_model_design.md](../design/phase3_ooo_execution_model_design.md)
  - [../design/pipeline_speculation_contracts.md](../design/pipeline_speculation_contracts.md)
  - [../design/post_wave7_frontend_lab_product_design.md](../design/post_wave7_frontend_lab_product_design.md)
  - [../design/post_wave7_ai_user_tasks_npu_performance_design.md](../design/post_wave7_ai_user_tasks_npu_performance_design.md)
- 当前计划：
  - 无。小型 schema wrapper 切片可直接落地，完成后回写状态和相关文档；跨模块或多阶段迁移再另开计划。
- 已完成计划：
  - [../plan/history_plan.md](../plan/history_plan.md)
  - [../plan/history_plan.md#simulator-evolution-slice2-debug-probe-event-summary-plan](../plan/history_plan.md#simulator-evolution-slice2-debug-probe-event-summary-plan)
  - [../plan/history_plan.md#simulator-evolution-slice1-observability-schema-plan](../plan/history_plan.md#simulator-evolution-slice1-observability-schema-plan)

## 目标 / 主题

`simulator-evolution` 的目标是把当前已可运行的模拟器原型，从功能堆叠后的
`reference-first + observability` 实践，收敛成更稳定的模拟器架构和实验平台：
统一可观察性 schema、明确 Lab 协议、参数化 pipeline 研究入口、推进 ISA 语义结构化，
并对 JIT / DBT opt-in 资产做继续推进或归档的决断。

## 当前状态

- `simulator-evolution` 已作为独立分线建立文档入口，并在 `.worktrees/simulator-evolution`
  上完成第一切片设计收口。
- 第一切片已建立
  [../design/simulator_evolution_observability_schema_design.md](../design/simulator_evolution_observability_schema_design.md)：
  当前只固定统一 observation event 的最小字段、producer / consumer 映射、版本策略和迁移边界，
  不直接改默认执行路径。
- 现有模拟器已经具备多处可观察性来源，并已按 schema 设计分为
  `stable-contract`、`diagnostic-output` 和 `candidate-for-schema`：
  - `ExecutionProfile` / hot path / branch target / memory observation。
  - `shadow_cache`、L1D 观察面和 cache lifecycle guardrail。
  - JIT / DBT dry-run、dispatch summary、debug probe 事件。
  - AI accelerator profile、bounded-dynamic shape 和前端 Evidence Drawer。
- 第一处代码迁移已经落地在 debug / probe 读侧：
  `run_debug_cli_probe.py` 现在输出 `observation-event: <json>`，包装已有 snapshot / profile
  事实，不改变 debug CLI command、backend、guest state 或既有文本行。
- 第二处代码迁移已经落地在 JIT / DBT dispatch 读侧：
  debug CLI `jit_dispatch` JSON 现在包含 `source=jit-dbt-dispatch`、`phase=dry-run`
  的 `observation_event` 对象，probe `--jit-dispatch` 会把它转发为
  `observation-event: <json>`；旧 `jit_dispatch` JSON 字段和 `jit-dispatch:` 文本保持不变。
- 第三处代码迁移已经落地在 memory observation / `shadow_cache` 读侧：
  `run_debug_cli_probe.py` 现在从既有 snapshot profile 追加 `source=memory-observation`
  和 `source=cache-shadow` 的统一 observation event；旧 `profile:`、`memory-top:`、
  `pc-cost:` 和 `shadow-cache:` 文本保持不变。
- 第四处代码迁移已经落地在 AI profile bridge：
  `mycpu --ai-profile-manifest` 现在在既有 `ai_profile`、`ai_profile_aggregate`
  和 `ai_profile_op` 文本行之后，追加 `source=ai-accelerator-profile`、
  `phase=profile-summary` 的统一 observation event；AI accelerator 设备合同、
  guest demo、profile lifecycle 和前端 response 字段保持不变。
- 第五处代码迁移已经落地在 `ExecutionProfile` 核心 snapshot 读侧：
  debug snapshot JSON 现在保留既有 `profile` 对象，并在同一 response 顶层追加
  `source=execution-profile`、`phase=snapshot-summary` 的 `observation_event`；
  `ExecutionProfileSnapshot` 结构、记录逻辑和旧 debug JSON 字段保持不变。
- 第一处 frontend consumer 已落地在 Lab Workbench / Evidence Drawer 读侧：
  前端状态归一层现在优先消费 debug snapshot 顶层 `observation_event`，并在缺失时回退到
  旧 `snapshot.profile`；Evidence Drawer 会显示 event `source`、`phase`、摘要行和
  `evidence_ref`，不把浏览器端变成 producer 或执行语义来源。
- pipeline 深层统计本轮仍不迁移。

## 关键历史节点

- `2026-06-11`
  - 完成 frontend Evidence Drawer / Lab Workbench 首个统一 event consumer。前端读侧现在
    通过 `normalizeObservationEvidence()` 优先读取 `snapshot.observation_event`，并保留
    `snapshot.profile` 兼容 fallback；渲染层只展示后端事实和 `evidence_ref`，不新增并行
    frontend 事实来源。
  - 完成 `ExecutionProfile` core snapshot wrapper。debug snapshot JSON 现在从既有
    `snapshot.profile` 派生 `source=execution-profile` 的只读 `observation_event`，
    不改变 `ExecutionProfileSnapshot` 结构、记录逻辑、旧 debug JSON 字段或 guest 可见行为。
  - 完成 AI accelerator profile bridge。`mycpu --ai-profile-manifest` 现在从既有
    `ai_profile*` 文本出口派生 `source=ai-accelerator-profile` 的
    `observation-event: <json>`，不改变 `AiAcceleratorProfileSummary` 字段语义、
    设备 profile lifecycle、guest demo 或前端 AI response 字段。
  - 完成 memory observation / `shadow_cache` 读侧 event wrapper。probe 现在从既有
    snapshot profile 派生 `source=memory-observation` 和 `source=cache-shadow`
    的 `observation-event: <json>`，不改变 `ExecutionProfileSnapshot`、旧 probe 文本或
    guest 可见行为。
- `2026-06-10`
  - 完成 JIT / DBT dispatch summary event wrapper。debug CLI `jit_dispatch` response
    新增只读 `observation_event`，显式携带 no-host-code / no-executable-memory /
    no-guest-execution 边界；probe 在 `--jit-dispatch` opt-in 时转发统一 event。
  - 完成 `simulator-evolution` Slice 2 / debug-probe event summary。`run_debug_cli_probe.py`
    现在输出 `source=debug-probe`、`phase=probe-summary` 的统一 observation event；旧
    probe 文本和 debug JSON 字段保持不变。
  - 完成 `simulator-evolution` Slice 1 / observability schema 设计收口，新增
    [../design/simulator_evolution_observability_schema_design.md](../design/simulator_evolution_observability_schema_design.md)。
    本轮仅建立长期设计边界和后续迁移候选，不改默认 backend、guest 可见语义或现有 debug / profile
    字段。
  - 建立 `simulator-evolution` 状态文档和第一切片计划入口。
  - 明确该分线可与 `course-os-kernel-alpha` 并行，但二者使用不同 worktree 和不同验收口径。

## 当前仍然有效的风险 / 限制

- 不允许把本状态文档写成新的仓库级实时主线；仓库级全局状态仍以
  [mainline_status.md](mainline_status.md) 为准。
- 不允许把统一 schema 做成一次性大迁移。小型 event wrapper 可以直接落地后回写状态和相关文档；跨模块或多阶段迁移仍需另开计划。
- 不改变 guest 可见 ISA、trap、memory、device 或 debug 执行语义。
- 不让 `pipeline`、JIT / DBT、AI profile 或 frontend Evidence Drawer 各自复制新的事实来源；
  当前 frontend 只能消费后端 event / profile，不能自行制造 event producer。
- 不把 `kernel_alpha` 课程 OS review、Undefined-OS 学习、Linux syscall breadth 和模拟器架构升级混成一个计划。

## 下一步

1. 下一刀可以继续扩 frontend Lab protocol / event view，但必须另开边界，不把首个
   Evidence Drawer consumer 扩成浏览器端 producer 或通用 telemetry 层。
2. 保持 pipeline 参数化和 Lab protocol 为后续独立切片，不并入已完成的 debug/probe、
   JIT/DBT、memory/cache、AI profile 与 ExecutionProfile wrapper。
3. 后续每个实现切片继续保留旧 debug JSON、probe 文本、host smoke 断言字段和 frontend response 字段，直到 consumer 迁移完成。

## 验证基线

- 文档 / 计划切片：
  - `git diff --check`
  - `git diff --cached --check`
- 后续实现切片按触及范围选择最窄验证：
  - debug / probe / workload harness：优先 `cd myCPU && make test-host-debug_cli_smoke test-host-run_debug_cli_probe`
  - pipeline：优先相关 `test-host-*pipeline*`，必要时扩到 `cd myCPU && make test-pipeline`
  - 架构相关路径：至少守住 `cd myCPU && make test`
