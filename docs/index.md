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
6. [design/phase3_ooo_execution_model_design.md](design/phase3_ooo_execution_model_design.md)
   当前 `pipeline / Phase 3` 边界和取舍判断。
7. [design/pipeline_speculation_contracts.md](design/pipeline_speculation_contracts.md)
   `pipeline` 投机执行和 side effect 可见性合同。
8. [design/phase4_preparation_design.md](design/phase4_preparation_design.md)
   `Phase 4` 当前准备性入口设计。
9. [design/wave5_cache_memory_system_design.md](design/wave5_cache_memory_system_design.md)
    主线 `Wave 5 / cache / memory-system` 当前设计入口。
10. [design/wave6_jit_dbt_readiness_design.md](design/wave6_jit_dbt_readiness_design.md)
    主线 `Wave 6 / JIT / DBT` 当前 readiness、证据链、translation contract、原型边界和
    `DBT translator + IR v0 dry-run` 完成态设计入口。
11. [design/wave7_productization_and_showcase_design.md](design/wave7_productization_and_showcase_design.md)
    主线 `Wave 7 / 产品化展示与在线控制台` 的历史设计入口，保留首页壳层与 demo workspace v1 语境。
12. [design/wave7_remote_cloud_dev_environment_design.md](design/wave7_remote_cloud_dev_environment_design.md)
    主线 `Wave 7` 在另一台云服务器上承接完整开发/验证环境的设计入口。
13. [design/post_wave7_linux_distribution_platform_design.md](design/post_wave7_linux_distribution_platform_design.md)
    `Wave 7` 阶段性收口后的 `标准 Linux 发行版平台` 新主线设计入口。
14. [status/linux_distribution_platform_status.md](status/linux_distribution_platform_status.md)
    `标准 Linux 发行版平台` 新主线的当前状态、风险和下一步。
15. [plan/history_plan.md#post-wave7-linux-distribution-platform-plan](plan/history_plan.md#post-wave7-linux-distribution-platform-plan)
    `标准 Linux 发行版平台` 第一阶段完成计划归档。
16. [plan/post_wave7_linux_distribution_platform_longterm_plan.md](plan/post_wave7_linux_distribution_platform_longterm_plan.md)
    `标准 Linux 发行版平台` 五阶段长线活跃计划。
17. [design/post_wave7_ai_user_tasks_npu_performance_design.md](design/post_wave7_ai_user_tasks_npu_performance_design.md)
    `Wave 7` 阶段性收口后的 `用户自定义 AI 任务 + NPU 性能模型` 新主线设计入口。
18. [plan/post_wave7_ai_user_tasks_npu_performance_plan.md](plan/post_wave7_ai_user_tasks_npu_performance_plan.md)
    `用户自定义 AI 任务 + NPU 性能模型` 新主线的当前活跃计划。
19. [design/post_wave7_frontend_lab_product_design.md](design/post_wave7_frontend_lab_product_design.md)
    `Post-Wave 7` 前端从 `demo workspace v1` 重构为 `Lab workbench` 的当前设计入口。
20. [plan/history_plan.md#post-wave7-frontend-lab-product-plan](plan/history_plan.md#post-wave7-frontend-lab-product-plan)
    `Post-Wave 7` 前端 `Lab workbench` 第一轮重构完成归档。
21. [plan/history_plan.md#post-wave7-frontend-lab-completion-plan](plan/history_plan.md#post-wave7-frontend-lab-completion-plan)
    `Post-Wave 7` 前端 `Lab workbench` 第二轮补完完成归档，覆盖真实 Linux Image、
    Scenario controls、Linux topic/live 连通和 JIT runtime evidence topic。
22. [design/vector_ml_workload_direction_design.md](design/vector_ml_workload_direction_design.md)
    向量扩展与 ML workload 的统一设计边界。
23. [design/npu_tpu_accelerator_direction_design.md](design/npu_tpu_accelerator_direction_design.md)
    独立 `MMIO NPU / TPU-like` AI accelerator 方向设计。
24. [design/debug_frontend_integration.md](design/debug_frontend_integration.md)
    `debug_session / protocol + frontend` 设计边界。
25. [design/spike_differential_validation_design.md](design/spike_differential_validation_design.md)
    Spike 外部差分验证边界。
26. [plan/wave7_remote_cloud_dev_environment_plan.md](plan/wave7_remote_cloud_dev_environment_plan.md)
    主线 `Wave 7` 远端云服务器开发/验证环境的当前活跃计划。
27. [plan/history_plan.md](plan/history_plan.md)
    已完成计划的统一归档入口。

## 专题入口

- `主线状态`
  - [status/mainline_status.md](status/mainline_status.md)
  - [design/wave6_jit_dbt_readiness_design.md](design/wave6_jit_dbt_readiness_design.md)
  - [design/wave7_productization_and_showcase_design.md](design/wave7_productization_and_showcase_design.md)
- `Post-Wave 7 Linux`
  - [status/linux_distribution_platform_status.md](status/linux_distribution_platform_status.md)
  - [design/post_wave7_linux_distribution_platform_design.md](design/post_wave7_linux_distribution_platform_design.md)
  - [plan/post_wave7_linux_distribution_platform_longterm_plan.md](plan/post_wave7_linux_distribution_platform_longterm_plan.md)
  - [plan/history_plan.md#post-wave7-linux-distribution-platform-plan](plan/history_plan.md#post-wave7-linux-distribution-platform-plan)
- `pipeline`
  - [design/phase3_ooo_execution_model_design.md](design/phase3_ooo_execution_model_design.md)
  - [design/pipeline_speculation_contracts.md](design/pipeline_speculation_contracts.md)
- `向量 / ML workload`
  - [design/vector_ml_workload_direction_design.md](design/vector_ml_workload_direction_design.md)
  - [plan/history_plan.md#vector-v4-plan](plan/history_plan.md#vector-v4-plan)
- `AI accelerator / NPU`
  - [status/npu_tpu_accelerator_status.md](status/npu_tpu_accelerator_status.md)
  - [design/post_wave7_ai_user_tasks_npu_performance_design.md](design/post_wave7_ai_user_tasks_npu_performance_design.md)
  - [plan/post_wave7_ai_user_tasks_npu_performance_plan.md](plan/post_wave7_ai_user_tasks_npu_performance_plan.md)
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
  - [plan/history_plan.md#mainline-wave6-jit-execution-layer-plan](plan/history_plan.md#mainline-wave6-jit-execution-layer-plan)
  - [plan/history_plan.md#mainline-wave6-dbt-translator-ir-v0-plan](plan/history_plan.md#mainline-wave6-dbt-translator-ir-v0-plan)
  - [plan/history_plan.md](plan/history_plan.md)
- `Wave 7 / 产品化展示`
  - [design/wave7_productization_and_showcase_design.md](design/wave7_productization_and_showcase_design.md)
  - [design/post_wave7_frontend_lab_product_design.md](design/post_wave7_frontend_lab_product_design.md)
  - [plan/history_plan.md#post-wave7-frontend-lab-product-plan](plan/history_plan.md#post-wave7-frontend-lab-product-plan)
  - [plan/history_plan.md#post-wave7-frontend-lab-completion-plan](plan/history_plan.md#post-wave7-frontend-lab-completion-plan)
  - [design/wave7_remote_cloud_dev_environment_design.md](design/wave7_remote_cloud_dev_environment_design.md)
  - [plan/wave7_remote_cloud_dev_environment_plan.md](plan/wave7_remote_cloud_dev_environment_plan.md)
  - [plan/history_plan.md#mainline-wave7-product-website-shell-plan](plan/history_plan.md#mainline-wave7-product-website-shell-plan)
- `kernel_alpha`
  - [status/kernel_alpha_status.md](status/kernel_alpha_status.md)
  - [design/platform_mmio_contract.md](design/platform_mmio_contract.md)
- `debug / frontend`
  - [design/wave7_productization_and_showcase_design.md](design/wave7_productization_and_showcase_design.md)
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
