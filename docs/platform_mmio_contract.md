# Platform MMIO Contract

这份文档定义当前模拟器对未来 OS 暴露的最小平台 MMIO 约定。对应常量的单一来源是 [platform_mmio.h](/home/liangjiaqi/projects/my_visual_CPU/myCPU/include/platform_mmio.h)。

当前目标不是一次对齐某个成熟平台标准，而是先把“足以写内核驱动”的地址、寄存器、命令和约束固定下来，并保持测试覆盖。

## 地址布局

| 基地址 | 大小 | 设备 |
|---|---:|---|
| `0x10000000` | `0x8` | UART |
| `0x10001000` | `0x400` | `SimpleStorage` |
| `0x02000000` | `0x10000` | CLINT |
| `0x0c000000` | `0x300000` | PLIC |
| `0x80000000` | `128 MiB` | RAM |

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
