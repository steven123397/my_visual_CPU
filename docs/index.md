# docs 文档索引

## 推荐读取顺序

建议先阅读仓库根 [README.md](../README.md)，再按下面顺序进入正式文档：

1. [background/request.md](background/request.md)
   项目背景与原始目标。
2. [status/mainline_status.md](status/mainline_status.md)
   仓库唯一的主线实时状态、当前优先级和下一步。
3. [status/kernel_alpha_status.md](status/kernel_alpha_status.md)
   `kernel_alpha` bring-up 子线状态。
4. [status/npu_tpu_accelerator_status.md](status/npu_tpu_accelerator_status.md)
   独立 `NPU / TPU-like` 方向的专项状态。
5. [design/regression_completion_criteria.md](design/regression_completion_criteria.md)
   当前 Phase 1 / Phase 2 回归收口标准。
6. [design/xv6_linux_jit_mainline_design.md](design/xv6_linux_jit_mainline_design.md)
   `xv6 / Linux` 主线切换设计边界。
7. [design/phase3_ooo_execution_model_design.md](design/phase3_ooo_execution_model_design.md)
   当前 `pipeline / Phase 3` 边界和取舍判断。
8. [design/pipeline_speculation_contracts.md](design/pipeline_speculation_contracts.md)
   `pipeline` 投机执行和 side effect 可见性合同。
9. [design/phase4_preparation_design.md](design/phase4_preparation_design.md)
   `Phase 4` 当前准备性入口设计。
10. [design/future_expansion_roadmap_design.md](design/future_expansion_roadmap_design.md)
    主线长期路线图和 wave 激活门槛。
11. [design/wave5_cache_memory_system_design.md](design/wave5_cache_memory_system_design.md)
    主线 `Wave 5 / cache / memory-system` 当前设计入口。
12. [design/wave6_jit_dbt_readiness_design.md](design/wave6_jit_dbt_readiness_design.md)
    主线 `Wave 6 / JIT / DBT` 当前 readiness、候选观察、translation contract、prototype、preflight guardrail、translation-plan dry-run 与 fallback replay 等价性设计入口。
13. [design/vector_ml_workload_direction_design.md](design/vector_ml_workload_direction_design.md)
    向量扩展与 ML workload 的统一设计边界。
14. [design/npu_tpu_accelerator_direction_design.md](design/npu_tpu_accelerator_direction_design.md)
    独立 `MMIO NPU / TPU-like` AI accelerator 方向设计。
15. [design/debug_frontend_integration.md](design/debug_frontend_integration.md)
    `debug_session / protocol + frontend` 设计边界。
16. [design/spike_differential_validation_design.md](design/spike_differential_validation_design.md)
    Spike 外部差分验证边界。
17. [plan/history_plan.md](plan/history_plan.md)
    已完成计划的统一归档入口。

## 专题入口

- `主线状态`
  - [status/mainline_status.md](status/mainline_status.md)
  - [design/future_expansion_roadmap_design.md](design/future_expansion_roadmap_design.md)
  - [design/xv6_linux_jit_mainline_design.md](design/xv6_linux_jit_mainline_design.md)
  - [design/wave6_jit_dbt_readiness_design.md](design/wave6_jit_dbt_readiness_design.md)
- `pipeline`
  - [design/phase3_ooo_execution_model_design.md](design/phase3_ooo_execution_model_design.md)
  - [design/pipeline_speculation_contracts.md](design/pipeline_speculation_contracts.md)
- `向量 / ML workload`
  - [design/vector_ml_workload_direction_design.md](design/vector_ml_workload_direction_design.md)
  - [plan/history_plan.md#vector-v4-plan](plan/history_plan.md#vector-v4-plan)
- `AI accelerator / NPU`
  - [status/npu_tpu_accelerator_status.md](status/npu_tpu_accelerator_status.md)
  - [design/npu_tpu_accelerator_direction_design.md](design/npu_tpu_accelerator_direction_design.md)
  - [plan/history_plan.md#mainline-wave4-ai-accelerator-slices-plan](plan/history_plan.md#mainline-wave4-ai-accelerator-slices-plan)
  - [plan/history_plan.md#npu-tpu-accelerator-wave3-plan](plan/history_plan.md#npu-tpu-accelerator-wave3-plan)
- `Phase 4 准备`
  - [design/phase4_preparation_design.md](design/phase4_preparation_design.md)
  - [design/platform_mmio_contract.md](design/platform_mmio_contract.md)
  - [plan/history_plan.md#phase4-prep2-memory-observation-shadow-cache-plan](plan/history_plan.md#phase4-prep2-memory-observation-shadow-cache-plan)
- `Wave 5 / cache / memory-system`
  - [design/wave5_cache_memory_system_design.md](design/wave5_cache_memory_system_design.md)
  - [plan/history_plan.md#mainline-wave5-closeout-wave6-readiness-plan](plan/history_plan.md#mainline-wave5-closeout-wave6-readiness-plan)
  - [plan/history_plan.md#mainline-wave5-cache-memory-system-slice-f-l1d-lifecycle-guardrail-plan](plan/history_plan.md#mainline-wave5-cache-memory-system-slice-f-l1d-lifecycle-guardrail-plan)
  - [plan/history_plan.md#mainline-wave5-cache-memory-system-slice-e-l1d-frontend-observation-plan](plan/history_plan.md#mainline-wave5-cache-memory-system-slice-e-l1d-frontend-observation-plan)
  - [plan/history_plan.md#mainline-wave5-cache-memory-system-slice-d-l1d-hardening-plan](plan/history_plan.md#mainline-wave5-cache-memory-system-slice-d-l1d-hardening-plan)
  - [plan/history_plan.md#mainline-wave5-cache-memory-system-slice-c-l1d-observation-guardrail-plan](plan/history_plan.md#mainline-wave5-cache-memory-system-slice-c-l1d-observation-guardrail-plan)
  - [plan/history_plan.md#mainline-wave5-cache-memory-system-slice-b-minimal-l1d-plan](plan/history_plan.md#mainline-wave5-cache-memory-system-slice-b-minimal-l1d-plan)
  - [plan/history_plan.md#mainline-wave5-cache-memory-system-slice-a-signal-contract-plan](plan/history_plan.md#mainline-wave5-cache-memory-system-slice-a-signal-contract-plan)
- `Wave 6 / JIT / DBT`
  - [design/wave6_jit_dbt_readiness_design.md](design/wave6_jit_dbt_readiness_design.md)
  - [plan/history_plan.md#mainline-wave6-jit-dbt-fallback-equivalence-slice-f-plan](plan/history_plan.md#mainline-wave6-jit-dbt-fallback-equivalence-slice-f-plan)
  - [plan/history_plan.md#mainline-wave6-jit-dbt-translation-plan-slice-e-plan](plan/history_plan.md#mainline-wave6-jit-dbt-translation-plan-slice-e-plan)
  - [plan/history_plan.md#mainline-wave6-jit-dbt-prototype-guardrail-slice-d-plan](plan/history_plan.md#mainline-wave6-jit-dbt-prototype-guardrail-slice-d-plan)
  - [plan/history_plan.md#mainline-wave6-jit-dbt-observation-and-slice-c-plan](plan/history_plan.md#mainline-wave6-jit-dbt-observation-and-slice-c-plan)
  - [plan/history_plan.md#mainline-wave6-jit-dbt-translation-contract-slice-b-plan](plan/history_plan.md#mainline-wave6-jit-dbt-translation-contract-slice-b-plan)
  - [plan/history_plan.md#mainline-wave6-jit-dbt-hot-path-evidence-slice-a-plan](plan/history_plan.md#mainline-wave6-jit-dbt-hot-path-evidence-slice-a-plan)
- `kernel_alpha`
  - [status/kernel_alpha_status.md](status/kernel_alpha_status.md)
  - [design/platform_mmio_contract.md](design/platform_mmio_contract.md)
- `debug / frontend`
  - [design/debug_frontend_integration.md](design/debug_frontend_integration.md)
  - [design/minimal_interactive_os_design.md](design/minimal_interactive_os_design.md)
  - [showcase/README.md](showcase/README.md)
- `Spike 外部差分验证`
  - [design/spike_differential_validation_design.md](design/spike_differential_validation_design.md)
  - [plan/history_plan.md#spike-external-differential-validation-plan](plan/history_plan.md#spike-external-differential-validation-plan)

## 目录说明

- [background](background)
  项目背景与原始目标。
- [status](status)
  当前状态、风险、优先级和下一步。
- [design](design)
  长期有效的设计边界、模块资料和阶段合同。
- [plan](plan)
  活跃计划、模板和已完成归档。
- [showcase](showcase)
  项目展示材料、HTML 预览页和展示截图。

## 维护约束

- `index.md` 只做导航，不重复维护状态正文。
- 仓库级实时主线状态只保留 [status/mainline_status.md](status/mainline_status.md)。
- 新增、重命名或删除正式文档后，必须同步更新本文件。
