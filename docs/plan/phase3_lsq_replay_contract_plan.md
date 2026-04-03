# Phase 3 LSQ replay-needed 合同实现计划

> **文档状态：** 已完成

> **面向 AI 代理的工作者：** 必需子技能：使用 `superpowers:executing-plans` 在当前会话中按任务推进，或在明确授权后使用 `superpowers:subagent-driven-development` 分任务执行。步骤使用复选框（`- [ ]`）语法跟踪进度。

**目标：** 在不引入真正自动 replay 的前提下，为当前 `Phase 3-B/C` 的 `LSQ` 补齐 “replay-needed” 的最小合同、观测面和持久回归。

**架构：** 本计划只把 `LSQ` 从“能阻塞 / 放行 younger load”推进到“能显式表达为什么阻塞、是否需要 replay”的形态，不直接接通真实 replay machinery。`pipeline` 继续保持单发射、in-order retire 与近似顺序 execute；本轮重点是把 `older store unresolved / known overlap / late overlap -> replay-needed` 这些 memory-order 合同变成稳定接口和测试，而不是立即做 flush + 重发。

**技术栈：** C++17、GNU Make、host-side smoke / differential、asm / guest regression、Node `--test`、RISC-V 交叉工具链。

## 文档定位

本文档用于把当前 `Phase 3-B/C` 在 `LSQ` 上的下一块结构收口，细化为一份可直接执行的 “replay-needed 但暂不自动 replay” 子计划。

它重点回答：

- 当前 `LSQ` 已经能表达什么，还缺什么。
- 首轮 `replay-needed` 应以什么最小状态落地，避免直接卷入复杂 replay。
- 这一轮应该补哪些 smoke、观测面和状态文档回写。

本文档只回答“怎么落地”。当前实时结果和完成态回写以 [status/mainline_status.md](../status/mainline_status.md) 为准。

## 关联文档

- 来源设计：
  - [design/phase3_ooo_execution_model_design.md](../design/phase3_ooo_execution_model_design.md)
  - [design/pipeline_speculation_contracts.md](../design/pipeline_speculation_contracts.md)
  - [plan/phase3_ooo_execution_plan.md](phase3_ooo_execution_plan.md)
- 目标状态：
  - [status/mainline_status.md](../status/mainline_status.md)

## 目标

- 让 `LSQ` 显式表达 younger load 当前是：
  - 被 older unresolved store 阻塞；
  - 被 known-overlap store 阻塞；
  - 或已命中 “late overlap，需要 replay” 的状态。
- 在不引入自动 replay 的前提下，把 `replay-needed` 至少暴露到 helper 接口、host smoke 与 debug 观测面。
- 保持当前 `Phase 3-B/C` 边界：不接真正的 flush + 重发 load，不把 MMIO 变成投机请求，不把 `functional` path 卷进来。

## 完成定义

- `LoadStoreQueue` 已提供最小 replay-related 状态查询接口，至少能区分：
  - `none`
  - `blocked_by_unresolved_store`
  - `blocked_by_overlapping_store`
  - `replay_required`
- `load_store_queue_smoke` 能覆盖：
  - unresolved older store 阻塞 younger load；
  - older store 地址 / 数据已知但确认 overlap 时继续阻塞；
  - load 已先通过、随后 older store 地址解析出 overlap 时被标记为 `replay_required`。
- `pipeline_speculation_contracts_smoke` 与 `debug_cli_smoke` 已能观察到最小 replay-needed 结果，而不会误把它解释成真实已完成的 replay。
- 以下验证通过：
  - `cd myCPU && make test-host-load_store_queue_smoke`
  - `cd myCPU && make test-host-pipeline_speculation_contracts_smoke`
  - `cd myCPU && make test-host-debug_cli_smoke`
  - `cd myCPU && make test-host-backend_differential_smoke`
  - `cd myCPU && make test-pipeline`
  - `cd myCPU && make test`

## 文件结构

### 计划内重点修改文件

- `myCPU/src/exec/load_store_queue.h`
- `myCPU/src/exec/load_store_queue.cpp`
- `myCPU/src/exec/pipeline_core_state.h`
- `myCPU/src/exec/pipeline_core_state.cpp`
- `myCPU/src/exec/pipeline_backend.h`
- `myCPU/src/exec/pipeline_backend.cpp`
- `myCPU/src/debug/debug_protocol.h`
- `myCPU/src/debug/debug_snapshot.h`
- `myCPU/src/debug/debug_protocol.cpp`
- `myCPU/tests/host/load_store_queue_smoke.cpp`
- `myCPU/tests/host/pipeline_speculation_contracts_smoke.cpp`
- `myCPU/tests/host/debug_cli_smoke.cpp`
- `docs/index.md`
- `myCPU/AGENTS.md`
- `docs/status/mainline_status.md`

### 本轮明确不改

- 不做真正的 automatic replay / flush + reissue。
- 不做 store-to-load forwarding。
- 不把 MMIO 请求纳入投机 replay 范围。
- 不扩成更激进的 memory disambiguation 策略。

## 任务

### 任务 1：补齐 `LSQ replay-needed` 的 helper 接口与 smoke

**文件：**
- 修改：`myCPU/src/exec/load_store_queue.h`
- 修改：`myCPU/src/exec/load_store_queue.cpp`
- 修改：`myCPU/tests/host/load_store_queue_smoke.cpp`

- [x] **步骤 1：先写失败测试。**
  至少覆盖：
  - unresolved older store 对 younger load 的阻塞原因可区分；
  - known overlap 的阻塞原因可区分；
  - 某条 younger load 先通过后，如果 older store 地址晚到且确认 overlap，会被标记为 `replay_required`。

- [x] **步骤 2：运行失败测试验证红灯。**
  运行：
  - `cd myCPU && make test-host-load_store_queue_smoke`
  预期：当前 `LSQ` 只能回答“要不要阻塞”，还不能回答“为什么阻塞 / 是否需要 replay”。

- [x] **步骤 3：写最小实现代码。**
  实现要求：
  - 不改当前 commit / retire 主路径。
  - `LSQ` 的 replay-related 状态以最小接口存在，不提前暴露过多调度器内部细节。
  - 不引入真正的自动 replay 执行。

- [x] **步骤 4：重跑 helper smoke，确认绿灯。**
  运行：
  - `cd myCPU && make test-host-load_store_queue_smoke`

- [x] **步骤 5：本轮不单独切 commit，纳入当前 `phase3-ooo-execution` 工作树累计变更。**

### 任务 2：把 replay-needed 状态接到 pipeline / debug 观测面

**文件：**
- 修改：`myCPU/src/exec/pipeline_backend.cpp`
- 修改：`myCPU/src/debug/debug_snapshot.h`
- 修改：`myCPU/src/debug/debug_protocol.cpp`
- 修改：`myCPU/tests/host/pipeline_speculation_contracts_smoke.cpp`
- 修改：`myCPU/tests/host/debug_cli_smoke.cpp`
- 验证：`myCPU/tests/host/backend_differential_smoke.cpp`

- [x] **步骤 1：先写失败测试。**
  至少覆盖：
  - `pipeline` 在 late-overlap 场景下能把 replay-needed 暴露给 smoke；
  - `debug_snapshot` / `debug_cli` 至少能透出一个最小字段，说明当前存在 replay-needed load 或等价状态；
  - `backend_differential_smoke` 保持 architected 一致，但允许 pipeline 额外暴露 replay-needed 中间态。

- [x] **步骤 2：运行失败测试验证红灯。**
  运行：
  - `cd myCPU && make test-host-pipeline_speculation_contracts_smoke`
  - `cd myCPU && make test-host-debug_cli_smoke`
  - `cd myCPU && make test-host-backend_differential_smoke`

- [x] **步骤 3：写最小接线代码。**
  实现要求：
  - replay-needed 只进入观察面，不改变现有 architected 结果。
  - 不把 snapshot 扩成内部调度器转储。
  - 不让 `functional` path 感知 replay-needed 的 pipeline 私有状态。

- [x] **步骤 4：重跑 smoke，确认绿灯。**
  运行：
  - `cd myCPU && make test-host-pipeline_speculation_contracts_smoke`
  - `cd myCPU && make test-host-debug_cli_smoke`
  - `cd myCPU && make test-host-backend_differential_smoke`

- [x] **步骤 5：本轮不单独切 commit，纳入当前 `phase3-ooo-execution` 工作树累计变更。**

### 任务 3：跑总门禁并回写状态文档

**文件：**
- 修改：`myCPU/AGENTS.md`
- 修改：`docs/status/mainline_status.md`

- [x] **步骤 1：运行总门禁。**
  运行：
  - `cd myCPU && make test-host-load_store_queue_smoke`
  - `cd myCPU && make test-host-pipeline_speculation_contracts_smoke`
  - `cd myCPU && make test-host-debug_cli_smoke`
  - `cd myCPU && make test-host-backend_differential_smoke`
  - `cd myCPU && make test-pipeline`
  - `cd myCPU && make test`

- [x] **步骤 2：回写文档。**
  需要更新：
  - 当前 `LSQ` 已不仅能阻塞 / 放行，也能显式暴露 replay-needed 合同。
  - 当前仍未进入真正 automatic replay / store-to-load forwarding / aggressive disambiguation。

- [x] **步骤 3：本轮不单独切 commit，保留在当前工作树累计变更中，并把本计划勾到完成态。**

## 完成结果摘要

- `LoadStoreQueue` 当前已提供最小 `LsqLoadState / LsqLoadStatus` 合同，能显式区分
  `none`、`blocked_by_unresolved_store`、`blocked_by_overlapping_store` 与 `replay_required`。
- younger load 先通过、随后 older store 地址晚到且确认 overlap 的场景，当前会被稳定标记为 `replay_required`，但仍然只停留在 helper / snapshot / debug 观察面，不会自动触发 flush + reissue。
- `pipeline` 当前已把最小 `LSQ replay-needed` 状态接到 `DebugSnapshot` 和 debug JSON 序列化，字段为：
  - `lsq_load_state`
  - `lsq_load_sequence_id`
  - `lsq_store_sequence_id`
- 当前仍未进入真正 automatic replay、store-to-load forwarding 或更激进的 memory disambiguation。

## 完成态回写要求

- 全部 checklist 必须勾完。
- 文件头必须改成“已完成”或等价完成态说明。
- [status/mainline_status.md](../status/mainline_status.md) 必须回写：
  - 本轮 `LSQ replay-needed` 已落地结果；
  - 当前 `Phase 3-B/C` 执行模型边界；
  - 仍然有效的剩余风险；
  - 下一步是否进入真正 replay / forwarding / memory speculation。
