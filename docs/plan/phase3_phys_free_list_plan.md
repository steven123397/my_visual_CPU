# Phase 3 phys free-list / recycle 实现计划

> **文档状态：** 已完成

> **面向 AI 代理的工作者：** 必需子技能：使用 `superpowers:executing-plans` 在当前会话中按任务推进，或在明确授权后使用 `superpowers:subagent-driven-development` 分任务执行。步骤使用复选框（`- [ ]`）语法跟踪进度。

**目标：** 为当前 `Phase 3-B/C` 的 `rename + ROB` 主路径补齐最小 `phys free-list / recycle`，让 phys tag 不再只依赖 `next_phys_++` 线性增长。

**架构：** 本计划不引入新的独立 allocator 子模块，而是在现有 `RenameMap` 内维护 free-list 与 checkpoint。decode/rename 继续通过 `RenameMap` 申请新 phys，ROB head commit 负责回收 old committed phys，flush / rollback 继续沿现有统一 rollback 路径恢复 speculative map 与 free-list 快照。整个过程不改变当前单发射、共享 ISA 真值来源和顺序退休的边界。

**技术栈：** C++17、GNU Make、host-side smoke / differential、asm / guest regression、Node `--test`、RISC-V 交叉工具链。

## 文档定位

本文档用于把当前 `Phase 3-B/C` 首轮 `rename + ROB + LSQ` 接线后的下一块结构收口，细化为一份可直接执行的 `phys free-list / recycle` 子计划。

它重点回答：

- 当前 phys tag 为什么还存在结构性泄漏风险。
- `free-list / recycle` 应该落在哪一层，而不破坏既有 `rename / ROB / flush` 接口。
- 这一轮需要补哪些 smoke，如何验证 commit 回收、rollback 回收和长路径复用。

本文档只回答“怎么落地”。当前实时结果和完成态回写以 [status/mainline_status.md](../status/mainline_status.md) 为准。

## 关联文档

- 来源设计：
  - [design/phase3_ooo_execution_model_design.md](../design/phase3_ooo_execution_model_design.md)
  - [design/pipeline_speculation_contracts.md](../design/pipeline_speculation_contracts.md)
  - [plan/phase3_ooo_execution_plan.md](phase3_ooo_execution_plan.md)
- 目标状态：
  - [status/mainline_status.md](../status/mainline_status.md)

## 目标

- 为 `RenameMap` 增加最小 free-list，使 phys 分配优先复用已回收 tag，而不是无限 `next_phys_++`。
- 让 ROB head commit 成为 old committed phys 回收的唯一入口，避免 younger speculative state 误回收仍在使用的 phys。
- 让 rollback / mispredict / trap flush 能恢复 checkpoint 时刻的 free-list，保证 younger 分配的 phys 不泄漏。
- 继续保持当前 `Phase 3-B/C` 的架构边界：不引入真正的 OoO execute、不新增独立 phys allocator 子系统、不污染 `functional` reference path。

## 完成定义

- `RenameMap` 的 checkpoint 已覆盖 speculative map 与 free-list 状态，`rename_dest()` 优先复用 free phys。
- `commit_dest()` 或等价接口能在 architectural mapping 切换后返回可回收的 old phys，并保证 `x0` / identity boot phys 等保留项不被误回收。
- rollback 后，younger 分配但尚未 commit 的 phys 会回到 free-list，不会在长 guest 路径上持续泄漏。
- 以下验证通过：
  - `cd myCPU && make test-host-rename_map_smoke`
  - `cd myCPU && make test-host-reorder_buffer_smoke`
  - `cd myCPU && make test-host-physical_register_file_smoke`
  - `cd myCPU && make test-host-pipeline_rename_commit_smoke`
  - `cd myCPU && make test-host-pipeline_speculation_contracts_smoke`
  - `cd myCPU && make test-pipeline`
  - `cd myCPU && make test`

## 文件结构

### 计划内重点修改文件

- `myCPU/src/exec/rename_map.h`
- `myCPU/src/exec/rename_map.cpp`
- `myCPU/src/exec/reorder_buffer.h`
- `myCPU/src/exec/reorder_buffer.cpp`
- `myCPU/src/exec/pipeline_backend.cpp`
- `myCPU/tests/host/rename_map_smoke.cpp`
- `myCPU/tests/host/reorder_buffer_smoke.cpp`
- `myCPU/tests/host/pipeline_rename_commit_smoke.cpp`
- `myCPU/tests/host/pipeline_speculation_contracts_smoke.cpp`
- `myCPU/AGENTS.md`
- `docs/status/mainline_status.md`

### 本轮明确不改

- 不新增独立 `phys tag allocator` 模块。
- 不扩成 superscalar、复杂 replay、store-to-load forwarding 或更激进的 memory speculation。
- 不改 `functional` backend 与共享 `InstructionSemantics` 的职责。
- 不扩 `debug/frontend` 的功能面；仅在必要时被动兼容新的可观察性字段。

## 任务

### 任务 1：补齐 `RenameMap` free-list 的最小接口与 helper smoke

**文件：**
- 修改：`myCPU/src/exec/rename_map.h`
- 修改：`myCPU/src/exec/rename_map.cpp`
- 修改：`myCPU/tests/host/rename_map_smoke.cpp`

- [x] **步骤 1：先写失败测试。**
  至少覆盖：
  - commit 后 old committed phys 会被回收，后续 `rename_dest()` 可复用该 phys。
  - rollback 到 checkpoint 后，checkpoint 之后新分配的 phys 会重新回到 free-list。
  - `x0` 与当前 architectural live phys 不会被误回收。

- [x] **步骤 2：运行失败测试验证红灯。**
  运行：
  - `cd myCPU && make test-host-rename_map_smoke`
  预期：当前实现仍是 `next_phys_++`，无法满足复用 / 回收断言。

- [x] **步骤 3：写最小实现代码。**
  实现要求：
  - free-list 直接放在 `RenameMap` 内，checkpoint 一并保存。
  - `commit_dest()` 返回可回收的 old phys，或提供等价接口给 pipeline commit 路径消费。
  - rollback 只恢复 checkpoint 时刻可见的 speculative 分配状态，不引入额外全局 side effect。

- [x] **步骤 4：重跑 helper smoke，确认绿灯。**
  运行：
  - `cd myCPU && make test-host-rename_map_smoke`

- [x] **步骤 5：提交一个聚焦 commit。**

### 任务 2：把 recycle 接到 ROB head commit 与 rollback 合同

**文件：**
- 修改：`myCPU/src/exec/reorder_buffer.h`
- 修改：`myCPU/src/exec/reorder_buffer.cpp`
- 修改：`myCPU/src/exec/pipeline_backend.cpp`
- 修改：`myCPU/tests/host/reorder_buffer_smoke.cpp`
- 修改：`myCPU/tests/host/pipeline_rename_commit_smoke.cpp`
- 修改：`myCPU/tests/host/pipeline_speculation_contracts_smoke.cpp`

- [x] **步骤 1：先写失败测试。**
  至少覆盖：
  - ROB head commit 后会把 stale `previous_phys_rd` 交给 rename/free-list 回收。
  - younger speculative rename 在 flush / rollback 后不会泄漏 phys。
  - 同一条依赖链长路径下 phys tag 会复用，而不是持续单调增长。

- [x] **步骤 2：运行失败测试验证红灯。**
  运行：
  - `cd myCPU && make test-host-reorder_buffer_smoke`
  - `cd myCPU && make test-host-pipeline_rename_commit_smoke`
  - `cd myCPU && make test-host-pipeline_speculation_contracts_smoke`

- [x] **步骤 3：写最小接线代码。**
  实现要求：
  - recycle 只能由 committed old phys 或 rollback 后 younger phys 触发。
  - 不改变当前 ROB in-order commit 与统一 flush 结构。
  - 保持现有 `phys_regs` rebuild / rollback 逻辑可工作，不提前引入复杂 phys liveness 扫描。

- [x] **步骤 4：重跑 smoke，确认绿灯。**
  运行：
  - `cd myCPU && make test-host-reorder_buffer_smoke`
  - `cd myCPU && make test-host-pipeline_rename_commit_smoke`
  - `cd myCPU && make test-host-pipeline_speculation_contracts_smoke`

- [x] **步骤 5：提交一个聚焦 commit。**

### 任务 3：跑总门禁并回写状态文档

**文件：**
- 修改：`myCPU/AGENTS.md`
- 修改：`docs/status/mainline_status.md`

- [x] **步骤 1：运行总门禁。**
  运行：
  - `cd myCPU && make test-host-physical_register_file_smoke`
  - `cd myCPU && make test-host-rename_map_smoke`
  - `cd myCPU && make test-host-reorder_buffer_smoke`
  - `cd myCPU && make test-host-pipeline_rename_commit_smoke`
  - `cd myCPU && make test-host-pipeline_speculation_contracts_smoke`
  - `cd myCPU && make test-pipeline`
  - `cd myCPU && make test`

- [x] **步骤 2：回写文档。**
  需要更新：
  - 当前 `Phase 3-B/C` 已不再依赖单纯 `uint32_t next_phys_` 延后回卷，而是具备最小 free-list / recycle。
  - 当前仍然有效的限制应收口到“尚无更激进的 OoO execute / replay / memory speculation”。

- [x] **步骤 3：提交收尾 commit，并把本计划勾到最新进度。**

## 完成态回写要求

- 全部 checklist 必须勾完。
- 文件头必须改成“已完成”或等价完成态说明。
- [status/mainline_status.md](../status/mainline_status.md) 必须回写：
  - 本轮 free-list / recycle 已落地结果；
  - 当前 `Phase 3-B/C` 执行模型边界；
  - 仍然有效的剩余风险；
  - 下一步是否进入更激进的 OoO execute / replay / memory speculation。
