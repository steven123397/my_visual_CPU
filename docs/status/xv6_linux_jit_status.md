# xv6 / Linux / JIT 主线状态

## 文档定位

本文档用于跟踪当前已经正式激活的 `xv6 / Linux / JIT` 主线切换：

- 当前到底推进到哪一步
- 当前仍然有效的风险 / 限制是什么
- 下一轮 4 条 workstream 各自要先做什么

本文档不记录完整执行 checklist；执行细节统一放在 `plan` 文档里。

## 关联文档

- 相关设计：
  - [../design/xv6_linux_jit_mainline_design.md](../design/xv6_linux_jit_mainline_design.md)
  - [../design/future_expansion_roadmap_design.md](../design/future_expansion_roadmap_design.md)
  - [../design/phase4_preparation_design.md](../design/phase4_preparation_design.md)
  - [../design/vector_ml_workload_direction_design.md](../design/vector_ml_workload_direction_design.md)
- 当前计划：
  - [../plan/xv6_linux_jit_wave1_plan.md](../plan/xv6_linux_jit_wave1_plan.md)
- 已完成计划：
  - 当前暂无；完成后统一归档到 [../plan/history_plan.md](../plan/history_plan.md)

## 目标 / 主题

当前主题不是“是否要评估 `xv6-riscv`”，而是已经正式把它作为当前主线的近端牵引目标，同时把后续 `Linux` 与 `JIT / 动态二进制翻译` 作为结构决策的长线约束。当前波次的任务不是直接跑起 `Linux` 或写出 `JIT`，而是把这条路径所需的 durable foundation 在不破坏现有稳定基线的前提下分 workstream 落下来。

## 当前状态

- `2026-04-21` 已明确从 `future_expansion_roadmap_design.md` 的候选切换线里正式激活 `Path B`：当前主线改为 `RV64A + virtio 平台 + CSR / privilege 补全 + xv6-riscv bring-up`。
- 当前已经同时保留默认延续线 guardrail：`kernel_alpha`、`interactive_os`、`V4`、`P4-prep-1`、debug/frontend、`make test` / `make test-pipeline` 仍然是本轮主线的稳定性底座，而不是被放弃的旧分支。
- 本轮已经按低交叉依赖拆成 4 条 workstream：
  - A：`RV64A + CSR / privilege foundation`
  - B：`virtio / platform foundation`
  - C：`external guest workload harness + xv6 bring-up`
  - D：`observation / profile foundation + default-line guardrail`
- 为了支持多对话、多分支、多个 worktree 并行推进，当前已经为 4 条 workstream 规划了独立 branch / worktree 和独立 ownership。
- 当前不会把 `Linux` 或 `JIT / DBT` 当成本轮直接交付项，但所有新引入的抽象都必须考虑它们的后续复用路径。
- 在 docs 基线尚未独立落成提交之前，当前协调者工作树中的 design / status / plan 文档仍是权威执行口径；各 agent worktree 默认按 prompt 中给出的绝对路径读取这些文档，而不是在自己的 branch 上复制一套共享 docs 正文。

## 关键历史节点

- `2026-04-21`
  - 正式决定从“默认延续线优先”切到“标准 OS bring-up 线为当前主线”。
  - 新增 `xv6 / Linux / JIT` 主线 design / status / wave 1 plan。
  - 确认按 4 个独立 worktree / 4 个独立对话并行推进。

## 当前仍然有效的风险 / 限制

- `xv6-riscv` 预期会暴露大量 CSR、trap、timer、interrupt、storage / block、platform contract 细节缺口；当前仍无法精确预估这批缺口的规模。
- Workstream C 天然依赖 A/B 的第一轮 contract 站稳，因此 bring-up 线虽然已经激活，但不能指望它独立闭门冲到 shell；它更适合作为 gap finder 和整体验证牵引。
- 当前 docs 基线尚未以单独提交形式落到各 agent branch，因此跨对话协同仍依赖协调者维护的权威计划文档和绝对路径 prompt。
- “不做短寿命最小实现”会显著提高本轮对抽象边界的要求；如果控制不好，容易出现过度设计。当前必须持续用 `xv6`、未来 `Linux` 和未来 `JIT / DBT` 三个真实复用目标来约束抽象范围。
- 当前 `pipeline`、`V4` 和 `P4-prep-1` 已经具备可用结构边界，但这并不意味着可以直接跳到更重的 cache / DMA / multicore 或更激进 speculation；这些仍应在本轮主线站稳之后再决定。

## 下一步

1. 先启动 Workstream A，完成 `RV64A` / `CSR` / privilege gap audit 与第一轮 durable contract 落地，给 `xv6` 和未来 `Linux` 提供语义底座。
2. 并行启动 Workstream B，以通用 `virtio-mmio + virtqueue + virtio-blk` 分层取代一次性设备特判，为 `xv6` 与后续 `Linux` 提供可复用平台入口。
3. 在 A/B 启动后推进 Workstream C：先把外部 guest workload harness 和 `xv6-riscv` boot / device / trap gap list 盘清，再逐步接到真实 smoke。
4. 全程并行推进 Workstream D：继续守住默认延续线回归，同时补 execution profile / observation 合同，为 future `Linux` 和 `JIT / DBT` 提供热路径、memory 行为与调试证据。
5. 由协调者按 `A/B -> C -> D` 的依赖顺序整合结果，并持续把实时状态回写到本文件与 [mainline_status.md](mainline_status.md)。

## 验证基线

- `cd myCPU && make test`
- `cd myCPU && make test-pipeline`
- `cd frontend && node --test`

本轮各 workstream 还应额外关注：

- Workstream A：`cd myCPU && make test-host-instruction_semantics_smoke`
- Workstream B：`cd myCPU && make test-unit-bus_device_guards`
- Workstream C：后续新增 `xv6` harness / boot smoke 后，把对应 target 接入计划文档
- Workstream D：`cd myCPU && make test-host-debug_cli_smoke`
