# AGENTS.md

## 作用

这是仓库根目录的总览指引文件。

使用顺序：

1. 先读本文件，理解项目范围、全局约定和默认工作流。
2. 进入具体子树后，再读对应子目录下的 `AGENTS.md`。
3. 需要看实时进度、当前优先级、active line / wave 或近端 blocker 时，只看 [docs/status/mainline_status.md](docs/status/mainline_status.md)。

本仓库只维护 `AGENTS.md` 体系，不再维护 `CLAUDE.md`。

## 项目概况

仓库当前主体是 [myCPU](myCPU)，一个从 C 原型逐步演进到模块化 C++ 架构的小型 RISC-V 模拟器。

当前定位：

- 已经是一个已可运行的模拟器原型，不是纯设计稿。
- 当前以 `reference-first`、可观察性和小步收口为默认方法。
- `Phase 1` 冻结基线已经形成，后续工作主要围绕 `xv6 / Linux` bring-up、`pipeline` 维护，以及更后续 wave 的准备性收口展开。

## 单一事实来源

- 仓库级实时状态、当前优先级、active line / wave、当前 blocker：
  - [docs/status/mainline_status.md](docs/status/mainline_status.md)
- 专项状态只在确有独立持续跟踪价值时保留，例如：
  - [docs/status/kernel_alpha_status.md](docs/status/kernel_alpha_status.md)
  - [docs/status/npu_tpu_accelerator_status.md](docs/status/npu_tpu_accelerator_status.md)
  - [docs/status/code_reself_status.md](docs/status/code_reself_status.md)
- 根目录和子树 `AGENTS.md` 只保留范围、规则、方法、局部边界和验证要求，不承载实时进度流水账。
- `design / plan / status` 分工保持严格分离：
  - `design` 记录长期有效的边界、取舍和契约。
  - `plan` 记录执行步骤、checklist 和完成归档。
  - `status` 记录当前状态、风险、优先级和下一步。

## 仓库结构

- [myCPU](myCPU)
  核心模拟器代码、guest runtime、测试和构建脚本。
- [frontend](frontend)
  本地调试服务、浏览器前端和 Node 测试。
- [docs](docs)
  按 `background / design / plan / status / showcase` 组织的正式技术文档和展示材料。
- [README.md](README.md)
  面向读者的项目概览、构建和运行说明。
- [docs/showcase](docs/showcase)
  课程结题、PPT、讲稿、截图、HTML 预览页和展示脚本材料。

## 子目录 AGENTS 索引

- [myCPU/AGENTS.md](myCPU/AGENTS.md)
  simulator 主体的方法、局部规则和验证要求。
- [myCPU/guest/AGENTS.md](myCPU/guest/AGENTS.md)
  guest runtime 的实现边界、局部规则和验证要求。
- [docs/AGENTS.md](docs/AGENTS.md)
  文档目录职责、索引要求和治理规则。

## 全局开发约定

- 任何实现改动都应优先维护 reference path 的正确性与可观察性。
- 共享 `InstructionSemantics + functional backend` 仍是 ISA 真值来源；`pipeline`、未来 `JIT` 和其他执行形态只消费共享语义。
- 文档要与实现同步，但不要制造并行事实来源。
- 状态文档优先保留当前状态、少量关键历史节点和下一步，不长期堆已完成 checklist。
- 不做没有结构收益的纯 cosmetic 重写或纯语言迁移。
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
  - `myCPU/workloads/linux_proto/linux_postinit_cleanup_smoke.elf`

## Agent 默认工作流

除非用户明确要求跳过、简化或改顺序，否则默认按下面流程推进：

1. 先确认上下文。
   至少阅读根 `AGENTS.md`、目标子树 `AGENTS.md` 和直接相关的 `design / status` 文档；预计会改代码或文档时，先本地确认 `git status --short --branch`。
2. 先判断是否需要设计文档。
   新模块、大功能面、较大行为变化或新长期边界，先更新 `docs/design/`；小修复或窄合同补洞可直接实施。
3. 再判断是否需要计划文档。
   多步骤、多阶段验收或明显需要 checklist 的任务，写入 `docs/plan/`；简单任务可以不单独写 `plan`。
4. 实施期间保持单一事实来源。
   需要回写实时状态时，只更新相关 `status` 文档；不要把同一进度同时抄到多个文件。
5. 工作完成后优先同步文档。
   至少检查相关 `status`、`AGENTS.md`、必要时的 `README.md` / `docs/index.md`。
6. 汇报结果，把提交和清理交还给开发者决定。
   不默认自动提交，也不默认自动清理 branch / worktree，除非用户明确要求。

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

## 报告与总结规则

- 描述项目时，要把当前仓库表述为“已可运行的模拟器原型”。
- 描述 C++ 重构时，要强调它是对复杂度增长的结构性响应，不是语言偏好。
- 报告里应明确区分：
  - 项目 owner 已完成的既有工作
  - 已落地的当前结构成果
  - 当前下一步工程任务
  - 更远期的 `Phase 2 / 3 / 4` 工作
