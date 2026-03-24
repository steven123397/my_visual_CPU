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
- [tests/unit](/home/liangjiaqi/projects/my_visual_CPU/myCPU/tests/unit)
  当前 host-side 单元回归，覆盖 loader 和 bus/device 边界防御。
- [guest/kernel_alpha/main.c](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/kernel_alpha/main.c)
  独立 kernel alpha bring-up 入口，当前回归其自建页表、MMIO lazy map 和 timer interrupt 最小路径。

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
- 非法整数保留编码稳定触发 `illegal instruction`
- `DIV/REM/DIVW/REMW` 的 `INT_MIN / -1` 边界按 RISC-V 语义返回
- ELF / flat binary 加载
- 纯 BSS `PT_LOAD` 段 zero-fill
- CSR 指令与基础访问控制
- M-mode trap / return
- 初步 `M/S/U` 特权流转
- 约束版 `medeleg` / `mideleg`
- 约束版 `mie` / `mip` / `sie` / `sip`
- `misa` 只读与 `satp.MODE` WARL
- machine counter CSR 与 `counteren`
- Sv39、最小 TLB、`sfence.vma`
- UART / CLINT / PLIC / SimpleStorage
- 设备区间重叠防御，以及第一轮 MMIO 非法访问宽度白名单
- 独立 `guest_kernel_alpha_demo` 回归，验证 guest 侧自建 Sv39 内核页表、UART / CLINT lazy map 和第一次 supervisor timer interrupt

具体测试列表以 [Makefile](/home/liangjiaqi/projects/my_visual_CPU/myCPU/Makefile) 为准。

## 当前已知问题

当前最需要优先处理的 simulator-side 问题见：

- [docs/code_self_review_2026-03-24.md](/home/liangjiaqi/projects/my_visual_CPU/docs/code_self_review_2026-03-24.md)

其中当前和本子树直接相关、但尚未完全收口的重点包括：

- [tests/asm](/home/liangjiaqi/projects/my_visual_CPU/myCPU/tests/asm) 和 [tests/unit](/home/liangjiaqi/projects/my_visual_CPU/myCPU/tests/unit)
  非法编码矩阵、MMIO 非法偏移/宽度、更真实 ELF 段布局等鲁棒性回归仍可继续扩展。
- [src/devices/simple_storage.cpp](/home/liangjiaqi/projects/my_visual_CPU/myCPU/src/devices/simple_storage.cpp)
  仍是最小同步块设备：`BLOCK_COUNT = 1`、无 completion interrupt、写入不回写宿主文件。
- [src/mem/bus.cpp](/home/liangjiaqi/projects/my_visual_CPU/myCPU/src/mem/bus.cpp) 和 [src/devices](/home/liangjiaqi/projects/my_visual_CPU/myCPU/src/devices)
  已完成第一轮边界防御，但未来若继续扩平台设备，仍需要更系统的契约和回归。

## 本子树下一步工作

近期优先级建议如下：

1. 在已修 reference correctness 基线之上，继续扩充非法编码、MMIO 边界和 ELF 段布局回归。
2. 继续用 `guest_kernel_alpha_demo` 验证 simulator 对独立 kernel bring-up 的支撑，再逐步扩 fault / panic / device readiness。
3. 在不打破 reference path 简洁性的前提下，继续完善特权 / CSR / 平台边界。
4. 等 Phase 1 稳定后，再讨论多 backend、pipeline、OoO 等后续扩展。

## 验证要求

只要触及以下路径之一：

- `src/cpu.cpp`
- `src/trap.cpp`
- `src/arch/*`
- `src/mem/*`
- `src/devices/*`
- `src/loader/*`
- `tests/asm/*`
- `tests/unit/*`

默认都应守住：

- `cd myCPU && make test`

如果行为变化是有意的，必须同步更新测试或文档说明。
