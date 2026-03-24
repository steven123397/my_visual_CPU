# AGENTS.md

## 适用范围

本文件适用于 [myCPU](/home/liangjiaqi/projects/my_visual_CPU/myCPU) 子树下的 simulator 主体代码、平台设备、加载路径、测试与构建逻辑。

如果工作落在 guest runtime 子树，请继续阅读：

- [guest/AGENTS.md](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/AGENTS.md)

## 当前实现基线

当前 simulator 侧已经落地的关键边界包括：

- `Machine + Bus + Ram + Device`
- `ElfLoader + BinaryLoader`
- `CoreState + CsrFile`
- `TrapController`
- `AddressSpace`
- 按指令族拆分的 `exec/*` 语义模块

当前平台设备包括：

- `Uart16550`
- `Clint`
- `Plic`
- `SimpleStorage`

当前参考路径仍然是单一的 fetch-decode-execute loop，不要把它打散成多个语义来源。

## 模块地图

- [src/main.cpp](/home/liangjiaqi/projects/my_visual_CPU/myCPU/src/main.cpp)
  CLI 参数、镜像选择、`Machine` 启动。
- [src/platform/machine.cpp](/home/liangjiaqi/projects/my_visual_CPU/myCPU/src/platform/machine.cpp)
  平台组装、镜像加载、执行循环。
- [src/cpu.cpp](/home/liangjiaqi/projects/my_visual_CPU/myCPU/src/cpu.cpp)
  CPU facade、取指/译码/执行接线。
- [src/arch](/home/liangjiaqi/projects/my_visual_CPU/myCPU/src/arch)
  `CoreState` / `CsrFile`。
- [src/trap.cpp](/home/liangjiaqi/projects/my_visual_CPU/myCPU/src/trap.cpp)
  trap / interrupt 路由与返回。
- [src/mem](/home/liangjiaqi/projects/my_visual_CPU/myCPU/src/mem)
  `Ram` / `Bus` / `AddressSpace`。
- [src/devices](/home/liangjiaqi/projects/my_visual_CPU/myCPU/src/devices)
  平台设备对象。
- [src/loader](/home/liangjiaqi/projects/my_visual_CPU/myCPU/src/loader)
  ELF / binary 装载边界。
- [tests/asm](/home/liangjiaqi/projects/my_visual_CPU/myCPU/tests/asm)
  当前 reference path 的汇编回归契约。

## 本子树的局部规则

- 保留一个简单、正确、可调试的 reference core。
- 不要把同一条指令语义复制到多个 backend 里。
- CPU 访存路径必须继续沿着：
  `CPU -> AddressSpace -> Bus -> Ram/Device`
- 平台事件继续沿着：
  `Device::tick() -> Bus::tick() -> TrapController`
- 新的结构拆分应围绕真实复杂度增长点，而不是为了“目录更像架构图”。
- 任何支持声明都必须以真实实现和回归验证为准。

## 当前已验证能力

当前 simulator 侧已经有回归覆盖的高层能力包括：

- RV64I / RV64M 基础整数与乘除语义
- ELF / flat binary 加载
- CSR 指令与基础访问控制
- M-mode trap / return
- 初步 `M/S/U` 特权流转
- 约束版 `medeleg` / `mideleg`
- 约束版 `mie` / `mip` / `sie` / `sip`
- `misa` 只读与 `satp.MODE` WARL
- machine counter CSR 与 `counteren`
- Sv39、最小 TLB、`sfence.vma`
- UART / CLINT / PLIC / SimpleStorage

具体测试列表以 [Makefile](/home/liangjiaqi/projects/my_visual_CPU/myCPU/Makefile) 为准。

## 当前已知问题

当前最需要优先处理的 simulator-side 问题见：

- [docs/code_self_review_2026-03-24.md](/home/liangjiaqi/projects/my_visual_CPU/docs/code_self_review_2026-03-24.md)

其中和本子树直接相关的重点包括：

- [src/exec/integer_ops.cpp](/home/liangjiaqi/projects/my_visual_CPU/myCPU/src/exec/integer_ops.cpp)
  对部分非法整数编码的判定仍不够严格。
- [src/exec/integer_ops.cpp](/home/liangjiaqi/projects/my_visual_CPU/myCPU/src/exec/integer_ops.cpp)
  `DIV/REM/DIVW/REMW` 的有符号溢出边界会触发宿主未定义行为。
- [src/loader/elf_loader.cpp](/home/liangjiaqi/projects/my_visual_CPU/myCPU/src/loader/elf_loader.cpp)
  对纯 BSS `PT_LOAD` 段的处理不完整。
- [src/mem/bus.cpp](/home/liangjiaqi/projects/my_visual_CPU/myCPU/src/mem/bus.cpp) 和 [src/devices](/home/liangjiaqi/projects/my_visual_CPU/myCPU/src/devices)
  对设备区间重叠和非法 MMIO 访问的防御偏弱。

## 本子树下一步工作

近期优先级建议如下：

1. 先修 reference correctness 问题，再继续扩功能。
2. 在不打破 reference path 简洁性的前提下，继续完善特权 / CSR / 平台边界。
3. 等 Phase 1 稳定后，再讨论多 backend、pipeline、OoO 等后续扩展。

## 验证要求

只要触及以下路径之一：

- `src/cpu.cpp`
- `src/trap.cpp`
- `src/arch/*`
- `src/mem/*`
- `src/devices/*`
- `src/loader/*`
- `tests/asm/*`

默认都应守住：

- `cd myCPU && make test`

如果行为变化是有意的，必须同步更新测试或文档说明。
