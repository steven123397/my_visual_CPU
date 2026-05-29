# Platform MMIO Contract

## 文档定位

本文档用于定义当前模拟器对 guest / 内核代码暴露的最小平台 MMIO 契约，包括地址布局、寄存器窗口、访问宽度与非法访问口径。

它是当前有效的设计 / 契约文档，不承担实时状态更新。

## 关联文档

- 状态文档：
  - [status/mainline_status.md](../status/mainline_status.md)
  - [status/kernel_alpha_status.md](../status/kernel_alpha_status.md)
- 已完成计划：
  - [plan/history_plan.md#phase1-hardening-regressions-plan](../plan/history_plan.md#phase1-hardening-regressions-plan)

## 当前有效性说明

- 当前有效 / 历史语境：当前有效，当前 host / guest 共享的 MMIO 约束以本文档和 [platform_mmio.h](../../myCPU/include/platform_mmio.h) 为准。
- 若契约发生变化，必须同步更新相关回归、状态文档和使用方代码。

这份文档定义当前模拟器对未来 OS 暴露的最小平台 MMIO 约定。对应常量的单一来源是 [platform_mmio.h](../../myCPU/include/platform_mmio.h)。

当前目标不是一次对齐某个成熟平台标准，而是先把“足以写内核驱动”的地址、寄存器、命令和约束固定下来，并保持测试覆盖。

除明确列出的寄存器和访问宽度外，其余偏移或宽度当前都不再保证“返回 0”或静默忽略。它们应视为非法 MMIO 访问，并通过 CPU 侧形成可观察 fault。

## 地址布局

| 基地址 | 大小 | 设备 |
|---|---:|---|
| `0x10000000` | `0x8` | UART |
| `0x10001000` | `0x400` | `SimpleStorage` when `simple_storage` transport is selected |
| `0x10001000` | `0x1000` | Virtio MMIO block device when `virtio-blk` transport is selected |
| `0x10002000` | `0x1000` | MMIO AI accelerator |
| `0x02000000` | `0x10000` | CLINT |
| `0x0c000000` | `0x300000` | PLIC |
| `0x80000000` | `128 MiB` | RAM |

## UART

当前 UART 只接通了最小发送和 THRE interrupt 相关寄存器。

### 已实现寄存器

| 偏移 | 宽度 | 名称 | 说明 |
|---:|---:|---|---|
| `0x0` | 8 | `THR` | 发送寄存器 |
| `0x1` | 8 | `IER` | 中断使能寄存器 |
| `0x2` | 8 | `IIR` | 中断识别寄存器 |
| `0x5` | 8 | `LSR` | 线路状态寄存器 |

### 访问约束

- 当前 UART 只接受 `1` 字节访问。
- 当前未列出的偏移视为非法 MMIO 访问。

## CLINT

当前只接通了最小的 machine timer 子集，供机器态和经 `mideleg` 委派后的 supervisor timer interrupt 路径使用。

### 已实现寄存器

| 偏移 | 宽度 | 名称 | 说明 |
|---:|---:|---|---|
| `0x4000` | 64 | `MTIMECMP` | 定时器比较值 |
| `0xBFF8` | 64 | `MTIME` | 单调递增平台时间基准 |

当前实现对这两个 64 位寄存器接受寄存器窗口内的 `1/2/4/8` 字节访问；这意味着客体既可以用 RV64 风格的 64 位整寄存器访问，也可以用更接近传统驱动习惯的 32 位分段访问。

寄存器窗口外访问或非 `1/2/4/8` 字节访问，当前都视为非法 MMIO 访问。

### 驱动约定

1. 读取 `MTIME`
2. 写 `MTIMECMP = MTIME + delta`
3. 等待 supervisor timer interrupt 或 machine timer interrupt 递送
4. 处理中断后把 `MTIMECMP` 设到未来值，或设成全 `1` 以临时关闭

当前模拟器中的 `time` CSR 也与 CLINT 的 `MTIME` 绑定，而不是单独使用 CPU cycle 计数，因此软件不会看到互相分叉的两套“平台时间”。

### 当前限制

- 只有单 hart，因此没有 per-hart `mtimecmp` 数组。
- 目前只服务定时器中断，不覆盖 software interrupt 语义。

## PLIC

当前实现的是一个非常小的 PLIC 子集，只覆盖现在真正接通的路径。

### Source ID

| Source ID | 说明 |
|---:|---|
| `1` | Virtio MMIO block interrupt |
| `9` | MMIO AI accelerator interrupt |
| `10` | UART THRE interrupt |

### Context

| Context ID | 说明 |
|---:|---|
| `0` | Machine context |
| `1` | Supervisor context |

### 已实现寄存器

| 偏移 | 说明 |
|---:|---|
| `4 * source_id` | source priority |
| `0x1000` | pending bits |
| `0x2000 + 0x80 * context` | enable bits |
| `0x200000 + 0x1000 * context` | threshold |
| `0x200000 + 0x1000 * context + 4` | claim / complete |

当前这些 PLIC 寄存器只接受 `4` 字节访问；其余宽度或未列出的偏移都视为非法 MMIO 访问。

### 驱动约定

1. 设置 `priority(source)` 为非零值。
2. 在目标 context 的 `enable` 中打开对应 source bit。
3. 将 `threshold` 设为 `0`。
4. Trap 进入后读取 `claim`，拿到 source id。
5. 处理设备中断源。
6. 把同一个 source id 写回 `claim/complete` 完成中断。

### 当前限制

- 目前只服务 `virtio-blk`、MMIO AI accelerator 和 UART THRE 这三类外部中断源；`riscv,ndev` / source array 上限因此覆盖到 source `10`。
- 只覆盖 machine/supervisor 两个 context。
- 没有优先级抢占、嵌套或更复杂的 PLIC 语义。

当前 Linux / xv6 board DTS 只暴露 UART source `10` 和 `virtio-blk` source `1`；AI accelerator source `9` 是模拟器和 host smoke 可用的平台源，当前不进入通用 Linux board profile。

## Block Transport 选择关系

`0x10001000` 是当前平台的块设备 transport 复用窗口，运行时只能绑定其中一种块设备：

- 默认 `simple_storage` transport 绑定 `SimpleStorage`，寄存器窗口大小为 `0x400`，供 guest runtime、`kernel_alpha` 和 storage 负向合同使用。
- `virtio-blk` transport 绑定 Virtio MMIO block device，寄存器窗口大小为 `0x1000`，供 xv6 / Linux-facing board profile 使用；对应 DTB `virtio_mmio@10001000` 的 PLIC source 为 `1`。
- 选择 `virtio-blk` 后，不再支持 `SimpleStorage` 专用的 `--disk-not-ready` / `--disk-bad-magic` 注入语义。

这意味着 guest-visible block device ABI 由启动入口选择决定，而不是两个设备同时出现在同一地址窗口。

## SimpleStorage

`SimpleStorage` 现在不再是顺序字节流接口，而是一个“自定义但块化”的 MMIO block device。

### 设备特征

- block size 固定为 `512` 字节
- 当前每次命令只支持 `1` 个 block
- 通过宿主 `--disk` / `-d` 参数附加镜像文件
- 可通过 `--disk-not-ready` 注入 attached-but-not-ready 状态
- 可通过 `--disk-bad-magic` 注入 metadata `MAGIC` 损坏状态
- 设备读写发生在内部 block data window 上，不做 DMA
- 命令同步完成，当前阶段推荐 polling 驱动

### 寄存器

| 偏移 | 宽度 | 名称 | 说明 |
|---:|---:|---|---|
| `0x00` | 64 | `MAGIC` | 固定魔数 `0x4d4d424c4b444556` |
| `0x08` | 64 | `VERSION` | 当前为 `1` |
| `0x10` | 64 | `BLOCK_SIZE` | 当前为 `512` |
| `0x18` | 64 | `CAPACITY_BLOCKS` | 当前镜像可访问 block 数 |
| `0x20` | 64 | `STATUS` | `ATTACHED` / `READY` / `ERROR` |
| `0x28` | 64 | `COMMAND` | 写入 `READ` / `WRITE` / `NONE` |
| `0x30` | 64 | `LBA` | 目标逻辑块号 |
| `0x38` | 64 | `BLOCK_COUNT` | 当前必须写成 `1` |
| `0x40` | 64 | `ERROR` | 最近一次错误码 |
| `0x80` | 512B window | `DATA_WINDOW` | block 数据缓冲区 |

控制寄存器当前只接受 `8` 字节访问；`DATA_WINDOW` 当前接受窗口内的 `1/2/4/8` 字节访问。其余宽度或未列出的偏移都视为非法 MMIO 访问。

### STATUS 位

| 位 | 名称 | 说明 |
|---:|---|---|
| `0` | `ATTACHED` | 已附加宿主镜像 |
| `1` | `READY` | 设备空闲，可接受命令 |
| `2` | `ERROR` | 最近一次命令失败 |

### COMMAND

| 值 | 名称 | 说明 |
|---:|---|---|
| `0` | `NONE` | 不执行 I/O，只清除粘滞错误 |
| `1` | `READ` | 把 `LBA` 对应 block 读入 `DATA_WINDOW` |
| `2` | `WRITE` | 把 `DATA_WINDOW` 写回 `LBA` 对应 block |

### ERROR

| 值 | 名称 | 说明 |
|---:|---|---|
| `0` | `NONE` | 无错误 |
| `1` | `NO_MEDIA` | 没有附加镜像 |
| `2` | `BAD_COMMAND` | 非法命令值 |
| `3` | `BAD_BLOCK_COUNT` | `BLOCK_COUNT` 不是当前支持的 `1` |
| `4` | `LBA_RANGE` | `LBA` 超出容量范围 |
| `5` | `NOT_READY` | 镜像已附加，但设备当前不接受命令 |

### 读块流程

1. 写 `LBA`
2. 写 `BLOCK_COUNT = 1`
3. 写 `COMMAND = READ`
4. 轮询 `STATUS.ERROR`
5. 从 `DATA_WINDOW` 读取 512B block 数据

### 写块流程

1. 把 512B 数据写入 `DATA_WINDOW`
2. 写 `LBA`
3. 写 `BLOCK_COUNT = 1`
4. 写 `COMMAND = WRITE`
5. 轮询 `STATUS.ERROR`

### 当前限制

- 命令同步完成，所以当前没有 storage completion interrupt。
- `BLOCK_COUNT` 目前只支持 `1`，驱动需要按 block 循环提交。
- 写入结果当前只更新模拟器内存中的 backing store，不回写宿主文件。

## Guest Driver 分层

客体侧最小平台驱动接口已经从纯测试辅助代码提升成可复用的 guest 平台层，并按两层消费当前 MMIO 契约：

1. `platform_mmio.h` 对应的裸寄存器访问层
2. `plic` / `simple_storage` 等最小设备驱动层

- 共享汇编驱动实现位于 [platform_drivers.inc](../../myCPU/guest/include/platform_drivers.inc)
- C 侧声明位于 [platform.h](../../myCPU/guest/include/platform.h)
- 可链接的 guest 平台库入口位于 [platform.S](../../myCPU/guest/lib/platform.S)
- 测试侧的 [platform_drivers.inc](../../myCPU/tests/asm/include/platform_drivers.inc) 现在只是转发到共享实现，避免双份维护

当前提供的最小入口包括：

- `platform_uart_putc`
- `platform_uart_enable_thre_irq`
- `platform_uart_disable_irq`
- `platform_plic_supervisor_init`
- `platform_plic_supervisor_claim`
- `platform_plic_supervisor_complete`
- `platform_clint_read_mtime`
- `platform_clint_write_mtimecmp`
- `platform_storage_read_u64`
- `platform_storage_write_u64`
- `platform_storage_read_status`
- `platform_storage_read_error`
- `platform_storage_issue_command`
- `platform_storage_read_block`
- `platform_storage_read_block_custom`
- `platform_shutdown`

这些入口的目标不是成为最终内核 ABI，而是先固定“客体侧如何消费当前 MMIO 契约”的最小驱动层。当前 guest 平台层已经形成以下稳定消费边界：

- guest kernel 基础设施已拆出 `console` / `storage` / `timer` / `trap` / `memory` / `pmm` / `vm` 等最小模块，并通过统一 trap dispatch、注册式 interrupt/exception handler 和 VM-owned page-fault policy/handling 路径，把 supervisor external interrupt、supervisor timer interrupt、demand-mapped fault 与可恢复 page fault 收口到同一套基础设施。
- 最小 guest runtime 已经接通 early bump allocator、bitmap-backed PMM、guest-side Sv39 page-table builder、`satp` 切换、fault-range-backed page-fault handling、recoverable page-fault policy registration、`vm_map_range` / `vm_unmap_page` 语义、本地 TLB 一致性维护合同，以及当前 user / kernel 地址窗口 helper、`sstatus.SUM` 驱动的 supervisor-side user-page 访问验证和第一批 kernel/user split helper。
- guest `storage.c` 当前把 storage 消费路径拆成两层：`storage_read_info()` / `storage_probe()` 负责 metadata / readiness，`storage_status()` / `storage_error()` / `storage_clear_error()` / `storage_read_block_with_count()` 负责最小错误合同消费。
- 独立 `kernel_alpha_demo` 已消费这套合同完成 storage metadata / readiness probe 与 block `LBA 0` 读取。
- 独立 `kernel_alpha_storage_no_media_demo`、`kernel_alpha_storage_not_ready_demo`、`kernel_alpha_storage_bad_magic_demo`、`kernel_alpha_storage_bad_block_count_demo`、`kernel_alpha_storage_lba_range_demo` 和 `kernel_alpha_storage_bad_command_demo` 已分别覆盖 `NO_MEDIA`、`NOT_READY`、bad-magic probe-fail、`BAD_BLOCK_COUNT`、`LBA_RANGE` 和 `BAD_COMMAND` 这几条 storage 负向合同。
- 独立 `kernel_alpha_plic_not_ready_demo` 和 `kernel_alpha_timer_not_ready_demo` 则继续覆盖 non-storage device readiness timeout / panic 合同。

当前这些能力已经足以支撑 `phase1-stable` 的首次小型 OS / kernel bring-up；后续若继续扩 guest VM / runtime，应把它视为 post-Phase1 hardening，而不是回退当前合同边界。

当前至少有 3 类消费方：

- 汇编 smoke coverage 见 [supervisor_platform_smoke.S](../../myCPU/tests/asm/supervisor_platform_smoke.S)
- 最小 C-based supervisor runtime/demo 见 [start.S](../../myCPU/guest/supervisor_demo/start.S) 和 [main.c](../../myCPU/guest/supervisor_demo/main.c)
- 独立 kernel alpha bring-up / negative demos 见 [main.c](../../myCPU/guest/kernel_alpha/main.c)、[fault_main.c](../../myCPU/guest/kernel_alpha/fault_main.c)、[storage_no_media_main.c](../../myCPU/guest/kernel_alpha/storage_no_media_main.c)、[storage_not_ready_main.c](../../myCPU/guest/kernel_alpha/storage_not_ready_main.c)、[storage_bad_magic_main.c](../../myCPU/guest/kernel_alpha/storage_bad_magic_main.c)、[storage_bad_block_count_main.c](../../myCPU/guest/kernel_alpha/storage_bad_block_count_main.c)、[storage_lba_range_main.c](../../myCPU/guest/kernel_alpha/storage_lba_range_main.c)、[storage_bad_command_main.c](../../myCPU/guest/kernel_alpha/storage_bad_command_main.c)、[plic_not_ready_main.c](../../myCPU/guest/kernel_alpha/plic_not_ready_main.c) 和 [timer_not_ready_main.c](../../myCPU/guest/kernel_alpha/timer_not_ready_main.c)
