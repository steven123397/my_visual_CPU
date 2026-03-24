# AGENTS.md

## 作用

这是仓库根目录的总览指引文件。

使用顺序：

1. 先读本文件，理解项目范围、阶段、全局约定和工作方向。
2. 进入具体子树工作时，再读对应子目录下的 `AGENTS.md`。

本仓库后续只使用 `AGENTS.md` 体系，不再维护 `CLAUDE.md`。

## 项目概况

仓库当前主体是 `myCPU`，一个从 C 原型逐步演进到模块化 C++ 架构的小型 RISC-V 模拟器。

当前定位：

- 已经是可运行的 functional simulator prototype，不是纯设计稿
- 正处于 Phase 1 bring-up 路线中
- 同时在推进一轮有明确结构收益的 C++ 重构

长期目标：

- 先把模拟器扩展到足以支撑一个自制的小型 OS / kernel bring-up
- 之后再考虑 pipeline、OoO、cache、multicore 等更复杂模型

## 仓库结构

- [myCPU](/home/liangjiaqi/projects/my_visual_CPU/myCPU)
  核心模拟器代码、guest runtime、测试和构建脚本。
- [docs](/home/liangjiaqi/projects/my_visual_CPU/docs)
  规划、契约、审查和设计类文档。
- [readme.md](/home/liangjiaqi/projects/my_visual_CPU/readme.md)
  面向读者的项目概览、构建和运行说明。

## 子目录 AGENTS 索引

- [myCPU/AGENTS.md](/home/liangjiaqi/projects/my_visual_CPU/myCPU/AGENTS.md)
  simulator 主体说明：CPU、CSR、trap、MMU、platform、device、loader、tests 的当前实现、局部约束和待处理问题。
- [myCPU/guest/AGENTS.md](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/AGENTS.md)
  guest runtime 说明：memory、PMM、VM、trap、runtime、user task/program、smoke orchestration 的当前边界、局部规则和下一步工作。
- [docs/AGENTS.md](/home/liangjiaqi/projects/my_visual_CPU/docs/AGENTS.md)
  文档维护规则：哪些内容留在根目录，哪些内容进实现子树，哪些内容放到专门文档。

如果后续继续细分子树规则，应在这里补充索引，而不是重新把所有细节堆回根目录。

## 当前状态

当前仓库已经具备以下高层能力：

- RV64I / RV64M 参考执行路径
- ELF64 和 flat binary 加载
- 基础 CSR 访问与 M-mode trap
- 初步的 `M/S/U` 特权路径
- 最小 UART / CLINT / PLIC / MMIO block storage 平台
- Sv39 虚拟内存、最小 TLB、`sfence.vma`
- 一套最小 guest supervisor runtime 和 `guest_supervisor_demo`

当前 guest 侧已经打通：

- early allocator
- bitmap PMM
- guest-side Sv39 page table / address space
- trap / runtime / process / user task / user program 分层
- 最小 U-mode enter / return
- timer / external / page-fault / user-`ecall` smoke 路径

最近一轮结构收口后的基线：

- `guest/supervisor_demo/main.c` 保持极简
- `supervisor_demo_smoke` 只保留单一 public demo runner
- `user_program_smoke` 对外只保留阶段化 helper

## 当前优先级

决策顺序保持不变：

0. 先把当前原型整理成更稳的 C++ 结构边界。
1. 保持正确、可调试的 ISA 级 reference model。
2. 把模拟器推进到足以支撑小型 OS / kernel bring-up。
3. 只有在 Phase 1 稳定后，才进入更多执行模型和微架构扩展。

不要把这 3 条路线混在同一实现步骤里。

## 当前计划焦点

当前阶段不再优先做 `main.c` 或 smoke API 的继续清理，重点转向 Phase 1 功能缺口收尾：

- 修复最近自检暴露出的 correctness 问题
- 继续推进 guest/runtime 的 process / runtime refinement
- 继续补 user interrupt / trap 覆盖
- 为第一次真正的小 OS / kernel bring-up 清掉基础障碍

最近一次系统性自检结果见：

- [docs/code_self_review_2026-03-24.md](/home/liangjiaqi/projects/my_visual_CPU/docs/code_self_review_2026-03-24.md)

其中当前最重要的 simulator-side 问题包括：

- 非法整数编码会被误执行
- 有符号 `DIV/REM` 溢出边界会触发宿主未定义行为
- ELF loader 对纯 BSS `PT_LOAD` 段处理不完整
- bus / device 访问边界仍偏宽松

## 技术栈

- host simulator：C + C++17
- guest runtime：C11 + RISC-V assembly
- 构建：GNU Make
- 交叉工具链：`riscv64-unknown-elf-gcc` / `riscv64-unknown-elf-objcopy`
- 文档：Markdown

## 全局开发约定

- 任何实现改动都应优先维护 reference path 的正确性与可观察性。
- README 中的功能声明必须与真实实现保持一致。
- 不要做没有结构收益的“纯语言迁移”或“纯 cosmetic 重写”。
- 优先小步落地，避免一次引入过大的抽象。
- 不要提交构建产物，尤其是：
  - `myCPU/guest/supervisor_demo.elf`
- 对 guest 相关描述，README 保持简洁；更细节的实现状态和局部规则写进子目录 `AGENTS.md` 或 `docs/`。

## 全局验证基线

修改架构相关路径后，至少应守住以下基线：

- `cd myCPU && make test`

如果改动主要集中在 guest runtime / demo bring-up，至少应额外关注：

- `cd myCPU && make test-guest-supervisor_demo`

## 开发阶段

### Phase 1

目标：

- 跑起一个小型自制 OS / kernel，具备基本中断、内存管理和控制台输出

### Phase 2

目标：

- 在保持统一 ISA 语义来源的前提下，引入多种执行模型

### Phase 3

目标：

- 在可测试前提下推进预测、重命名、ROB、LSQ 等高级微架构

### Phase 4

目标：

- 逐步加入 cache hierarchy、DMA、multicore 和一致性

## 报告与总结规则

- 描述项目时，要把当前仓库表述为“已可运行的模拟器原型”。
- 描述 C++ 重构时，要强调它是对复杂度增长的结构性响应，不是语言偏好。
- 报告里应明确区分：
  - 项目 owner 已完成的既有工作
  - 已落地的 C++ / guest runtime 重构
  - 当前下一步工程任务
  - 更远期的 Phase 2/3/4 工作
