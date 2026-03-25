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
  reference path 的汇编回归契约。
- [tests/unit](/home/liangjiaqi/projects/my_visual_CPU/myCPU/tests/unit)
  host-side 单元回归。

## 局部规则

- 保留一个简单、正确、可调试的 reference core。
- 不要把同一条指令语义复制到多个 backend 里。
- CPU 访存路径必须继续沿着：
  `CPU -> AddressSpace -> Bus -> Ram/Device`
- 平台事件继续沿着：
  `Device::tick() -> Bus::tick() -> TrapController`
- 任何支持声明都必须以真实实现和回归验证为准。

## 当前已验证能力

当前 simulator 侧已经有回归覆盖的高层能力包括：

- RV64I / RV64M 基础整数与乘除语义。
- 非法整数保留编码稳定触发 `illegal instruction`。
- `DIV/REM/DIVW/REMW` 的 `INT_MIN / -1` 边界按 RISC-V 语义返回。
- ELF / flat binary 加载。
- 纯 BSS `PT_LOAD` zero-fill。
- CSR 指令与基础访问控制。
- M-mode trap / return。
- 初步 `M/S/U` 特权流转。
- `misa` 只读、`satp.MODE` WARL、`counteren`、Sv39、最小 TLB、`sfence.vma`。
- UART / CLINT / PLIC / `SimpleStorage`。
- bus / device 第一轮区间与访问宽度防御。
- 独立 `kernel_alpha` 正向与九条负向 guest 回归。

具体测试列表以 [Makefile](/home/liangjiaqi/projects/my_visual_CPU/myCPU/Makefile) 为准。

## 关键历史节点

- `2026-03-25` 已完成一批 simulator-side correctness 修复：
  - 非法整数编码误执行
  - `DIV/REM` 宿主未定义行为边界
  - ELF pure-BSS `PT_LOAD`
  - bus / device 第一轮边界防御

## 当前仍需关注的问题

- [tests/asm](/home/liangjiaqi/projects/my_visual_CPU/myCPU/tests/asm) 和 [tests/unit](/home/liangjiaqi/projects/my_visual_CPU/myCPU/tests/unit)
  非法编码矩阵、MMIO 非法偏移 / 宽度、更真实 ELF 段布局等鲁棒性回归仍可继续扩展。
- [src/devices/simple_storage.cpp](/home/liangjiaqi/projects/my_visual_CPU/myCPU/src/devices/simple_storage.cpp)
  当前已支持 attached-but-not-ready readiness 注入、bad-magic probe 注入与 `STORAGE_ERR_NOT_READY`，但仍是最小同步块设备：`BLOCK_COUNT = 1`、无 completion interrupt、写入不回写宿主文件。
- [guest/kernel/kernel_runtime.c](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/kernel/kernel_runtime.c)
  `kernel_alpha` 入口的 `trap_context` / `address_space` / `interrupt_state` 已收口为最小 runtime 对象，但仍只是 Phase 1 的早期内核 runtime 骨架。
- [guest/kernel/kernel_bringup.c](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/kernel/kernel_bringup.c)
  通用 `K/M/V` bring-up 已下沉到 guest kernel 基础设施层，`supervisor_demo` 和 `kernel_alpha` 共享同一份早期启动骨架。
- [guest/kernel_alpha/storage_contract.c](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/kernel_alpha/storage_contract.c)
  storage 负向合同已开始从入口下沉到专门 helper，避免六条 storage demo 继续各自手写 probe / read / clear-error 协议细节。
- [guest/kernel_alpha/interrupt_contract.c](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/kernel_alpha/interrupt_contract.c)
  non-storage readiness / panic 合同也已开始从入口下沉到共享 helper，`fault`、`PLIC not-ready`、`timer not-ready` 与标准 interrupt post-handler 不再分散在各入口。
- [src/mem/bus.cpp](/home/liangjiaqi/projects/my_visual_CPU/myCPU/src/mem/bus.cpp) 和 [src/devices](/home/liangjiaqi/projects/my_visual_CPU/myCPU/src/devices)
  已完成第一轮收口，但未来若继续扩设备，仍需要更系统的契约和回归。

## 本子树下一步工作

近期优先级建议如下：

1. 在已修 correctness 基线之上，继续扩充非法编码、MMIO 边界和 ELF 段布局回归。
2. 继续用 `guest_kernel_alpha_demo`、`guest_kernel_alpha_fault_demo`、`guest_kernel_alpha_storage_no_media_demo`、`guest_kernel_alpha_storage_not_ready_demo`、`guest_kernel_alpha_storage_bad_magic_demo`、`guest_kernel_alpha_storage_bad_block_count_demo`、`guest_kernel_alpha_storage_lba_range_demo`、`guest_kernel_alpha_storage_bad_command_demo`、`guest_kernel_alpha_plic_not_ready_demo` 和 `guest_kernel_alpha_timer_not_ready_demo` 验证 simulator 对独立 kernel bring-up 的支撑，再逐步扩 device readiness。
3. 在不打破 reference path 简洁性的前提下，继续完善特权 / CSR / 平台边界。
4. 在保持 Phase 1 已达成核心目标的前提下，先继续做必要稳定化，再讨论多 backend、pipeline、OoO 等后续扩展。

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
