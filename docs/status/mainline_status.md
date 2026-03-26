# 主线状态

## 文档定位

本文档用于记录 `phase1-stable` 冻结后、`pipeline core` 与 `debug/frontend` 已完成正式接入之后，当前主线仍需继续推进的工程任务。

它面向下一轮实现工作，重点回答：

- Phase 1 近期主线还剩什么
- 已接入的 Phase 2 能力当前按什么方式继续推进
- 新对话继续工作时，应该优先看哪些入口文档和验证门禁

## 关联文档

- 相关设计：
  - [design/regression_completion_criteria.md](/home/liangjiaqi/projects/my_visual_CPU/docs/design/regression_completion_criteria.md)
  - [design/cpp_refactor_design.md](/home/liangjiaqi/projects/my_visual_CPU/docs/design/cpp_refactor_design.md)
  - [design/minimal_interactive_os_design.md](/home/liangjiaqi/projects/my_visual_CPU/docs/design/minimal_interactive_os_design.md)
- 当前计划：
  - 当前暂无专门的主线执行计划；后续如有新任务，再单独建立对应 `plan` 文档。
- 已完成计划：
  - [plan/docs_information_architecture_reorg_plan.md](/home/liangjiaqi/projects/my_visual_CPU/docs/plan/docs_information_architecture_reorg_plan.md)
  - [plan/phase1-hardening-regressions_plan.md](/home/liangjiaqi/projects/my_visual_CPU/docs/plan/phase1-hardening-regressions_plan.md)
  - [plan/pipeline_core_integration_plan.md](/home/liangjiaqi/projects/my_visual_CPU/docs/plan/pipeline_core_integration_plan.md)
  - [plan/debug_frontend_integration_plan.md](/home/liangjiaqi/projects/my_visual_CPU/docs/plan/debug_frontend_integration_plan.md)

## 当前状态

当前主线已经稳定成立的事实如下：

- 仓库当前已经是一个已可运行的模拟器原型，而不是纯设计稿。
- `phase1-stable`（`283aee6`）对应的 Phase 1 核心 bring-up 冻结基线已经形成。
- 默认 `functional` reference path、独立 `kernel_alpha` 正向与九条负向回归、`make test` 主门禁均已打通。
- `pipeline core`、`--backend pipeline`、`make test-pipeline`、`debug_session/protocol`、本地 Node 调试服务与浏览器前端教学演示链路都已经正式接入。

这意味着当前主线不再把 `pipeline` 与 `debug/frontend` 视为“待合入功能”，而是把它们视为已经落地、需要继续稳定化的现有能力。

关于当前主线中“回归相关工作做到什么程度可认为阶段性收口”的统一判断口径，见：

- [design/regression_completion_criteria.md](/home/liangjiaqi/projects/my_visual_CPU/docs/design/regression_completion_criteria.md)

## 2026-03-26 补充进展

本轮主线已经完成一批新的 hardening 回归扩充：

- `tests/asm/illegal_integer_encodings.S` 已扩展更多非法整数编码样本。
- `tests/asm/mmio_access_faults.S` 已把 CPU 侧非法 MMIO 访问的 access-fault 合同接入 asm 门禁。
- `tests/unit/elf_loader_segments.cpp`、`tests/unit/elf_loader_rejects.cpp` 和 `tests/unit/elf_loader_header_rejects.cpp` 已补上更真实的 ELF segment/layout 与 malformed-input reject 回归。
- `tests/unit/bus_device_guards.cpp` 与 `tests/unit/mmio_contract_matrix.cpp` 已把 host-side MMIO guard / contract 做到第一轮矩阵化。
- `tests/asm/csr_illegal_matrix.S` 已把 CSR 非法访问、跨特权级访问和只读 CSR 写入的 trap 合同补成第一轮矩阵。
- `tests/asm/sv39_mprv.S` 已把 `mstatus.MPRV` 驱动的 Sv39 数据访存合同接入 asm 门禁；当前 `M-mode` 下 `load/store` 在 `MPRV=1` 时，会按 `MPP` 指定的有效特权级走地址翻译，并遵守 `SUM/MXR` 权限检查。
- `tests/asm/sv39_pagewalk_contracts.S` 已把 Sv39 page-walk 的 misaligned superpage 与 non-leaf PTE 保留位合同接入 asm 门禁；当前 non-leaf PTE 不再把带 `U/A/D` 保留位的条目误当成有效下级页表。
- `guest/kernel/kernel_runtime.c` 已新增 `kernel_runtime_run_bringup()`，把 common bring-up options 里的 runtime/self-context 装配下沉到 guest runtime 基础设施层；当前 `kernel_alpha` 入口与 interrupt/storage contract helper 不再手写 `pre_vm_context = &runtime`，相关边界已由 `tests/unit/kernel_runtime.c`、`tests/unit/kernel_alpha_interrupt.c` 与 `tests/unit/kernel_alpha_storage.c` 守住。
- `tests/asm/sv39_exec_privilege.S` 已把 Sv39 `U/S` 特权边界补成一条更完整的 asm 合同；当前 `S-mode` 对 `U=1` 可执行页的取指，以及 `U-mode` 对 supervisor-only 可执行页 / data page 的取指、load、store都会稳定触发 page fault，这条合同也已同时守住 `pipeline` asm 门禁。
- `tests/host/address_space_faults_smoke.cpp` 已补上 `AddressSpace::fetch32_result()` / `fetch32()` 在 Sv39 instruction page fault 下的接口合同；当前 result API 会返回 fault 而不落 trap CSR，legacy wrapper 仍会稳定进入 trap。
- `tests/host/backend_differential_smoke.cpp` 已新增 `delegated_user_ecall_to_supervisor`、`sret_to_user_halt`、`machine_timer_interrupt_at_cycle_start`、`sv39_mprv_fault`、`sv39_instruction_page_fault`、`sv39_load_page_fault`、`sv39_store_page_fault`、`sv39_reserved_non_leaf_fault`、`supervisor_mmio_instruction_access_fault`、`supervisor_mmio_load_access_fault`、`supervisor_mmio_store_access_fault`、`supervisor_timer_interrupt_after_mret`、`supervisor_timer_interrupt_after_sip_write`、`supervisor_timer_interrupt_after_sie_write`、`supervisor_timer_interrupt_after_sstatus_write`、`supervisor_timer_interrupt_at_cycle_start`、`user_mode_supervisor_timer_interrupt_at_cycle_start`、`user_mode_supervisor_external_interrupt_after_sret`、`supervisor_external_interrupt_after_sip_write` 与 `user_mode_supervisor_external_interrupt_at_cycle_start` 二十条 host-side 差分场景；当前已把 machine timer interrupt 基线、delegated user-ecall / `sret` privilege transition、delegated supervisor page-fault、delegated supervisor MMIO instruction/load/store access-fault，以及由 `sip/sie/sstatus/mret/sret` 驱动的 supervisor timer / external interrupt 在 S-mode / U-mode 下的 cycle-start / commit-boundary 稳定路径接入差分门禁。
- `tests/host/pipeline_backend_smoke.cpp` 已补上 `CLINT` 驱动的 supervisor timer interrupt 与 `PLIC+UART` 驱动的 supervisor external interrupt 两条 host-side smoke，用来守真实平台事件源在 `pipeline` 下的 flush / handler / return 基线；这两类路径按 cycle 前进，不再强行并入 functional-vs-pipeline 的逐事件差分。
- `tests/host/debug_cli_smoke.cpp` 已改成自包含 flat-binary smoke：当前会直接驱动最小 supervisor timer / external interrupt 场景，并检查 `DebugSnapshot` 中 `CLINT` / `PLIC` / `UART` 的关键字段，以及 `trap` / `flush` / `halt` 等 pipeline 可观察性事件。
- `frontend/tests/debug_server.test.mjs` 已把本地调试服务的 richer snapshot / event 透传和 WebSocket 广播接入 Node 门禁；当前会检查 `CLINT` / `PLIC` / `UART` 关键字段、pipeline `trap_flush / committed` 标志，以及 `run` 中重新 `load` 会先停掉旧会话定时器，避免教学 demo 在会话切换后被后台偷偷推进。
- `pipeline` 本轮又修正了一处 commit-boundary 时序：由刚退休 CSR 写入或 `mret/sret` trap-return 新触发可递送的 pending interrupt，不再让返回后的 younger 指令继续退休；同时保留“对 cycle 起点已可递送的 interrupt 仍可抢占”的路径，避免 tight loop 下的 timer interrupt 饥饿。
- `design/regression_completion_criteria.md` 已成为当前 Phase 1 / Phase 2 回归收口的正式判断口径。
- `docs/` 正式文档已经收口到 `background / design / plan / status + AGENTS.md + index.md` 结构，后续不再新增 `contracts / templates / archive / superpowers` 这类平行正式目录。

## Phase 1 近期主线

当前仍应优先推进的 Phase 1 / post-Phase1 主线工作如下：

1. 继续稳住 simulator reference path 的 correctness 与可观察性。
2. 在已落地第一轮 illegal / MMIO / ELF / CSR hardening 矩阵的基础上，继续按合同补洞，而不是重复堆叠同类回归。
3. 继续守住 `kernel_alpha` 十条回归基线：
   - `kernel_alpha_demo`
   - `kernel_alpha_fault_demo`
   - `kernel_alpha_storage_no_media_demo`
   - `kernel_alpha_storage_not_ready_demo`
   - `kernel_alpha_storage_bad_magic_demo`
   - `kernel_alpha_storage_bad_block_count_demo`
   - `kernel_alpha_storage_lba_range_demo`
   - `kernel_alpha_storage_bad_command_demo`
   - `kernel_alpha_plic_not_ready_demo`
   - `kernel_alpha_timer_not_ready_demo`
4. 继续推进 guest runtime 的 process / runtime refinement 与大文件拆分，尤其守住 `vm*`、`trap*`、`kernel_runtime`、`kernel_bringup` 当前已经形成的边界。

这些工作仍然是近期主线，不应因为 `pipeline` / `debug/frontend` 已接入而被搁置。

## Phase 2 当前安排

当前对 Phase 2 的理解和安排如下：

- `pipeline core` 与 `debug/frontend` 的正式接入工作已经完成。
- 近期不再把 Phase 2 理解成“继续搬运更多旧分支代码”，而是进入稳定化和验证补强阶段。
- 当前最重要的 Phase 2 工程问题，不是继续扩 UI 或继续引入新模型，而是按已新增的回归收口标准把出门条件落实到差分和快照门禁。

在这个前提下，Phase 2 近期优先级如下：

1. 当前仓库对 Phase 2 的最小完成标准已经基本成立，后续优先维护既有门禁而不是继续扩功能面。
2. 继续维护 `pipeline` 的 correctness / differential / robustness 门禁；新增问题出现时补最小回归，而不是重复堆叠同类 case。
3. 继续把 `debug/frontend` 限定在“教学演示可用”的最小范围，重点守住快照结构、协议输出和本地测试门禁。
4. 在上述工作稳定之前，不急着把更多执行模型或更大的调试功能面并入当前主线。

## 当前仍然有效的风险 / 限制

- reference robustness 回归已经完成第一轮系统扩充；当前 `pipeline differential` 的高风险主干矩阵已基本形成闭环，后续剩余工作主要转为低收益变体控制和新增 bug 的定向回归。
- guest runtime 虽已完成第一轮拆分，但 `vm*`、`trap*`、`kernel_runtime`、`kernel_bringup` 仍需要继续守住边界，避免回退到单个大文件。
- `kernel_alpha` 已经达到 Phase 1 核心完成态，但更多 device readiness / fault / panic / runtime refinement 仍属于 post-Phase1 hardening。
- `pipeline` 已经正式可用，privileged / trap / interrupt / MMIO 行为的一致性验证主干也已基本成体系；后续以维护既有差分门禁和按新增问题补最小持久回归为主。
- `debug/frontend` 已经正式接入，但仍应避免膨胀成断点 / 条件暂停 / 任意文件加载的通用调试器。

## 下一步

1. 先沿 reference path 继续维护已形成闭环的 `privilege / Sv39`、illegal / MMIO / ELF / CSR 合同矩阵，并在新增 bug 出现时补最小持久回归。
2. 继续把 `kernel_alpha` 十条回归和 `guest_supervisor_demo` 守在稳定输出上。
3. 继续推进 guest runtime 的 process / runtime refinement 与大文件拆分，但避免破坏现有层次边界。
4. 当前 Phase 2 的最小收口已经基本成立；后续按 [design/regression_completion_criteria.md](/home/liangjiaqi/projects/my_visual_CPU/docs/design/regression_completion_criteria.md) 以维护既有 `pipeline` 差分 / 快照门禁和新增 bug 定向回归为主，而不是继续做低收益 case 堆叠。
5. 在不扩功能面的前提下，继续维护 `debug/frontend` 教学演示链路的稳定测试。

## 建议入口

新对话如果要继续推进当前主线工作，建议优先阅读：

- [AGENTS.md](/home/liangjiaqi/projects/my_visual_CPU/AGENTS.md)
- [myCPU/AGENTS.md](/home/liangjiaqi/projects/my_visual_CPU/myCPU/AGENTS.md)
- [myCPU/guest/AGENTS.md](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/AGENTS.md)
- [design/regression_completion_criteria.md](/home/liangjiaqi/projects/my_visual_CPU/docs/design/regression_completion_criteria.md)
- [status/code_self_review_status.md](/home/liangjiaqi/projects/my_visual_CPU/docs/status/code_self_review_status.md)
- [status/kernel_alpha_status.md](/home/liangjiaqi/projects/my_visual_CPU/docs/status/kernel_alpha_status.md)

## 验证基线

- `cd myCPU && make test`
- `cd myCPU && make test-pipeline`
- `cd frontend && node --test`
- `cd myCPU && make test-unit-supervisor_runtime`
- `cd myCPU && make test-unit-kernel_runtime`
- `cd myCPU && make test-unit-kernel_alpha_common`
- `cd myCPU && make test-unit-kernel_alpha_interrupt`
- `cd myCPU && make test-unit-kernel_alpha_storage`
- `cd myCPU && make test-guest-supervisor_demo`
- `cd myCPU && make test-guest-kernel_alpha_demo`
- `cd myCPU && make test-guest-kernel_alpha_fault_demo`
- `cd myCPU && make test-guest-kernel_alpha_storage_no_media_demo`
- `cd myCPU && make test-guest-kernel_alpha_storage_not_ready_demo`
- `cd myCPU && make test-guest-kernel_alpha_storage_bad_magic_demo`
- `cd myCPU && make test-guest-kernel_alpha_storage_bad_block_count_demo`
- `cd myCPU && make test-guest-kernel_alpha_storage_lba_range_demo`
- `cd myCPU && make test-guest-kernel_alpha_storage_bad_command_demo`
- `cd myCPU && make test-guest-kernel_alpha_plic_not_ready_demo`
- `cd myCPU && make test-guest-kernel_alpha_timer_not_ready_demo`
