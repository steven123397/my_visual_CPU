# docs 文档索引

## 推荐读取顺序

建议按下面的顺序进入当前正式文档：

1. [background/request.md](background/request.md)
   项目背景和原始目标。
2. [status/mainline_status.md](status/mainline_status.md)
   当前主线状态、近期任务和验证基线。
3. [status/project_priority_roadmap.md](status/project_priority_roadmap.md)
   基于当前 `main` 分支状态整理出的修正版优先级路线图。
4. [status/kernel_alpha_status.md](status/kernel_alpha_status.md)
   `kernel_alpha` bring-up 子线状态。
5. [status/code_self_review_status.md](status/code_self_review_status.md)
   当前仍有效的自检结论和 `interactive_os terminal` 专项风险。
6. [design/regression_completion_criteria.md](design/regression_completion_criteria.md)
   当前 Phase 1 / Phase 2 回归收口标准。
7. [design/cpp_refactor_design.md](design/cpp_refactor_design.md)
   C++ 结构重构的长期边界。
8. [design/platform_mmio_contract.md](design/platform_mmio_contract.md)
   guest / simulator 共享 MMIO 契约。
9. [design/pipeline_core_integration.md](design/pipeline_core_integration.md)
   `pipeline core` 接回主线的结构边界和历史语境。
10. [design/debug_frontend_integration.md](design/debug_frontend_integration.md)
   `debug_session/protocol + frontend` 的正式接入边界。
11. [design/minimal_interactive_os_design.md](design/minimal_interactive_os_design.md)
    最小可交互 `interactive_os` 路线的 host / frontend / guest 合同。
12. [design/phase3_branch_prediction_design.md](design/phase3_branch_prediction_design.md)
    `Phase 3-A` 分支预测增强边界。
13. [design/phase3_ooo_execution_model_design.md](design/phase3_ooo_execution_model_design.md)
    `Phase 3-B/C` 首轮执行模型边界。
14. [design/pipeline_speculation_contracts.md](design/pipeline_speculation_contracts.md)
    `pipeline` 投机执行与 commit-visible side-effect 契约。
15. [plan/history_plan.md#p1-pipeline-backend-boundary-refinement-plan](plan/history_plan.md#p1-pipeline-backend-boundary-refinement-plan)
    最近完成的 `P1-1` `pipeline_backend` 边界收口归档。
16. [plan/history_plan.md#p1-guest-public-header-boundary-refinement-plan](plan/history_plan.md#p1-guest-public-header-boundary-refinement-plan)
    最近完成的 `P1-5` guest 公共头文件边界收口归档。
17. [plan/history_plan.md#p1-guest-smoke-orchestration-refinement-plan](plan/history_plan.md#p1-guest-smoke-orchestration-refinement-plan)
    最近完成的 `P1-2` guest smoke orchestration 收口归档。
18. [plan/history_plan.md#phase3-ooo-execution-plan](plan/history_plan.md#phase3-ooo-execution-plan)
    已完成的 `Phase 3-B/C` 首轮总收口归档。
19. [plan/history_plan.md](plan/history_plan.md)
    已完成计划的统一归档入口，按完成顺序保留“什么时候做了什么”。
20. [design/kernel_alpha_storage_error_contract.md](design/kernel_alpha_storage_error_contract.md)
    `kernel_alpha` storage 错误合同设计。

## 目录说明

- [background](background)
  只放项目背景与原始目标。
- [design](design)
  只放长期有效的设计、契约和阶段边界。
- [plan](plan)
  只保留活跃计划、计划模板和完成态归档。
- [status](status)
  只放当前状态、风险、少量关键历史节点和下一步。

## 当前重点入口

- [status/mainline_status.md](status/mainline_status.md)
  当前主线实时状态。
- [status/project_priority_roadmap.md](status/project_priority_roadmap.md)
  当前项目的修正版优先级路线图。
- [status/kernel_alpha_status.md](status/kernel_alpha_status.md)
  `kernel_alpha` 子线实时状态。
- [status/code_self_review_status.md](status/code_self_review_status.md)
  当前仍有效的自检结果和调试链路风险。
- [plan/history_plan.md#p1-pipeline-backend-boundary-refinement-plan](plan/history_plan.md#p1-pipeline-backend-boundary-refinement-plan)
  最近完成的 `P1-1` `pipeline_backend` 边界收口归档。
- [plan/history_plan.md#p1-guest-public-header-boundary-refinement-plan](plan/history_plan.md#p1-guest-public-header-boundary-refinement-plan)
  最近完成的 `P1-5` guest 公共头文件边界收口归档。
- [plan/history_plan.md#p1-guest-smoke-orchestration-refinement-plan](plan/history_plan.md#p1-guest-smoke-orchestration-refinement-plan)
  最近完成的 `P1-2` guest smoke orchestration 收口归档。
- [plan/history_plan.md#phase3-ooo-execution-plan](plan/history_plan.md#phase3-ooo-execution-plan)
  最近完成的 `Phase 3-B/C` 首轮总收口归档。
- [plan/history_plan.md](plan/history_plan.md)
  已完成计划的统一归档入口；当前 `pipeline core`、Phase 1 hardening、Sv39 补洞和已完成的 `Phase 3` 子计划都收口在这里。
- [design/phase3_ooo_execution_model_design.md](design/phase3_ooo_execution_model_design.md)
  `Phase 3-B/C` 执行模型的正式边界。
- [design/pipeline_speculation_contracts.md](design/pipeline_speculation_contracts.md)
  `pipeline` 投机执行与提交契约。

## 保留的计划记录

当前 `plan/` 只保留以下正式入口：

- [plan/history_plan.md](plan/history_plan.md)
  已完成计划的统一归档文件；完成态计划先归档到这里，再删除原文件。
- [plan/template.md](plan/template.md)
  新计划文档统一从这里派生。

## 维护约束

- `index.md` 只做导航，不重复维护模块状态正文。
- 新增、重命名或删除正式文档后，必须同步更新本文件。
- 如果一份计划文档已经完成，应先把摘要归档到 `plan/history_plan.md`，再删除原文件，而不是在 `plan/` 中继续堆历史 checklist。
