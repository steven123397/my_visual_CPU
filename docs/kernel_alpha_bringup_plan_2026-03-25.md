# Kernel Alpha Bring-up 状态

## 文档定位

本文档用于记录第一次独立 `kernel_alpha` bring-up 的里程碑定义、当前状态和下一步工作。

它现在不再保留早期执行 checklist，只保留仍然有效的状态信息和少量关键历史节点。

## 目标

新增一条独立于 `guest_supervisor_demo` 的 `kernel_alpha` bring-up 路径，完成第一次真正的小 kernel alpha 启动，并把它纳入稳定回归。

## 架构约束

- 保持现有 `supervisor_demo_smoke` 不继续膨胀。
- 在 `guest/` 下维护独立入口 ELF。
- 复用共享的 guest kernel 基础设施。
- 正向 bring-up 只验证第一次真正的小 kernel alpha 所需的最小路径，不一次性做完整 OS。

## 当前正向里程碑

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

## 当前负向回归

在首个 alpha 里程碑完成后，已经补上两条独立负向回归：

### `guest_kernel_alpha_fault_demo`

当前用于验证：

- VM 已开启
- UART 仍可输出
- CLINT 未映射时的内核 MMIO 访问会进入 fault / panic 路径

当前输出：

- `KMVX`

### `guest_kernel_alpha_storage_no_media_demo`

当前用于验证：

- VM 已开启
- storage MMIO 可达
- metadata / status 可观察
- 未附加镜像时 block read 返回 `NO_MEDIA`

当前输出：

- `KMVNX`

## 关键历史节点

- `2026-03-25` 已完成首个独立 `kernel_alpha_demo` alpha bring-up。
- 随后补上了：
  - `guest_kernel_alpha_fault_demo`
  - `guest_kernel_alpha_storage_no_media_demo`

## 当前仍未完成的部分

当前 `kernel_alpha` 仍是 alpha 形态，还没有真正进入“小型内核骨架完成态”。

当前仍然缺少的重点包括：

- 更完整的 kernel 对象与 runtime 组织，而不只是 bring-up 流程。
- 更系统的 device readiness / error 合同覆盖。
- 更完整的内核地址空间管理与 process / runtime refinement。
- 更真实的 device probe、fault / panic 路径和后续扩展点。

## 下一步建议

1. 在 `kernel_alpha_demo` / `kernel_alpha_fault_demo` / `kernel_alpha_storage_no_media_demo` 基线上继续补更多 device readiness 与 fault / panic 回归。
2. 继续推进 guest runtime 的 process / runtime refinement。
3. 继续拆分 `guest/kernel/vm.c`、`guest/kernel/trap.c` 等过大的实现文件。
4. 在不打破 reference path 简洁性的前提下，为第一次真正的小型 OS / kernel bring-up 清掉剩余基础障碍。

## 验证基线

- `cd myCPU && make test-guest-kernel_alpha_demo`
- `cd myCPU && make test-guest-kernel_alpha_fault_demo`
- `cd myCPU && make test-guest-kernel_alpha_storage_no_media_demo`
- `cd myCPU && make test`
