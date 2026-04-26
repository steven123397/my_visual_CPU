# docs 文档索引

## 推荐读取顺序

建议先阅读仓库根 [README.md](../README.md)，再按下面顺序进入当前正式文档：

1. [background/request.md](background/request.md)
   项目背景与原始目标。
2. [showcase/README.md](showcase/README.md)
   项目展示材料、演示路径和展示截图入口。
3. [status/mainline_status.md](status/mainline_status.md)
   当前主线状态、活跃风险和下一步。
4. [status/project_priority_roadmap.md](status/project_priority_roadmap.md)
   当前仍然开放的优先级判断。
5. [status/xv6_linux_jit_status.md](status/xv6_linux_jit_status.md)
   当前已激活的 `xv6 / Linux / JIT` 主线切换状态。
6. [status/kernel_alpha_status.md](status/kernel_alpha_status.md)
   `kernel_alpha` bring-up 子线状态。
7. [design/regression_completion_criteria.md](design/regression_completion_criteria.md)
   当前 Phase 1 / Phase 2 回归收口标准。
8. [design/spike_differential_validation_design.md](design/spike_differential_validation_design.md)
   Spike 外部差分验证的当前实现边界、用户入口和扩展方向。
9. [design/platform_mmio_contract.md](design/platform_mmio_contract.md)
   当前平台 MMIO 地址布局、寄存器窗口与访问合同。
10. [design/debug_frontend_integration.md](design/debug_frontend_integration.md)
   `debug_session / protocol + frontend` 的统一设计边界。
11. [design/minimal_interactive_os_design.md](design/minimal_interactive_os_design.md)
    `interactive_os` 最小可交互 monitor 的当前设计边界。
12. [design/phase3_ooo_execution_model_design.md](design/phase3_ooo_execution_model_design.md)
    当前 `pipeline / Phase 3` 执行模型、`rename / ROB / LSQ` 边界和当前取舍判断。
13. [design/pipeline_speculation_contracts.md](design/pipeline_speculation_contracts.md)
    `pipeline` 的投机执行、commit boundary 与 side effect 可见性合同。
14. [design/vector_ml_workload_direction_design.md](design/vector_ml_workload_direction_design.md)
    `V-lite` 当前统一设计边界：最小 ISA、workload、vector-aware `pipeline` 与 `Phase 4` 衔接口径。
15. [design/npu_tpu_accelerator_direction_design.md](design/npu_tpu_accelerator_direction_design.md)
    独立 `MMIO NPU / TPU-like` AI 加速器方向：静态子图、`scratchpad + DMA`、host / guest 共用设备 ABI 的未来设计边界。
16. [design/phase4_preparation_design.md](design/phase4_preparation_design.md)
    `Phase 4` 当前准备性入口设计，以及 `P4-prep-1` 的正式边界。
17. [design/future_expansion_roadmap_design.md](design/future_expansion_roadmap_design.md)
    未来扩展路线图：ISA 补全、workload 升级、微架构深化、系统级跃迁的统一规划与依赖图。
18. [status/npu_tpu_accelerator_status.md](status/npu_tpu_accelerator_status.md)
    独立 `MMIO NPU / TPU-like` AI accelerator 的专项状态：当前冻结边界、风险和下一步。
19. [plan/history_plan.md#npu-tpu-accelerator-wave3-plan](plan/history_plan.md#npu-tpu-accelerator-wave3-plan)
    `NPU / TPU-like` AI accelerator Wave 3 的完成归档：runtime-shape fail-closed matrix、manifest 负向矩阵、itemized profile 文本出口与 lifecycle 收口。
20. [plan/history_plan.md#npu-tpu-accelerator-wave2-plan](plan/history_plan.md#npu-tpu-accelerator-wave2-plan)
    `NPU / TPU-like` AI accelerator Wave 2 的完成归档：profile attribution、tiny model、bounded dynamic shape 合同与 dynamic `GEMM / FC-like` 第一刀。
21. [plan/history_plan.md#npu-tpu-accelerator-wave1-plan](plan/history_plan.md#npu-tpu-accelerator-wave1-plan)
    `NPU / TPU-like` AI accelerator Wave 1 的完成归档：`DMA-ready` 基座、图包、控制面、数据面和 host/guest 接入结果。
22. [design/xv6_linux_jit_mainline_design.md](design/xv6_linux_jit_mainline_design.md)
    当前已激活的 `xv6 / Linux / JIT` 主线切换设计。
23. [plan/xv6_linux_jit_wave1_plan.md](plan/xv6_linux_jit_wave1_plan.md)
    当前 `xv6 / Linux / JIT` 主线切换的 Wave 1 执行计划、worktree 布局和 agent prompt。
24. [plan/history_plan.md#phase4-prep1-bus-memory-region-plan](plan/history_plan.md#phase4-prep1-bus-memory-region-plan)
    `P4-prep-1` 的完成归档与结果摘要。
25. [plan/history_plan.md](plan/history_plan.md)
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
- `AI accelerator / NPU`
  - [status/npu_tpu_accelerator_status.md](status/npu_tpu_accelerator_status.md)
  - [design/npu_tpu_accelerator_direction_design.md](design/npu_tpu_accelerator_direction_design.md)
  - [plan/history_plan.md#npu-tpu-accelerator-wave3-plan](plan/history_plan.md#npu-tpu-accelerator-wave3-plan)
  - [plan/history_plan.md#npu-tpu-accelerator-wave2-plan](plan/history_plan.md#npu-tpu-accelerator-wave2-plan)
  - [plan/history_plan.md#npu-tpu-accelerator-wave1-plan](plan/history_plan.md#npu-tpu-accelerator-wave1-plan)
  - [design/future_expansion_roadmap_design.md](design/future_expansion_roadmap_design.md)
  - [design/phase4_preparation_design.md](design/phase4_preparation_design.md)
- `debug / frontend`
  - [showcase/README.md](showcase/README.md)
  - [showcase/preview.html](showcase/preview.html)
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
- [showcase](showcase)
  项目展示材料、HTML 汇报预览页和展示截图。

## 维护约束

- `index.md` 只做导航，不重复维护状态正文。
- 新增、重命名或删除正式文档后，必须同步更新本文件。
- 已完成计划统一归档到 [plan/history_plan.md](plan/history_plan.md)，不在 `index.md` 长期维护逐条完成流水账。
