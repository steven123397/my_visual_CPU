# docs 文档索引

## 推荐读取顺序

建议先阅读仓库根 [README.md](../README.md)，再按下面顺序进入当前正式文档：

1. [background/request.md](background/request.md)
   项目背景与原始目标。
2. [status/mainline_status.md](status/mainline_status.md)
   当前主线状态、活跃风险和下一步。
3. [status/project_priority_roadmap.md](status/project_priority_roadmap.md)
   当前仍然开放的优先级判断。
4. [status/xv6_linux_jit_status.md](status/xv6_linux_jit_status.md)
   当前已激活的 `xv6 / Linux / JIT` 主线切换状态。
5. [status/kernel_alpha_status.md](status/kernel_alpha_status.md)
   `kernel_alpha` bring-up 子线状态。
6. [design/regression_completion_criteria.md](design/regression_completion_criteria.md)
   当前 Phase 1 / Phase 2 回归收口标准。
7. [design/spike_differential_validation_design.md](design/spike_differential_validation_design.md)
   Spike 外部差分验证的当前实现边界、用户入口和扩展方向。
8. [design/platform_mmio_contract.md](design/platform_mmio_contract.md)
   当前平台 MMIO 地址布局、寄存器窗口与访问合同。
9. [design/debug_frontend_integration.md](design/debug_frontend_integration.md)
   `debug_session / protocol + frontend` 的统一设计边界。
10. [design/minimal_interactive_os_design.md](design/minimal_interactive_os_design.md)
    `interactive_os` 最小可交互 monitor 的当前设计边界。
11. [design/phase3_ooo_execution_model_design.md](design/phase3_ooo_execution_model_design.md)
    当前 `pipeline / Phase 3` 执行模型、`rename / ROB / LSQ` 边界和当前取舍判断。
12. [design/pipeline_speculation_contracts.md](design/pipeline_speculation_contracts.md)
    `pipeline` 的投机执行、commit boundary 与 side effect 可见性合同。
13. [design/vector_ml_workload_direction_design.md](design/vector_ml_workload_direction_design.md)
    `V-lite` 当前统一设计边界：最小 ISA、workload、vector-aware `pipeline` 与 `Phase 4` 衔接口径。
14. [design/phase4_preparation_design.md](design/phase4_preparation_design.md)
    `Phase 4` 当前准备性入口设计，以及 `P4-prep-1` 的正式边界。
15. [design/future_expansion_roadmap_design.md](design/future_expansion_roadmap_design.md)
    未来扩展路线图：ISA 补全、workload 升级、微架构深化、系统级跃迁的统一规划与依赖图。
16. [design/xv6_linux_jit_mainline_design.md](design/xv6_linux_jit_mainline_design.md)
    当前已激活的 `xv6 / Linux / JIT` 主线切换设计。
17. [plan/xv6_linux_jit_wave1_plan.md](plan/xv6_linux_jit_wave1_plan.md)
    当前 `xv6 / Linux / JIT` 主线切换的 Wave 1 执行计划、worktree 布局和 agent prompt。
18. [plan/history_plan.md#phase4-prep1-bus-memory-region-plan](plan/history_plan.md#phase4-prep1-bus-memory-region-plan)
    `P4-prep-1` 的完成归档与结果摘要。
19. [plan/history_plan.md](plan/history_plan.md)
    已完成计划的统一归档入口。

## 专题入口

- `pipeline`
  - [design/phase3_ooo_execution_model_design.md](design/phase3_ooo_execution_model_design.md)
  - [design/pipeline_speculation_contracts.md](design/pipeline_speculation_contracts.md)
- `向量 / ML workload`
  - [design/vector_ml_workload_direction_design.md](design/vector_ml_workload_direction_design.md)
  - [plan/history_plan.md#vector-v0-v1-plan](plan/history_plan.md#vector-v0-v1-plan)
  - [plan/history_plan.md#vector-v2-plan](plan/history_plan.md#vector-v2-plan)
  - [plan/history_plan.md#vector-v3-plan](plan/history_plan.md#vector-v3-plan)
  - [plan/history_plan.md#vector-v3-hardening-v4-design-plan](plan/history_plan.md#vector-v3-hardening-v4-design-plan)
  - [plan/history_plan.md#vector-v4-plan](plan/history_plan.md#vector-v4-plan)
- `debug / frontend`
  - [design/debug_frontend_integration.md](design/debug_frontend_integration.md)
  - [design/minimal_interactive_os_design.md](design/minimal_interactive_os_design.md)
  - [plan/history_plan.md#p1-debug-frontend-boundary-refinement-plan](plan/history_plan.md#p1-debug-frontend-boundary-refinement-plan)
  - [plan/history_plan.md#vector-frontend-visualization-plan](plan/history_plan.md#vector-frontend-visualization-plan)
- `Phase 4 准备`
  - [design/phase4_preparation_design.md](design/phase4_preparation_design.md)
  - [design/platform_mmio_contract.md](design/platform_mmio_contract.md)
  - [design/vector_ml_workload_direction_design.md](design/vector_ml_workload_direction_design.md)
  - [plan/history_plan.md#phase4-prep1-bus-memory-region-plan](plan/history_plan.md#phase4-prep1-bus-memory-region-plan)
- `platform / MMIO`
  - [design/platform_mmio_contract.md](design/platform_mmio_contract.md)
  - [design/regression_completion_criteria.md](design/regression_completion_criteria.md)
- `kernel_alpha`
  - [status/kernel_alpha_status.md](status/kernel_alpha_status.md)
  - [design/platform_mmio_contract.md](design/platform_mmio_contract.md)
- `Spike 外部差分验证`
  - [design/spike_differential_validation_design.md](design/spike_differential_validation_design.md)
  - [plan/history_plan.md#spike-external-differential-validation-plan](plan/history_plan.md#spike-external-differential-validation-plan)
- `未来扩展路线图`
  - [design/future_expansion_roadmap_design.md](design/future_expansion_roadmap_design.md)
- `xv6 / Linux / JIT foundation`
  - [status/xv6_linux_jit_status.md](status/xv6_linux_jit_status.md)
  - [design/xv6_linux_jit_mainline_design.md](design/xv6_linux_jit_mainline_design.md)
  - [plan/xv6_linux_jit_wave1_plan.md](plan/xv6_linux_jit_wave1_plan.md)

## 目录说明

- [background](background)
  项目背景与原始目标。
- [status](status)
  当前状态、风险、少量关键历史节点和下一步。
- [design](design)
  当前仍然有效的设计边界、模块参考资料与阶段合同。
- [plan](plan)
  活跃计划、计划模板和已完成计划归档。

## 维护约束

- `index.md` 只做导航，不重复维护状态正文。
- 新增、重命名或删除正式文档后，必须同步更新本文件。
- 已完成计划统一归档到 [plan/history_plan.md](plan/history_plan.md)，不在 `index.md` 长期维护逐条完成流水账。
