# Phase 3 LSQ automatic replay 实现计划

> **文档状态：** 已完成

> **面向 AI 代理的工作者：** 必需子技能：使用 `superpowers:executing-plans` 在当前会话中按任务推进，或在明确授权后使用 `superpowers:subagent-driven-development` 分任务执行。步骤使用复选框（`- [ ]`）语法跟踪进度。

**目标：** 把当前已经存在于 `LSQ` helper / debug 观测面的 `replay_required`，推进成最小可执行的 automatic replay 动作。

**架构：** 本计划故意只做一刀最小 `RAM-only` replay recovery：当 `LSQ` 中出现 `replay_required` load 时，`pipeline` 在下一拍沿现有统一 rollback / flush 主路径，回到当前 committed 边界重新取指，丢弃所有未提交的 speculative `rename / ROB / phys / LSQ` 状态。整个过程不引入新的调度器、不做 store-to-load forwarding，也不把 MMIO 纳入 replay 范围。

**技术栈：** C++17、GNU Make、host-side smoke / differential、RISC-V 交叉工具链。

## 文档定位

本文档用于把当前 `Phase 3-B/C` 中已经落地的 `LSQ replay-needed` 观察面，继续收口成一份可直接执行的 automatic replay 子计划。

它重点回答：

- 当前 `replay_required` 应以什么最小动作落地。
- recovery 应落在 `LSQ helper`、`PipelineCoreState` 还是 `PipelineBackend` orchestration。
- 这一轮需要补哪些 smoke、观测面和状态文档回写。

本文档只回答“怎么落地”。当前实时结果和完成态回写以 [status/mainline_status.md](../status/mainline_status.md) 为准。

## 关联文档

- 来源设计：
  - [design/phase3_ooo_execution_model_design.md](../design/phase3_ooo_execution_model_design.md)
  - [design/pipeline_speculation_contracts.md](../design/pipeline_speculation_contracts.md)
  - [plan/phase3_ooo_execution_plan.md](phase3_ooo_execution_plan.md)
  - [plan/phase3_lsq_replay_contract_plan.md](phase3_lsq_replay_contract_plan.md)
- 目标状态：
  - [status/mainline_status.md](../status/mainline_status.md)

## 目标

- 让 `pipeline` 不再只暴露 `replay_required` 状态，而是在观察到该状态后执行一次最小 automatic replay recovery。
- recovery 继续沿现有统一 rollback / flush 主路径完成，不单独发明新的局部回滚器。
- replay 后重新从当前 committed 边界取指，保证 architected state、RAM / MMIO side effect、retire trace 仍保持 precise。
- 为后续 forwarding / 更激进 memory speculation 留出接口，但本轮不直接实现。

## 完成定义

- `PipelineBackend` 能在 cycle 入口观察 `LSQ active_replay()`，并在 `replay_required` 时触发一次 automatic replay flush。
- replay flush 后：
  - speculative `rename / ROB / phys / LSQ` younger state 被清空；
  - architected state 保持为当前 committed baseline；
  - 下一拍会从 committed `pc` 重新取指；
  - `DebugSnapshot` 至少能说明本拍发生过 replay flush。
- `pipeline_speculation_contracts_smoke` 能覆盖：
  - 注入 `replay_required` 后，backend 不会错误 commit 受污染 load；
  - automatic replay flush 后，`lsq` 清空、`pc` 回到 committed 边界、architected state 不变；
  - replay flush 不会把 `replay_required` 误解释成 trap / interrupt。
- `debug_cli_smoke` 能覆盖最小 replay flush 观测面。
- 以下验证通过：
  - `cd myCPU && make test-host-pipeline_speculation_contracts_smoke`
  - `cd myCPU && make test-host-debug_cli_smoke`
  - `cd myCPU && make test-host-backend_differential_smoke`
  - `cd myCPU && make test-pipeline`
  - `cd myCPU && make test`

## 文件结构

### 计划内重点修改文件

- `myCPU/src/exec/pipeline_core_state.h`
- `myCPU/src/exec/pipeline_core_state.cpp`
- `myCPU/src/exec/pipeline_backend.h`
- `myCPU/src/exec/pipeline_backend.cpp`
- `myCPU/src/debug/debug_snapshot.h`
- `myCPU/src/debug/debug_protocol.cpp`
- `myCPU/tests/host/pipeline_speculation_contracts_smoke.cpp`
- `myCPU/tests/host/debug_cli_smoke.cpp`
- `myCPU/tests/host/backend_differential_smoke.cpp`
- `docs/index.md`
- `myCPU/AGENTS.md`
- `docs/status/mainline_status.md`

### 本轮明确不改

- 不做 store-to-load forwarding。
- 不把 MMIO 请求纳入 replay 执行范围。
- 不引入新的 issue queue / scheduler / 多级 recovery。
- 不改变 `functional` backend 的 ISA 真值职责。

## 任务

### 任务 1：补 automatic replay flush 的失败测试

**文件：**
- 修改：`myCPU/tests/host/pipeline_speculation_contracts_smoke.cpp`
- 修改：`myCPU/tests/host/debug_cli_smoke.cpp`
- 验证：`myCPU/tests/host/backend_differential_smoke.cpp`

- [x] **步骤 1：先写失败测试。**
  至少覆盖：
  - 人工注入 `replay_required` 后，backend 下一拍会执行 replay flush，而不是继续提交受污染 load。
  - replay flush 后 `lsq` 被清空、`pc` 回到 committed 边界、architected state 保持不变。
  - debug JSON 能透出最小 replay flush 标记。

- [x] **步骤 2：运行失败测试验证红灯。**
  运行：
  - `cd myCPU && make test-host-pipeline_speculation_contracts_smoke`
  - `cd myCPU && make test-host-debug_cli_smoke`
  预期：当前只会暴露 `replay_required` 状态，不会自动执行 recovery。

### 任务 2：接 automatic replay 的最小 recovery 主路径

**文件：**
- 修改：`myCPU/src/exec/pipeline_core_state.h`
- 修改：`myCPU/src/exec/pipeline_core_state.cpp`
- 修改：`myCPU/src/exec/pipeline_backend.h`
- 修改：`myCPU/src/exec/pipeline_backend.cpp`
- 修改：`myCPU/src/debug/debug_snapshot.h`
- 修改：`myCPU/src/debug/debug_protocol.cpp`

- [x] **步骤 1：写最小实现代码。**
  实现要求：
  - replay 触发点放在 `PipelineBackend` 的 cycle 入口，优先于本拍 commit / execute。
  - recovery 直接复用现有 committed rollback + pipeline flush，而不是发明新的部分重放器。
  - replay flush 需要有一个最小可观察标记，但不要把 snapshot 扩成调度器内部转储。

- [x] **步骤 2：重跑 smoke，确认绿灯。**
  运行：
  - `cd myCPU && make test-host-pipeline_speculation_contracts_smoke`
  - `cd myCPU && make test-host-debug_cli_smoke`
  - `cd myCPU && make test-host-backend_differential_smoke`

### 任务 3：跑总门禁并回写状态文档

**文件：**
- 修改：`docs/index.md`
- 修改：`myCPU/AGENTS.md`
- 修改：`docs/status/mainline_status.md`

- [x] **步骤 1：运行总门禁。**
  运行：
  - `cd myCPU && make test-host-pipeline_speculation_contracts_smoke`
  - `cd myCPU && make test-host-debug_cli_smoke`
  - `cd myCPU && make test-host-backend_differential_smoke`
  - `cd myCPU && make test-pipeline`
  - `cd myCPU && make test`

- [x] **步骤 2：回写文档。**
  需要更新：
  - 当前 `LSQ replay-needed` 已进入最小 automatic replay。
  - 当前 automatic replay 仍然只是 coarse、RAM-only recovery。
  - 当前仍未进入 forwarding / 更激进 OoO execute。

- [x] **步骤 3：本轮不单独切 commit，纳入当前 `phase3-ooo-execution` 工作树累计变更。**

## 完成结果摘要

- `PipelineBackend` 当前已能在 cycle 入口消费 `LSQ active_replay()`；一旦观察到 `replay_required`，会直接沿现有 committed rollback + pipeline flush 主路径回到安全边界。
- automatic replay 当前是 coarse、RAM-only recovery：恢复动作会清空 speculative `rename / ROB / phys / LSQ` 状态，并从当前 committed `pc` 重新取指，而不是尝试局部重放单条 load。
- `DebugSnapshot` / debug JSON 当前已新增 `replay_flush` 观测位；`pipeline_speculation_contracts_smoke` 与 `debug_cli_smoke` 已分别守住“受污染 younger load 不得先 commit”和“replay flush 可观察”这两条合同。
- 当前仍未进入 store-to-load forwarding，也还没有更激进的自然 replay 触发路径或更激进的 OoO execute。

## 完成态回写要求

- 全部 checklist 必须勾完。
- 文件头必须改成“已完成”或等价完成态说明。
- [status/mainline_status.md](../status/mainline_status.md) 必须回写：
  - 本轮 automatic replay 已落地结果；
  - 当前 `Phase 3-B/C` 执行模型边界；
  - 仍然有效的剩余风险；
  - 下一步是否进入 forwarding / 更激进 memory speculation。
