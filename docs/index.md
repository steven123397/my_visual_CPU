# docs 文档索引

## 推荐读取顺序

建议按下面的顺序进入当前正式文档：

1. [background/request.md](background/request.md)
   项目背景和原始目标。
2. [status/mainline_status.md](status/mainline_status.md)
   当前主线状态、近期任务和验证基线。
3. [status/kernel_alpha_status.md](status/kernel_alpha_status.md)
   `kernel_alpha` bring-up 子线状态。
4. [status/code_self_review_status.md](status/code_self_review_status.md)
   当前仍有效的自检结论和 `interactive_os terminal` 专项风险。
5. [design/regression_completion_criteria.md](design/regression_completion_criteria.md)
   当前 Phase 1 / Phase 2 回归收口标准。
6. [design/cpp_refactor_design.md](design/cpp_refactor_design.md)
   C++ 结构重构的长期边界。
7. [design/platform_mmio_contract.md](design/platform_mmio_contract.md)
   guest / simulator 共享 MMIO 契约。
8. [design/pipeline_core_integration.md](design/pipeline_core_integration.md)
   `pipeline core` 接回主线的结构边界和历史语境。
9. [design/debug_frontend_integration.md](design/debug_frontend_integration.md)
   `debug_session/protocol + frontend` 的正式接入边界。
10. [design/minimal_interactive_os_design.md](design/minimal_interactive_os_design.md)
    最小可交互 `interactive_os` 路线的 host / frontend / guest 合同。
11. [design/phase3_branch_prediction_design.md](design/phase3_branch_prediction_design.md)
    `Phase 3-A` 分支预测增强边界。
12. [design/phase3_ooo_execution_model_design.md](design/phase3_ooo_execution_model_design.md)
    `Phase 3-B/C` 首轮执行模型边界。
13. [design/pipeline_speculation_contracts.md](design/pipeline_speculation_contracts.md)
    `pipeline` 投机执行与 commit-visible side-effect 契约。
14. [plan/phase3_ooo_execution_plan.md](plan/phase3_ooo_execution_plan.md)
    当前活跃的 `Phase 3-B/C` `OoO / rename / ROB / LSQ` 接线计划。
15. [plan/phase3_minimal_ooo_execute_plan.md](plan/phase3_minimal_ooo_execute_plan.md)
    已完成的“最小真实 OoO execute”子计划。
16. [plan/phase3_lsq_automatic_replay_plan.md](plan/phase3_lsq_automatic_replay_plan.md)
    已完成的 `Phase 3-B/C` LSQ automatic replay 子计划。
17. [plan/phase3_lsq_store_to_load_forwarding_plan.md](plan/phase3_lsq_store_to_load_forwarding_plan.md)
    已完成的 `Phase 3-B/C` LSQ store-to-load forwarding 子计划。
18. [plan/phase3_lsq_replay_contract_plan.md](plan/phase3_lsq_replay_contract_plan.md)
    已完成的 `Phase 3-B/C` LSQ replay-needed 合同子计划。
19. [plan/phase3_phys_free_list_plan.md](plan/phase3_phys_free_list_plan.md)
    已完成的 `Phase 3-B/C` phys free-list / recycle 子计划。
20. [plan/phase3_ooo_readiness_plan.md](plan/phase3_ooo_readiness_plan.md)
    已完成的 `Phase 3-B/C` OoO readiness 前置准备记录。
21. [design/kernel_alpha_storage_error_contract.md](design/kernel_alpha_storage_error_contract.md)
    `kernel_alpha` storage 错误合同设计。

## 目录说明

- [background](background)
  只放项目背景与原始目标。
- [design](design)
  只放长期有效的设计、契约和阶段边界。
- [plan](plan)
  只保留仍有参考价值的计划记录。
- [status](status)
  只放当前状态、风险、少量关键历史节点和下一步。

## 当前重点入口

- [status/mainline_status.md](status/mainline_status.md)
  当前主线实时状态。
- [status/kernel_alpha_status.md](status/kernel_alpha_status.md)
  `kernel_alpha` 子线实时状态。
- [status/code_self_review_status.md](status/code_self_review_status.md)
  当前仍有效的自检结果和调试链路风险。
- [plan/phase3_ooo_execution_plan.md](plan/phase3_ooo_execution_plan.md)
  当前正在执行的 `Phase 3-B/C` 接线计划。
- [plan/phase3_minimal_ooo_execute_plan.md](plan/phase3_minimal_ooo_execute_plan.md)
  已完成的“最小真实 OoO execute”子计划，记录 `ROB` 驱动退休与最小 memory execute 的收口。
- [plan/phase3_lsq_automatic_replay_plan.md](plan/phase3_lsq_automatic_replay_plan.md)
  已完成的 `LSQ automatic replay` 子计划。
- [plan/phase3_lsq_store_to_load_forwarding_plan.md](plan/phase3_lsq_store_to_load_forwarding_plan.md)
  已完成的 `LSQ store-to-load forwarding` 子计划。
- [plan/phase3_lsq_replay_contract_plan.md](plan/phase3_lsq_replay_contract_plan.md)
  已完成的 `LSQ replay-needed` 合同子计划。
- [plan/phase3_phys_free_list_plan.md](plan/phase3_phys_free_list_plan.md)
  已完成的 phys free-list / recycle 子计划。
- [plan/phase3_ooo_readiness_plan.md](plan/phase3_ooo_readiness_plan.md)
  已完成的 `Phase 3-B/C` OoO readiness 收口记录，也是下一份大块 OoO 接线计划的直接前置入口。
- [design/phase3_ooo_execution_model_design.md](design/phase3_ooo_execution_model_design.md)
  `Phase 3-B/C` 执行模型的正式边界。
- [design/pipeline_speculation_contracts.md](design/pipeline_speculation_contracts.md)
  `pipeline` 投机执行与提交契约。

## 保留的计划记录

当前 `plan/` 只保留仍然值得独立回看的计划记录：

- [plan/phase3_ooo_execution_plan.md](plan/phase3_ooo_execution_plan.md)
  当前活跃，承接真正的大块 `OoO / rename / ROB / LSQ` 接线。
- [plan/phase3_minimal_ooo_execute_plan.md](plan/phase3_minimal_ooo_execute_plan.md)
  当前已完成，记录“最小真实 OoO execute”这块从近似顺序 execute 到最小真实 OoO 完成窗口的收口。
- [plan/phase3_lsq_automatic_replay_plan.md](plan/phase3_lsq_automatic_replay_plan.md)
  当前已完成，记录 `LSQ replay-needed` 到 coarse automatic replay 的收口。
- [plan/phase3_lsq_store_to_load_forwarding_plan.md](plan/phase3_lsq_store_to_load_forwarding_plan.md)
  当前已完成，记录最小 `RAM-only store-to-load forwarding` 的收口。
- [plan/phase3_lsq_replay_contract_plan.md](plan/phase3_lsq_replay_contract_plan.md)
  当前已完成，记录 `LSQ replay-needed` 这块 memory-order 合同收口。
- [plan/phase3_phys_free_list_plan.md](plan/phase3_phys_free_list_plan.md)
  当前已完成，记录 `phys free-list / recycle` 这块结构收口。
- [plan/phase3_ooo_readiness_plan.md](plan/phase3_ooo_readiness_plan.md)
  当前已完成，保留为下一轮大块 `OoO / rename / ROB / LSQ` 接线的前置记录。
- [plan/phase1-hardening-regressions_plan.md](plan/phase1-hardening-regressions_plan.md)
- [plan/pipeline_core_integration_plan.md](plan/pipeline_core_integration_plan.md)
- [plan/phase3_branch_prediction_plan.md](plan/phase3_branch_prediction_plan.md)
- [plan/sv39_mprv_semantics_plan.md](plan/sv39_mprv_semantics_plan.md)
- [plan/sv39_pagewalk_contracts_plan.md](plan/sv39_pagewalk_contracts_plan.md)
- [plan/kernel_alpha_storage_error_contract_plan.md](plan/kernel_alpha_storage_error_contract_plan.md)

已被 `design/status` 吸收的完成态计划，当前不再单独保留。

## 维护约束

- `index.md` 只做导航，不重复维护模块状态正文。
- 新增、重命名或删除正式文档后，必须同步更新本文件。
- 如果一份文档已经只剩历史流水账价值，应优先合并到更合适的 `design/status` 文档，而不是继续增加平行入口。
