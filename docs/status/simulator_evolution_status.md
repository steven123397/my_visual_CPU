# simulator-evolution 状态

## 文档定位

本文档记录 `simulator-evolution` 分线的当前定位、活跃切片、风险和下一步。

这条线只跟踪模拟器自身的模型、架构、协议和可观察性升级；不承接 `kernel_alpha`
课程 OS 后续阶段，也不把 Linux 用户态兼容层继续扩到 `rustc`、Stage 12 或 Stage 13。

## 关联文档

- 相关路线 / 设计：
  - [../../PROJECT_EVOLUTION_PLAN.md](../../PROJECT_EVOLUTION_PLAN.md)
  - [../design/wave6_jit_dbt_readiness_design.md](../design/wave6_jit_dbt_readiness_design.md)
  - [../design/wave5_cache_memory_system_design.md](../design/wave5_cache_memory_system_design.md)
  - [../design/phase3_ooo_execution_model_design.md](../design/phase3_ooo_execution_model_design.md)
  - [../design/pipeline_speculation_contracts.md](../design/pipeline_speculation_contracts.md)
  - [../design/post_wave7_frontend_lab_product_design.md](../design/post_wave7_frontend_lab_product_design.md)
  - [../design/post_wave7_ai_user_tasks_npu_performance_design.md](../design/post_wave7_ai_user_tasks_npu_performance_design.md)
- 当前计划：
  - [../plan/simulator_evolution_slice1_observability_schema_plan.md](../plan/simulator_evolution_slice1_observability_schema_plan.md)
- 已完成计划：
  - [../plan/history_plan.md](../plan/history_plan.md)

## 目标 / 主题

`simulator-evolution` 的目标是把当前已可运行的模拟器原型，从功能堆叠后的
`reference-first + observability` 实践，收敛成更稳定的模拟器架构和实验平台：
统一可观察性 schema、明确 Lab 协议、参数化 pipeline 研究入口、推进 ISA 语义结构化，
并对 JIT / DBT opt-in 资产做继续推进或归档的决断。

## 当前状态

- `simulator-evolution` 已作为独立分线建立文档入口，准备从 `main` 派生独立 worktree。
- 当前第一切片限定为 `observability schema` 设计与口径收敛，不直接改默认执行路径。
- 现有模拟器已经具备多处可观察性来源：
  - `ExecutionProfile` / hot path / branch target / memory observation。
  - `shadow_cache`、L1D 观察面和 cache lifecycle guardrail。
  - JIT / DBT dry-run、dispatch summary、debug probe 事件。
  - AI accelerator profile、bounded-dynamic shape 和前端 Evidence Drawer。
- 上述来源仍以各模块局部格式为主；第一切片要先固定统一事件模型和迁移边界，再决定具体代码迁移顺序。

## 关键历史节点

- `2026-06-10`
  - 建立 `simulator-evolution` 状态文档和第一切片计划入口。
  - 明确该分线可与 `course-os-kernel-alpha` 并行，但二者使用不同 worktree 和不同验收口径。

## 当前仍然有效的风险 / 限制

- 不允许把本状态文档写成新的仓库级实时主线；仓库级全局状态仍以
  [mainline_status.md](mainline_status.md) 为准。
- 不允许把统一 schema 做成一次性大迁移。第一切片只建立设计口径和最小迁移规则。
- 不改变 guest 可见 ISA、trap、memory、device 或 debug 执行语义。
- 不让 `pipeline`、JIT / DBT、AI profile、frontend Evidence Drawer 各自复制新的事实来源。
- 不把 `kernel_alpha` 课程 OS review、Undefined-OS 学习、Linux syscall breadth 和模拟器架构升级混成一个计划。

## 下一步

1. 基于 `main` 创建 `.worktrees/simulator-evolution` 和 `simulator-evolution` 分支。
2. 执行第一切片：
   - 盘点现有 observation producer / consumer。
   - 建立或更新统一 observability schema 设计文档。
   - 固定第一批迁移候选和非目标。
   - 切片完成时同步更新对应设计文档口径、本文档和 `history_plan.md`。
3. 在第一切片完成后，再决定下一切片是 Lab 协议、pipeline 参数化入口、ISA 语义结构化占位，还是 JIT / DBT opt-in 决断。

## 验证基线

- 文档 / 计划切片：
  - `git diff --check`
  - `git diff --cached --check`
- 后续实现切片按触及范围选择最窄验证：
  - debug / probe / workload harness：优先 `cd myCPU && make test-host-debug_cli_smoke test-host-run_debug_cli_probe`
  - pipeline：优先相关 `test-host-*pipeline*`，必要时扩到 `cd myCPU && make test-pipeline`
  - 架构相关路径：至少守住 `cd myCPU && make test`
