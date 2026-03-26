# docs 文档索引

## 读取顺序

建议按以下顺序阅读当前正式文档：

1. [background/request.md](/home/liangjiaqi/projects/my_visual_CPU/docs/background/request.md)
   了解项目背景和原始目标。
2. [status/mainline_status.md](/home/liangjiaqi/projects/my_visual_CPU/docs/status/mainline_status.md)
   查看当前主线状态、近期主线任务和验证基线。
3. [status/kernel_alpha_status.md](/home/liangjiaqi/projects/my_visual_CPU/docs/status/kernel_alpha_status.md)
   查看独立 `kernel_alpha` bring-up 的当前状态和历史节点。
4. [status/code_self_review_status.md](/home/liangjiaqi/projects/my_visual_CPU/docs/status/code_self_review_status.md)
   查看最近一轮系统性自检留下的仍有效风险和整改进展。
5. [design/regression_completion_criteria.md](/home/liangjiaqi/projects/my_visual_CPU/docs/design/regression_completion_criteria.md)
   了解当前 Phase 1 / Phase 2 回归何时可以认为达到阶段性收口。
6. [design/cpp_refactor_design.md](/home/liangjiaqi/projects/my_visual_CPU/docs/design/cpp_refactor_design.md)
   理解当前 C++ 结构重构的动机和边界。
7. [design/platform_mmio_contract.md](/home/liangjiaqi/projects/my_visual_CPU/docs/design/platform_mmio_contract.md)
   理解 guest / simulator 共享的 MMIO 契约。
8. [design/pipeline_integration_prep.md](/home/liangjiaqi/projects/my_visual_CPU/docs/design/pipeline_integration_prep.md)
   理解 `phase1-stable` 冻结后，后续流水线代码如何接回当前主线。
9. [design/pipeline_core_integration.md](/home/liangjiaqi/projects/my_visual_CPU/docs/design/pipeline_core_integration.md)
   理解第一轮 `pipeline core` 接回主线的设计与结果。
10. [design/debug_frontend_integration.md](/home/liangjiaqi/projects/my_visual_CPU/docs/design/debug_frontend_integration.md)
   理解第二轮 `debug_session/protocol` 与本地前端教学演示链路接回主线的设计与结果。
11. [design/minimal_interactive_os_design.md](/home/liangjiaqi/projects/my_visual_CPU/docs/design/minimal_interactive_os_design.md)
   理解“前端桌面壳 + guest 串口 monitor 内核”这一最小可交互 OS 的设计边界。
12. [design/phase3_branch_prediction_design.md](/home/liangjiaqi/projects/my_visual_CPU/docs/design/phase3_branch_prediction_design.md)
   理解 `Phase 3-A` 以分支预测增强开启高级微架构工作的结构边界。

## 目录说明

- [background](/home/liangjiaqi/projects/my_visual_CPU/docs/background)
  只放背景和原始目标。当前唯一正式文件是 `request.md`。
- [design](/home/liangjiaqi/projects/my_visual_CPU/docs/design)
  放设计、契约、阶段标准和长期有效的方案边界。
- [plan](/home/liangjiaqi/projects/my_visual_CPU/docs/plan)
  放执行计划与已完成计划记录。实现 checklist 只应出现在这里。
- [status](/home/liangjiaqi/projects/my_visual_CPU/docs/status)
  放实时状态、风险、历史节点和下一步。当前进度只应以这里为准。

## 当前主入口

- [mainline_status.md](/home/liangjiaqi/projects/my_visual_CPU/docs/status/mainline_status.md)
  当前主线实时状态。
- [kernel_alpha_status.md](/home/liangjiaqi/projects/my_visual_CPU/docs/status/kernel_alpha_status.md)
  `kernel_alpha` 子线实时状态。

## 重要计划记录

- [sv39_mprv_semantics_plan.md](/home/liangjiaqi/projects/my_visual_CPU/docs/plan/sv39_mprv_semantics_plan.md)
  已完成的 `privilege / Sv39` 回归补洞计划，聚焦 `MPRV + Sv39` 数据访存语义。
- [sv39_pagewalk_contracts_plan.md](/home/liangjiaqi/projects/my_visual_CPU/docs/plan/sv39_pagewalk_contracts_plan.md)
  已完成的 `Sv39` page-walk 合同补洞计划，聚焦 superpage 对齐与 non-leaf PTE 保留位。
- [docs_information_architecture_reorg_plan.md](/home/liangjiaqi/projects/my_visual_CPU/docs/plan/docs_information_architecture_reorg_plan.md)
  已完成的文档治理重组计划。
- [phase1-hardening-regressions_plan.md](/home/liangjiaqi/projects/my_visual_CPU/docs/plan/phase1-hardening-regressions_plan.md)
  已完成的 Phase 1 hardening 回归扩充计划。
- [pipeline_core_integration_plan.md](/home/liangjiaqi/projects/my_visual_CPU/docs/plan/pipeline_core_integration_plan.md)
  已完成的 `pipeline core` 主线集成计划。
- [debug_frontend_integration_plan.md](/home/liangjiaqi/projects/my_visual_CPU/docs/plan/debug_frontend_integration_plan.md)
  已完成的 `debug/frontend` 接入计划。

## 维护约束

- `index.md` 只做入口，不重复维护模块状态正文。
- 新增或重命名正式文档后，必须同步更新本文件。
- 如果某份文档已不再属于 `background / design / plan / status` 四类之一，应优先合并或删除，而不是继续新开平行目录。
