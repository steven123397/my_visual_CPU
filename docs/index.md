# 文档入口

项目是 RISC-V 系统模拟器，不再以课程项目定性。
当前工作看 [current.md](current.md)，待办由
[GitHub Issues](https://github.com/steven123397/my_visual_CPU/issues) 承载。
开发流程遵循 NexusKit，规则见根 [AGENTS.md](../AGENTS.md)。

## 当前文档结构

- `current.md`：会话现场。
- `design/`：28 份待分析的旧设计资料，本轮保留正文，修正历史引用。
- `plans/`、`reviews/`、`ideation/`、`solutions/`：有对应新产物时再建立。
- 根 `CONCEPTS.md`：有合格领域术语时再建立。
- 使用说明：[README](../README.md)、[前端](../frontend/README.md)、[部署](../deploy/README.md)。

旧 showcase、background、status、plan 及根方向文档已移除，历史由 Git 保留。
网页使用的 4 张截图归属 `frontend/app/assets/`。

## Design 初步分析

以下是基于文档内容的处置建议，不是已确认的现行架构或开发优先级。
本轮未逐项复验实现。旧维护规则失效，保留资料不会恢复旧任务。

### 优先核对的技术契约：8 份

具有明确接口、状态、边界或差分语义，建议核对实现后提炼；剥离 Wave / Phase 历史。

- [平台 MMIO](design/platform_mmio_contract.md)：地址、寄存器、中断与 guest 驱动约束。
- [Pipeline 推测与提交](design/pipeline_speculation_contracts.md)：副作用、异常、rollback 和 commit boundary。
- [Pipeline 执行模型](design/phase3_ooo_execution_model_design.md)：rename / ROB / LSQ 与共享语义职责。
- [L1D 与 memory system](design/wave5_cache_memory_system_design.md)：cacheability、bypass、fault 和生命周期。
- [V-lite](design/vector_ml_workload_direction_design.md)：编码、状态、访存和 vector commit。
- [Spike 差分](design/spike_differential_validation_design.md)：外部 oracle 范围与 final-state 比较。
- [观测 schema](design/simulator_evolution_observability_schema_design.md)：event wrapper 与 producer / consumer 边界。
- [Debug / frontend](design/debug_frontend_integration.md)：会话、terminal、snapshot 与受控写能力。

### 按运行模块合并提炼：10 份

已有实现资产仍存在；课程叙事与验收阶段应移除，技术边界按模块收敛。

- [Guest 基线](design/course_os_kernel_alpha_course_os_baseline_design.md)
- [Linux compat](design/course_os_kernel_alpha_linux_compat_plus_design.md)
- [在线调度](design/course_os_preemptive_scheduler_design.md)
- [调度 timing](design/course_os_scheduler_timing_contract.md)
- [UART 输入](design/course_os_uart_interrupt_input_design.md)
- [ELF 加载](design/course_os_real_user_elf_design.md)
- [Monitor](design/minimal_interactive_os_design.md)
- [OSComp 外部验证](design/course_os_oscomp_external_validation_design.md)
- [Lab 工作台](design/post_wave7_frontend_lab_product_design.md)
- [远端环境](design/wave7_remote_cloud_dev_environment_design.md)

重点核对：调度 timing 的旧“不实现在线调度”限定与后续在线调度设计之间的关系；
远端环境实际部署状况本轮未验证。

### 已实现合同与未来方向混合：5 份

先区分已有接口、opt-in 资产和未实现提案，不直接沿用整篇规划。

- [Linux 发行版](design/post_wave7_linux_distribution_platform_design.md)
- [AI 设备方向](design/npu_tpu_accelerator_direction_design.md)
- [AI 用户任务与 timing](design/post_wave7_ai_user_tasks_npu_performance_design.md)
- [AI Linux-facing](design/ai_accelerator_linux_facing_contract_design.md)：host facade 与未来 driver / ioctl 必须分开。
- [JIT / DBT](design/wave6_jit_dbt_readiness_design.md)：保留研究资产边界，不据此重启正式 backend 路线。

### 淘汰或局部提炼候选：5 份

本轮尚未删除；需要先判断是否含有其他文件未覆盖的技术价值。

- [课程缺口收口](design/course_os_gap_closure_boundary_design.md)：课程验收失效，但目录枚举等窄接口合同可能值得提炼。
- [Phase 4 准备](design/phase4_preparation_design.md)：memory region 合同有价值，未做真实 cache 的阶段叙事已被后续 L1D 设计超越。
- [Wave 7 产品展示](design/wave7_productization_and_showcase_design.md)：文档自身已声明历史语境，由 Lab 设计接替。
- [回归收口标准](design/regression_completion_criteria.md)：风险与合同驱动验证的经验可提炼，旧阶段门禁和教学定位失效。
- [旧模板](design/template.md)：属于旧体系模板，不是技术设计，新产物不使用。

后续建议从平台 MMIO 或 pipeline 提交边界选一个专题，逐项对照实现与门禁后确定去向。
