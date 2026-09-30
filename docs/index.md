# 文档入口

先读 [项目概览](../README.md) 与 [当前状态](current.md)，再按任务定向查阅。
本项目自 2026-09-30 起采用 NexusKit；下列旧资料保留用于渐进迁移。

## NexusKit 入口

- [current.md](current.md)：当前工作范围、证据、阻断和下一步。
- [AGENTS.md](AGENTS.md)：文档职责与迁移规则。
- [GitHub Issues](https://github.com/steven123397/my_visual_CPU/issues)：待办承载。
- `plans/`、`reviews/`、`ideation/`：首个对应过程产物产生时建立。
- `solutions/`：首条长期经验或架构决策沉淀时建立。
- 根 `CONCEPTS.md`：首个合格领域词条产生时建立。

## 使用与展示

- [部署说明](../deploy/README.md)
- [展示材料总入口](showcase/README.md)
- [Course OS 展示](showcase/course-os/README.md)
- [模拟器展示](showcase/simulator/README.md)
- [AI demo 操作指南](showcase/simulator/post_wave7_ai_demo_v1_guide.md)

## 待迁移技术资料

以下链接是旧体系资料导航，不表示恢复旧分线或沿用历史优先级。
旧状态中的“当前”和“下一步”须对照实现及本轮证据重新核实。

| 专题 | 技术边界 | 历史状态 / 计划输入 |
|---|---|---|
| 验证与平台 | [回归标准](design/regression_completion_criteria.md)、[MMIO 契约](design/platform_mmio_contract.md)、[Spike 差分](design/spike_differential_validation_design.md) | [审查整改记录](status/code_reself_status.md) |
| 执行与缓存 | [OoO 模型](design/phase3_ooo_execution_model_design.md)、[推测合同](design/pipeline_speculation_contracts.md)、[L1D](design/wave5_cache_memory_system_design.md)、[JIT 研究资产](design/wave6_jit_dbt_readiness_design.md) | [模拟器演进记录](status/simulator_evolution_status.md) |
| 观测与 Lab | [观测 schema](design/simulator_evolution_observability_schema_design.md)、[Lab 工作台](design/post_wave7_frontend_lab_product_design.md)、[调试接线](design/debug_frontend_integration.md) | [旧主线快照](status/mainline_status.md) |
| Course OS | [课程基线](design/course_os_kernel_alpha_course_os_baseline_design.md)、[Linux compat](design/course_os_kernel_alpha_linux_compat_plus_design.md)、[调度时序](design/course_os_scheduler_timing_contract.md) | [guest 状态记录](status/kernel_alpha_status.md) |
| Linux 发行版 | [平台边界](design/post_wave7_linux_distribution_platform_design.md) | [发行版记录](status/linux_distribution_platform_status.md)、[长期计划](plan/post_wave7_linux_distribution_platform_longterm_plan.md) |
| AI / NPU | [用户任务与性能模型](design/post_wave7_ai_user_tasks_npu_performance_design.md)、[Linux-facing 合同](design/ai_accelerator_linux_facing_contract_design.md) | [AI 记录](status/npu_tpu_accelerator_status.md) |
| 远端部署 | [部署设计](design/wave7_remote_cloud_dev_environment_design.md) | [旧部署计划](plan/wave7_remote_cloud_dev_environment_plan.md) |

其他旧设计文件继续保留在 `design/`，按具体任务读取。
旧 [P1](plan/project_evolution_priority_p1_plan.md)、[P2](plan/project_evolution_priority_p2_plan.md)、
[P3](plan/project_evolution_priority_p3_plan.md)、[演进规划](../PROJECT_EVOLUTION_PLAN.md)
和 [方向评审](../PROJECT_DIRECTION_REVIEW.md) 只作为重新确定任务的输入。
已完成过程见 [历史计划](plan/history_plan.md) 或 Git 历史，不再追加历史汇总。
