# kernel_alpha 状态

## 文档定位

本文档用于记录独立 `kernel_alpha` bring-up 路线的当前状态、仍然有效的限制、少量关键历史节点和下一步工作。

它已经从早期执行计划收口成状态跟踪文档，不再保留逐项 checklist 或中间执行流水账。

## 关联文档

- 相关设计：
  - [design/regression_completion_criteria.md](../design/regression_completion_criteria.md)
  - [design/kernel_alpha_storage_error_contract.md](../design/kernel_alpha_storage_error_contract.md)
- 已完成计划归档：
  - [plan/history_plan.md#kernel-alpha-storage-error-contract-plan](../plan/history_plan.md#kernel-alpha-storage-error-contract-plan)

## 目标 / 主题

新增一条独立于 `guest_supervisor_demo` 的 `kernel_alpha` bring-up 路径，完成第一次真正的小 kernel alpha 启动，并把它纳入稳定回归。

## 架构约束

- 保持现有 `supervisor_demo_smoke` 不继续膨胀。
- 在 `guest/` 下维护独立入口 ELF。
- 复用共享的 guest kernel 基础设施。
- 正向 bring-up 只验证第一次真正的小 kernel alpha 所需的最小路径，不一次性做完整 OS。

## 当前状态

当前正向 bring-up 标记定义为串行输出 `KMVPETDS`：

- `K`：进入独立 kernel 入口
- `M`：memory / PMM 初始化完成
- `V`：自建页表启用后，内核镜像 / early heap / managed RAM 显式映射已可工作
- `P`：PLIC 在 VM 下完成最小 supervisor 初始化
- `E`：UART THRE -> PLIC -> supervisor external interrupt 路径已经到达
- `T`：第一次 timer interrupt 已经到达
- `D`：storage metadata / readiness probe 已完成
- `S`：storage block `LBA 0` 读取和签名校验已经完成

当前正向里程碑已经落地，覆盖：

- 独立 kernel ELF / entry
- boot marker
- early allocator / PMM
- 自建 Sv39 内核页表
- 内核镜像 / early heap / managed RAM 显式映射
- UART / CLINT / PLIC / storage 的 MMIO fault-range lazy map
- 一次 supervisor external interrupt
- 一次 supervisor timer interrupt
- 一次 storage readiness probe
- 一次 block `LBA 0` 读取
- 一组最小 storage 错误合同负向回归

当前负向回归包括：

在首个 alpha 里程碑完成后，已经补上九条独立负向回归：

### `guest_kernel_alpha_fault_demo`

当前用于验证：

- VM 已开启
- UART 仍可输出
- CLINT 未映射时的内核 MMIO 访问会进入 fault / panic 路径

当前输出：

- `KMVX`

### `guest_kernel_alpha_plic_not_ready_demo`

当前用于验证：

- VM 已开启
- UART / CLINT / PLIC MMIO 可达
- PLIC 已映射但未完成 supervisor 初始化时，UART THRE 不会变成 supervisor external interrupt
- bring-up 会在 deadline 超时后进入 fault / panic 路径，而不是误以为 device readiness 已满足

当前输出：

- `KMVPX`

### `guest_kernel_alpha_timer_not_ready_demo`

当前用于验证：

- VM 已开启
- UART / CLINT / PLIC MMIO 可达
- PLIC 已完成 supervisor 初始化，第一次 external interrupt 已成功到达
- 未安排第一次 timer delivery 时，bring-up 会在 deadline 超时后进入 fault / panic 路径，而不是误以为 timer readiness 已满足

当前输出：

- `KMVPETX`

### `guest_kernel_alpha_storage_no_media_demo`

当前用于验证：

- VM 已开启
- storage MMIO 可达
- metadata / status 可观察
- 未附加镜像时 block read 返回 `NO_MEDIA`

当前输出：

- `KMVNX`

### `guest_kernel_alpha_storage_not_ready_demo`

当前用于验证：

- VM 已开启
- storage MMIO 可达，metadata / `ATTACHED` / capacity 可观察
- attached 但 `READY` 缺失时，`storage_probe()` 不会误判 readiness 已满足
- 首次 block read 返回 `NOT_READY`
- `COMMAND = NONE` clear-error 后不会把设备误恢复成 ready

当前输出：

- `KMVRX`

### `guest_kernel_alpha_storage_bad_magic_demo`

当前用于验证：

- VM 已开启
- storage MMIO 可达，attached / ready / capacity 成功态仍可观察
- `MAGIC` 元数据损坏时，`storage_read_info()` 与 `storage_probe()` 不会误判 probe 已成功
- block read 数据路径仍可工作，证明失败点在 probe metadata 合同，而不是 block read 本身

当前输出：

- `KMVGX`

### `guest_kernel_alpha_storage_bad_block_count_demo`

当前用于验证：

- VM 已开启
- storage MMIO 可达且 attached / ready 成功态可观察
- `BLOCK_COUNT != 1` 的 block read 返回 `BAD_BLOCK_COUNT`
- `COMMAND = NONE` 会清掉粘滞 error 状态

当前输出：

- `KMVBX`

### `guest_kernel_alpha_storage_lba_range_demo`

当前用于验证：

- VM 已开启
- storage MMIO 可达且 attached / ready 成功态可观察
- `LBA == capacity_blocks` 的 block read 返回 `LBA_RANGE`
- `COMMAND = NONE` 会清掉粘滞 error 状态

当前输出：

- `KMVLX`

### `guest_kernel_alpha_storage_bad_command_demo`

当前用于验证：

- VM 已开启
- storage MMIO 可达且 attached / ready 成功态可观察
- 非法 `COMMAND` 值会返回 `BAD_COMMAND`
- `COMMAND = NONE` 会清掉粘滞 error 状态

当前输出：

- `KMVCX`

当前冻结稳定基线为：

- tag：`phase1-stable`
- commit：`283aee6`
- 含义：`Phase 1` 核心 bring-up 目标已达成；后续 `kernel_alpha` / guest runtime 工作默认视为 post-Phase1 hardening
- 当前这十条 `kernel_alpha` demo 与 `guest_supervisor_demo` 一起，构成 [design/regression_completion_criteria.md](../design/regression_completion_criteria.md) 定义的 Phase 1 核心 guest 门禁
- 当前本地 `frontend` 调试测试清单已覆盖：
  - `guest_kernel_alpha_demo`
  - `guest_kernel_alpha_fault_demo`
  - `guest_kernel_alpha_storage_no_media_demo`
  - `guest_kernel_alpha_storage_not_ready_demo`
  - `guest_kernel_alpha_storage_bad_magic_demo`
  - `guest_kernel_alpha_storage_bad_block_count_demo`
  - `guest_kernel_alpha_storage_lba_range_demo`
  - `guest_kernel_alpha_storage_bad_command_demo`
  - `guest_kernel_alpha_plic_not_ready_demo`
  - `guest_kernel_alpha_timer_not_ready_demo`

## 关键历史节点

- `2026-03-25` 已完成首个独立 `kernel_alpha_demo` alpha bring-up。
- 在同一轮中期阶段，独立 `kernel_alpha` 曾先形成“1 条正向 + 6 条负向”的中间基线（`KMVPETDS`、`KMVX`、`KMVPX`、`KMVNX`、`KMVBX`、`KMVLX`、`KMVCX`），随后继续扩展到当前十条核心 guest 门禁。
- 随后补上了：
  - `guest_kernel_alpha_fault_demo`
  - `guest_kernel_alpha_storage_no_media_demo`
- 在同一轮 storage 错误合同扩展中，又在保持 `SimpleStorage` 设备语义不变的前提下，补上 guest 侧 `storage status / error / clear-error / custom block-count read` 最小 helper，并接通以下独立负向回归：
  - `guest_kernel_alpha_storage_bad_block_count_demo`
  - `guest_kernel_alpha_storage_lba_range_demo`
  - `guest_kernel_alpha_storage_bad_command_demo`
- 随后在 storage readiness 合同扩展中，又把 simulator 侧 `SimpleStorage`
  扩成可注入 attached-but-not-ready 状态，新增 `STORAGE_ERR_NOT_READY` 与
  `--disk-not-ready` 宿主参数，并接通：
  - `guest_kernel_alpha_storage_not_ready_demo`
- 随后继续在 storage probe 合同扩展中，又把 simulator 侧 `SimpleStorage`
  扩成可注入 bad-magic 元数据状态，新增 `--disk-bad-magic` 宿主参数，并接通：
  - `guest_kernel_alpha_storage_bad_magic_demo`
- 随后在 non-storage device readiness 扩展中，又补上：
  - `guest_kernel_alpha_plic_not_ready_demo`
  - `guest_kernel_alpha_timer_not_ready_demo`
- 同日已把各入口重复的 PMM / VM / trap bring-up 骨架收口到
  `guest/kernel_alpha/common.c`，让后续负向回归只保留各自合同差异。
- 随后继续把 first external / timer delivery 的 interrupt state、policy
  安装与 deadline orchestration 从 `kernel_alpha` 入口移到
  `guest/kernel/supervisor_runtime.c`，并进一步补上 self-bound interrupt
  contract、pre-VM policy adapter 与 `supervisor_demo_smoke` 共享
  platform interrupt wait / cleanup helper，完成当前轮
  supervisor-side process / runtime sequencing 收口。
- 随后开始拆分 guest 基础设施大文件，先把 `trap` 的 default policy /
  dispatch 拆到 `guest/kernel/trap_dispatch.c`，再把 `vm_process_*`
  生命周期与 region binding 从 `guest/kernel/vm.c` 拆到
  `guest/kernel/vm_process.c`，继续把结构复杂度从入口与大文件中移走。
- 随后继续把 `guest/kernel/vm.c` 中剩余的 address space / object /
  fault policy 拆到 `guest/kernel/vm_address_space.c`、
  `guest/kernel/vm_object.c` 和 `guest/kernel/vm_fault.c`，把 `vm.c`
  收口为低层 page-table / map / unmap / TLB primitive。
- 随后继续把 `kernel_alpha` 各入口重复出现的 `trap_context` /
  `address_space` / `interrupt_state` 收口到 `guest/kernel/kernel_runtime.c`，
  并进一步把 common bring-up options 的 runtime / self-context 装配一并下沉，
  让 bring-up 入口开始从“裸拼编排”转向最小 kernel runtime 对象。
- 随后继续把通用 `K/M/V` bring-up 从 `guest/kernel_alpha/common.c` 下沉到
  `guest/kernel/kernel_bringup.c`，并把 `guest/kernel_alpha/common.c` 改成
  PLIC / first delivery / storage probe 的 phase helper，避免
  `supervisor_demo` 再依赖 alpha 私有骨架。
- 随后继续把六条 storage 负向回归里的 probe / read / clear-error 合同检查
  收口到 `guest/kernel_alpha/storage_contract.c`，让各入口进一步退化成
  “storage-only bring-up + 选择哪条合同 + marker / panic”。
- 随后继续把 non-storage readiness / panic 相关的 interrupt bring-up、
  platform interrupt readiness、`PLIC not-ready`、`timer not-ready` 与
  标准 post-handler 合同收口到 `guest/kernel_alpha/interrupt_contract.c`，
  让 `kernel_alpha` 主入口与两条 non-storage 负向入口进一步只保留场景差异。
- `2026-03-31` 又继续把 alpha 共享 bring-up phase helper 往基础设施层收口：
  `guest/kernel/kernel_runtime.c` 当前已承接默认 runtime self-context 装配，以及
  `PLIC / first delivery / storage probe / signature` 这组真实实现；
  `guest/kernel_alpha/common.c` 只保留 alpha marker / 命名 wrapper，
  避免这批 phase 细节重新膨胀回各入口。

## 当前仍然有效的风险 / 限制

对“第一次真正的小型 OS / kernel bring-up”这一层目标而言，当前基础障碍已经清掉，`kernel_alpha` 已经达到 Phase 1 核心完成态。

当前仍然有效的剩余项，已经从“阻塞首次 bring-up 的基础障碍”转为 post-Phase1 hardening，主要包括：

- 更完整的 kernel 对象与 runtime 组织，而不只是 bring-up 流程。
- 更系统的 device readiness / error 合同覆盖。
- 更完整的内核地址空间管理与 process / runtime refinement。
- 更真实的 device probe、fault / panic 路径和后续扩展点。

## 下一步

1. 继续把 `kernel_alpha_demo`、`kernel_alpha_fault_demo`、六条 storage 负向 demo、`kernel_alpha_plic_not_ready_demo` 和 `kernel_alpha_timer_not_ready_demo` 这十条回归守在稳定输出上；它们当前就是 Phase 1 回归收口的核心 guest 门禁。
2. 继续守住 `guest/kernel/vm.c` / `guest/kernel/vm_address_space.c` / `guest/kernel/vm_process.c` / `guest/kernel/vm_object.c` / `guest/kernel/vm_fault.c` 的边界，避免后续修改重新耦合。
3. 继续守住 `guest/kernel/trap.c` 与 `guest/kernel/trap_dispatch.c` 的 lifecycle / dispatch 边界，避免后续修改重新耦合。
4. 在不打破 reference path 简洁性的前提下，把剩余 device readiness / fault / panic / runtime refinement 继续当作 post-Phase1 hardening，而不再视为首次小型 OS / kernel bring-up 的基础阻塞项。

## 验证基线

- `cd myCPU && make test-unit-supervisor_runtime`
- `cd myCPU && make test-unit-kernel_runtime`
- `cd myCPU && make test-unit-kernel_alpha_common`
- `cd myCPU && make test-unit-kernel_alpha_interrupt`
- `cd myCPU && make test-unit-kernel_alpha_storage`
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
- `cd myCPU && make test`
