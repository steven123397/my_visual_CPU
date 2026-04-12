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
- 独立 `Spike` 外部差分当前已覆盖基础 ALU / control-flow / trap / delegated privilege、第一批 device-free `Sv39/page fault` final-state 子集，以及带 `mret / sret` 的 returning trap handler first-trap checkpoint summary；这条线保持独立离线入口，不进入默认 `make test` / `make test-pipeline` 依赖。
- `Phase 3-A` 首轮分支预测增强：最小 `branch_predictor` 子模块、`jal` static predict-taken、条件分支 `2-bit` bimodal counter + target 记忆，以及继续复用现有 flush / redirect 的 mispredict 恢复路径。
- `pipeline_backend_smoke` 当前还额外覆盖真实 `CLINT` / `PLIC+UART` 平台事件源驱动的 supervisor timer / external interrupt smoke，避免把 cycle-sensitive 设备递送硬塞进 functional-vs-pipeline 逐事件差分。
- `pipeline_backend_smoke` 当前也已补上 `jal` predict-hit、predictable branch loop、以及 backend rebuild 后 predictor cold-reset 的 host-side smoke。
- `pipeline` 当前已经具备 `sequence_id` / bounded retire trace、共享 `commit boundary` helper，以及拆开的 `pipeline_core_state` / `pipeline_hazards`；相关边界由 `pipeline_commit_trace_smoke`、`pipeline_speculation_contracts_smoke`、`pipeline_backend_smoke`、`backend_differential_smoke` 与 `debug_cli_smoke` 一起守住。
- `Phase 3-B/C` 当前已接上首轮最小 `rename + ROB + LSQ +` 真实 `OoO execute` 主路径：decode 侧会完成 `rename + ROB allocate`，non-memory 指令可直接把结果写入 phys-state 与 `ROB ready`，`ROB head` 已成为真实的顺序退休入口，RAM / faulting access 会通过最小独立 memory execute 形成可被 younger ALU 越过的完成窗口；当前 `LSQ` 已能显式区分 `blocked_by_unresolved_store`、`blocked_by_overlapping_store` 与 `replay_required` 这三类 memory-order 状态，backend 已具备最小 coarse automatic replay flush，并且 `step_mem(load)` 已支持 `RAM-only` full-cover store-to-load forwarding，其中 RAM / MMIO store 仍只会在 commit boundary 真正落地，已知 MMIO load 则继续维持 non-speculative 执行。
- `Phase 3-B/C` 的 rollback 合同当前也已接到统一 flush 路径：mispredict、trap、commit-boundary interrupt service 与 trap-return flush 会一起回滚 speculative `rename / ROB / phys / LSQ` younger state；`RenameMap` 现在同时维护 committed / speculative mapping 与 free-list，ROB head commit 会回收 stale phys，phys tag 也已扩为 `uint32_t` 以支撑长 guest 路径。
- 这轮 phys free-list / recycle 收口还补出并修正了一条实际回归：如果 recycled phys 在后续再次成为 committed live phys，free-list 必须在 commit 时把它移除；否则 trap-return / interrupt flush 之后会把 live phys 再次发出。当前 `rename_map_smoke` 与 `timer_interrupt (pipeline)` 已共同守住这条边界。
- `pipeline_rename_commit_smoke`、`pipeline_speculation_contracts_smoke` 与 `load_store_queue_smoke` 当前分别守住 `rename + ROB commit +` 最小真实 OoO execute、中间态 rollback / non-speculative store / coarse automatic replay / RAM-only forwarding 合同，以及 `LSQ` ready / retire / flush / replay-needed / forwarding 接口。
- `BinaryLoader` 直接单测与 `Machine::load_elf()/load_binary()` reload/reset 回归已经接入现有门禁，当前明确语义是“替换 RAM 并 reset CPU/backend”。
- `DebugSnapshot`、`DebugSession`、`--debug-cli` 与本地 `frontend` 教学演示链路；当前 `debug_cli_smoke` 已用自包含 flat-binary 覆盖 delegated supervisor timer / external interrupt 的中间态与完成态快照、predictor mode / counters / 最近一次预测字段，以及最小 `ROB / LSQ` 队列深度、head-sequence、`lsq_load_state / lsq_load_sequence_id / lsq_store_sequence_id`、`replay_flush` 和 `stall_reason` 观测面，守住 `CLINT` / `PLIC` / `UART`、predictor 和 OoO readiness 可观察性输出。
- 真实 `debug server + mycpu --debug-cli` 端到端 smoke 与 Node/C++ 两侧预算常量收口已经落地，当前最小调试链路已进入现有门禁。
- `2026-04-05` 又补上一组更窄的 `debug/frontend` 压力验证：Node/runtime 级持续 `run/pause`、运行中 session replacement、高吞吐 terminal 输入聚合，以及 `DebugCliSession` timeout fail-closed，避免迟到 CLI 响应错配后续请求。
- `2026-04-05` 也已把 decode 级 `BlockedByUnresolvedStore` 收窄到“仅 older store 地址未知才阻塞”；地址已知但 data 未 ready 的 older store 已不再全局阻塞非重叠 younger load，重叠场景继续走 `BlockedByOverlappingStore`，相关 `load_store_queue_smoke`、`pipeline_speculation_contracts_smoke` 与 `make test-pipeline` 已守住。
- `2026-04-11` 已在 `V4` 首刀之上补上一轮更窄的 direct dependency hardening：pending serializing vector 仍会阻塞 younger vector ALU，但 ready older non-memory vector ALU 如果只是被更老 scalar ROB head 挡住 commit，direct dependent younger vector ALU 现在可以以前驱 materialized result 完成 execute；对应 `vector_pipeline_smoke` 也已补上更像真实依赖链的 host 回归。
- `2026-04-11` 同日也补上一轮更窄的 vector memory hardening：`vle.v / vse.v` 在 commit boundary 现在会先对整段 span 做预校验；live `MMIO` 与非 RAM span 会直接 `access-fault` fail-closed，不再留下 UART 输入消费、UART 输出 / `IER` 改写或 RAM 部分写入副作用；对应 `vector_vlite_smoke` 已补齐 UART / RAM fault 回归。
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
- [src/debug](src/debug) 和 [../frontend](../frontend)
  当前最小调试链路已经正式接入并可用，Node/runtime 级持续 `run`、session replacement、高吞吐 terminal 输入聚合、repeated `run/pause` 长会话、`reset` cadence 与真实 `interactive_os` e2e 回归也已补上；对当前单用户、本地教学/调试使用，这组门禁已经足够。
- [src/platform/machine.cpp](src/platform/machine.cpp)
  `Machine::load_elf()/load_binary()` 当前语义已经明确为“替换 RAM 并 reset CPU/backend”，但这还不是完整平台 reset；设备状态是否也要复位，仍是后续独立设计问题。
- [src/exec/load_store_queue.cpp](src/exec/load_store_queue.cpp) 和 [src/exec/pipeline_backend.cpp](src/exec/pipeline_backend.cpp)
  decode 级 `BlockedByUnresolvedStore` 当前最小收窄已经落地：它只保留给 older store 地址未知场景；地址已知但 data 未 ready 的 older store 不再全局阻塞非重叠 younger load。后续这条线的取舍判断也已经完成：在当前 decode 级 load 前置分类、单 `ex_mem` memory 通道与 coarse replay flush 基线上，不主动继续扩大更激进的 `issue / replay / speculation`；只有在出现真实 workload 证据或明确研究目标时，才值得重开，且应先看 issue decoupling。
- [src/exec/reorder_buffer.cpp](src/exec/reorder_buffer.cpp)、[src/exec/pipeline_backend_execute.cpp](src/exec/pipeline_backend_execute.cpp) 和 [tests/host/vector_pipeline_smoke.cpp](tests/host/vector_pipeline_smoke.cpp)
  当前 `V4` 已经补上第一轮 direct dependency hardening：pending serializing vector 仍会阻塞 younger vector ALU，但 ready older non-memory vector ALU 如果只是被更老 scalar ROB head 挡住 commit，direct dependent younger vector ALU 现在可以以前驱 materialized result 完成 execute。后续若继续扩，优先仍是 bug-driven hardening 与更窄 workload 观察，不直接跳到向量 memory path / lane 模型。
- [tests/host/spike_differential/*](tests/host/spike_differential)
  当前独立 `Spike` 外部差分已经接上 first-trap checkpoint，但仍只保持“单次运行抓首个 trap 入口 + 最终态”的最小形态；后续按真实 bug 或明确收益补更广 `Sv39 / privilege` 或更复杂多 checkpoint 变体，不主动扩大到设备场景、`configure hook` 或逐提交 trace。
- [src/debug](src/debug) 和 [src/exec/pipeline_backend.cpp](src/exec/pipeline_backend.cpp)
  当前 debug snapshot / CLI 已新增更窄的 `stall_reason` 观测，可直接区分 `blocked_by_unresolved_store`、`blocked_by_overlapping_store`、`memory_path_busy`、`non_ram_load_waiting_for_rob_head`、`serializing_system_wait_for_rob_head`、`source_operands_not_ready` 与 `decode_backpressure`；后续若要重开 `Phase 3`，应优先用这组观测去判断 stall hotspot，而不是先拍脑袋扩 speculation。
- [guest/kernel/kernel_runtime.c](guest/kernel/kernel_runtime.c)
  `kernel_alpha` 入口的 `trap_context` / `address_space` / `interrupt_state` 已收口为最小 runtime 对象；当前 `supervisor_demo` 的入口级 trap bring-up、`interactive_os` 的 identity-superpage bring-up、common bring-up options 的默认 self-context 装配，以及 `PLIC / first delivery / storage probe/signature` 这组早期 phase helper 也已继续下沉到这里，但整体仍只是 Phase 1 的早期内核 runtime 骨架。
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
2. 在已接通的 correctness hardening、loader、guest smoke、debug smoke 和 `pipeline` 门禁基础上，继续按新增 bug 或新合同补最小回归，不重复堆叠低收益变体。
3. `debug/frontend` 当前不再主动扩大浏览器端压力验证；后续按真实 bug 或明确新需求补最小回归，不要在这一层抢跑断点、条件暂停或更大 UI / 协议面。
4. `Phase 3` 当前不主动继续扩大更激进的 `issue / replay / speculation`；后续若出现真实 stall hotspot，再优先评估 issue decoupling 这类更有结构收益的最小切片，而不是回头重复讨论已完成的 decode 边界。
5. 继续用 `make test`、`make test-pipeline`、loader 单测、`debug_cli_smoke`、`interactive_terminal_smoke` 和 guest 正负回归守住当前稳定基线，不让 `pipeline` 与调试链路反向污染 reference path。
6. 当前 `V-lite` `V0 / V1`、`V2`、`V3`、一轮更窄的 `V3 hardening`、`V4` 首刀与第一轮更窄的 `V4` hardening 都已落地：shared semantics、`functional` reference path、最小 host 回归、non-memory vector ALU 的最小 vector-aware execute/commit 边界、独立最小 guest 向量 demo、固定 `conv -> relu` 的最小 CNN-style guest demo，以及守住 mixed `SEW/VL` `conv -> relu` 链路、全负 `relu` 零钳位、ready older vector producer -> direct dependent consumer 依赖链，以及 pending serializing vector guard 的 host smoke 都已接通。当前更健康的下一步不是顺势扩到 `Pool / FC`、向量 load/store path 或更重 `Phase 4`，而是继续围绕已落地的 `V4` 边界做 bug-driven hardening，再决定下一刀。

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
- `tests/host/interactive_terminal_smoke.cpp`
- `tests/host/vector_pipeline_smoke.cpp`

还应至少额外关注：

- `cd myCPU && make test-host-physical_register_file_smoke`
- `cd myCPU && make test-host-rename_map_smoke`
- `cd myCPU && make test-host-reorder_buffer_smoke`
- `cd myCPU && make test-host-load_store_queue_smoke`
- `cd myCPU && make test-host-pipeline_rename_commit_smoke`
- `cd myCPU && make test-host-pipeline_speculation_contracts_smoke`
- `cd myCPU && make test-host-debug_cli_smoke`
- `cd myCPU && make test-host-interactive_terminal_smoke`
- `cd myCPU && make test-host-vector_pipeline_smoke`
- `cd myCPU && make test-host-vector_cnn_smoke`

如果触及以下任一路径：

- `src/platform/machine.cpp`
- `src/loader/*`

还应额外关注：

- `cd myCPU && make test-unit-binary_loader`
- `cd myCPU && make test-unit-machine_loader_reset`

如果触及以下任一路径：

- `src/debug/*`
- `src/main.cpp`
- `../frontend/*`

还应额外守住：

- `cd frontend && node --test`

如果行为变化是有意的，必须同步更新测试或文档说明。
