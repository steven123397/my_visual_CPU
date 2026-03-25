# AGENTS.md

## 作用

这是仓库根目录的总览指引文件。

使用顺序：

1. 先读本文件，理解项目范围、阶段目标和全局约定。
2. 进入具体子树后，再读对应子目录下的 `AGENTS.md`。

本仓库后续只维护 `AGENTS.md` 体系，不再维护 `CLAUDE.md`。

## 项目概况

仓库当前主体是 [myCPU](/home/liangjiaqi/projects/my_visual_CPU/myCPU)，一个从 C 原型逐步演进到模块化 C++ 架构的小型 RISC-V 模拟器。

当前定位：

- 已经是一个已可运行的模拟器原型，不是纯设计稿。
- 当前已达成 Phase 1 核心 bring-up 目标，正处于 Phase 1 冻结后的稳定化阶段。
- 正在同步推进一轮有明确结构收益的 C++ 重构。

长期目标：

- 先把模拟器扩展到足以支撑一个自制的小型 OS / kernel bring-up。
- 在保持统一 ISA 语义来源的前提下，继续深化 pipeline，并逐步进入 OoO、cache、multicore 等更复杂模型。

## 仓库结构

- [myCPU](/home/liangjiaqi/projects/my_visual_CPU/myCPU)
  核心模拟器代码、guest runtime、测试和构建脚本。
- [frontend](/home/liangjiaqi/projects/my_visual_CPU/frontend)
  本地调试服务、浏览器前端和 Node 测试。
- [docs](/home/liangjiaqi/projects/my_visual_CPU/docs)
  规划、契约、审查和状态类文档。
- [readme.md](/home/liangjiaqi/projects/my_visual_CPU/readme.md)
  面向读者的项目概览、构建和运行说明。

## 子目录 AGENTS 索引

- [myCPU/AGENTS.md](/home/liangjiaqi/projects/my_visual_CPU/myCPU/AGENTS.md)
  simulator 主体说明：CPU、CSR、trap、MMU、platform、device、loader、tests 的当前实现与局部规则。
- [myCPU/guest/AGENTS.md](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/AGENTS.md)
  guest runtime 说明：memory、PMM、VM、trap、runtime、user task/program 与独立 kernel alpha bring-up 的当前边界和下一步。
- [docs/AGENTS.md](/home/liangjiaqi/projects/my_visual_CPU/docs/AGENTS.md)
  文档维护规则：哪些内容留在根目录，哪些内容写入实现子树，哪些内容进入专门文档。

## 当前状态

当前仓库已经具备以下高层能力：

- RV64I / RV64M 参考执行路径。
- `functional` / `pipeline` 两种执行后端。
- ELF64 与 flat binary 加载。
- 基础 CSR 访问与 M-mode trap。
- 初步 `M/S/U` 特权路径。
- 最小 UART / CLINT / PLIC / MMIO block storage 平台。
- Sv39、最小 TLB、`sfence.vma`。
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

- `2026-03-25` 已完成一批 simulator-side correctness 修复：非法整数编码、`DIV/REM` 溢出边界、ELF pure-BSS `PT_LOAD`、bus / device 第一轮边界防御。
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
- 继续扩充非法编码、MMIO 边界和 ELF 段布局回归。
- 把独立 `kernel_alpha` 十条回归基线维持在可回归的 Phase 1 完成态，并继续做必要 hardening。
- 继续推进 guest runtime 的 process / runtime refinement 与大文件拆分，作为 post-Phase1 结构优化。
- 在不破坏 reference path 清晰性的前提下，继续稳住已接入 Phase 2 能力的语义边界与验证基线。
- 保持 `pipeline` 与本地调试前端可运行、可验证，但不让它们反向污染 reference path。

当前对 Phase 2 的工程安排是：

- `pipeline core` 与 `debug/frontend` 的正式接入工作已经完成，不再把它们当作待合入功能。
- 近期先不继续扩功能面，而是优先明确 Phase 2 出门标准，并补强 `pipeline` 的 correctness / differential / robustness 验证。
- `debug/frontend` 继续限定在“教学演示可用”的最小范围，重点放在快照稳定性、测试门禁和对现有 demo 的可用性维护。

相关状态文档见：

- [docs/status/current_mainline_status_2026-03-25.md](/home/liangjiaqi/projects/my_visual_CPU/docs/status/current_mainline_status_2026-03-25.md)
- [docs/status/code_self_review_2026-03-24.md](/home/liangjiaqi/projects/my_visual_CPU/docs/status/code_self_review_2026-03-24.md)
- [docs/status/kernel_alpha_bringup_status.md](/home/liangjiaqi/projects/my_visual_CPU/docs/status/kernel_alpha_bringup_status.md)

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

## 全局验证基线

修改架构相关路径后，至少应守住：

- `cd myCPU && make test`

如果改动主要集中在 guest runtime / demo bring-up，还应额外关注：

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
