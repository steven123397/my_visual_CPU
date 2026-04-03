# AGENTS.md

## 适用范围

本文件适用于 [myCPU](.) 子树下的 simulator 主体代码、平台设备、加载路径、测试与构建逻辑。

如果工作落在 guest runtime 子树，请继续阅读：

- [guest/AGENTS.md](guest/AGENTS.md)

## 当前实现基线

当前 simulator 侧已经落地的关键边界包括：

- `Machine + Bus + Ram + Device`
- `ExecutionBackend + FunctionalBackend + PipelineBackend`
- `pipeline_sequence + pipeline_commit_boundary + pipeline_core_state + pipeline_hazards`
- `rename_map + reorder_buffer + load_store_queue`
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

- [src/main.cpp](src/main.cpp)
  CLI 参数、镜像选择、`Machine` 启动。
- [src/platform/machine.cpp](src/platform/machine.cpp)
  平台组装、镜像加载、执行循环。
- [src/cpu.cpp](src/cpu.cpp)
  CPU facade、取指/译码/执行接线。
- [src/arch](src/arch)
  `CoreState` / `CsrFile`。
- [src/trap.cpp](src/trap.cpp)
  trap / interrupt 路由与返回。
- [src/mem](src/mem)
  `Ram` / `Bus` / `AddressSpace`。
- [src/devices](src/devices)
  平台设备对象。
- [src/loader](src/loader)
  ELF / binary 装载边界。
- [src/debug](src/debug)
  调试快照、debug session 与 `--debug-cli` 协议。
- [src/exec](src/exec)
  `pipeline` / predictor / commit boundary / `rename + ROB + LSQ +` 最小 OoO execute 主路径。
- [tests/asm](tests/asm)
  reference path 的汇编回归契约。
- [tests/unit](tests/unit)
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
- `misa` 只读、`satp.MODE` WARL、`counteren`、Sv39、最小 TLB、`satp` 写入后的本地 TLB 刷新、`sfence.vma`。
- `mstatus.MPRV` 数据访存语义，按 `MPP` 走 Sv39 翻译与 `SUM/MXR` 权限检查。
- Sv39 page-walk 的 misaligned superpage 与 non-leaf reserved-bit fault 合同。
- Sv39 特权边界：`S-mode` 对 `U=1` 可执行页的取指，以及 `U-mode` 对 supervisor-only 可执行页 / data page 的取指、load、store都会稳定触发 page fault；当前该合同已进入 asm / pipeline 主门禁，host-side `AddressSpace` result API 也已补 smoke。
- UART / CLINT / PLIC / `SimpleStorage`。
- bus / device 第一轮区间与访问宽度防御。
- CPU 侧 MMIO 非法 offset / width 稳定触发 access-fault trap。
- host-side MMIO guard 与 contract matrix。
- `Machine` 侧 backend 抽象、共享 ISA 语义层，以及 `pipeline` 的 asm / host / guest 门禁。
- `pipeline` host-side differential 当前已覆盖基础 ALU / 控制流 / trap-return / illegal trap、machine timer interrupt cycle-start baseline、delegated user-ecall / `sret` privilege transition、`Sv39 + MPRV`、delegated instruction/load/store-page-fault、delegated supervisor MMIO instruction/load/store access-fault、reserved page-walk fault，以及由 `sip/sie/sstatus/mret/sret` 驱动的 supervisor timer/external interrupt 在 S-mode / U-mode 下的 cycle-start / commit-boundary 场景；用户态 delegated supervisor timer / external interrupt 已都纳入差分门禁。
- `Phase 3-A` 首轮分支预测增强：最小 `branch_predictor` 子模块、`jal` static predict-taken、条件分支 `2-bit` bimodal counter + target 记忆，以及继续复用现有 flush / redirect 的 mispredict 恢复路径。
- `pipeline_backend_smoke` 当前还额外覆盖真实 `CLINT` / `PLIC+UART` 平台事件源驱动的 supervisor timer / external interrupt smoke，避免把 cycle-sensitive 设备递送硬塞进 functional-vs-pipeline 逐事件差分。
- `pipeline_backend_smoke` 当前也已补上 `jal` predict-hit、predictable branch loop、以及 backend rebuild 后 predictor cold-reset 的 host-side smoke。
- `pipeline` 当前已经具备 `sequence_id` / bounded retire trace、共享 `commit boundary` helper，以及拆开的 `pipeline_core_state` / `pipeline_hazards`；相关边界由 `pipeline_commit_trace_smoke`、`pipeline_speculation_contracts_smoke`、`pipeline_backend_smoke`、`backend_differential_smoke` 与 `debug_cli_smoke` 一起守住。
- `Phase 3-B/C` 当前已接上首轮最小 `rename + ROB + LSQ +` 真实 `OoO execute` 主路径：decode 侧会完成 `rename + ROB allocate`，non-memory 指令可直接把结果写入 phys-state 与 `ROB ready`，`ROB head` 已成为真实的顺序退休入口，RAM / faulting access 会通过最小独立 memory execute 形成可被 younger ALU 越过的完成窗口；当前 `LSQ` 已能显式区分 `blocked_by_unresolved_store`、`blocked_by_overlapping_store` 与 `replay_required` 这三类 memory-order 状态，backend 已具备最小 coarse automatic replay flush，并且 `step_mem(load)` 已支持 `RAM-only` full-cover store-to-load forwarding，其中 RAM / MMIO store 仍只会在 commit boundary 真正落地，已知 MMIO load 则继续维持 non-speculative 执行。
- `Phase 3-B/C` 的 rollback 合同当前也已接到统一 flush 路径：mispredict、trap、commit-boundary interrupt service 与 trap-return flush 会一起回滚 speculative `rename / ROB / phys / LSQ` younger state；`RenameMap` 现在同时维护 committed / speculative mapping 与 free-list，ROB head commit 会回收 stale phys，phys tag 也已扩为 `uint32_t` 以支撑长 guest 路径。
- 这轮 phys free-list / recycle 收口还补出并修正了一条实际回归：如果 recycled phys 在后续再次成为 committed live phys，free-list 必须在 commit 时把它移除；否则 trap-return / interrupt flush 之后会把 live phys 再次发出。当前 `rename_map_smoke` 与 `timer_interrupt (pipeline)` 已共同守住这条边界。
- `pipeline_rename_commit_smoke`、`pipeline_speculation_contracts_smoke` 与 `load_store_queue_smoke` 当前分别守住 `rename + ROB commit +` 最小真实 OoO execute、中间态 rollback / non-speculative store / coarse automatic replay / RAM-only forwarding 合同，以及 `LSQ` ready / retire / flush / replay-needed / forwarding 接口。
- `DebugSnapshot`、`DebugSession`、`--debug-cli` 与本地 `frontend` 教学演示链路；当前 `debug_cli_smoke` 已用自包含 flat-binary 覆盖 delegated supervisor timer / external interrupt 的中间态与完成态快照、predictor mode / counters / 最近一次预测字段，以及最小 `ROB / LSQ` 队列深度、head-sequence、`lsq_load_state / lsq_load_sequence_id / lsq_store_sequence_id` 与 `replay_flush` 观测面，守住 `CLINT` / `PLIC` / `UART`、predictor 和 OoO readiness 可观察性输出。
- 独立 `kernel_alpha` 正向与九条负向 guest 回归。

具体测试列表以 [Makefile](Makefile) 为准。

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

- [tests/asm](tests/asm) 和 [tests/unit](tests/unit)
  非法编码、MMIO 非法偏移 / 宽度、ELF 段布局和 CSR / 特权非法访问回归已经完成第一轮系统扩充；`pipeline differential` 的高风险主干场景也已基本闭环，后续主要按新增 bug 或明确新合同做最小补洞。
- [src/devices/simple_storage.cpp](src/devices/simple_storage.cpp)
  当前已支持 attached-but-not-ready readiness 注入、bad-magic probe 注入与 `STORAGE_ERR_NOT_READY`，但仍是最小同步块设备：`BLOCK_COUNT = 1`、无 completion interrupt、写入不回写宿主文件。
- [guest/kernel/kernel_runtime.c](guest/kernel/kernel_runtime.c)
  `kernel_alpha` 入口的 `trap_context` / `address_space` / `interrupt_state` 已收口为最小 runtime 对象；当前 common bring-up options 的默认 self-context 装配，以及 `PLIC / first delivery / storage probe/signature` 这组早期 phase helper 也已继续下沉到这里，但整体仍只是 Phase 1 的早期内核 runtime 骨架。
- [guest/kernel/kernel_bringup.c](guest/kernel/kernel_bringup.c)
  通用 `K/M/V` bring-up 已下沉到 guest kernel 基础设施层，`supervisor_demo` 和 `kernel_alpha` 共享同一份早期启动骨架。
- [guest/kernel_alpha/storage_contract.c](guest/kernel_alpha/storage_contract.c)
  storage 负向合同已开始从入口下沉到专门 helper，避免六条 storage demo 继续各自手写 probe / read / clear-error 协议细节。
- [guest/kernel_alpha/interrupt_contract.c](guest/kernel_alpha/interrupt_contract.c)
  non-storage readiness / panic 合同也已开始从入口下沉到共享 helper，`fault`、`PLIC not-ready`、`timer not-ready` 与标准 interrupt post-handler 不再分散在各入口。
- [src/mem/bus.cpp](src/mem/bus.cpp) 和 [src/devices](src/devices)
  已完成第一轮收口，但未来若继续扩设备，仍需要更系统的契约和回归。
- [src/exec/branch_predictor.cpp](src/exec/branch_predictor.cpp)
  当前仍是 `Phase 3-A` 首轮最小 predictor：条件分支使用 `2-bit` counter + target 记忆，`jal` 走静态 predict-taken，`jalr` 仍不预测；后续应先以 bug-driven hardening 和最小回归补洞为主，不急着扩成复杂 BTB / RAS 组合。

## 本子树下一步工作

近期优先级建议如下：

1. 继续稳住 simulator reference path 的 correctness 与可观察性。
2. 在已补第一轮 correctness hardening 矩阵的基础上，继续按合同补洞，而不是重复堆叠非法编码、MMIO 边界、ELF 段布局和 CSR / privilege 同类回归。
3. 继续用 `guest_kernel_alpha_demo`、`guest_kernel_alpha_fault_demo`、`guest_kernel_alpha_storage_no_media_demo`、`guest_kernel_alpha_storage_not_ready_demo`、`guest_kernel_alpha_storage_bad_magic_demo`、`guest_kernel_alpha_storage_bad_block_count_demo`、`guest_kernel_alpha_storage_lba_range_demo`、`guest_kernel_alpha_storage_bad_command_demo`、`guest_kernel_alpha_plic_not_ready_demo` 和 `guest_kernel_alpha_timer_not_ready_demo` 守住 `phase1-stable` bring-up 基线，再把额外 readiness / panic 路径当作 post-Phase1 hardening 渐进扩充。
4. 在不打破 reference path 简洁性的前提下，继续完善特权 / CSR / 平台边界；当前 `privilege / Sv39` 与 `pipeline` 差分主干已经基本成体系，后续以新增 bug 的最小持久回归为主。
5. 当前对 Phase 2 的近期安排，不再是继续做“正式接入”；`pipeline` 的最小 differential / robustness 收口已经基本成立，后续重点转为维护既有门禁、控制低收益 case 膨胀，并在新增问题出现时补最小回归。
   当前 `pipeline` 已经正式接入到 asm / host / guest 验证层：默认 `functional` 继续守 `make test`，`pipeline` 通过 `make test-pipeline` 守住同一批 asm 参考输出、host-side smoke/differential，以及 `guest_supervisor_demo` 与 `kernel_alpha` 正负回归。
6. 继续把 `debug/frontend` 限定在“教学演示可用”的最小范围：加载仓库内现有 demo、查看快照、按 cycle / commit 单步，不要在这一层直接扩成带断点 / 条件暂停 / 任意文件加载的通用调试器。
7. `Phase 3-B/C` 的首轮 `rename + ROB + LSQ + phys free-list / recycle`、最小 `LSQ replay-needed` 合同、coarse automatic replay recovery、`RAM-only` store-to-load forwarding 与最小真实 `OoO execute` 已经落地；下一步优先继续守住现有 host / guest / debug 门禁，按新增 bug 补最小持久回归，再考虑更激进的 issue / replay / memory speculation。

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

如果改动主要集中在以下 helper 或对应 smoke：

- `src/exec/physical_register_file.*`
- `src/exec/rename_map.*`
- `src/exec/reorder_buffer.*`
- `src/exec/load_store_queue.*`
- `tests/host/physical_register_file_smoke.cpp`
- `tests/host/rename_map_smoke.cpp`
- `tests/host/reorder_buffer_smoke.cpp`
- `tests/host/load_store_queue_smoke.cpp`
- `tests/host/pipeline_rename_commit_smoke.cpp`
- `tests/host/pipeline_speculation_contracts_smoke.cpp`
- `tests/host/debug_cli_smoke.cpp`

还应至少额外关注：

- `cd myCPU && make test-host-physical_register_file_smoke`
- `cd myCPU && make test-host-rename_map_smoke`
- `cd myCPU && make test-host-reorder_buffer_smoke`
- `cd myCPU && make test-host-load_store_queue_smoke`
- `cd myCPU && make test-host-pipeline_rename_commit_smoke`
- `cd myCPU && make test-host-pipeline_speculation_contracts_smoke`
- `cd myCPU && make test-host-debug_cli_smoke`

如果触及以下任一路径：

- `src/debug/*`
- `src/main.cpp`
- `../frontend/*`

还应额外守住：

- `cd frontend && node --test`

如果行为变化是有意的，必须同步更新测试或文档说明。
