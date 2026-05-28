# kernel_alpha 状态

## 文档定位

本文档只记录 `kernel_alpha` 的当前定位、历史 Phase 1 基线、当前仍有效的限制和下一步。

它不再维护逐条执行流水账；更细的实现过程统一回写到 [../plan/history_plan.md](../plan/history_plan.md)。

## 关联文档

- 相关设计：
  - [../design/course_os_kernel_alpha_stage1_design.md](../design/course_os_kernel_alpha_stage1_design.md)
  - [../design/regression_completion_criteria.md](../design/regression_completion_criteria.md)
  - [../design/platform_mmio_contract.md](../design/platform_mmio_contract.md)
- 相关状态：
  - [mainline_status.md](mainline_status.md)
- 当前活跃计划：
  - [../plan/course_os_kernel_alpha_stage1_plan.md](../plan/course_os_kernel_alpha_stage1_plan.md)
- 已完成计划归档：
  - [../plan/history_plan.md#kernel-alpha-storage-error-contract-plan](../plan/history_plan.md#kernel-alpha-storage-error-contract-plan)

## 当前状态

`kernel_alpha` 当前已经从 Phase 1 bring-up demo 重新定位为《操作系统课程设计》第一阶段主线入口。
第一阶段目标以 [../design/course_os_kernel_alpha_stage1_design.md](../design/course_os_kernel_alpha_stage1_design.md)
为准：只承诺进程、内存、文件系统 3 个模块的 9 个功能点，并提供只读 `/proc` 指标证据面。

旧 Phase 1 `KMVPETDS` 正向输出不再作为后续课程 OS 当前行为承诺；它降级为历史 bring-up 基线：

- `K`：进入独立 kernel 入口
- `M`：memory / PMM 初始化完成
- `V`：自建 Sv39 内核页表启用并稳定工作
- `P`：PLIC 最小 supervisor 初始化完成
- `E`：第一次 supervisor external interrupt 到达
- `T`：第一次 timer interrupt 到达
- `D`：storage readiness / metadata probe 完成
- `S`：`LBA 0` 读取和签名校验完成

与旧正向基线一起形成的 9 条负向 demo 继续保留为基础设施 guardrail 和历史回归：

- `guest_kernel_alpha_fault_demo = KMVX`
- `guest_kernel_alpha_plic_not_ready_demo = KMVPX`
- `guest_kernel_alpha_timer_not_ready_demo = KMVPETX`
- `guest_kernel_alpha_storage_no_media_demo = KMVNX`
- `guest_kernel_alpha_storage_not_ready_demo = KMVRX`
- `guest_kernel_alpha_storage_bad_magic_demo = KMVGX`
- `guest_kernel_alpha_storage_bad_block_count_demo = KMVBX`
- `guest_kernel_alpha_storage_lba_range_demo = KMVLX`
- `guest_kernel_alpha_storage_bad_command_demo = KMVCX`

在课程 OS 第一阶段实现切换之前，现有 `kernel_alpha` guest 回归仍用于防止基础 bring-up、
storage / interrupt / fault 合同倒退；进入实现切换时，`kernel_alpha_demo` 的正向期望输出、
测试名和状态文档必须同步更新，不能继续强绑固定 `KMVPETDS`。

## 关键历史节点

- `2026-05-29`
  - `kernel_alpha` 课程 OS 第一阶段方案定稿，当前定位改为“课程 OS 主线入口 + Phase 1 历史基线”。
  - 第一阶段范围冻结为：FCFS / RR / CFS-lite，Demand Paging / Clock / `kmalloc` / `kfree`，文件 / 目录 CRUD / `seek` / B 树目录索引，以及只读 `/proc` 指标面。
- `2026-03-31`
  - alpha 共享 bring-up helper 继续下沉到 `kernel_runtime`、`kernel_bringup`、`storage_contract` 和 `interrupt_contract`，`kernel_alpha` 入口进一步退回到场景组合层。
- `2026-03-25` 到 `2026-03-30`
  - 从首个独立 `kernel_alpha_demo` alpha bring-up 开始，逐步扩展到当前 10 条核心 guest 基线，并把 storage / interrupt / common bring-up 合同从入口收口到共享 guest 基础设施层。

## 当前仍然有效的风险 / 限制

- 当前 `kernel_alpha` 代码仍停留在 Phase 1 bring-up 输出，课程 OS 第一阶段尚未实现。
- 课程 OS 第一阶段只覆盖 3 个模块、9 个功能点；AI/NPU、JIT、Pipeline-aware scheduling、前端面板、微内核和安全隔离不进入第一阶段。
- 旧 1 条正向 + 9 条负向 guest 回归仍是基础设施 guardrail，但实现切换时必须避免把旧 marker 输出误当成新的课程 OS 验收。
- `SimpleStorage` 仍然是单块、同步、无 completion interrupt、无宿主持久化回写的最小模型；第一阶段文件系统应先建立课程演示所需的最小语义，不提前承诺完整磁盘一致性。
- `/proc` 第一阶段只读为主，不作为调度、内存或文件系统的写控制接口。

## 下一步

1. 按 [../plan/course_os_kernel_alpha_stage1_plan.md](../plan/course_os_kernel_alpha_stage1_plan.md) 执行课程 OS 第一阶段。
2. 在第一刀实现前先新增最窄 guest smoke，固定调度、内存、文件系统和 `/proc` 的可观察验收面。
3. 解冻 `kernel_alpha_demo` 正向行为时，同步更新测试期望、状态文档和必要的 README / index 引用。
4. 保留旧 Phase 1 负向 demo 作为基础设施 guardrail；除非真实 bug 或课程 OS 迁移需要，不继续扩旧 bring-up marker 面。

## 验证基线

- `cd myCPU && make test`
- `cd myCPU && make test-guest-kernel_alpha_demo`
- `cd myCPU && make test-guest-kernel_alpha_fault_demo`
- `cd myCPU && make test-guest-kernel_alpha_storage_no_media_demo`
- `cd myCPU && make test-guest-kernel_alpha_storage_not_ready_demo`
- `cd myCPU && make test-guest-kernel_alpha_storage_bad_magic_demo`
- `cd myCPU && make test-guest-kernel_alpha_storage_bad_block_count_demo`
- `cd myCPU && make test-guest-kernel_alpha_storage_lba_range_demo`
- `cd myCPU && make test-guest-kernel_alpha_storage_bad_command_demo`
- `cd myCPU && make test-guest-kernel_alpha_plic_not_ready_demo`
- `cd myCPU && make test-guest-kernel_alpha_timer_not_ready_demo`
- 第一阶段实现触及 shared guest runtime 时，额外关注：
  - `cd myCPU && make test-unit-kernel_runtime`
  - `cd myCPU && make test-unit-vm_address_space`
  - `cd myCPU && make test-unit-vm_process`
  - `cd myCPU && make test-unit-vm_fault`
  - `cd myCPU && make test-unit-user_task`
