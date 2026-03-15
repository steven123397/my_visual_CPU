# myCPU — RISC-V 模拟器

用 C 实现的 RV64IMC 模拟器，支持裸机程序执行、UART 串口输出、M-mode 异常/中断处理。

## 目录结构

```
myCPU/
├── src/
│   ├── main.c          # 入口，CLI 参数，加载镜像，主循环
│   ├── cpu.c/h         # 寄存器、PC、fetch-decode-execute
│   ├── memory.c/h      # 物理内存 + MMIO 分发
│   ├── decode.c/h      # 指令解码
│   ├── trap.c/h        # 异常/中断入口与返回
│   └── elf_loader.c    # ELF64 解析与加载
├── tests/asm/          # 汇编测试程序
└── Makefile
```

## 模块关系与运行流程

模拟器从命令行读取镜像路径，初始化内存与 CPU，然后进入单步执行循环。整体调用关系如下：

```text
main.c
  ├── mem_init()                    初始化 128MB 主内存和 MMIO 状态
  ├── elf_load()/mem_load_binary()  加载 ELF 或平坦二进制
  ├── cpu_init()                    初始化寄存器、PC、CSR
  └── while (!halted)
        └── cpu_step()
              ├── 更新 mtime / 检查定时器中断
              ├── mem_read()        取指
              ├── decode()          指令译码
              ├── execute()         执行指令
              │     ├── mem_read()/mem_write()   访问主内存或 MMIO
              │     ├── csr_read()/csr_write()   访问 CSR
              │     └── trap_enter()/trap_return()
              └── cycle++
```

从模块职责看：

- `main.c` 负责 CLI 参数、镜像加载和主循环驱动。
- `cpu.c` 负责寄存器、PC、CSR 和指令执行。
- `decode.c` 负责把 32 位机器码拆成执行阶段可用的字段。
- `memory.c` 负责主内存和 MMIO 分发。
- `trap.c` 负责异常/中断入口与返回。
- `elf_loader.c` 负责 ELF64 装载。

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
sudo apt install gcc-riscv64-unknown-elf
make test
```

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

- `main.c`
  程序入口。负责解析 `-b` 参数、初始化 `Memory`、加载 ELF 或平坦二进制、初始化 `CPU`，并循环调用 `cpu_step()` 直到停机。

- `cpu.h`
  CPU 状态和接口定义，包含 32 个通用寄存器、`pc`、CSR 空间、周期计数和停机标志。

- `cpu.c`
  核心执行器。实现 `cpu_init()`、`csr_read()/csr_write()`、`execute()`、`check_interrupts()` 和 `cpu_step()`，覆盖算术、分支、访存、乘除、CSR 和系统指令执行。

- `decode.h`
  `Insn` 指令结构定义，供译码阶段和执行阶段共享。

- `decode.c`
  译码器实现。根据 opcode 判断 I/S/B/U/J 等格式，提取 `rd/rs1/rs2`、`funct3/funct7` 和符号扩展后的立即数。

- `memory.h`
  地址空间和 `Memory` 结构定义，包含主内存、UART、CLINT 的基地址，以及 `mtime/mtimecmp` 状态。

- `memory.c`
  内存和 MMIO 实现。`mem_read()/mem_write()` 会先判断是否访问 UART 或 CLINT，否则落到主内存；`mem_load_binary()` 用于把平坦二进制直接装入指定地址。

- `trap.h`
  异常/中断入口与返回接口声明。

- `trap.c`
  M-mode 陷入处理实现。`trap_enter()` 负责保存现场、写入 `mepc/mcause/mtval` 并跳转到 `mtvec`；`trap_return()` 负责从 `mepc` 恢复执行。

- `elf_loader.c`
  简化版 ELF64 加载器。检查 ELF 文件头，遍历程序头装载 `PT_LOAD` 段，把段内容写入模拟内存，并对 BSS 区域清零。

### `myCPU/tests/asm/`

- `hello.S`：通过 UART MMIO 输出 `Hello, RISC-V!\n` 的最小样例。
- `sum.S`：计算 `1+2+...+10` 并输出 `55` 的整数运算与分支样例。

## 当前项目定位

这个项目当前已经是一个可运行的 RISC-V 裸机模拟器雏形，不只是代码框架。它适合用于理解 ISA、寄存器、取指译码执行流程、异常中断和 MMIO 的基本机制，也能运行简单的汇编裸机程序。

不过它还不是完整系统平台。当前实现以 4 字节指令取指为主，尚未看到压缩指令 `C` 扩展的执行支持；分页/MMU、操作系统运行支撑等内容也还没有展开。因此更准确地说，它现在是一个偏教学和验证用途的最小 RISC-V 模拟器。
