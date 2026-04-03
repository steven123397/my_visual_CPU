# Phase 3-C 最小真实 OoO Execute 实现计划

> **文档状态：** 已完成

> **面向 AI 代理的工作者：** 必需子技能：使用 `superpowers:executing-plans` 在当前会话中按任务推进，或在明确授权后使用 `superpowers:subagent-driven-development` 分任务执行。步骤使用复选框（`- [ ]`）语法跟踪进度。

**目标：** 在保持单发射 fetch / decode / rename、统一 ISA 真值来源和顺序退休的前提下，把当前 `pipeline` 从“近似顺序 execute”推进到“最小真实 OoO execute”：允许 younger ALU 在 older memory op 未完成时先完成并写回 ROB ready，但所有 architected side effect 仍只在 commit boundary 生效。

**架构：** 本计划不引入完整 issue queue，也不重写现有前端。实现方式是把“结果完成”和“顺序退休”从 `mem_wb` 单槽解绑，让 `ROB head` 成为唯一退休入口，同时把 memory 路径收口成一个最小独立执行单元。这样可以在不放宽 MMIO / CSR / trap / precise interrupt 合同的前提下，先获得一个真实但克制的 OoO 完成窗口。

**技术栈：** C++17、GNU Make、host-side smoke / differential、asm / guest regression、本地 debug CLI。

## 文档定位

本文档用于把 `rename + ROB + LSQ replay/forwarding` 已具备的底座，继续接到 `Phase 3-C` 设计里要求的“最小真实 OoO execute”完成态。

它重点回答：

- 现有 backend 还差哪一块才算真正进入 OoO execute。
- 哪些状态需要从阶段寄存器搬到 ROB / memory unit。
- 哪些测试先写成红灯，证明 younger ALU 已能越过 older memory op 完成。

本文档只回答“怎么落地”。实时完成结果统一回写到 [status/mainline_status.md](../status/mainline_status.md)。

## 关联文档

- 来源设计：
  - [design/phase3_ooo_execution_model_design.md](../design/phase3_ooo_execution_model_design.md)
  - [design/pipeline_speculation_contracts.md](../design/pipeline_speculation_contracts.md)
  - [plan/phase3_ooo_execution_plan.md](./phase3_ooo_execution_plan.md)
- 目标状态：
  - [status/mainline_status.md](../status/mainline_status.md)

## 目标

- 让 `ROB` 成为真实的完成 / 退休分离边界，而不是继续由 `mem_wb` 单槽隐式代表“可退休指令”。
- 引入一个最小独立 memory execute 路径，让 older load/store 未完成时，younger ALU 仍可完成并写 `ROB ready`。
- 保持顺序退休、precise trap / interrupt、RAM store commit-boundary、MMIO non-speculative 和 replay/forwarding 合同不变。
- 继续复用现有 `rename_map / reorder_buffer / load_store_queue / physical_register_file`，不另外发明第二套数据面。
- 同步维护 host smoke、differential、debug snapshot 与文档口径。

## 完成定义

- `PipelineBackend` 的退休逻辑不再要求“ROB head 必须正好位于 `mem_wb`”；退休输入改由 `ROB head` 的已完成结果驱动。
- older memory op 进入独立 memory 执行单元后，younger ALU 可以在其未完成时先执行并把结果写入 `phys_regs + ROB ready`。
- 被 younger ALU 先完成的结果，在 older ROB head 未退休前仍然对 architected GPR / CSR / RAM / MMIO 不可见。
- 当前 `LSQ replay-needed`、automatic replay、RAM-only forwarding 与 trap / mispredict rollback 合同保持成立。
- 以下验证通过：
  - `cd myCPU && make test-host-pipeline_rename_commit_smoke`
  - `cd myCPU && make test-host-pipeline_speculation_contracts_smoke`
  - `cd myCPU && make test-host-backend_differential_smoke`
  - `cd myCPU && make test-host-debug_cli_smoke`
  - `cd myCPU && make test-pipeline`
  - `cd myCPU && make test`

## 文件结构

### 计划内重点修改文件

- `myCPU/src/exec/reorder_buffer.h`
- `myCPU/src/exec/reorder_buffer.cpp`
- `myCPU/src/exec/pipeline_types.h`
- `myCPU/src/exec/pipeline_core_state.h`
- `myCPU/src/exec/pipeline_core_state.cpp`
- `myCPU/src/exec/pipeline_backend.h`
- `myCPU/src/exec/pipeline_backend.cpp`
- `myCPU/src/debug/debug_snapshot.h`
- `myCPU/src/debug/debug_protocol.cpp`
- `myCPU/tests/host/pipeline_rename_commit_smoke.cpp`
- `myCPU/tests/host/pipeline_speculation_contracts_smoke.cpp`
- `myCPU/tests/host/backend_differential_smoke.cpp`
- `myCPU/tests/host/debug_cli_smoke.cpp`
- `docs/plan/phase3_ooo_execution_plan.md`
- `docs/status/mainline_status.md`
- `docs/index.md`
- `myCPU/AGENTS.md`

### 本轮明确不改

- 不引入 superscalar fetch / decode / issue。
- 不引入新的复杂 issue queue、reservation station 或多 memory port。
- 不改变 `functional + shared InstructionSemantics` 作为唯一 architected 真值来源的定位。
- 不放宽 MMIO non-speculative、CSR commit-boundary、trap-return precise、TLB / `sfence.vma` 合同。

## 任务

### 任务 1：先补最小真实 OoO execute 的红灯测试

**文件：**
- 修改：`myCPU/tests/host/pipeline_rename_commit_smoke.cpp`
- 修改：`myCPU/tests/host/pipeline_speculation_contracts_smoke.cpp`
- 修改：`myCPU/tests/host/backend_differential_smoke.cpp`

- [x] **步骤 1：新增一个“older memory / younger ALU” 场景。**
  至少覆盖：
  - older load 或 store 进入 memory 路径后，younger ALU 已经把结果写进 `phys_regs` 且对应 ROB entry ready。
  - 此时 architected `x` 寄存器仍保持旧值，证明只是 speculative completion，不是提前 commit。
  - commit 顺序仍按 ROB head 推进，最终 architected 结果与 reference 一致。

- [x] **步骤 2：补一个 speculation 合同红灯。**
  至少覆盖：
  - younger ALU 已完成但 older memory 尚未退休时，trap / replay / rollback 仍能裁掉 younger side effect。
  - automatic replay 不会因为 younger ALU 已 ready 就把其结果错误提交。

- [x] **步骤 3：运行目标测试验证红灯。**
  运行：
  - `cd myCPU && make test-host-pipeline_rename_commit_smoke`
  - `cd myCPU && make test-host-pipeline_speculation_contracts_smoke`
  - `cd myCPU && make test-host-backend_differential_smoke`
  预期：至少一个新断言失败，证明当前实现仍被 `mem_wb` 单槽退休模型限制。

### 任务 2：把完成与退休从 `mem_wb` 单槽解绑

**文件：**
- 修改：`myCPU/src/exec/reorder_buffer.h`
- 修改：`myCPU/src/exec/reorder_buffer.cpp`
- 修改：`myCPU/src/exec/pipeline_types.h`
- 修改：`myCPU/src/exec/pipeline_core_state.h`
- 修改：`myCPU/src/exec/pipeline_core_state.cpp`
- 修改：`myCPU/src/exec/pipeline_backend.h`
- 修改：`myCPU/src/exec/pipeline_backend.cpp`

- [x] **步骤 1：扩 `ROB` entry，使其能承载 commit 所需完成态。**
  实现要求：
  - ROB ready 结果需能覆盖 `rd_write` / trap / redirect / CSR / memory side effect 的 commit 输入。
  - backend 不再依赖 “ROB head 恰好位于 `mem_wb`” 才能退休。

- [x] **步骤 2：引入最小 memory execute 单元状态。**
  实现要求：
  - older memory op 进入独立状态后，不再阻塞 younger ALU 的 execute 完成。
  - 仍保持单 memory op 执行，不扩成多端口或复杂调度器。
  - 继续沿用现有 `LSQ` 做 replay / forwarding / store commit 管理。

- [x] **步骤 3：让 non-memory 指令可以直接完成到 ROB。**
  实现要求：
  - younger ALU 可在 older memory op 未完成时把结果写到 `phys_regs` 并 `ROB mark_ready`。
  - 分支、trap、CSR 仍只在 commit boundary 对 architected 状态生效。
  - 继续守住 mispredict / trap / interrupt 的 precise rollback。

- [x] **步骤 4：重跑目标测试确认绿灯。**
  运行：
  - `cd myCPU && make test-host-pipeline_rename_commit_smoke`
  - `cd myCPU && make test-host-pipeline_speculation_contracts_smoke`
  - `cd myCPU && make test-host-backend_differential_smoke`

### 任务 3：同步 debug / 文档口径并做完整回归

**文件：**
- 修改：`myCPU/src/debug/debug_snapshot.h`
- 修改：`myCPU/src/debug/debug_protocol.cpp`
- 修改：`myCPU/tests/host/debug_cli_smoke.cpp`
- 修改：`docs/plan/phase3_ooo_execution_plan.md`
- 修改：`docs/status/mainline_status.md`
- 修改：`docs/index.md`
- 修改：`myCPU/AGENTS.md`

- [x] **步骤 1：必要时补最小可观察性字段或调整现有断言。**
  原则：
  - 只暴露支撑验证的最小观测面，不把 debug 快照扩成调度器全量内部结构 dump。
  - 如果阶段语义调整，相关 smoke 要改成对新合同稳定的断言，而不是继续绑旧阶段位置。

- [x] **步骤 2：回写总计划与状态文档。**
  需要明确：
  - `Phase 3-C` 中“最小真实 OoO execute”是否完成。
  - 剩余未完成项是否只剩更激进的 issue / memory speculation / superscalar 方向。

- [x] **步骤 3：运行完整验证。**
  运行：
  - `cd myCPU && make test-host-debug_cli_smoke`
  - `cd myCPU && make test-pipeline`
  - `cd myCPU && make test`
  - `git -C /home/liangjiaqi/projects/my_visual_CPU/.worktrees/phase3-ooo-execution diff --check`

## 完成态回写要求

- 全部 checklist 必须勾完。
- 文件头必须改成“已完成”或等价完成态说明。
- `docs/status/mainline_status.md` 必须回写：
  - 最小真实 `OoO execute` 已完成的结果摘要
  - `Phase 3-C` 的关键历史节点
  - 剩余仍未完成的风险或后续方向

## 完成结果摘要

- `PipelineBackend` 已改成 `ROB head` 驱动退休，不再依赖 `mem_wb` 单槽作为唯一可退休入口。
- backend 已接入最小独立 memory execute：RAM / faulting access 会形成可被 younger ALU 越过的最小 OoO 完成窗口；已知 MMIO load 继续维持 non-speculative 执行。
- `pipeline_rename_commit_smoke` 与 `pipeline_speculation_contracts_smoke` 当前已直接守住“older memory 未完成时 younger ALU 先完成，但 architected state 仍按 ROB 顺序退休”的新合同。
- 本计划收口后，`cd myCPU && make test-pipeline` 与 `cd myCPU && make test` 已重新验证通过。
