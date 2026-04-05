# docs 文档索引

## 推荐读取顺序

建议先阅读仓库根 [README.md](../README.md)，再按下面顺序进入当前正式文档：

1. [background/request.md](background/request.md)
   项目背景和原始目标。
2. [status/mainline_status.md](status/mainline_status.md)
   当前主线状态、活跃风险和下一步。
3. [status/project_priority_roadmap.md](status/project_priority_roadmap.md)
   当前仍然开放的优先级判断。
4. [status/kernel_alpha_status.md](status/kernel_alpha_status.md)
   `kernel_alpha` bring-up 子线状态。
5. [design/regression_completion_criteria.md](design/regression_completion_criteria.md)
   当前 Phase 1 / Phase 2 回归收口标准。
6. [design/debug_frontend_integration.md](design/debug_frontend_integration.md)
   `debug_session/protocol + frontend` 的正式接入边界。
7. [design/minimal_interactive_os_design.md](design/minimal_interactive_os_design.md)
   最小可交互 `interactive_os` 的 host / frontend / guest 合同。
8. [design/phase3_ooo_execution_model_design.md](design/phase3_ooo_execution_model_design.md)
   `Phase 3-B/C` 执行模型边界。
9. [design/blocked_by_unresolved_store_boundary.md](design/blocked_by_unresolved_store_boundary.md)
   decode 级 `BlockedByUnresolvedStore` 串行化边界专项设计。
10. [design/phase3_issue_replay_speculation_assessment.md](design/phase3_issue_replay_speculation_assessment.md)
   decode 边界收窄之后，`Phase 3` 是否继续扩更激进 `issue / replay / speculation` 的取舍结论。
11. [design/pipeline_speculation_contracts.md](design/pipeline_speculation_contracts.md)
   `pipeline` 投机执行与提交契约。
12. [plan/history_plan.md](plan/history_plan.md)
    已完成计划的统一归档入口。

## 专题入口

- `pipeline`
  - [design/pipeline_core_integration.md](design/pipeline_core_integration.md)
  - [design/phase3_branch_prediction_design.md](design/phase3_branch_prediction_design.md)
  - [design/phase3_ooo_execution_model_design.md](design/phase3_ooo_execution_model_design.md)
  - [design/blocked_by_unresolved_store_boundary.md](design/blocked_by_unresolved_store_boundary.md)
  - [design/phase3_issue_replay_speculation_assessment.md](design/phase3_issue_replay_speculation_assessment.md)
  - [design/pipeline_speculation_contracts.md](design/pipeline_speculation_contracts.md)
- `debug/frontend`
  - [design/debug_frontend_integration.md](design/debug_frontend_integration.md)
  - [design/minimal_interactive_os_design.md](design/minimal_interactive_os_design.md)
- `platform / MMIO`
  - [design/platform_mmio_contract.md](design/platform_mmio_contract.md)
  - [design/regression_completion_criteria.md](design/regression_completion_criteria.md)
- `kernel_alpha`
  - [status/kernel_alpha_status.md](status/kernel_alpha_status.md)
  - [design/kernel_alpha_storage_error_contract.md](design/kernel_alpha_storage_error_contract.md)
- `代码审查 / 修改`
  - [status/code_reself_status.md](status/code_reself_status.md)

## 目录说明

- [background](background)
  项目背景与原始目标。
- [status](status)
  当前状态、风险、少量关键历史节点和下一步。
- [design](design)
  长期有效的设计、契约和阶段边界。
- [plan](plan)
  活跃计划、计划模板和已完成计划归档。

## 维护约束

- `index.md` 只做导航，不重复维护状态正文。
- 新增、重命名或删除正式文档后，必须同步更新本文件。
- 已完成计划统一归档到 [plan/history_plan.md](plan/history_plan.md)，不在 `index.md` 长期维护逐条完成流水账。
