# AGENTS.md

## 适用范围

本文件适用于 [myCPU](/home/liangjiaqi/projects/my_visual_CPU/myCPU) 子树下的 simulator 主体代码、平台设备、加载路径、测试与构建逻辑。

如果工作落在 guest runtime 子树，请继续阅读：

- [guest/AGENTS.md](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/AGENTS.md)

## 当前实现基线

当前 simulator 侧已经落地的关键边界包括：

- `Machine + Bus + Ram + Device`
- `ExecutionBackend + FunctionalBackend + PipelineBackend`
- `DebugSnapshot + DebugSession + debug_protocol`
- `ElfLoader + BinaryLoader`
- `CoreState + CsrFile`
- `TrapController`
- `AddressSpace`
- `InstructionSemantics + ExecutionContext + InsnEffects`
- 按指令族拆分的 `exec/*` 语义模块

当前平台设备包括：

- `Uart16550`
- `Clint`
- `Plic`
- `SimpleStorage`

当前 reference 真值来源仍然是共享 `InstructionSemantics` 和默认 `functional` backend，不要把指令语义复制到多个 backend 里；`pipeline` 只能复用共享语义层，不得另起一套 ISA 解释。

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
- [src/debug](/home/liangjiaqi/projects/my_visual_CPU/myCPU/src/debug)
  调试快照、debug session 与 `--debug-cli` 协议。
- [tests/asm](/home/liangjiaqi/projects/my_visual_CPU/myCPU/tests/asm)
  reference path 的汇编回归契约。
- [tests/unit](/home/liangjiaqi/projects/my_visual_CPU/myCPU/tests/unit)
  host-side 单元回归。

## 局部规则

- 保留一个简单、正确、可调试的 reference core。
- 不要把同一条指令语义复制到多个 backend 里。
- `pipeline` 当前已经具备独立 asm / host / guest 门禁，但仍不是新的 ISA 语义来源；语义修复优先落在共享语义层与公共 simulator 边界。
- `debug/frontend` 当前已经正式接入，但它们只消费 backend / machine / device 的只读快照，不得反向成为执行语义来源。
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
- 更真实的 ELF 多 `PT_LOAD` / mixed data+BSS 布局，以及 ELF header / program-header malformed-input reject。
- CSR 指令与基础访问控制。
- CSR 非法访问矩阵，包括 M/S/U 跨级访问、只读 counter CSR 写保护，以及 `misa` 只读写保护。
- M-mode trap / return。
- 初步 `M/S/U` 特权流转。
- `misa` 只读、`satp.MODE` WARL、`counteren`、Sv39、最小 TLB、`sfence.vma`。
- UART / CLINT / PLIC / `SimpleStorage`。
- bus / device 第一轮区间与访问宽度防御。
- CPU 侧 MMIO 非法 offset / width 稳定触发 access-fault trap。
- host-side MMIO guard 与 contract matrix。
- `Machine` 侧 backend 抽象、共享 ISA 语义层，以及 `pipeline` 的 asm / host / guest 门禁。
- `DebugSnapshot`、`DebugSession`、`--debug-cli` 与本地 `frontend` 教学演示链路。
- 独立 `kernel_alpha` 正向与九条负向 guest 回归。

具体测试列表以 [Makefile](/home/liangjiaqi/projects/my_visual_CPU/myCPU/Makefile) 为准。

## 关键历史节点

- `2026-03-25` 已完成一批 simulator-side correctness 修复：
  - 非法整数编码误执行
  - `DIV/REM` 宿主未定义行为边界
  - ELF pure-BSS `PT_LOAD`
  - bus / device 第一轮边界防御
- `2026-03-26` 已完成第一轮更系统的 Phase 1 hardening 回归扩充：
  - `tests/asm/illegal_integer_encodings.S`
  - `tests/asm/mmio_access_faults.S`
  - `tests/asm/csr_illegal_matrix.S`
  - `tests/unit/elf_loader_segments.cpp`
  - `tests/unit/elf_loader_rejects.cpp`
  - `tests/unit/elf_loader_header_rejects.cpp`
  - `tests/unit/bus_device_guards.cpp`
  - `tests/unit/mmio_contract_matrix.cpp`
- 当前冻结稳定基线 tag 为 `phase1-stable`（`283aee6`），后续 simulator/guest 改动默认应以此为 Phase 1 完成态参考点。

## 当前仍需关注的问题

- [tests/asm](/home/liangjiaqi/projects/my_visual_CPU/myCPU/tests/asm) 和 [tests/unit](/home/liangjiaqi/projects/my_visual_CPU/myCPU/tests/unit)
  非法编码、MMIO 非法偏移 / 宽度、ELF 段布局和 CSR / 特权非法访问回归已经完成第一轮系统扩充，但 `privilege / Sv39 / pipeline differential` 仍可继续补洞。
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

1. 继续稳住 simulator reference path 的 correctness 与可观察性。
2. 在已补第一轮 correctness hardening 矩阵的基础上，继续按合同补洞，而不是重复堆叠非法编码、MMIO 边界、ELF 段布局和 CSR / privilege 同类回归。
3. 继续用 `guest_kernel_alpha_demo`、`guest_kernel_alpha_fault_demo`、`guest_kernel_alpha_storage_no_media_demo`、`guest_kernel_alpha_storage_not_ready_demo`、`guest_kernel_alpha_storage_bad_magic_demo`、`guest_kernel_alpha_storage_bad_block_count_demo`、`guest_kernel_alpha_storage_lba_range_demo`、`guest_kernel_alpha_storage_bad_command_demo`、`guest_kernel_alpha_plic_not_ready_demo` 和 `guest_kernel_alpha_timer_not_ready_demo` 守住 `phase1-stable` bring-up 基线，再把额外 readiness / panic 路径当作 post-Phase1 hardening 渐进扩充。
4. 在不打破 reference path 简洁性的前提下，继续完善特权 / CSR / 平台边界，优先补 `privilege / Sv39` 和 `pipeline` 差分仍未成体系的空洞。
5. 当前对 Phase 2 的近期安排，不再是继续做“正式接入”，而是先按 [docs/design/regression_completion_criteria.md](/home/liangjiaqi/projects/my_visual_CPU/docs/design/regression_completion_criteria.md) 明确出门标准，并继续补强 `pipeline` 的 correctness / differential / robustness 验证。
   当前 `pipeline` 已经正式接入到 asm / host / guest 验证层：默认 `functional` 继续守 `make test`，`pipeline` 通过 `make test-pipeline` 守住同一批 asm 参考输出、host-side smoke/differential，以及 `guest_supervisor_demo` 与 `kernel_alpha` 正负回归。
6. 继续把 `debug/frontend` 限定在“教学演示可用”的最小范围：加载仓库内现有 demo、查看快照、按 cycle / commit 单步，不要在这一层直接扩成带断点 / 条件暂停 / 任意文件加载的通用调试器。

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

如果触及以下任一路径：

- `src/main.cpp`
- `src/platform/machine.cpp`
- `src/debug/*`
- `src/exec/*`
- `src/isa/*`
- `guest/*`
- `tests/host/*`

还应额外守住：

- `cd myCPU && make test-pipeline`

如果触及以下任一路径：

- `src/debug/*`
- `src/main.cpp`
- `../frontend/*`

还应额外守住：

- `cd frontend && node --test`

如果行为变化是有意的，必须同步更新测试或文档说明。
