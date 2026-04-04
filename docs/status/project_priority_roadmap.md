# 当前项目优先级路线图

## 文档定位

本文档只保留当前仍然开放的优先级判断，不再重复记录已经完成的整段执行过程。

它不替代 [mainline_status.md](mainline_status.md)。`mainline_status` 负责回答“现在主线是什么状态”，本文档负责回答“下一轮最值得做什么，以及什么不该抢跑”。

## 关联文档

- 相关设计：
  - [design/regression_completion_criteria.md](../design/regression_completion_criteria.md)
  - [design/debug_frontend_integration.md](../design/debug_frontend_integration.md)
  - [design/phase3_ooo_execution_model_design.md](../design/phase3_ooo_execution_model_design.md)
  - [design/pipeline_speculation_contracts.md](../design/pipeline_speculation_contracts.md)
- 相关状态：
  - [mainline_status.md](mainline_status.md)
  - [kernel_alpha_status.md](kernel_alpha_status.md)
- 当前活跃计划：
  - 当前无活跃计划。
- 已完成计划归档：
  - [plan/history_plan.md#p2-validation-gap-backfill-round-1](../plan/history_plan.md#p2-validation-gap-backfill-round-1)
  - [plan/history_plan.md#p2-validation-gap-backfill-round-2](../plan/history_plan.md#p2-validation-gap-backfill-round-2)
  - [plan/history_plan.md#p1-debug-frontend-boundary-refinement-plan](../plan/history_plan.md#p1-debug-frontend-boundary-refinement-plan)
  - [plan/history_plan.md#p1-reference-platform-contract-refinement-plan](../plan/history_plan.md#p1-reference-platform-contract-refinement-plan)
  - [plan/history_plan.md#p1-pipeline-backend-boundary-refinement-plan](../plan/history_plan.md#p1-pipeline-backend-boundary-refinement-plan)

## 当前判断

- `P0` correctness 修补已经完成，`P1` 结构收口已经全部关闭，`P2` 首轮验证补洞也已经完成两轮收口。
- 因此，当前路线图不再需要继续维护一长串“已完成问题记录”；现在真正开放的事项已经缩小到少数几个具体边界。
- 当前如果新开计划，优先级应围绕 `debug/frontend` 的压力验证和 `Phase 3` 的具体串行化边界展开，而不是继续泛泛地写“继续 hardening / 继续 refinement”。

## 当前优先级

### 1. `debug/frontend` 长会话与高吞吐压力验证

- 真实 `debug server + mycpu --debug-cli` 端到端 smoke 已经落地，但它仍主要覆盖最小交互和短会话。
- 下一轮更值得补的是持续 `run`、更高频 terminal 输入输出、真实浏览器交互时序和会话替换压力，而不是再扩 UI 按钮或协议面。
- 这条线的目标是把当前“教学演示可用”的链路继续压实成更稳的门禁，而不是把它扩成通用调试器。

### 2. `Phase 3` decode 级 `BlockedByUnresolvedStore` 串行化边界

- 当前 `Phase 3-B/C` 已经完成最小真实 `OoO execute`，下一步最具体的开放问题不是抽象的 “memory speculation”。
- 更真实的 blocker 是：decode 阶段对 `BlockedByUnresolvedStore` 的处理仍然决定了现有 OoO 模型的保守边界。
- 如果下一轮继续碰 `Phase 3`，应先把这条边界单列成专项问题，明确合同、风险和最小回归，再判断是否值得继续扩 issue / replay / memory disambiguation。

### 3. 常态维护项

- 继续维护 reference correctness 矩阵，不让 illegal / MMIO / ELF / CSR / Sv39 合同回退。
- 继续守住 `kernel_alpha` 十条 guest 基线和 `guest_supervisor_demo` 的稳定输出。
- 继续维护 guest runtime 已经形成的 `vm*`、`trap*`、`kernel_bringup`、`kernel_runtime`、`user_program*` 边界，不让新增 bug 修复重新把职责揉回去。

## 当前明确不优先做的事

1. 不继续把 `debug/frontend` 往断点、条件暂停、任意文件加载或更大 UI 功能面扩张。
2. 在当前压力验证和具体串行化边界没有继续收口前，不抢跑更激进的 `Phase 3` 扩展。
3. 不把 `interactive_os` 当作新的产品主线；它当前仍应服务于 monitor / terminal / 调试链路验证。
4. 不把 `SimpleStorage` 更完整的设备模型抢在当前 correctness / structure hardening 前面推进。

## 如需新开计划

1. 优先单开一份 `debug/frontend` 压力验证计划，目标明确落在长会话、高吞吐和真实浏览器时序。
2. 如果继续碰 `Phase 3`，优先单开一份 decode 级 `BlockedByUnresolvedStore` 边界计划，不再沿用笼统的 “memory speculation” 表述。
3. 如果下一轮还要并行推进，建议围绕“压力验证 / Phase 3 边界 / guest runtime bug-driven hardening”拆 ownership，而不是继续机械沿用旧的 `P2-1..P2-7` 编号分线。
