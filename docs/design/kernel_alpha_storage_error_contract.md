# kernel_alpha storage 错误合同扩展设计

## 文档定位

本文档记录独立 `kernel_alpha` 路径中 storage 错误合同扩展的设计边界。

对应工作已经完成，因此本文档保留为历史语境设计说明，不承担实时状态更新。

## 关联文档

- 状态文档：
  - [status/kernel_alpha_status.md](../status/kernel_alpha_status.md)
- 相关计划：
  - [plan/kernel_alpha_storage_error_contract_plan.md](../plan/kernel_alpha_storage_error_contract_plan.md)

## 当前有效性说明

- 当前有效 / 历史语境：历史语境，保留当时为何切这条 storage 合同扩展的设计理由。
- 当前正式结果以 [status/kernel_alpha_status.md](../status/kernel_alpha_status.md) 为准。

## 背景

当前独立 `kernel_alpha` 路径已经覆盖：

- 正向 bring-up：`KMVPETDS`
- `NO_MEDIA` 负向路径：`KMVNX`
- 未映射 `CLINT` fault / panic 负向路径：`KMVX`

但 guest 侧对 `SimpleStorage` 的消费仍停留在最小 `probe + read_block` 级别。设备侧已经实现的 `BAD_BLOCK_COUNT`、`LBA_RANGE` 和 `COMMAND = NONE` 清错合同，还没有被 `kernel_alpha` 路径消费和回归验证。

这会留下一个明显缺口：独立 kernel bring-up 已经依赖 storage readiness，但对 storage error contract 的覆盖仍然不完整。

## 目标

在不扩张 `SimpleStorage` 设备语义的前提下，补齐 guest 侧对当前 storage 错误合同的最小消费能力，并新增独立 `kernel_alpha` 负向回归。

本次只覆盖：

- guest 侧读取 storage `STATUS/ERROR`
- guest 侧提交自定义 `BLOCK_COUNT` 的 read 命令
- `BAD_BLOCK_COUNT` 错误可观察
- `COMMAND = NONE` 清错合同可观察

本次不覆盖：

- storage completion interrupt
- 多 block I/O
- write-back 到宿主文件
- 更高层块缓存或文件系统抽象

## 方案

### 1. guest platform/storage helper 扩展

在现有 guest platform layer 上补最小 helper：

- `platform_storage_write_u64(offset, value)`
- `platform_storage_issue_command(command)`
- `platform_storage_read_status()`
- `platform_storage_read_error()`
- `platform_storage_read_block_custom(lba, block_count, destination)`

其中 `platform_storage_read_block_custom()` 只做以下事情：

1. 写 `LBA`
2. 写 `BLOCK_COUNT`
3. 写 `COMMAND = READ`
4. 读取 `STATUS`
5. 若 `ERROR` 置位，则返回 `ERROR`
6. 否则把 `DATA_WINDOW` 拷到目标 buffer

它仍然保持 polling + 同步完成模型，不引入 DMA 或中断。

在 guest `storage.c` 上提供更高一层但仍然很薄的 helper：

- `storage_status()`
- `storage_error()`
- `storage_clear_error()`
- `storage_read_block_with_count(lba, block_count, destination)`

保留现有 `storage_probe()` 和 `storage_read_block()`，使已有路径无需改动。

### 2. 新增独立 kernel_alpha 负向回归

新增一条独立入口：

- `guest_kernel_alpha_storage_bad_block_count_demo`

目标输出为：

- `KMVBX`

含义：

- `K`：进入独立 kernel 入口
- `M`：memory / PMM 初始化完成
- `V`：VM 已开启且 storage MMIO lazy map 可工作
- `B`：提交 `BLOCK_COUNT != 1` 的 read 命令后，观察到 `BAD_BLOCK_COUNT`
- `X`：按既有 panic 路径结束

这条路径还要同时验证：

- 错误发生后 `STATUS.ERROR` 被置位
- `ERROR == STORAGE_ERR_BAD_BLOCK_COUNT`
- `COMMAND = NONE` 后 `STATUS.ERROR` 被清掉
- 清错后 `ERROR == STORAGE_ERR_NONE`

## 为什么这样切

相比直接扩更复杂的 device probe 或 panic 分类，本次切片更适合作为当前阶段的下一步：

- 与现有 `kernel_alpha_storage_no_media_demo` 连续，都是 device readiness / error contract
- 改动集中在 guest 侧，风险低
- 不要求改变 reference path 的设备实现
- 可以直接扩稳定回归，而不是只写文档

## 影响文件

- 新增：`myCPU/guest/kernel_alpha/storage_bad_block_count_main.c`
- 修改：`myCPU/guest/include/platform.h`
- 修改：`myCPU/guest/include/platform_drivers.inc`
- 修改：`myCPU/guest/include/storage.h`
- 修改：`myCPU/guest/kernel/storage.c`
- 修改：`myCPU/Makefile`
- 修改：`docs/status/kernel_alpha_status.md`
- 修改：`readme.md`

## 验证

至少需要验证：

- `cd myCPU && make test-guest-kernel_alpha_storage_bad_block_count_demo`
- `cd myCPU && make test-guest-kernel_alpha_storage_no_media_demo`
- `cd myCPU && make test-guest-kernel_alpha_demo`
- `cd myCPU && make test`
