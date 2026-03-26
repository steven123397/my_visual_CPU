# docs 文档索引

## 读取顺序

建议按以下顺序阅读当前正式文档：

1. [background/request.md](/home/liangjiaqi/projects/my_visual_CPU/docs/background/request.md)
   了解项目背景和原始目标。
2. [status/current_mainline_status_2026-03-25.md](/home/liangjiaqi/projects/my_visual_CPU/docs/status/current_mainline_status_2026-03-25.md)
   了解 `phase1-stable` 冻结后、`pipeline` 与 `debug/frontend` 已接入，以及 `2026-03-26` 第一轮 hardening 回归扩充之后的当前主线任务切分和下一步优先级。
3. [status/regression_completion_criteria_2026-03-26.md](/home/liangjiaqi/projects/my_visual_CPU/docs/status/regression_completion_criteria_2026-03-26.md)
   了解当前 Phase 1 / Phase 2 主线中，回归相关工作做到什么程度可认为阶段性收口。
4. [design/cpp_refactor_plan.md](/home/liangjiaqi/projects/my_visual_CPU/docs/design/cpp_refactor_plan.md)
   理解当前 C++ 结构重构的动机和边界。
5. [design/pipeline_integration_prep.md](/home/liangjiaqi/projects/my_visual_CPU/docs/design/pipeline_integration_prep.md)
   理解 `phase1-stable` 冻结后，后续流水线代码如何接回当前主线。该文档记录的是已完成的准备边界。
6. [design/pipeline_core_integration.md](/home/liangjiaqi/projects/my_visual_CPU/docs/design/pipeline_core_integration.md)
   理解第一轮如何把 `pipeline core` 按当前主线边界分批重接。该文档记录的是已完成的第一轮设计。
7. [design/debug_frontend_integration.md](/home/liangjiaqi/projects/my_visual_CPU/docs/design/debug_frontend_integration.md)
   理解第二轮如何把 `debug_session/protocol` 与本地前端教学演示链路接回主线。该文档记录的是已完成的第二轮设计。
8. [contracts/platform_mmio_contract.md](/home/liangjiaqi/projects/my_visual_CPU/docs/contracts/platform_mmio_contract.md)
   理解 guest / simulator 共享的 MMIO 契约。
9. [status/code_self_review_2026-03-24.md](/home/liangjiaqi/projects/my_visual_CPU/docs/status/code_self_review_2026-03-24.md)
   查看最近一轮系统性自检留下的仍有效风险。
10. [status/kernel_alpha_bringup_status.md](/home/liangjiaqi/projects/my_visual_CPU/docs/status/kernel_alpha_bringup_status.md)
   查看独立 `kernel_alpha` bring-up 的当前状态和下一步。

## 目录说明

- [background](/home/liangjiaqi/projects/my_visual_CPU/docs/background)
  背景说明和原始目标。
- [design](/home/liangjiaqi/projects/my_visual_CPU/docs/design)
  长期有效的结构设计文档。
- [contracts](/home/liangjiaqi/projects/my_visual_CPU/docs/contracts)
  共享接口和平台契约。
- [status](/home/liangjiaqi/projects/my_visual_CPU/docs/status)
  当前主维护状态文档。
- [templates](/home/liangjiaqi/projects/my_visual_CPU/docs/templates)
  模板类文档。
- [archive](/home/liangjiaqi/projects/my_visual_CPU/docs/archive)
  已完成工作的归档材料，不属于主文档入口。
- [superpowers](/home/liangjiaqi/projects/my_visual_CPU/docs/superpowers)
  本地 AI 规划 / 执行记录，默认不作为正式文档入口。
