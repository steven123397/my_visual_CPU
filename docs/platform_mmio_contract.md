# Platform MMIO Contract

这份文档定义当前模拟器对未来 OS 暴露的最小平台 MMIO 约定。对应常量的单一来源是 [platform_mmio.h](/home/liangjiaqi/projects/my_visual_CPU/myCPU/include/platform_mmio.h)。

当前目标不是一次对齐某个成熟平台标准，而是先把“足以写内核驱动”的地址、寄存器、命令和约束固定下来，并保持测试覆盖。

除明确列出的寄存器和访问宽度外，其余偏移或宽度当前都不再保证“返回 0”或静默忽略。它们应视为非法 MMIO 访问，并通过 CPU 侧形成可观察 fault。

## 地址布局

| 基地址 | 大小 | 设备 |
|---|---:|---|
| `0x10000000` | `0x8` | UART |
| `0x10001000` | `0x400` | `SimpleStorage` |
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
| `1` | UART THRE interrupt |

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

- 目前只实现了 UART THRE 这一条外部中断源。
- 只覆盖 machine/supervisor 两个 context。
- 没有优先级抢占、嵌套或更复杂的 PLIC 语义。

## SimpleStorage

`SimpleStorage` 现在不再是顺序字节流接口，而是一个“自定义但块化”的 MMIO block device。

### 设备特征

- block size 固定为 `512` 字节
- 当前每次命令只支持 `1` 个 block
- 通过宿主 `--disk` / `-d` 参数附加镜像文件
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

## 驱动准备建议

内核侧建议尽快抽象出两层：

1. `platform_mmio.h` 对应的裸寄存器访问层
2. `plic` / `simple_storage` 的最小驱动层

这样后续即使把 `SimpleStorage` 再升级成更像真实块设备，或者给它补中断完成路径，也只会改驱动层，不会污染更高层的块缓存或文件系统代码。

## Guest Platform Layer

客体侧最小平台驱动接口现在已经从纯测试辅助代码提升成了可复用的 guest 平台层：

- 共享汇编驱动实现位于 [platform_drivers.inc](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/include/platform_drivers.inc)
- C 侧声明位于 [platform.h](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/include/platform.h)
- 可链接的 guest 平台库入口位于 [platform.S](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/lib/platform.S)
- 测试侧的 [platform_drivers.inc](/home/liangjiaqi/projects/my_visual_CPU/myCPU/tests/asm/include/platform_drivers.inc) 现在只是转发到共享实现，避免双份维护

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
- `platform_storage_read_block`
- `platform_shutdown`

这些入口的目标不是成为最终内核 ABI，而是先固定“客体侧如何消费当前 MMIO 契约”的最小驱动层。当前 guest 层已经拆出了 `console` / `storage` / `timer` / `trap` / `memory` / `pmm` / `vm` 这些最小模块，并用统一 trap dispatch、注册式 interrupt/exception handler 和 VM-owned page-fault policy/handling 路径，把 supervisor external interrupt、supervisor timer interrupt、demand-mapped fault 和可恢复的 page fault 路径收口到同一条入口；同时最小 demo 已经通过 linker symbol 暴露的内存布局接通了 early bump allocator，在其上建立了 bitmap-backed physical page manager，并进一步接上了 guest-side Sv39 page-table builder、`satp` 切换、fault-range-backed page-fault handling、recoverable page-fault policy registration、较严格的 `vm_map_range` / `vm_unmap_page` 语义、VM 启用后成功 map/unmap 自动维护本地 TLB 一致性的合同，以及当前 user `< MEM_BASE` / kernel `[MEM_BASE, MEM_BASE + MEM_SIZE)` 地址窗口 helper、user fault-range 注册、`sstatus.SUM` 驱动的 supervisor-side user-page 访问验证、text/rodata/data 的页粒度内核权限映射验证和第一批 kernel/user split helper。现在 guest `storage.c` 也已经把 storage 消费路径拆成两层：`storage_read_info()` 负责读取 metadata / status，`storage_probe()` 负责在其上判断 ready/attached 成功态。独立 `kernel_alpha_demo` 已经消费这套合同完成 storage metadata / readiness probe 与 block `LBA 0` 读取；独立 `kernel_alpha_storage_no_media_demo` 则继续覆盖“VM 已开启但未附加镜像”时 metadata / `NO_MEDIA` error 的负向合同。下一阶段应继续沿着这个 VM-owned region/policy 层推进更完整的 kernel/user address-space 管理。当前至少有 3 类消费方：

- 汇编 smoke coverage 见 [supervisor_platform_smoke.S](/home/liangjiaqi/projects/my_visual_CPU/myCPU/tests/asm/supervisor_platform_smoke.S)
- 最小 C-based supervisor runtime/demo 见 [start.S](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/supervisor_demo/start.S) 和 [main.c](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/supervisor_demo/main.c)
- 独立 kernel alpha bring-up / negative demos 见 [main.c](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/kernel_alpha/main.c)、[fault_main.c](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/kernel_alpha/fault_main.c) 和 [storage_no_media_main.c](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/kernel_alpha/storage_no_media_main.c)
