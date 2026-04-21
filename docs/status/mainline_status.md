# 主线状态

## 文档定位

本文档只记录当前 `main` 分支的稳定快照、少量关键历史节点、当前仍有效的风险和下一步。

执行过程、阶段性 checklist 和专项落地细节统一归档到 [../plan/history_plan.md](../plan/history_plan.md)；更细的优先级判断见 [project_priority_roadmap.md](project_priority_roadmap.md)。

## 关联文档

- 相关设计：
  - [../design/regression_completion_criteria.md](../design/regression_completion_criteria.md)
  - [../design/debug_frontend_integration.md](../design/debug_frontend_integration.md)
  - [../design/minimal_interactive_os_design.md](../design/minimal_interactive_os_design.md)
  - [../design/phase3_ooo_execution_model_design.md](../design/phase3_ooo_execution_model_design.md)
  - [../design/pipeline_speculation_contracts.md](../design/pipeline_speculation_contracts.md)
  - [../design/vector_ml_workload_direction_design.md](../design/vector_ml_workload_direction_design.md)
  - [../design/phase4_preparation_design.md](../design/phase4_preparation_design.md)
  - [../design/future_expansion_roadmap_design.md](../design/future_expansion_roadmap_design.md)
  - [../design/xv6_linux_jit_mainline_design.md](../design/xv6_linux_jit_mainline_design.md)
- 相关状态：
  - [project_priority_roadmap.md](project_priority_roadmap.md)
  - [kernel_alpha_status.md](kernel_alpha_status.md)
  - [xv6_linux_jit_status.md](xv6_linux_jit_status.md)
- 当前计划：
  - [../plan/xv6_linux_jit_wave1_plan.md](../plan/xv6_linux_jit_wave1_plan.md)
- 已完成计划归档：
  - [../plan/history_plan.md#phase4-prep1-bus-memory-region-plan](../plan/history_plan.md#phase4-prep1-bus-memory-region-plan)
  - [../plan/history_plan.md#vector-v4-plan](../plan/history_plan.md#vector-v4-plan)
  - [../plan/history_plan.md#vector-frontend-visualization-plan](../plan/history_plan.md#vector-frontend-visualization-plan)
  - [../plan/history_plan.md#spike-external-differential-validation-plan](../plan/history_plan.md#spike-external-differential-validation-plan)
  - [../plan/history_plan.md#p2-validation-gap-backfill-round-2](../plan/history_plan.md#p2-validation-gap-backfill-round-2)

## 当前快照

- 当前仓库已经是一个已可运行的模拟器原型，不是纯设计稿。
- `phase1-stable`（`283aee6`）对应的 Phase 1 核心 bring-up 冻结基线已经形成，`functional` reference path、`kernel_alpha` 正向与 9 条负向回归都已稳定接通。
- `pipeline`、`make test-pipeline`、`debug_session / protocol`、本地 Node 调试服务和浏览器前端都已经正式接入主线，不再是待合入功能。
- `P1` 结构收口与 `P2` 首轮验证补洞已经完成；当前不再只是继续做“默认延续线收口”，而是自 `2026-04-21` 起正式把标准 OS bring-up 切换线提升为当前 active program。
- 当前近端主线已经明确切到 `RV64A + virtio 平台 + CSR / privilege 补全 + xv6-riscv bring-up`，对应的当前设计、状态和计划分别见 [../design/xv6_linux_jit_mainline_design.md](../design/xv6_linux_jit_mainline_design.md)、[xv6_linux_jit_status.md](xv6_linux_jit_status.md) 和 [../plan/xv6_linux_jit_wave1_plan.md](../plan/xv6_linux_jit_wave1_plan.md)。
- 此次切主线并不放弃默认延续线：`V4`、`P4-prep-1`、`kernel_alpha`、`debug/frontend`、Spike 外部差分和现有回归矩阵继续作为新主线的 correctness / observation guardrail。
- `向量扩展 + ML workload` 仍保留为默认延续线和代表性 workload corpus，不再是当前唯一主线，但仍继续为 profile / observation 和更后续 `Phase 4` 判断提供信号。
- `Phase 4` 当前只正式打开准备性入口：`P4-prep-1` 已完成，`bus / memory region` 已成为统一事实来源；后续是否继续 `P4-prep-2`，仍取决于更稳定的 workload 信号。
- Spike 外部差分验证已经形成一条独立离线 oracle，当前处于维护态，主要服务 reference correctness 疑点排查，而不是新的默认主门禁。

## 关键历史节点

- `2026-04-21`
  - 正式把 `future_expansion_roadmap_design.md` 中的标准 OS bring-up 切换线提升为当前 active program。
  - 新增 `xv6 / Linux / JIT` 主线 design / status / wave 1 plan，并按 4 个独立 worktree 启动并行工作流。
- `2026-04-12`
  - 完成 `P4-prep-1`，`Bus` 已统一暴露 `RAM / MMIO / unmapped` 与保守 region 属性。
  - 向量 / CNN 可视化正式接入 `debug/frontend`。
- `2026-04-10` 到 `2026-04-11`
  - `V-lite` `V0 ~ V4` 及第一轮更窄 hardening 落地，形成固定 `conv -> relu` 的最小 CNN-style guest 闭环与最小 vector-aware pipeline 边界。
- `2026-04-07`
  - Spike 外部差分扩到第一批 device-free `Sv39 / page fault` final-state subset，并补上 returning trap handler 的 first-trap checkpoint。
- `2026-04-05`
  - decode 级 `BlockedByUnresolvedStore` 边界收窄完成，且主线已明确：当前不主动继续扩大更激进的 `issue / replay / speculation`。
  - `debug/frontend` 补上更窄的长会话、session replacement 与 terminal 输入压力验证。
- `2026-04-04`
  - `P1` 结构收口与 `P2` 首轮验证补洞完成两轮收口，新增 loader 单测、guest smoke 窄单测、真实 debug e2e smoke 与 pipeline smoke 拆分。

## 当前仍然有效的风险 / 限制

- `debug/frontend` 当前已经够用，但它的正式定位仍然是“教学演示可用 + 最小工程调试”，不应顺势扩成通用调试器。
- 当前 `pipeline` 已具备最小真实 `OoO execute`，但仍是单发射、顺序退休、保守 replay 的克制形态；当前没有足够证据支持继续主动扩大更激进的 `issue / replay / speculation`。
- 当前 aggressive mainline switch 会同时打开 ISA、platform、guest workload 三类缺口；如果 4 条 workstream 的 ownership 失控，cross-branch 冲突和回归波动会很快放大。
- 当前 `xv6-riscv` 仍处在 foundation / harness / gap audit 阶段，尚不能把 `Linux` 或 `JIT / DBT` 直接当成本轮实现交付。
- 当前 `V4` 虽已落地，但仍刻意不扩到向量 load/store path、lane 模型、vector rename 或更重 memory speculation；在继续 hardening 与 workload 观察之前，直接抢跑更重 `Phase 4` 的性价比仍然偏低。
- 当前 `P4-prep-1` 只是准备性收口，不代表 cache / DMA / multicore 已进入正式实施阶段。
- guest runtime 的 `vm*`、`trap*`、`kernel_bringup`、`kernel_runtime` 等边界已经比早期清晰得多，但后续仍要防止真实 bug 修复把职责重新揉回大文件。

## 下一步

1. 先推进 `RV64A + CSR / privilege foundation`，把 `xv6` 和未来 `Linux` 必需的 ISA / architected contract 站稳。
2. 并行推进 `virtio-mmio + virtqueue + virtio-blk` 平台基础，避免把标准 OS bring-up 写成一次性设备特判。
3. 在 A / B 第一轮 contract 站稳后，继续推进 `xv6-riscv` external workload harness、boot gap audit 和真实 smoke。
4. 全程并行保留默认延续线 guardrail，并补 execution profile / observation foundation，为后续 `Linux` 与 `JIT / DBT` 保留调试与热路径证据。
5. 继续把 `pipeline`、guest runtime、`kernel_alpha` 十条基线、`debug/frontend` 和 Spike 外部差分限定在当前已接入、可验证的范围内维护，不让新主线反向污染 reference path。

## 验证基线

- `cd myCPU && make test`
- `cd myCPU && make test-pipeline`
- `cd frontend && node --test`

如果改动集中在 loader、guest smoke orchestration 或调试链路，至少额外关注：

- `cd myCPU && make test-unit-binary_loader`
- `cd myCPU && make test-unit-machine_loader_reset`
- `cd myCPU && make test-unit-supervisor_demo_smoke`
- `cd myCPU && make test-unit-user_program_smoke`
- `cd myCPU && make test-host-debug_cli_smoke`
- `cd myCPU && make test-host-interactive_terminal_smoke`
