# Phase 3 LSQ store-to-load forwarding 实现计划

> **文档状态：** 已完成

> **面向 AI 代理的工作者：** 必需子技能：使用 `superpowers:executing-plans` 在当前会话中按任务推进，或在明确授权后使用 `superpowers:subagent-driven-development` 分任务执行。步骤使用复选框（`- [ ]`）语法跟踪进度。

**目标：** 为当前 `Phase 3-B/C` 的 `LSQ` 补齐最小 `RAM-only store-to-load forwarding`，把已经可观测、可恢复的 memory-order 状态推进成最小数据旁路能力。

**架构：** 本计划只做一刀最小 forwarding：当 younger RAM load 在 `step_mem()` 阶段遇到一个更老、已 `address_ready + data_ready + order_ready` 且能够完整覆盖该 load 字节范围的 RAM store 时，直接从 `LSQ` 取数，而不是去 RAM 读旧值。整个过程不引入 MMIO forwarding，不做复杂 partial merge，不把当前 coarse automatic replay 和更激进的 memory speculation 一起卷进来。

**技术栈：** C++17、GNU Make、host-side smoke / differential、RISC-V 交叉工具链。

## 文档定位

本文档用于把当前 `Phase 3-B/C` 中已经落地的 `LSQ replay-needed + coarse automatic replay` 继续收口成下一份可直接执行的 forwarding 子计划。

它重点回答：

- 最小 forwarding 应该支持什么，不支持什么。
- forwarding helper 应该落在 `LSQ` 还是直接散落在 `PipelineBackend`。
- 这一轮应该补哪些 smoke，如何验证“不从 RAM 读旧值”这条合同。

本文档只回答“怎么落地”。当前实时结果和完成态回写以 [status/mainline_status.md](../status/mainline_status.md) 为准。

## 关联文档

- 来源设计：
  - [design/phase3_ooo_execution_model_design.md](../design/phase3_ooo_execution_model_design.md)
  - [design/pipeline_speculation_contracts.md](../design/pipeline_speculation_contracts.md)
  - [plan/phase3_ooo_execution_plan.md](phase3_ooo_execution_plan.md)
  - [plan/phase3_lsq_automatic_replay_plan.md](phase3_lsq_automatic_replay_plan.md)
- 目标状态：
  - [status/mainline_status.md](../status/mainline_status.md)

## 目标

- 为 `LSQ` 增加一个最小 forwarding 查询接口，能够回答“当前这条 younger RAM load 能否从更老的 ready store 直接取值”。
- 让 `PipelineBackend::step_mem()` 对 load 先尝试 `LSQ` forwarding，再决定是否回落到 `AddressSpace::load_result()`。
- 保持当前 precise exception / interrupt / MMIO / commit-boundary 合同不被破坏。

## 完成定义

- `LoadStoreQueue` 已能为给定 `(sequence_id, load_addr, load_size)` 返回最小 forwarding 结果，至少覆盖：
  - older ready RAM store 完整覆盖该 load 时可以前递；
  - 遇到更近但无法完整前递的 overlapping store 时，不能错误地退回到更老 store；
  - MMIO / 非 RAM 范围不参与 forwarding。
- `pipeline_speculation_contracts_smoke` 能覆盖：
  - 在 older store 尚未 commit 到 RAM 时，younger load 仍能通过 `LSQ` 取到该 store 的值；
  - RAM 内容在 store commit 前仍保持旧值，说明取值来自 forwarding 而不是提前写内存；
  - forwarding 不会误报成 trap / replay。
- `load_store_queue_smoke` 能覆盖最小 helper 合同。
- 以下验证通过：
  - `cd myCPU && make test-host-load_store_queue_smoke`
  - `cd myCPU && make test-host-pipeline_speculation_contracts_smoke`
  - `cd myCPU && make test-host-backend_differential_smoke`
  - `cd myCPU && make test-pipeline`
  - `cd myCPU && make test`

## 文件结构

### 计划内重点修改文件

- `myCPU/src/exec/load_store_queue.h`
- `myCPU/src/exec/load_store_queue.cpp`
- `myCPU/src/exec/pipeline_backend.cpp`
- `myCPU/tests/host/load_store_queue_smoke.cpp`
- `myCPU/tests/host/pipeline_speculation_contracts_smoke.cpp`
- `docs/index.md`
- `myCPU/AGENTS.md`
- `docs/status/mainline_status.md`

### 本轮明确不改

- 不做 MMIO forwarding。
- 不做复杂 partial merge / store byte-enable 拼接策略。
- 不把 current automatic replay 扩成复杂 replay storm control。
- 不引入新的 scheduler / issue queue / superscalar 行为。

## 任务

### 任务 1：补 forwarding helper 的失败测试

**文件：**
- 修改：`myCPU/tests/host/load_store_queue_smoke.cpp`
- 修改：`myCPU/tests/host/pipeline_speculation_contracts_smoke.cpp`

- [x] **步骤 1：先写失败测试。**
  至少覆盖：
  - full-cover older RAM store 能为 younger load 提供 forwarding 值；
  - 遇到更近但不完整覆盖的 overlapping store 时，不能错误使用更老 store 的值；
  - pipeline load 在 older store 尚未 commit 到 RAM 时，也能观察到 forwarding 的结果。

- [x] **步骤 2：运行失败测试验证红灯。**
  运行：
  - `cd myCPU && make test-host-load_store_queue_smoke`
  - `cd myCPU && make test-host-pipeline_speculation_contracts_smoke`
  预期：当前 `LSQ` 还没有 forwarding helper，pipeline load 只能去 RAM 取值。

### 任务 2：接最小 LSQ forwarding helper 与 pipeline load path

**文件：**
- 修改：`myCPU/src/exec/load_store_queue.h`
- 修改：`myCPU/src/exec/load_store_queue.cpp`
- 修改：`myCPU/src/exec/pipeline_backend.cpp`

- [x] **步骤 1：写最小实现代码。**
  实现要求：
  - forwarding helper 放在 `LoadStoreQueue`，避免把内存年龄扫描逻辑散落进 backend。
  - 只支持 RAM-only、full-cover forwarding。
  - `step_mem(load)` 先查 forwarding，未命中才走 `AddressSpace::load_result()`。

- [x] **步骤 2：重跑 smoke，确认绿灯。**
  运行：
  - `cd myCPU && make test-host-load_store_queue_smoke`
  - `cd myCPU && make test-host-pipeline_speculation_contracts_smoke`
  - `cd myCPU && make test-host-backend_differential_smoke`

### 任务 3：跑总门禁并回写状态文档

**文件：**
- 修改：`docs/index.md`
- 修改：`myCPU/AGENTS.md`
- 修改：`docs/status/mainline_status.md`

- [x] **步骤 1：运行总门禁。**
  运行：
  - `cd myCPU && make test-host-load_store_queue_smoke`
  - `cd myCPU && make test-host-pipeline_speculation_contracts_smoke`
  - `cd myCPU && make test-host-backend_differential_smoke`
  - `cd myCPU && make test-pipeline`
  - `cd myCPU && make test`

- [x] **步骤 2：回写文档。**
  需要更新：
  - 当前 `LSQ` 已具备最小 RAM-only store-to-load forwarding。
  - 当前 forwarding 仍然不覆盖 MMIO，也不做复杂 partial merge。
  - 当前下一步才是更激进的 OoO execute / memory speculation。

- [x] **步骤 3：本轮不单独切 commit，纳入当前 `phase3-ooo-execution` 工作树累计变更。**

## 完成结果摘要

- `LoadStoreQueue` 当前已提供最小 `forwardable_load()` helper，能够对 younger RAM load 查询更老 ready store 是否能提供 full-cover forwarding。
- `PipelineBackend::step_mem(load)` 当前会先查 `LSQ` forwarding；命中时直接把前递值写入 `ROB + phys + load LSQ entry`，未命中才回落到 `AddressSpace::load_result()`。
- 当前 forwarding 严格限定在最小边界：只支持 `RAM-only`、只支持 full-cover forwarding、不支持 MMIO forwarding，也不做复杂 partial merge。
- `load_store_queue_smoke` 与 `pipeline_speculation_contracts_smoke` 当前分别守住 helper 合同和“store 未 commit 到 RAM 但 younger load 已能通过 forwarding 取值”这两条边界。

## 完成态回写要求

- 全部 checklist 必须勾完。
- 文件头必须改成“已完成”或等价完成态说明。
- [status/mainline_status.md](../status/mainline_status.md) 必须回写：
  - 本轮 forwarding 已落地结果；
  - 当前 `Phase 3-B/C` 执行模型边界；
  - 仍然有效的剩余风险；
  - 下一步是否进入更激进的 OoO execute / memory speculation。
