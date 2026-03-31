# docs 文档索引

## 推荐读取顺序

建议按下面的顺序进入当前正式文档：

1. [background/request.md](/home/liangjiaqi/projects/my_visual_CPU/docs/background/request.md)
   项目背景和原始目标。
2. [status/mainline_status.md](/home/liangjiaqi/projects/my_visual_CPU/docs/status/mainline_status.md)
   当前主线状态、近期任务和验证基线。
3. [status/kernel_alpha_status.md](/home/liangjiaqi/projects/my_visual_CPU/docs/status/kernel_alpha_status.md)
   `kernel_alpha` bring-up 子线状态。
4. [status/code_self_review_status.md](/home/liangjiaqi/projects/my_visual_CPU/docs/status/code_self_review_status.md)
   当前仍有效的自检结论和 `interactive_os terminal` 专项风险。
5. [design/regression_completion_criteria.md](/home/liangjiaqi/projects/my_visual_CPU/docs/design/regression_completion_criteria.md)
   当前 Phase 1 / Phase 2 回归收口标准。
6. [design/cpp_refactor_design.md](/home/liangjiaqi/projects/my_visual_CPU/docs/design/cpp_refactor_design.md)
   C++ 结构重构的长期边界。
7. [design/platform_mmio_contract.md](/home/liangjiaqi/projects/my_visual_CPU/docs/design/platform_mmio_contract.md)
   guest / simulator 共享 MMIO 契约。
8. [design/pipeline_core_integration.md](/home/liangjiaqi/projects/my_visual_CPU/docs/design/pipeline_core_integration.md)
   `pipeline core` 接回主线的结构边界和历史语境。
9. [design/debug_frontend_integration.md](/home/liangjiaqi/projects/my_visual_CPU/docs/design/debug_frontend_integration.md)
   `debug_session/protocol + frontend` 的正式接入边界。
10. [design/minimal_interactive_os_design.md](/home/liangjiaqi/projects/my_visual_CPU/docs/design/minimal_interactive_os_design.md)
    最小可交互 `interactive_os` 路线的 host / frontend / guest 合同。
11. [design/phase3_branch_prediction_design.md](/home/liangjiaqi/projects/my_visual_CPU/docs/design/phase3_branch_prediction_design.md)
    `Phase 3-A` 分支预测增强边界。
12. [design/kernel_alpha_storage_error_contract.md](/home/liangjiaqi/projects/my_visual_CPU/docs/design/kernel_alpha_storage_error_contract.md)
    `kernel_alpha` storage 错误合同设计。

## 目录说明

- [background](/home/liangjiaqi/projects/my_visual_CPU/docs/background)
  只放项目背景与原始目标。
- [design](/home/liangjiaqi/projects/my_visual_CPU/docs/design)
  只放长期有效的设计、契约和阶段边界。
- [plan](/home/liangjiaqi/projects/my_visual_CPU/docs/plan)
  只保留仍有参考价值的计划记录。
- [status](/home/liangjiaqi/projects/my_visual_CPU/docs/status)
  只放当前状态、风险、少量关键历史节点和下一步。

## 当前重点入口

- [status/mainline_status.md](/home/liangjiaqi/projects/my_visual_CPU/docs/status/mainline_status.md)
  当前主线实时状态。
- [status/kernel_alpha_status.md](/home/liangjiaqi/projects/my_visual_CPU/docs/status/kernel_alpha_status.md)
  `kernel_alpha` 子线实时状态。
- [status/code_self_review_status.md](/home/liangjiaqi/projects/my_visual_CPU/docs/status/code_self_review_status.md)
  当前仍有效的自检结果和调试链路风险。

## 保留的计划记录

当前 `plan/` 只保留仍然值得独立回看的计划记录：

- [plan/phase1-hardening-regressions_plan.md](/home/liangjiaqi/projects/my_visual_CPU/docs/plan/phase1-hardening-regressions_plan.md)
- [plan/pipeline_core_integration_plan.md](/home/liangjiaqi/projects/my_visual_CPU/docs/plan/pipeline_core_integration_plan.md)
- [plan/phase3_branch_prediction_plan.md](/home/liangjiaqi/projects/my_visual_CPU/docs/plan/phase3_branch_prediction_plan.md)
- [plan/sv39_mprv_semantics_plan.md](/home/liangjiaqi/projects/my_visual_CPU/docs/plan/sv39_mprv_semantics_plan.md)
- [plan/sv39_pagewalk_contracts_plan.md](/home/liangjiaqi/projects/my_visual_CPU/docs/plan/sv39_pagewalk_contracts_plan.md)
- [plan/kernel_alpha_storage_error_contract_plan.md](/home/liangjiaqi/projects/my_visual_CPU/docs/plan/kernel_alpha_storage_error_contract_plan.md)

已被 `design/status` 吸收的完成态计划，当前不再单独保留。

## 维护约束

- `index.md` 只做导航，不重复维护模块状态正文。
- 新增、重命名或删除正式文档后，必须同步更新本文件。
- 如果一份文档已经只剩历史流水账价值，应优先合并到更合适的 `design/status` 文档，而不是继续增加平行入口。
