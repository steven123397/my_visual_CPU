# 当前项目优先级路线图

## 文档定位

本文档只保留当前仍然开放的优先级判断，不再重复记录已经完成的整段执行过程。

它不替代 [mainline_status.md](mainline_status.md)。`mainline_status` 负责回答“现在主线是什么状态”，本文档负责回答“下一轮最值得做什么，以及什么不该抢跑”。

## 关联文档

- 相关设计：
  - [design/regression_completion_criteria.md](../design/regression_completion_criteria.md)
  - [design/debug_frontend_integration.md](../design/debug_frontend_integration.md)
  - [design/phase3_ooo_execution_model_design.md](../design/phase3_ooo_execution_model_design.md)
  - [design/blocked_by_unresolved_store_boundary.md](../design/blocked_by_unresolved_store_boundary.md)
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
- 当前如果新开计划，优先级应围绕 `Phase 3` 的具体串行化边界后续判断，以及 reference / guest 的 bug-driven hardening 展开；`debug/frontend` 不再需要主动新开更重的浏览器压力验证计划。

## 当前优先级

### 1. `debug/frontend` 维持当前够用门禁，不再主动扩大压力面

- 真实 `debug server + mycpu --debug-cli` 端到端 smoke 已经落地，但它仍主要覆盖最小交互和短会话。
- 当前已经补上一组更窄的 Node/runtime 回归：持续 `run/pause`、运行中 session replacement，以及更高吞吐 terminal 输入聚合。
- 当前也已经进一步补到 repeated `run/pause` 长会话恢复、`reset` 后 terminal reset / offset 重启语义，以及真实 `guest_interactive_os_demo` 的 `run/pause + terminal-input` e2e。
- 对当前单用户、本地教学/调试使用，这组门禁已经足够；后续按真实 bug 或明确新需求补最小回归即可，不再主动扩大到更长时间 soak 或更重浏览器压力。
- 这条线的目标仍然只是“教学演示可用”，不是通用调试器，也不需要为当前使用方式预先建设更重的浏览器端压测体系。

### 2. `Phase 3` decode 级 `BlockedByUnresolvedStore` 边界后续判断

- 当前 `Phase 3-B/C` 已经完成最小真实 `OoO execute`，而 decode 级 `BlockedByUnresolvedStore` 的第一轮边界收窄也已经落地。
- 现在已确认：`BlockedByUnresolvedStore` 只表示“older store 地址未知才阻塞”；地址已知但 data 未 ready 的 older store 不再全局阻塞非重叠年轻 load，而重叠场景继续暴露 `BlockedByOverlappingStore`。
- 下一步如果还要继续碰这条线，重点应转向“是否值得继续扩 issue / replay / memory disambiguation”，而不是再重复讨论当前最小收窄本身。

### 3. 常态维护项

- 继续维护 reference correctness 矩阵，不让 illegal / MMIO / ELF / CSR / Sv39 合同回退。
- 继续守住 `kernel_alpha` 十条 guest 基线和 `guest_supervisor_demo` 的稳定输出。
- 继续维护 guest runtime 已经形成的 `vm*`、`trap*`、`kernel_bringup`、`kernel_runtime`、`user_program*` 边界，不让新增 bug 修复重新把职责揉回去。

## 当前明确不优先做的事

1. 不继续把 `debug/frontend` 往断点、条件暂停、任意文件加载或更大 UI 功能面扩张。
2. 不为了当前单用户本地使用场景，继续主动补更重的浏览器端压力验证或多客户端门禁。
3. 不把 `interactive_os` 当作新的产品主线；它当前仍应服务于 monitor / terminal / 调试链路验证。
4. 不把 `SimpleStorage` 更完整的设备模型抢在当前 correctness / structure hardening 前面推进。

## 如需新开计划

1. 如果继续碰 `Phase 3`，优先单开一份后续 issue / replay / memory disambiguation 取舍计划，不再回头为已完成的 decode 边界重复开计划。
2. 如果后续出现真实 `debug/frontend` bug，再围绕具体故障单开最小修复 / 回归计划，而不是泛化成新的浏览器压力专项。
3. 如果下一轮还要并行推进，建议围绕“Phase 3 后续判断 / guest runtime bug-driven hardening / reference correctness 补洞”拆 ownership，而不是继续机械沿用旧的 `P2-1..P2-7` 编号分线。
