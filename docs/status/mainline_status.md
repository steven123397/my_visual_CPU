# 主线状态

## 文档定位

本文档只记录当前 `main` 分支的稳定快照、少量关键时间节点、当前仍有效的风险和下一步。

执行过程与已完成 checklist 统一归档到 [plan/history_plan.md](../plan/history_plan.md)；具体优先级判断见 [project_priority_roadmap.md](project_priority_roadmap.md)。

## 关联文档

- 相关设计：
  - [design/regression_completion_criteria.md](../design/regression_completion_criteria.md)
  - [design/debug_frontend_integration.md](../design/debug_frontend_integration.md)
  - [design/phase3_ooo_execution_model_design.md](../design/phase3_ooo_execution_model_design.md)
  - [design/pipeline_speculation_contracts.md](../design/pipeline_speculation_contracts.md)
- 相关状态：
  - [project_priority_roadmap.md](project_priority_roadmap.md)
  - [kernel_alpha_status.md](kernel_alpha_status.md)
- 当前计划：
  - 当前无活跃计划。
- 已完成计划归档：
  - [plan/history_plan.md#p2-validation-gap-backfill-round-1](../plan/history_plan.md#p2-validation-gap-backfill-round-1)
  - [plan/history_plan.md#p2-validation-gap-backfill-round-2](../plan/history_plan.md#p2-validation-gap-backfill-round-2)
  - [plan/history_plan.md#p1-debug-frontend-boundary-refinement-plan](../plan/history_plan.md#p1-debug-frontend-boundary-refinement-plan)
  - [plan/history_plan.md#p1-reference-platform-contract-refinement-plan](../plan/history_plan.md#p1-reference-platform-contract-refinement-plan)
  - [plan/history_plan.md#p1-pipeline-backend-boundary-refinement-plan](../plan/history_plan.md#p1-pipeline-backend-boundary-refinement-plan)
  - [plan/history_plan.md#p1-guest-public-header-boundary-refinement-plan](../plan/history_plan.md#p1-guest-public-header-boundary-refinement-plan)
  - [plan/history_plan.md#p1-guest-smoke-orchestration-refinement-plan](../plan/history_plan.md#p1-guest-smoke-orchestration-refinement-plan)

## 当前快照

- 当前仓库已经是一个已可运行的模拟器原型，不是纯设计稿。
- `phase1-stable`（`283aee6`）对应的 Phase 1 核心 bring-up 冻结基线已经形成。
- 默认 `functional` reference path、`make test` 主门禁，以及 `kernel_alpha` 正向与九条负向回归都已稳定接通。
- `pipeline core`、`make test-pipeline`、`debug_session/protocol`、本地 Node 调试服务和浏览器前端都已经正式接入主线，不再是待合入功能。
- `P1` 结构收口已经全部完成；`P2` 首轮验证补洞也已完成两轮收口，新增 loader 单测、guest smoke 窄单测、真实 debug e2e smoke、预算常量收口和 pipeline smoke 拆分都已进入现有门禁。
- 当前主线不再把重点放在继续扩功能面，而是转向两件更具体的事：`debug/frontend` 的长会话 / 高吞吐压力验证，以及 `Phase 3` decode 级 `BlockedByUnresolvedStore` 串行化边界。

## 近期时间线（按时间倒序）

- `2026-04-04`
  - 完成 `P2` 首轮验证补洞两轮收口：`BinaryLoader` 直接单测、`Machine::load_elf()/load_binary()` 最小 reload/reset 回归、`supervisor_demo_smoke` 与 `user_program_smoke` 更窄直测、真实 `debug server + mycpu --debug-cli` 端到端 smoke、Node/C++ 两侧调试预算常量收口，以及 `pipeline` mega-smoke 拆分。
  - 同日也完成最后一批 `P1` 结构收口：`pipeline_backend` 拆分、`debug/frontend` 协议与运行时边界收口、guest public header 与 smoke orchestration 收口，以及 reference / platform 合同补洞。
- `2026-04-03`
  - `interactive_os / monitor / vm_debug`、browser terminal 壳和 Node debug server 的最小交互闭环已经稳定，相关 smoke 和 Node 测试都已接入主门禁。
  - 文档体系完成一轮归并，已完成计划统一回写到 `history_plan`，不再长期保留活跃 checklist。
- `2026-04-02`
  - `Phase 3-B/C` 首轮最小真实 `OoO execute` 基线形成：`rename + ROB + LSQ`、统一 rollback、coarse replay 和 `RAM-only` forwarding 已进入当前实现。
- `2026-03-27`
  - `Phase 3-A` 第一轮分支预测增强落地，`pipeline` 获得最小 predictor 与相应 smoke / differential 门禁。
- `2026-03-26`
  - Phase 1 hardening 第一轮矩阵化回归落地：illegal encoding、MMIO、ELF、CSR / privilege、Sv39 / `MPRV` 等关键合同已进入 asm / host / guest 门禁。
- `2026-03-25`
  - 完成一批 simulator-side correctness 修复，包括非法整数编码、`DIV/REM` 边界、ELF pure-BSS `PT_LOAD`，以及 bus / device 第一轮边界防御。

## 当前仍然有效的风险 / 限制

- `debug/frontend` 当前已经可用，但长会话、持续 `run`、更高吞吐输入输出和真实浏览器时序下的压力验证仍然不足。
- Node 侧 `debug_budget.mjs` 与 C++ 侧 `debug_budget.h` 已分别收口，但它们仍是分语言维护，不是跨语言单一事实来源。
- guest runtime 的 `vm*`、`trap*`、`kernel_bringup`、`kernel_runtime` 等边界已经比早期清晰得多，但后续仍要防止重新膨胀回大文件或重新暴露临时内部布局。
- `Machine::load_elf()/load_binary()` 当前语义已经明确为“替换 RAM 并 reset CPU/backend”，但这不是完整平台 reset；设备状态是否也要复位，仍是后续独立设计问题。
- `Phase 3-B/C` 当前仍是单发射、顺序退休、最小完成窗口的克制形态；下一步更具体的开放边界不是泛泛的“memory speculation”，而是 decode 级 `BlockedByUnresolvedStore` 串行化。

## 下一步

1. 为 `debug/frontend` 补更长会话、持续 `run`、更高吞吐输入输出和真实浏览器节奏下的压力验证。
2. 如果继续推进 `Phase 3`，先把 decode 级 `BlockedByUnresolvedStore` 串行化边界单列成专项问题，再决定是否做更激进的 issue / replay / speculation。
3. 继续以 bug-driven hardening 的方式维护 guest runtime、`kernel_alpha` 十条基线和 reference correctness 矩阵，不做无关大重构。
4. 继续把 `pipeline` 与 `debug/frontend` 限定在当前已接入、可验证的范围内，避免在现有门禁没有继续增强前再扩更多功能面。

## 验证基线

- `cd myCPU && make test`
- `cd myCPU && make test-pipeline`
- `cd frontend && node --test`

如果改动集中在本轮已补洞的入口，至少额外关注：

- `cd myCPU && make test-unit-binary_loader`
- `cd myCPU && make test-unit-machine_loader_reset`
- `cd myCPU && make test-unit-supervisor_demo_smoke`
- `cd myCPU && make test-unit-user_program_smoke`
- `cd myCPU && make test-host-debug_cli_smoke`
- `cd myCPU && make test-host-interactive_terminal_smoke`
