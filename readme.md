# myCPU — RISC-V 模拟器

当前处于从 C 原型向模块化 C++ 架构迁移的早期阶段。现有功能路径仍以原始 C 核心语义为主，已支持裸机程序执行、UART 串口输出、M-mode 异常/中断处理，以及第一批 supervisor 特权语义地基。

## 目录结构

```
myCPU/
├── src/
│   ├── main.cpp        # C++ 入口，CLI 参数与 Machine 启动
│   ├── cpu.cpp/h       # CPU 外观接口 + 参考执行路径
│   ├── memory.c/h      # 主内存 backing 与底层访存辅助
│   ├── decode.c/h      # 指令解码
│   ├── trap.cpp/h      # TrapController：异常/中断路由与返回
│   ├── arch/           # CoreState / CsrFile 状态边界
│   ├── exec/           # 指令语义分块（integer / control-flow / memory / system）
│   ├── platform/       # C++ Machine 骨架与平台地址映射
│   ├── mem/            # C++ Ram/Bus 骨架
│   ├── devices/        # UART / CLINT 设备对象
│   ├── loader/         # C++ ELF / flat binary 镜像装载边界
│   └── ...
├── tests/asm/          # 汇编测试程序
└── Makefile
```

## 模块关系与运行流程

模拟器从命令行读取镜像路径，通过 `Machine` 组装平台对象，然后进入单步执行循环。整体调用关系如下：

```text
main.cpp
  └── Machine
        ├── Ram                      初始化 128MB 主内存
        ├── Uart16550 / Clint        独立设备对象
        ├── Bus                      分发 RAM 与设备访问
        ├── ElfLoader / BinaryLoader     加载 ELF 或平坦二进制
        ├── cpu_init()               初始化 CoreState、CsrFile
        └── while (!halted)
              └── cpu_step(cpu, bus)
              ├── Bus::tick() 返回平台事件 / 通过 TrapController 检查定时器中断
              ├── bus.load()        取指
              ├── decode()          指令译码
              ├── execute()         执行指令
              │     ├── bus.load()/bus.store()   访问主内存或设备
              │     ├── csr_read()/csr_write()   访问 CSR
              │     └── TrapController 处理 trap 进入/返回
              └── cycle++
```

从模块职责看：

- `main.cpp` 负责 CLI 参数解析和启动 `Machine`。
- `platform/machine.*` 负责组装 CPU、Ram、Bus，并驱动主执行循环。
- `platform/address_map.h` 负责平台地址映射常量，避免设备层依赖 legacy `Memory` 头。
- `arch/core_state.*` 负责通用寄存器、`pc`、周期计数和停机状态。
- `arch/csr_file.*` 负责已实现 CSR 集合、`cycle/time` 等特殊读取规则，以及 `sstatus/sie/sip` 对 `mstatus/mie/mip` 的别名视图。
- `mem/ram.*` 和 `mem/bus.*` 提供平台总线与 RAM 边界。
- `devices/uart16550.*` 和 `devices/clint.*` 提供独立 MMIO 设备对象。
- `devices/device.h` 提供统一设备接口，供 `Bus` 附加和分发。
- `loader/elf_loader.*` 和 `loader/binary_loader.*` 提供镜像装载边界，直接通过 `Ram` 接口写入镜像内容。
- `cpu.cpp/h` 负责把 `CoreState + CsrFile + TrapController` 接回现有参考执行路径。
- `decode.c` 负责把 32 位机器码拆成执行阶段可用的字段。
- `memory.c` 负责主内存访问，MMIO 分发由 `Bus` 与设备对象处理。
- `trap.cpp/h` 负责 `TrapController`，集中处理异常/中断入口、`mret/sret` 返回、定时器中断路由，以及当前最小 `medeleg` 异常委托路径。
- `exec/integer_ops.*`、`exec/control_flow_ops.*`、`exec/memory_ops.*`、`exec/system_ops.*` 负责按指令族拆分参考语义，避免 `cpu.cpp` 持续膨胀。

一次指令执行的数据流可以概括为：

```text
PC
  -> mem_read(取指)
  -> decode(解析 opcode/寄存器/立即数)
  -> execute(算术/跳转/访存/CSR/系统指令)
  -> 可能访问 memory / UART / CLINT
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

`make test` 会构建汇编样例，并校验 UART 输出是否与预期一致；单个样例异常卡死时会超时失败。当前除综合回归外，还包含 `loads_signed_unsigned`、`alu_word`、`branches_signed_unsigned`、`muldiv`、`fence_noop` 这类更细粒度的指令族回归，以及 `privilege_transitions`、`sret_transitions`、`supervisor_exception_delegation`、`csr_access_control` 这类特权/CSR 回归。

## 内存映射

| 地址范围 | 设备 |
|---|---|
| `0x80000000` + 128MB | 主内存 |
| `0x10000000` | UART (16550) |
| `0x02000000` | CLINT (定时器) |

## 已实现特性

- RV64I 基础整数指令集
- RV64M 乘除法扩展
- ELF64 程序加载
- CSR 指令（CSRRW/CSRRS/CSRRC 及立即数变体）
- M-mode 异常与中断（ECALL、EBREAK、MRET）
- 第一批 M/S/U 特权语义：`MPP` 跟踪、`ecall` cause 区分、`sret` 返回
- 基于 `medeleg` 的最小 supervisor 异常委托
- CSR 特权级/只读属性检查，非法访问触发 illegal-instruction trap
- UART MMIO（写入直接输出到 stdout）
- CLINT 定时器中断
- `ecall` a7=93 退出约定

## 源码文件说明

### 根目录

- `readme.md`：项目总说明，包含功能、编译、运行和测试方式。
- `docs/request.md`：课程项目背景与目标说明，描述了“从 0 实现一个可运行程序的指令集模拟器”的教学目标。

### `myCPU/`

- `Makefile`：本地编译规则、RISC-V 汇编样例构建规则和 `make test` 测试入口。
- `mycpu`：编译产物，运行后加载并执行 RISC-V 程序镜像。

### `myCPU/src/`

- `main.cpp`
  程序入口。负责解析 `-b` 参数、创建 `Machine`，并根据镜像类型调用 `load_elf()` 或 `load_binary()` 后启动执行。

- `platform/machine.h`
  `Machine` 类声明。聚合 `CPU`、`Ram` 和 `Bus`，为后续平台化重构提供统一入口。

- `platform/machine.cpp`
  `Machine` 实现。当前负责镜像加载、`cpu_init()` 调用和执行循环，镜像装载通过 `ElfLoader/BinaryLoader` 完成，执行阶段仍复用现有参考语义。

- `platform/address_map.h`
  平台地址映射常量定义。集中声明 RAM、UART、CLINT 的基地址与大小，供入口、设备和 legacy 内存 backing 共享。

- `mem/ram.h`
  `Ram` 类声明。负责主内存生命周期、RAM 范围内的 load/store，以及供 loader 使用的 bulk write/fill 接口。

- `mem/ram.cpp`
  `Ram` 实现。内部调用现有 `mem_init()/mem_free()` 管理 RAM backing，并通过显式 RAM 接口暴露单点和批量写入能力。

- `mem/bus.h`
  `Bus` 类声明。维护统一设备映射表，并向上暴露统一的 load/store/tick 接口；`tick()` 返回平台事件而不是暴露具体设备状态。

- `mem/bus.cpp`
  `Bus` 实现。负责设备附加、地址分发以及平台 tick 结果汇总；RAM 也作为总线设备接入，不再保留专门的 RAM 分支。

- `devices/device.h`
  设备基类声明。定义统一的 `contains/load/store` 接口，供平台总线附加和寻址。

- `devices/uart16550.h`
  `Uart16550` 类声明。封装最小 16550 串口寄存器访问与 stdout 输出行为。

- `devices/uart16550.cpp`
  `Uart16550` 实现。当前支持最小发送路径和 `LSR` 就绪读取。

- `devices/clint.h`
  `Clint` 类声明。封装 `mtime/mtimecmp`、定时器 tick 和 MMIO 访问接口。

- `devices/clint.cpp`
  `Clint` 实现。负责定时器状态推进、`mtime/mtimecmp` 读写，并在 tick 时返回是否产生待处理中断。

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
  `CsrFile` 实现。负责 CSR 状态复位、普通 CSR 读写，以及 `cycle/time` 这类特殊读取规则。

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
  系统/CSR 指令执行实现。负责 `ecall`、`ebreak`、`mret` 以及 CSR 读改写语义，并通过 `TrapController` 进入 trap 路径。

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
  `TrapController` 实现。负责保存现场、写入 `mepc/mcause/mtval`、根据 `mtvec` 进入 trap、处理定时器中断挂起位，并在 `mret` 时从 `mepc` 恢复执行。


### `myCPU/tests/asm/`

- `hello.S`：通过 UART MMIO 输出 `Hello, RISC-V!\n` 的最小样例。
- `sum.S`：计算 `1+2+...+10` 并输出 `55` 的整数运算与分支样例。
- `control_flow.S`：验证 `beq`、`jal`、`jalr` 和反向分支回跳的控制流回归样例。
- `csr_trap.S`：验证 CSR 读写、立即数 CSR 指令以及 `ecall`/`mret` 的基础陷入返回路径。
- `timer_interrupt.S`：验证 CLINT 定时器中断、`mtvec` 向量入口以及 `mret` 返回后的继续执行。
- `mtvec_modes.S`：验证 `mtvec` direct/vectored 两种模式下，异常和定时器中断命中正确的 trap 入口。
- `trap_state.S`：验证 trap 进入/返回时 `mstatus` 的 `MIE/MPIE` 状态变化，以及 `mepc` 的保存与恢复。
- `exception_traps.S`：验证 `ebreak` 与非法指令 trap 的 `mcause`、`mepc`、`mtval` 行为。
- `loads_signed_unsigned.S`：验证 `LB/LBU`、`LH/LHU`、`LW/LWU`、`LD` 的符号扩展与零扩展语义。
- `alu_word.S`：验证 `ADDIW/SLLIW/SRLIW/SRAIW` 与 `ADDW/SUBW/SLLW/SRLW/SRAW` 的 32 位结果截断和符号扩展。
- `branches_signed_unsigned.S`：验证 `BLT/BGE` 与 `BLTU/BGEU` 在相同输入下的 signed/unsigned 语义差异。
- `muldiv.S`：验证 `RV64M` 乘除、取模以及除零边界行为。
- `fence_noop.S`：固定当前 `FENCE/FENCE.I` 在参考模型中的 no-op 行为。

## 当前项目定位

这个项目当前已经是一个可运行的 RISC-V 裸机模拟器雏形，不只是代码框架。它适合用于理解 ISA、寄存器、取指译码执行流程、异常中断和 MMIO 的基本机制，也能运行简单的汇编裸机程序。

不过它还不是完整系统平台。当前实现以 4 字节指令取指为主，尚未看到压缩指令 `C` 扩展的执行支持；分页/MMU、操作系统运行支撑等内容也还没有展开。因此更准确地说，它现在是一个偏教学和验证用途的最小 RISC-V 模拟器，并且正在向更模块化的 C++ 架构演进。
