# AGENTS.md

## 作用

这是仓库根目录的总览指引文件。

使用顺序：

1. 先读本文件，理解项目范围、阶段目标和全局约定。
2. 进入具体子树后，再读对应子目录下的 `AGENTS.md`。

本仓库后续只维护 `AGENTS.md` 体系，不再维护 `CLAUDE.md`。

## 项目概况

仓库当前主体是 [myCPU](myCPU)，一个从 C 原型逐步演进到模块化 C++ 架构的小型 RISC-V 模拟器。

当前定位：

- 已经是一个已可运行的模拟器原型，不是纯设计稿。
- 当前已达成 Phase 1 核心 bring-up 目标，正处于 Phase 1 冻结后的稳定化，以及Phase2、3、4的有序推进阶段。
- 正在同步推进一轮有明确结构收益的 C++ 重构。

长期目标：

- 先把模拟器扩展到足以支撑一个自制的小型 OS / kernel bring-up。
- 在保持统一 ISA 语义来源的前提下，继续深化 pipeline，并逐步进入 OoO、cache、multicore 等更复杂模型。

## 仓库结构

- [myCPU](myCPU)
  核心模拟器代码、guest runtime、测试和构建脚本。
- [frontend](frontend)
  本地调试服务、浏览器前端和 Node 测试。
- [docs](docs)
  按 `background / design / plan / status` 组织的正式技术文档，以及统一入口 [docs/index.md](docs/index.md)。
- [README.md](README.md)
  面向读者的项目概览、构建和运行说明。

## 子目录 AGENTS 索引

- [myCPU/AGENTS.md](myCPU/AGENTS.md)
  simulator 主体说明：CPU、CSR、trap、MMU、platform、device、loader、tests 的当前实现与局部规则。
- [myCPU/guest/AGENTS.md](myCPU/guest/AGENTS.md)
  guest runtime 说明：memory、PMM、VM、trap、runtime、user task/program 与独立 kernel alpha bring-up 的当前边界和下一步。
- [docs/AGENTS.md](docs/AGENTS.md)
  文档维护规则：四分法目录职责、模板、创建条件、完成态回写规则与索引要求。

## 当前状态

当前仓库已经具备以下高层能力：

- RV64I / RV64M 参考执行路径。
- `functional` / `pipeline` 两种执行后端。
- `pipeline` 当前已具备首轮 `rename + ROB + LSQ +` 最小真实 `OoO execute` 主路径。
- ELF64 与 flat binary 加载。
- 基础 CSR 访问与 M-mode trap。
- 初步 `M/S/U` 特权路径。
- 最小 UART / CLINT / PLIC / MMIO block storage 平台。
- Sv39、最小 TLB、`sfence.vma`。
- Sv39 特权边界：`S-mode` 对 `U=1` 可执行页的取指，以及 `U-mode` 对 supervisor-only 可执行页 / data page 的取指、load、store都会稳定触发 page fault。
- 一套最小 guest supervisor runtime。
- 一条独立的 `kernel_alpha` bring-up 路径及其正负回归。
- 一条本地 `debug_session/protocol + frontend` 教学演示链路。

当前 guest 侧已经打通：

- early allocator。
- bitmap PMM。
- guest-side Sv39 page table / address space。
- trap / runtime / process / user task / user program 分层。
- 最小 U-mode enter / return。
- timer / external / page-fault / user-`ecall` smoke 路径。
- 独立 kernel alpha 的 boot / PMM / Sv39 / external interrupt / timer interrupt / storage readiness probe / storage read 正向路径。
- 独立 kernel alpha 的 CLINT unmapped、timer not-ready、PLIC not-ready、storage no-media、storage not-ready、storage bad-magic、storage bad-block-count、storage LBA-range 与 storage bad-command 九条负向路径。
- 当前冻结稳定基线 tag 为 `phase1-stable`（`283aee6`），后续 guest/runtime 收口默认按 post-Phase1 hardening 理解。

最近一轮关键历史节点只保留以下几项：

- `2026-04-05` 已完成 decode 级收窄之后的 `Phase 3` 后续取舍评估：在当前单发射、decode 级 load 前置分类、单 memory execute 通道与 coarse replay flush 基线上，不主动继续扩大更激进的 `issue / replay / speculation`；若未来重开，优先先看 issue decoupling 是否值得做。
- `2026-04-05` 同日也已补上一层更窄的 `pipeline stall attribution` 观测：当前 debug snapshot / CLI 已能直接暴露 `stall_reason`，为后续是否值得重开 issue decoupling 提供更直接的证据。
- `2026-04-05` 已把 decode 级 `BlockedByUnresolvedStore` 串行化边界收窄到“仅 older store 地址未知才阻塞”；地址已知但 data 未 ready 的 older store 不再全局阻塞非重叠 younger load，重叠场景继续走 `BlockedByOverlappingStore`。
- `2026-04-05` 已为 `debug/frontend` 补上一组更窄的压力验证：Node/runtime 级持续 `run/pause`、运行中 session replacement、高吞吐 terminal 输入聚合，以及 `DebugCliSession` timeout fail-closed，避免迟到 CLI 响应错配后续请求。
- `2026-04-05` 同日也继续把 `debug/frontend` 压力验证外推到 repeated `run/pause` 长会话、`reset` 后 terminal reset / offset 重启语义，以及真实 `debug server + mycpu --debug-cli` 下 `guest_interactive_os_demo` 的 `run/pause + terminal-input` e2e。
- `2026-04-04` 已完成 `P1` 最后一批结构收口与 `P2` 首轮验证补洞两轮收口：`BinaryLoader` 直接单测、`Machine::load_elf()/load_binary()` 最小 reload/reset 回归、`supervisor_demo_smoke` / `user_program_smoke` 更窄单测、真实 `debug server + mycpu --debug-cli` 端到端 smoke、Node/C++ 两侧调试预算常量收口，以及 `pipeline` mega-smoke 拆分都已进入现有门禁。
- `2026-03-25` 已完成一批 simulator-side correctness 修复：非法整数编码、`DIV/REM` 溢出边界、ELF pure-BSS `PT_LOAD`、bus / device 第一轮边界防御。
- `2026-03-26` 已完成一轮更系统的 Phase 1 hardening 回归扩充：非法编码样本扩展、CPU 侧 MMIO access-fault asm、ELF segment/reject/header 单元回归、host-side MMIO contract matrix，以及 CSR illegal matrix 均已接入现有门禁。
- `2026-03-26` 已新增 [docs/design/regression_completion_criteria.md](docs/design/regression_completion_criteria.md)，作为当前 Phase 1 / Phase 2 回归收口标准。
- `kernel_alpha_demo` 已完成首个可回归 alpha bring-up，当前正向输出为 `KMVPETDS`。
- `kernel_alpha_fault_demo` 当前负向输出为 `KMVX`。
- `kernel_alpha_storage_no_media_demo` 当前负向输出为 `KMVNX`。
- `kernel_alpha_storage_not_ready_demo` 当前负向输出为 `KMVRX`。
- `kernel_alpha_storage_bad_magic_demo` 当前负向输出为 `KMVGX`。
- `kernel_alpha_storage_bad_block_count_demo` 当前负向输出为 `KMVBX`。
- `kernel_alpha_storage_lba_range_demo` 当前负向输出为 `KMVLX`。
- `kernel_alpha_storage_bad_command_demo` 当前负向输出为 `KMVCX`。
- `kernel_alpha_plic_not_ready_demo` 当前负向输出为 `KMVPX`。
- `kernel_alpha_timer_not_ready_demo` 当前负向输出为 `KMVPETX`。

## 当前优先级

决策顺序保持不变：

0. 先把当前原型整理成更稳的 C++ 结构边界。
1. 保持正确、可调试的 ISA 级 reference model。
2. 把模拟器推进到足以支撑小型 OS / kernel bring-up。
3. 只有在 Phase 1 稳定后，才进入更多执行模型和微架构扩展。

不要把这几条路线混在同一实现步骤里。

## 当前焦点

当前阶段的主线工作是：

- 继续稳住 simulator reference path 的 correctness 与可观察性。
- 继续沿已落地的 hardening 矩阵，维护非法编码、MMIO 边界、ELF 段布局以及特权 / CSR 合同闭环，并按新增 bug 补最小回归。
- 把独立 `kernel_alpha` 十条回归基线维持在可回归的 Phase 1 完成态，并继续做 bug-driven hardening。
- `P1` 结构收口和 `P2` 首轮验证补洞已经全部完成；当前不再把重点放在继续扩功能面。
- `debug/frontend` 当前已经补上 Node/runtime 级持续 `run/pause`、session replacement、高吞吐 terminal 聚合、repeated `run/pause` 长会话、`reset` cadence 与真实 `interactive_os` e2e；对当前单用户、本地教学/调试使用，这组门禁已经足够，后续按真实 bug 或明确新需求补最小回归即可。
- `Phase 3` 的 decode 级 `BlockedByUnresolvedStore` 最小收窄之后，主线判断已经完成：当前不主动继续扩大更激进的 `issue / replay / speculation`；若未来重开，优先先看 issue decoupling 这类有明确结构收益的最小切片，而不是直接放大 memory speculation / replay。
- 继续把 `pipeline`、loader/debug smoke 和 guest runtime 保持在当前已接入、可验证的范围内，不让它们反向污染 reference path。

相关状态文档见：

- [docs/status/mainline_status.md](docs/status/mainline_status.md)
- [docs/status/project_priority_roadmap.md](docs/status/project_priority_roadmap.md)
- [docs/design/regression_completion_criteria.md](docs/design/regression_completion_criteria.md)
- [docs/status/kernel_alpha_status.md](docs/status/kernel_alpha_status.md)

## 技术栈

- host simulator：C + C++17
- guest runtime：C11 + RISC-V assembly
- 构建：GNU Make
- 交叉工具链：`riscv64-unknown-elf-gcc` / `riscv64-unknown-elf-objcopy`
- 文档：Markdown

## 全局开发约定

- 任何实现改动都应优先维护 reference path 的正确性与可观察性。
- README、`docs/` 和各层 `AGENTS.md` 必须与当前实现同步。
- 状态文档优先保留当前状态、少量关键历史节点和下一步，不要长期堆积已完成 checklist。
- 不要做没有结构收益的纯 cosmetic 重写或纯语言迁移。
- 优先小步落地，避免一次引入过大的抽象。
- 不要提交构建产物，尤其是：
  - `myCPU/guest/supervisor_demo.elf`
  - `myCPU/guest/kernel_alpha_demo.elf`
  - `myCPU/guest/kernel_alpha_fault_demo.elf`
  - `myCPU/guest/kernel_alpha_storage_no_media_demo.elf`
  - `myCPU/guest/kernel_alpha_storage_not_ready_demo.elf`
  - `myCPU/guest/kernel_alpha_storage_bad_magic_demo.elf`
  - `myCPU/guest/kernel_alpha_storage_bad_block_count_demo.elf`
  - `myCPU/guest/kernel_alpha_storage_lba_range_demo.elf`
  - `myCPU/guest/kernel_alpha_storage_bad_command_demo.elf`
  - `myCPU/guest/kernel_alpha_plic_not_ready_demo.elf`
  - `myCPU/guest/kernel_alpha_timer_not_ready_demo.elf`

## Agent 默认工作流

除非用户明确要求跳过、简化或改顺序，否则后续对话默认按下面流程推进。

### 实现 / 设计类任务

1. 先确认上下文。
   至少阅读仓库根 `AGENTS.md`、目标子树 `AGENTS.md`、相关 `status/design` 文档，并在预计会改代码或文档时先本地确认 `git status`、当前分支和未提交改动。
2. 遇到新增大模块、大功能面、较大行为变化或新边界时，先和开发者对齐设计，再在 `docs/design/` 撰写或更新设计文档。
   如果只是小功能、小修复、小范围合同补洞，可以按实际收益决定是否单独写设计文档。
3. 设计或方向确定后，先同步 `docs/status/` 和相关 `AGENTS.md` 里的当前主线、下一步、优先级或边界说明，不要等代码写完再回头补口径。
4. 决定实施载体。
   根据工作规模和风险，明确是在当前根目录直接工作，还是新开 worktree / branch；同时决定是否需要多 agent 并行协作。
5. 决定是否写 `plan`。
   任务较大、步骤较多、需要分阶段验收或多人并行时，在 `docs/plan/` 撰写计划文档；简单任务可以不单独写 `plan`。
6. 根据 `plan` 或用户要求开始执行。
   优先小步落地，先补最窄回归或最小验证，再扩到实现和更大门禁。
7. 工作完成后，优先同步文档。
   至少检查并更新相关 `status`、各级 `AGENTS.md`、必要时的 `README.md` / `docs/index.md`，确保文档口径和当前进度一致。
8. 汇报结果，并把提交与清理交还给开发者决定。
   汇报里要说明改动摘要、验证结果、剩余风险和建议下一步；不要默认自动提交，也不要默认自动清理 worktree / branch，除非用户明确要求。

### 代码审查 / 修改类任务

1. 审查发现默认集中写入 `docs/status/code_reself_status.md`。
   如果文件不存在，就先创建；如果没有发现问题，也要明确写清“当前无活跃问题”。
2. 审查结论形成后，先同步 `docs/status/` 和相关 `AGENTS.md` 里的下一步、优先级和处理口径。
3. 后续如果要进入修复，实现流程默认回到上面的第 4 步到第 8 步执行。

### 额外约束

- 不要跳过文档同步。
- 不要在未经确认的情况下直接提交或清理分支 / worktree。
- 不要把设计、状态、计划和实现混成一份文档；按 `docs/AGENTS.md` 的分工维护。
- 如果用户给了更具体的流程或边界，用户指令优先。

## 全局验证基线

修改架构相关路径后，至少应守住：

- `cd myCPU && make test`

如果改动主要集中在 loader、guest smoke orchestration 或本轮新增窄门禁，还应额外关注：

- `cd myCPU && make test-unit-binary_loader`
- `cd myCPU && make test-unit-machine_loader_reset`
- `cd myCPU && make test-unit-supervisor_demo_smoke`
- `cd myCPU && make test-unit-user_program_smoke`

如果改动主要集中在 guest runtime / demo bring-up，还应额外关注：

- `cd myCPU && make test-unit-supervisor_runtime`
- `cd myCPU && make test-unit-kernel_bringup`
- `cd myCPU && make test-unit-kernel_runtime`
- `cd myCPU && make test-unit-vm_address_space`
- `cd myCPU && make test-unit-vm_process`
- `cd myCPU && make test-unit-vm_object`
- `cd myCPU && make test-unit-vm_fault`
- `cd myCPU && make test-unit-trap_runtime`
- `cd myCPU && make test-unit-trap_dispatch`
- `cd myCPU && make test-unit-user_task`
- `cd myCPU && make test-unit-user_task_bootstrap`
- `cd myCPU && make test-unit-user_program`
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

## 开发阶段

### Phase 1

目标：

- 跑起一个小型自制 OS / kernel，具备基本中断、内存管理和控制台输出。

### Phase 2

目标：

- 在保持统一 ISA 语义来源的前提下，引入多种执行模型。

### Phase 3

目标：

- 在可测试前提下推进预测、重命名、ROB、LSQ 等高级微架构。

### Phase 4

目标：

- 逐步加入 cache hierarchy、DMA、multicore 和一致性。

## 报告与总结规则

- 描述项目时，要把当前仓库表述为“已可运行的模拟器原型”。
- 描述 C++ 重构时，要强调它是对复杂度增长的结构性响应，不是语言偏好。
- 报告里应明确区分：
  - 项目 owner 已完成的既有工作
  - 已落地的当前结构成果
  - 当前下一步工程任务
  - 更远期的 Phase 2 / 3 / 4 工作
