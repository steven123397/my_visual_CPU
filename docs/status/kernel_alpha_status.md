# kernel_alpha 状态

## 文档定位

本文档只记录独立 `kernel_alpha` bring-up 路线的当前基线、少量关键历史节点、当前仍有效的限制和下一步。

它不再维护逐条执行流水账；更细的实现过程统一回写到 [plan/history_plan.md](../plan/history_plan.md)。

## 关联文档

- 相关设计：
  - [design/regression_completion_criteria.md](../design/regression_completion_criteria.md)
  - [design/kernel_alpha_storage_error_contract.md](../design/kernel_alpha_storage_error_contract.md)
- 相关状态：
  - [mainline_status.md](mainline_status.md)
- 已完成计划归档：
  - [plan/history_plan.md#kernel-alpha-storage-error-contract-plan](../plan/history_plan.md#kernel-alpha-storage-error-contract-plan)

## 当前状态

当前 `kernel_alpha` 已经达到 Phase 1 核心 bring-up 完成态，正向输出固定为 `KMVPETDS`：

- `K`：进入独立 kernel 入口
- `M`：memory / PMM 初始化完成
- `V`：自建 Sv39 内核页表启用并稳定工作
- `P`：PLIC 最小 supervisor 初始化完成
- `E`：第一次 supervisor external interrupt 到达
- `T`：第一次 timer interrupt 到达
- `D`：storage readiness / metadata probe 完成
- `S`：`LBA 0` 读取和签名校验完成

当前与它一起稳定维护的负向基线如下：

- `guest_kernel_alpha_fault_demo = KMVX`
- `guest_kernel_alpha_plic_not_ready_demo = KMVPX`
- `guest_kernel_alpha_timer_not_ready_demo = KMVPETX`
- `guest_kernel_alpha_storage_no_media_demo = KMVNX`
- `guest_kernel_alpha_storage_not_ready_demo = KMVRX`
- `guest_kernel_alpha_storage_bad_magic_demo = KMVGX`
- `guest_kernel_alpha_storage_bad_block_count_demo = KMVBX`
- `guest_kernel_alpha_storage_lba_range_demo = KMVLX`
- `guest_kernel_alpha_storage_bad_command_demo = KMVCX`

当前这 1 条正向 + 9 条负向回归与 `guest_supervisor_demo` 一起，构成当前 Phase 1 核心 guest 门禁的一部分。

## 关键历史节点（按时间倒序）

- `2026-03-31`
  - alpha 共享 bring-up phase helper 继续下沉到 `kernel_runtime`、`kernel_bringup`、`storage_contract` 和 `interrupt_contract`，`kernel_alpha` 入口进一步退回到场景组合层。
- `2026-03-25` 到 `2026-03-30`
  - 从首个独立 `kernel_alpha_demo` alpha bring-up 开始，逐步扩展到当前 10 条核心 guest 基线，并把 storage / interrupt / common bring-up 合同从入口收口到共享 guest 基础设施层。

## 当前仍然有效的风险 / 限制

- `kernel_alpha` 虽已达到 Phase 1 核心完成态，但更多 runtime / object 组织仍属于 post-Phase1 hardening，而不是完整内核系统。
- 当前 device readiness / error 合同已经够用，但仍是最小覆盖；后续只应在真实 bug 或新里程碑需要时继续扩。
- `SimpleStorage` 仍然是单块、同步、无 completion interrupt、无宿主持久化回写的最小模型；`kernel_alpha` 当前基线默认建立在这条克制边界上。

## 下一步

1. 继续把这 10 条 `kernel_alpha` guest 回归维持为稳定基线，不让输出和合同回退。
2. 继续以 bug-driven hardening 的方式推进 guest runtime、`kernel_bringup`、storage / interrupt 共享 helper 的边界收口。
3. 如果后续要扩 `kernel_alpha`，优先围绕明确的新 bring-up 里程碑或已出现的 bug，而不是继续泛化功能面。

## 验证基线

- `cd myCPU && make test`
- `cd myCPU && make test-pipeline`
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
