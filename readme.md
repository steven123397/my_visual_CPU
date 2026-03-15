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
