# myCPU — RISC-V 模拟器

当前处于从 C 原型向模块化 C++ 架构迁移的早期阶段。现有功能路径仍以原始参考语义为主，但已经具备更完整的 Phase 1 OS bring-up 地基：裸机程序执行、UART/CLINT/PLIC/MMIO block storage 平台、M/S/U 特权路径、Sv39 虚拟内存，以及一个带 early allocator、最小 PMM、guest-side Sv39 页表层、VM-owned page-fault policy/handling、fault-range-backed 缺页映射和页粒度 kernel mapping 的 supervisor runtime 骨架。

## 目录结构

```
myCPU/
├── guest/              # 最小 guest supervisor runtime / 平台层 / demo
├── src/
│   ├── main.cpp        # C++ 入口，CLI 参数与 Machine 启动
│   ├── cpu.cpp/h       # CPU 外观接口 + 参考执行路径
│   ├── memory.c/h      # 主内存 backing 与底层访存辅助
│   ├── decode.c/h      # 指令解码
│   ├── trap.cpp/h      # TrapController：异常/中断路由与返回
│   ├── arch/           # CoreState / CsrFile 状态边界
│   ├── exec/           # 指令语义分块（integer / control-flow / memory / system）
│   ├── platform/       # C++ Machine 骨架与平台地址映射
│   ├── mem/            # C++ Ram/Bus/AddressSpace 边界
│   ├── devices/        # UART / CLINT / PLIC / SimpleStorage 设备对象
│   ├── loader/         # C++ ELF / flat binary 镜像装载边界
│   └── ...
├── tests/asm/          # 汇编测试程序与平台 smoke coverage
├── docs/               # 规划与平台契约文档
└── Makefile
```

## 模块关系与运行流程

模拟器从命令行读取镜像路径，通过 `Machine` 组装平台对象，然后进入单步执行循环。整体调用关系如下：

```text
main.cpp
  └── Machine
        ├── Ram                      初始化 128MB 主内存
        ├── Uart16550 / Clint / Plic / SimpleStorage
        ├── Bus                      分发 RAM 与设备访问
        ├── ElfLoader / BinaryLoader     加载 ELF 或平坦二进制
        ├── cpu_init()               初始化 CoreState、CsrFile
        └── while (!halted)
              └── cpu_step(cpu, bus)
              ├── Bus::tick() 返回平台事件 / 通过 TrapController 检查 timer / external interrupt
              ├── AddressSpace::fetch32() 取指
              ├── decode()          指令译码
              ├── execute()         执行指令
              │     ├── AddressSpace::load/store() 访问主内存或设备
              │     ├── csr_read()/csr_write()   访问 CSR
              │     └── TrapController 处理 trap 进入/返回
              └── cycle++
```

从模块职责看：

- `main.cpp` 负责 CLI 参数解析和启动 `Machine`。
- `platform/machine.*` 负责组装 CPU、Ram、Bus、PLIC、storage 等平台对象，并驱动主执行循环。
- `platform/address_map.h` 负责平台地址映射常量，避免设备层依赖 legacy `Memory` 头。
- `arch/core_state.*` 负责通用寄存器、`pc`、周期计数和停机状态。
- `arch/csr_file.*` 负责已实现 CSR 集合、`misa/satp/time` 这类带架构约束的特殊语义，以及 `sstatus/sie/sip` 对 `mstatus/mie/mip` 的别名视图。
- `mem/ram.*` 和 `mem/bus.*` 提供平台总线与 RAM 边界。
- `mem/address_space.*` 负责 CPU 侧地址访问边界，当前提供 bare-mode 直通、Sv39 三级页表遍历、最小 TLB、受限 `satp` 模式切换、`sfence.vma` 刷新，以及 instruction/load/store 的 fault 路由。
- `devices/uart16550.*`、`devices/clint.*`、`devices/plic.*` 和 `devices/simple_storage.*` 提供独立 MMIO 设备对象。
- `devices/device.h` 提供统一设备接口，供 `Bus` 附加和分发。
- `loader/elf_loader.*` 和 `loader/binary_loader.*` 提供镜像装载边界，直接通过 `Ram` 接口写入镜像内容。
- `cpu.cpp/h` 负责把 `CoreState + CsrFile + TrapController` 接回现有参考执行路径。
- `decode.c` 负责把 32 位机器码拆成执行阶段可用的字段。
- `memory.c` 负责主内存访问，MMIO 分发由 `Bus` 与设备对象处理。
- `trap.cpp/h` 负责 `TrapController`，集中处理异常/中断入口、`mret/sret` 返回、timer/external interrupt 路由，以及当前最小 `medeleg/mideleg` supervisor trap 委托路径。
- `exec/integer_ops.*`、`exec/control_flow_ops.*`、`exec/memory_ops.*`、`exec/system_ops.*` 负责按指令族拆分参考语义，避免 `cpu.cpp` 持续膨胀。

一次指令执行的数据流可以概括为：

```text
PC
  -> mem_read(取指)
  -> decode(解析 opcode/寄存器/立即数)
  -> execute(算术/跳转/访存/CSR/系统指令)
  -> 可能访问 memory / UART / CLINT / PLIC / SimpleStorage
  -> 可能进入 trap
  -> 更新 PC
```

## 编译

```bash
cd myCPU
make
```

## 运行

```bash
# 运行 ELF 程序
./mycpu <program.elf>

# 运行平坦二进制（指定加载地址，十六进制）
./mycpu -b 80000000 <program.bin>
```

## 测试

需要 RISC-V 交叉编译工具链：

```bash
sudo apt install gcc-riscv64-unknown-elf binutils-riscv64-unknown-elf
make test
```

`make test` 会构建汇编样例和最小 guest supervisor demo，并校验 UART 输出是否与预期一致；单个样例异常卡死时会超时失败。当前除综合回归外，还包含 `loads_signed_unsigned`、`alu_word`、`branches_signed_unsigned`、`muldiv`、`fence_noop` 这类更细粒度的指令族回归，以及 `privilege_transitions`、`sret_transitions`、`supervisor_exception_delegation`、`supervisor_timer_interrupt`、`csr_access_control`、`access_faults`、`csr_semantic_consistency` 这类特权/异常/CSR 语义一致性回归，`plic_*` / `storage_device_basic` / `supervisor_platform_smoke` / `clint_split_access` 这类平台回归，`sv39_*` 这类虚拟内存/TLB 回归，以及覆盖 guest-side demand paging、recoverable fault policy、fault-range-backed remap、以及 VM 已启用后 `vm_map_range` / `vm_unmap_page` 自动维护本地 TLB 一致性的 `guest_supervisor_demo` bring-up 回归。

## 内存映射

| 地址范围 | 设备 |
|---|---|
| `0x80000000` + 128MB | 主内存 |
| `0x10000000` | UART (16550) |
| `0x10001000` | SimpleStorage (MMIO block device) |
| `0x02000000` | CLINT (定时器) |
| `0x0c000000` | PLIC |

## 已实现特性

- RV64I 基础整数指令集
- RV64M 乘除法扩展
- ELF64 程序加载
- CSR 指令（CSRRW/CSRRS/CSRRC 及立即数变体）
- M-mode 异常与中断（ECALL、EBREAK、MRET）
- 第一批 M/S/U 特权语义：`MPP` 跟踪、`ecall` cause 区分、`sret` 返回、CSR 访问约束
- 基于 `medeleg` 的最小 supervisor 异常委托
- 基于 `mideleg` 的最小 supervisor 定时器/外部中断递送
- **Sv39 虚拟内存**：3 级页表遍历、页错误、权限检查、大页支持、最小 TLB、A/D bit 维护
- `satp` CSR 支持（MODE 字段按 WARL 约束到 bare/Sv39；不支持值不会以“读得出分页模式、实际却 bare” 的形式泄漏）
- `sfence.vma` 指令（当前执行本地 TLB 全量失效）
- `AddressSpace` 访问边界，以及 unmapped fetch/load/store 的 access-fault trap
- CSR 特权级/只读属性检查，非法访问触发 illegal-instruction trap
- `misa` 作为固定实现能力视图对 guest 只读暴露，不允许软件改写 ISA 声明
- UART MMIO（写入直接输出到 stdout）
- CLINT 定时器中断，以及 `mtime/mtimecmp` 的 1/2/4/8 字节 MMIO 访问
- `time` CSR 与 CLINT `mtime` 保持一致，guest 侧 CSR/MMIO 看到同一平台时间源
- PLIC machine/supervisor external interrupt 最小路径
- host-backed block-oriented MMIO storage device
- 最小 guest supervisor platform layer、统一 trap dispatch、注册式 interrupt/exception handler、VM-owned page-fault policy/handling，以及 linker-backed early allocator + bitmap-backed PMM + guest-side Sv39 page-table builder + fault-range-backed page-fault handling + recoverable fault policy registration + page-granular kernel mapping / fault handling + VM 启用后的 map/unmap 本地 TLB 同步
- `ecall` a7=93 退出约定

## 源码文件说明

### 根目录

- `readme.md`：项目总说明，包含功能、编译、运行和测试方式。
- `docs/request.md`：课程项目背景与目标说明，描述了“从 0 实现一个可运行程序的指令集模拟器”的教学目标。

### `myCPU/`

- `Makefile`：本地编译规则、RISC-V 汇编样例构建规则和 `make test` 测试入口。
- `mycpu`：编译产物，运行后加载并执行 RISC-V 程序镜像。

### `myCPU/guest/`

- `include/`：guest 平台层、trap、timer、memory、pmm、vm 等最小内核接口。
- `kernel/`：`console` / `storage` / `timer` / `trap` / `memory` / `pmm` / `vm` 这些最小 guest 侧模块实现，当前已覆盖页粒度 kernel mapping、VM-owned page-fault dispatch/policy、fault-range-backed 缺页映射、可恢复 page-fault 注册、较严格的 `vm_map_range` / `vm_unmap_page` 语义、以及 VM 启用后成功 map/unmap 自动维护本地 TLB 一致性。
- `lib/platform.S`：共享 guest MMIO 平台库入口。
- `supervisor_demo/`：最小 supervisor runtime、linker script 和 bring-up demo。

### `myCPU/src/`

- `main.cpp`
  程序入口。负责解析 `-b` 参数、创建 `Machine`，并根据镜像类型调用 `load_elf()` 或 `load_binary()` 后启动执行。

- `platform/machine.h`
  `Machine` 类声明。聚合 `CPU`、`Ram` 和 `Bus`，为后续平台化重构提供统一入口。

- `platform/machine.cpp`
  `Machine` 实现。当前负责镜像加载、`cpu_init()` 调用和执行循环，镜像装载通过 `ElfLoader/BinaryLoader` 完成，执行阶段仍复用现有参考语义。

- `platform/address_map.h`
  平台地址映射常量定义。集中声明 RAM、UART、CLINT、PLIC、storage 的基地址与大小，供入口、设备和 legacy 内存 backing 共享。

- `mem/ram.h`
  `Ram` 类声明。负责主内存生命周期、RAM 范围内的 load/store，以及供 loader 使用的 bulk write/fill 接口。

- `mem/ram.cpp`
  `Ram` 实现。内部调用现有 `mem_init()/mem_free()` 管理 RAM backing，并通过显式 RAM 接口暴露单点和批量写入能力。

- `mem/bus.h`
  `Bus` 类声明。维护统一设备映射表，并向上暴露统一的 load/store/tick 接口；`tick()` 返回平台事件而不是暴露具体设备状态。

- `mem/bus.cpp`
  `Bus` 实现。负责设备附加、地址分发以及平台 tick 结果汇总；RAM 也作为总线设备接入，不再保留专门的 RAM 分支。

- `mem/address_space.h`
  `AddressSpace` 类声明。定义 CPU 侧 fetch/load/store 访问入口，提供虚拟地址到物理地址的转换边界。

- `mem/address_space.cpp`
  `AddressSpace` 实现。支持 bare-mode 直通和 Sv39 三级页表遍历。M-mode 始终使用物理地址；S/U-mode 根据受限 `satp.MODE` 决定是否启用分页。页表遍历包含权限检查（R/W/X/U 位）、大页对齐检查，以及 instruction/load/store page fault 触发。

- `devices/device.h`
  设备基类声明。定义统一的 `contains/load/store` 接口，供平台总线附加和寻址。

- `devices/uart16550.h`
  `Uart16550` 类声明。封装最小 16550 串口寄存器访问与 stdout 输出行为。

- `devices/uart16550.cpp`
  `Uart16550` 实现。当前支持最小发送路径和 `LSR` 就绪读取。

- `devices/clint.h`
  `Clint` 类声明。封装 `mtime/mtimecmp`、定时器 tick 和 MMIO 访问接口。

- `devices/clint.cpp`
  `Clint` 实现。负责定时器状态推进、`mtime/mtimecmp` 读写、分宽度 MMIO 访问，并在 tick 时返回是否产生待处理中断。

- `devices/plic.h` / `devices/plic.cpp`
  最小 PLIC 设备实现。当前支持 machine/supervisor context、UART THRE source、claim/complete 与 pending/enable/threshold 路径。

- `devices/simple_storage.h` / `devices/simple_storage.cpp`
  最小块化 MMIO storage 设备实现。通过 `LBA/BLOCK_COUNT/COMMAND/DATA_WINDOW` 暴露同步 block read/write 接口，并支持宿主 `--disk` 镜像附加。

- `loader/elf_loader.h`
  `ElfLoader` 类声明。定义 ELF 镜像装载接口。

- `loader/elf_loader.cpp`
  `ElfLoader` 实现。解析最小 ELF64 头和程序头，把可加载段通过 `Ram` 接口写入内存，并返回入口地址。

- `loader/binary_loader.h`
  `BinaryLoader` 类声明。定义平坦二进制装载接口。

- `loader/binary_loader.cpp`
  `BinaryLoader` 实现。校验镜像大小后，把平坦二进制通过 `Ram` 接口装入指定地址。

- `cpu.h`
  CPU 外观接口定义。当前聚合 `CoreState`、`CsrFile` 和 `TrapController`，并通过 `Bus` 访问平台内存与设备。

- `cpu.cpp`
  CPU 调度入口。实现 `cpu_init()`、`csr_read()/csr_write()`、`execute()` 和 `cpu_step()`，当前主要负责取操作数、按 opcode 分发到各个 `exec/` 模块，并通过 `CoreState + CsrFile + TrapController` 管理 CPU 状态与 trap 路由。

- `arch/core_state.h`
  `CoreState` 声明。封装 32 个通用寄存器、`pc`、周期计数和停机状态，为后续继续拆语义和执行后端提供稳定状态边界。

- `arch/core_state.cpp`
  `CoreState` 实现。负责状态复位、寄存器读写、PC 更新、周期推进和停机标志维护。

- `arch/csr_file.h`
  `CsrFile` 声明。封装 CSR 地址常量、`mstatus/mie/mip` 位定义以及 CSR 存储接口。

- `arch/csr_file.cpp`
  `CsrFile` 实现。负责 CSR 状态复位、普通 CSR 读写，以及固定 `misa` 视图、受限 `satp` WARL 语义和 `time -> CLINT mtime` 这类特殊规则。

- `exec/integer_ops.h`
  整数指令族执行接口声明。承接 `LUI/AUIPC`、整数立即数、整数寄存器、`W` 变体和当前 `FENCE` no-op 路径。

- `exec/integer_ops.cpp`
  整数指令族执行实现。负责 RV64I/RV64M 中的整数算术、比较、移位和 `W` 类语义。

- `exec/control_flow_ops.h`
  控制流指令执行接口声明。承接跳转与条件分支语义。

- `exec/control_flow_ops.cpp`
  控制流指令执行实现。负责 `JAL`、`JALR` 以及各类条件分支的目标地址与 next-pc 更新。

- `exec/memory_ops.h`
  访存指令执行接口声明。承接 load/store 语义。

- `exec/memory_ops.cpp`
  访存指令执行实现。负责带符号/无符号 load 和各宽度 store，并通过 `Bus` 访问 RAM 或设备。

- `exec/system_ops.h`
  系统/CSR 指令执行接口声明。当前承接 `opcode 0x73` 的语义执行入口。

- `exec/system_ops.cpp`
  系统/CSR 指令执行实现。负责 `ecall`、`ebreak`、`mret`、`sret`、`sfence.vma` 以及 CSR 读改写语义，并通过 `TrapController` 进入 trap 路径。

- `decode.h`
  `Insn` 指令结构定义，供译码阶段和执行阶段共享。

- `decode.c`
  译码器实现。根据 opcode 判断 I/S/B/U/J 等格式，提取 `rd/rs1/rs2`、`funct3/funct7` 和符号扩展后的立即数。

- `memory.h`
  `Memory` 结构和 legacy RAM backing 接口定义，包括底层单点/批量读写辅助；平台地址常量已拆到 `platform/address_map.h`。

- `memory.c`
  RAM 访问实现。提供底层单点读写与批量写入/填零辅助，供 `Ram` 封装后复用。

- `trap.h`
  `TrapController` 声明。封装异常进入、中断进入、`mret` 返回以及待处理中断检查接口。

- `trap.cpp`
  `TrapController` 实现。负责保存现场、写入 `mepc/mcause/mtval`、根据 `mtvec/stvec` 进入 trap、处理 timer/external interrupt 挂起位，以及在 `mret/sret` 时恢复执行。


### `myCPU/tests/asm/`

- `hello.S`：通过 UART MMIO 输出 `Hello, RISC-V!\n` 的最小样例。
- `sum.S`：计算 `1+2+...+10` 并输出 `55` 的整数运算与分支样例。
- `control_flow.S`：验证 `beq`、`jal`、`jalr` 和反向分支回跳的控制流回归样例。
- `csr_trap.S`：验证 CSR 读写、立即数 CSR 指令以及 `ecall`/`mret` 的基础陷入返回路径。
- `timer_interrupt.S`：验证 CLINT 定时器中断、`mtvec` 向量入口以及 `mret` 返回后的继续执行。
- `mtvec_modes.S`：验证 `mtvec` direct/vectored 两种模式下，异常和定时器中断命中正确的 trap 入口。
- `supervisor_timer_interrupt.S`：验证 `mideleg` 驱动的 supervisor timer interrupt 递送、`stvec` 向量入口以及 `sret` 返回。
- `plic_supervisor_external_interrupt.S`：验证 PLIC 驱动的 supervisor external interrupt 递送与 claim/complete。
- `storage_device_basic.S`：验证块化 MMIO storage 的 read/write/error 路径。
- `clint_split_access.S`：验证 CLINT `mtime/mtimecmp` 的 8/4/2/1 字节拆分访问与定时器触发一致性。
- `supervisor_platform_smoke.S`：验证 guest 平台层对 UART/PLIC/storage 契约的最小消费。
- `trap_state.S`：验证 trap 进入/返回时 `mstatus` 的 `MIE/MPIE` 状态变化，以及 `mepc` 的保存与恢复。
- `exception_traps.S`：验证 `ebreak` 与非法指令 trap 的 `mcause`、`mepc`、`mtval` 行为。
- `loads_signed_unsigned.S`：验证 `LB/LBU`、`LH/LHU`、`LW/LWU`、`LD` 的符号扩展与零扩展语义。
- `alu_word.S`：验证 `ADDIW/SLLIW/SRLIW/SRAIW` 与 `ADDW/SUBW/SLLW/SRLW/SRAW` 的 32 位结果截断和符号扩展。
- `branches_signed_unsigned.S`：验证 `BLT/BGE` 与 `BLTU/BGEU` 在相同输入下的 signed/unsigned 语义差异。
- `muldiv.S`：验证 `RV64M` 乘除、取模以及除零边界行为。
- `fence_noop.S`：固定当前 `FENCE/FENCE.I` 在参考模型中的 no-op 行为。
- `csr_semantic_consistency.S`：验证 `misa` 只读语义、`satp.MODE` 受限 WARL 语义，以及 `time` CSR 与 CLINT `mtime` 的一致性。

## 当前项目定位

这个项目当前已经是一个可运行的 RISC-V 裸机模拟器雏形，不只是代码框架。它适合用于理解 ISA、寄存器、取指译码执行流程、异常中断和 MMIO 的基本机制，也能运行简单的汇编裸机程序。

不过它还不是完整系统平台。当前实现仍是单核、顺序参考执行路径，还没有压缩指令 `C` 扩展、完整 privileged CSR 集、真实磁盘协议或 OS 级页分配/页表管理。因此更准确地说，它现在是一个已经能支撑 OS bring-up 前期工作的功能模拟器，并且正在向更模块化的 C++/guest 双侧架构继续演进。
