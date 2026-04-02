# Phase 3-B/C OoO 前置准备实现计划

> **文档状态：** 已完成（2026-04-02）

> **面向 AI 代理的工作者：** 必需子技能：使用 `superpowers:subagent-driven-development`（推荐）或 `superpowers:executing-plans` 逐任务实现此计划。步骤使用复选框（`- [ ]`）语法来跟踪进度。

**目标：** 在不立即把当前 `pipeline` 改造成 OoO 后端的前提下，先补齐设计契约、提交边界、结构拆分和验证基建，把仓库推进到可以安全直接接入大块 `OoO / rename / ROB / LSQ` 的状态。

**架构：** 本计划不直接实现 `OoO / rename / ROB / LSQ`，而是先交付 4 类前置成果：`Phase 3-B/C` 设计与投机契约、可稳定追踪年龄与退休顺序的后端观测面、显式的 architectural commit boundary，以及未接线但已单测的 `rename_map / reorder_buffer / load_store_queue` helper。完成后，后续“大块 OoO 实现”将变成一轮明确的后端接线与验证扩展，而不是“重写 pipeline + 重写协议 + 重写回归”的混合大分支。

**技术栈：** C++17、GNU Make、host-side g++ smoke tests、现有 asm / guest 回归、Node `--test`、RISC-V 交叉工具链。

## 执行结果摘要

- `Phase 3-B/C` 执行模型设计与投机执行合同文档已补齐。
- `pipeline` 已具备 `sequence_id` / retire trace 观测面，以及共享的 architectural commit boundary helper。
- `PipelineBackend` 已拆出 `pipeline_core_state` 与 `pipeline_hazards`，当前主路径已收口为更明确的 orchestration shell。
- `rename_map / reorder_buffer / load_store_queue` 已作为未接线 helper 独立存在，并各自拥有 host-side smoke。
- 当前仓库已经具备直接新开“大块 `OoO / rename / ROB / LSQ` 接线计划”的前置条件。

## 文档定位

本文档用于把“还需要做什么，才可以正式开始做 `Phase 3` 的大块 `OoO / rename / ROB / LSQ`”收口成一份正式执行计划。

它不回答最终的 OoO 方案细节如何实现，而是回答：

- 进入大块 OoO 之前还缺哪些前置工程
- 这些前置工程应按什么顺序落地
- 每一轮要改哪些文件、补哪些测试、以什么门禁确认已经具备下一步条件

本文档只回答“怎么把仓库推进到 OoO-ready”。实时完成情况与后续主线结论以 [mainline_status.md](../status/mainline_status.md) 为准。

## 关联文档

- 来源设计：
  - [design/phase3_branch_prediction_design.md](../design/phase3_branch_prediction_design.md)
- 目标状态：
  - [status/mainline_status.md](../status/mainline_status.md)

---

## 参考文档

- [AGENTS.md](../../AGENTS.md)
- [myCPU/AGENTS.md](../../myCPU/AGENTS.md)
- [myCPU/guest/AGENTS.md](../../myCPU/guest/AGENTS.md)
- [design/regression_completion_criteria.md](../design/regression_completion_criteria.md)
- [design/pipeline_core_integration.md](../design/pipeline_core_integration.md)
- [design/debug_frontend_integration.md](../design/debug_frontend_integration.md)
- [status/code_self_review_status.md](../status/code_self_review_status.md)
- [phase3_branch_prediction_plan.md](phase3_branch_prediction_plan.md)

## 目标

- 为后续大块 `OoO / rename / ROB / LSQ` 实现补齐正式设计文档与投机执行契约，避免边做边改边界。
- 把当前 `PipelineBackend` 的年龄顺序、退休记录、提交边界与状态编排显式化，而不是继续埋在单体类内部。
- 为后续 `ROB / rename / LSQ` 接线先准备可独立单测的 helper 模块，而不是第一次接入时同时发明数据结构和改后端主路径。
- 把“squash younger side effects / precise exception / interrupt visibility / MMIO non-speculative”这组高风险合同压成稳定门禁。
- 在保持 `functional + shared InstructionSemantics` 为唯一 ISA 真值来源的前提下，为下一轮大块 OoO 实现清掉结构性前置障碍。

## 完成定义

- 新增一份 `Phase 3-B/C` 执行模型设计文档，以及一份投机执行 / architectural commit contract 设计文档。
- `pipeline` 内部已经具备稳定的 sequence / retire trace，可在 smoke、differential 和 `debug_cli` 中观察年龄与退休顺序。
- 当前后端的 architected side effects 已收口到显式 commit boundary helper，而不是分散在 `PipelineBackend` 的局部流程里。
- `PipelineBackend` 的状态寄存器、hazard / forwarding 判定、redirect / fetch-fault / trap-flush 编排不再全部硬编码在单个实现文件里。
- `rename_map`、`reorder_buffer` 和 `load_store_queue` 已作为未接线 helper 独立存在，并拥有最小 host-side 单元 / smoke 门禁。
- `make test`、`make test-pipeline`、`cd frontend && node --test` 在上述前置改动后继续通过。
- 到该计划收尾时，可以单独新开一份“大块 OoO/rename/ROB/LSQ 接线计划”，而不需要先回头补基础设施。

## 文件结构

### 新增文件

- `docs/design/phase3_ooo_execution_model_design.md`
  定义 `Phase 3-B/C` 的正式目标、范围边界、非目标、接线顺序和完成判据。
- `docs/design/pipeline_speculation_contracts.md`
  定义投机执行下的 precise exception、interrupt、CSR、MMIO、`sfence` / TLB、load/store visibility 合同。
- `myCPU/src/exec/pipeline_sequence.h`
  `sequence_id`、`retire trace` 与年龄顺序辅助类型。
- `myCPU/src/exec/pipeline_sequence.cpp`
  `sequence_id` / `retire trace` 的最小实现。
- `myCPU/src/exec/pipeline_commit_boundary.h`
  commit boundary 输入、输出和 side-effect apply helper 接口。
- `myCPU/src/exec/pipeline_commit_boundary.cpp`
  `PipelineBackend` 与 `FunctionalBackend` 共享的 commit-boundary 编排实现。
- `myCPU/src/exec/pipeline_core_state.h`
  当前五级 backend 的核心状态载体：stage regs、fetch / redirect / pending fault / sequence counter 等。
- `myCPU/src/exec/pipeline_core_state.cpp`
  `pipeline_core_state` 的 reset / rotate / helper 实现。
- `myCPU/src/exec/pipeline_hazards.h`
  decode hazard、load-use stall、operand forwarding 等 helper 接口。
- `myCPU/src/exec/pipeline_hazards.cpp`
  当前 in-order backend 的 hazard / forwarding 实现。
- `myCPU/src/exec/rename_map.h`
  物理寄存器映射表与 checkpoint / rollback helper 的独立接口。
- `myCPU/src/exec/rename_map.cpp`
  最小 rename map 实现。
- `myCPU/src/exec/reorder_buffer.h`
  `ROB` entry、allocate / mark-ready / commit / flush-younger 接口。
- `myCPU/src/exec/reorder_buffer.cpp`
  最小 `ROB` helper 实现。
- `myCPU/src/exec/load_store_queue.h`
  `LSQ` entry、enqueue / complete / retire / replay tag 接口。
- `myCPU/src/exec/load_store_queue.cpp`
  最小 `LSQ` helper 实现。
- `myCPU/tests/host/pipeline_commit_trace_smoke.cpp`
  `sequence_id` / `retire trace` 最小门禁。
- `myCPU/tests/host/pipeline_speculation_contracts_smoke.cpp`
  squashed younger store / MMIO、commit-visible CSR、fault / interrupt precision 合同门禁。
- `myCPU/tests/host/rename_map_smoke.cpp`
  rename map 独立 smoke。
- `myCPU/tests/host/reorder_buffer_smoke.cpp`
  `ROB` 独立 smoke。
- `myCPU/tests/host/load_store_queue_smoke.cpp`
  `LSQ` 独立 smoke。

### 重点修改文件

- `myCPU/src/isa/effects.h`
  把可投机执行结果与必须在 commit boundary 生效的动作边界补清楚。
- `myCPU/src/exec/pipeline_types.h`
  给 stage slot 增加 sequence / commit metadata，并为后续 `ROB` 接线预留 age / tag 字段。
- `myCPU/src/exec/pipeline_backend.h`
  把原本散落的状态、helper 和 snapshot cache 收口到新 helper 类型。
- `myCPU/src/exec/pipeline_backend.cpp`
  从“单体五级后端”逐步收口为 orchestration shell。
- `myCPU/src/exec/functional_backend.cpp`
  与 `pipeline` 共用 commit boundary / side-effect helper，保持 architected 行为一致。
- `myCPU/src/debug/debug_snapshot.h`
  增加 retire trace / sequence 最小观测字段。
- `myCPU/src/debug/debug_protocol.cpp`
  稳定序列化 retire trace / sequence 字段。
- `myCPU/tests/host/pipeline_backend_smoke.cpp`
  补后端结构与 commit boundary 行为 smoke。
- `myCPU/tests/host/backend_differential_smoke.cpp`
  补 speculative contract 与 retire order 差分场景。
- `myCPU/tests/host/debug_cli_smoke.cpp`
  补 retire trace / sequence / commit-boundary JSON 字段 smoke。
- `myCPU/Makefile`
  接入新增 host smoke，并把 OoO readiness 门禁并入 `test-pipeline`。
- `myCPU/AGENTS.md`
  回写 OoO-ready 前置结果、边界与验证要求。
- `docs/status/mainline_status.md`
  回写当前活跃计划、完成结果与是否已具备“大块 OoO”启动条件。
- `docs/index.md`
  同步新增计划文档入口。

### 本轮明确不改

- 不直接把 `PipelineBackend` 接成真正的 `OoO / rename / ROB / LSQ` 后端。
- 不把 `functional` 变成新的微架构实验场；它仍只负责 reference 真值。
- 不主动扩 `frontend/app/*` 的 UI 功能面；最多只被动兼容新的 snapshot 字段。
- 不把 guest runtime、`kernel_alpha`、`interactive_os` 语义变化混入本轮。

## 任务

### 任务 1：补齐 `Phase 3-B/C` 设计文档与投机执行契约

**文件：**
- 创建：`docs/design/phase3_ooo_execution_model_design.md`
- 创建：`docs/design/pipeline_speculation_contracts.md`
- 修改：`docs/status/mainline_status.md`

- [x] **步骤 1：先写 `Phase 3-B/C` 执行模型设计文档**

  这份设计必须明确以下边界，而不是只写“未来要做 OoO”：

  - 首个“大块 OoO”到底是什么：
    - 是否仍保持单发射 fetch / decode
    - 是否采用 in-order retire
    - `rename / ROB / LSQ` 的接线顺序
    - 第一轮是否支持 branch checkpoint
  - 明确非目标：
    - superscalar
    - cache hierarchy
    - multicore
    - memory disambiguation 的激进变体

  建议至少落成如下骨架：

  ```md
  ## 目标
  ## 非目标
  ## 当前 in-order backend 的结构障碍
  ## Phase 3-B：rename + ROB 最小接线
  ## Phase 3-C：LSQ + OoO execute 最小接线
  ## 完成定义
  ```

- [x] **步骤 2：再写投机执行契约文档**

  这份文档至少要写清：

  - precise exception / interrupt 的可观察边界
  - MMIO load / store 的非投机规则
  - CSR 写、`mret/sret`、`sfence.vma`、TLB flush 的 commit-visible 时机
  - RAM store、MMIO store、device side effects 在 squash 时如何处理
  - `LSQ` 第一轮允许和不允许的行为

  建议先把核心规则压成显式条目，例如：

  ```md
  - younger squashed store 不得落到 RAM / MMIO
  - MMIO 读写默认不得在 commit 前对设备生效
  - interrupt 只能在 architecturally precise 的 commit boundary 被观察
  - trap / fault 恢复必须按 retire 顺序裁剪 younger 指令
  ```

- [x] **步骤 3：把两份设计接回主线状态**

  修改 `docs/status/mainline_status.md`，把“当前活跃计划”与“建议入口”指向本计划，并在下一步里明确：

  - OoO-ready 之前先完成这两份设计
  - 直接做大块 `OoO / rename / ROB / LSQ` 不再被视为当前默认策略

- [x] **步骤 4：做最小文档校验**

  运行：

  - `git diff --check`
  - `python3 - <<'PY'`
    `from pathlib import Path`
    `for path in [Path('docs/design/phase3_ooo_execution_model_design.md'), Path('docs/design/pipeline_speculation_contracts.md')]:`
    `    assert path.exists(), path`
    `print('docs ok')`
    `PY`

  预期：

  - PASS
  - 新设计文档与主线状态链接都存在

- [x] **步骤 5：Commit**

  ```bash
  git add docs/design/phase3_ooo_execution_model_design.md docs/design/pipeline_speculation_contracts.md docs/status/mainline_status.md
  git commit -m "docs(phase3): 补齐 OoO 前置设计与投机合同"
  ```

### 任务 2：引入 sequence / retire trace，建立年龄顺序观测面

**文件：**
- 创建：`myCPU/src/exec/pipeline_sequence.h`
- 创建：`myCPU/src/exec/pipeline_sequence.cpp`
- 创建：`myCPU/tests/host/pipeline_commit_trace_smoke.cpp`
- 修改：`myCPU/src/exec/pipeline_types.h`
- 修改：`myCPU/src/exec/pipeline_backend.h`
- 修改：`myCPU/src/exec/pipeline_backend.cpp`
- 修改：`myCPU/src/debug/debug_snapshot.h`
- 修改：`myCPU/src/debug/debug_protocol.cpp`
- 修改：`myCPU/tests/host/pipeline_backend_smoke.cpp`
- 修改：`myCPU/tests/host/debug_cli_smoke.cpp`
- 修改：`myCPU/Makefile`

- [x] **步骤 1：先写失败的 retire trace smoke**

  在 `pipeline_commit_trace_smoke.cpp` 中先补 3 组最小断言：

  - 取指进入 pipeline 的指令拥有单调递增的 `sequence_id`
  - 分支 flush 后，被 squashed 的 younger 指令不会进入 retire trace
  - trap / fault / `mret/sret` 之后，retire trace 仍保持 architected retire 顺序

  建议先按下面的接口假设写测试：

  ```cpp
  const auto snapshot = backend.debug_snapshot();
  assert(snapshot.pipeline.last_sequence_id > 0);
  assert(snapshot.pipeline.retire_trace.back().sequence_id == expected_seq);
  ```

- [x] **步骤 2：运行测试验证失败**

  运行：`cd myCPU && make test-host-pipeline_commit_trace_smoke`

  预期：

  - FAIL
  - 报错指向缺少 `sequence_id` / `retire_trace` 字段或 Makefile 尚未接入

- [x] **步骤 3：实现 sequence / retire trace**

  在 `pipeline_types.h` 与新 helper 中至少引入：

  ```cpp
  struct SequenceId {
      uint64_t value{0};
  };

  struct RetireTraceEntry {
      uint64_t sequence_id{0};
      uint64_t pc{0};
      uint32_t raw{0};
      bool trap{false};
      bool redirect{false};
  };
  ```

  然后：

  - 在 fetch 时分配 `sequence_id`
  - 在真正 retired 时写入 bounded retire trace
  - 在 `debug_snapshot()` 与 `debug_protocol.cpp` 中暴露最小只读字段

- [x] **步骤 4：运行针对性验证**

  运行：

  - `cd myCPU && make test-host-pipeline_commit_trace_smoke`
  - `cd myCPU && make test-host-pipeline_backend_smoke`
  - `cd myCPU && make test-host-debug_cli_smoke`
  - `cd frontend && node --test`

  预期：

  - PASS
  - retire trace / sequence 字段在 host smoke 与 debug protocol 中可观察

- [x] **步骤 5：Commit**

  ```bash
  git add myCPU/src/exec/pipeline_sequence.h myCPU/src/exec/pipeline_sequence.cpp myCPU/tests/host/pipeline_commit_trace_smoke.cpp myCPU/src/exec/pipeline_types.h myCPU/src/exec/pipeline_backend.h myCPU/src/exec/pipeline_backend.cpp myCPU/src/debug/debug_snapshot.h myCPU/src/debug/debug_protocol.cpp myCPU/tests/host/pipeline_backend_smoke.cpp myCPU/tests/host/debug_cli_smoke.cpp myCPU/Makefile
  git commit -m "feat(pipeline): 引入 sequence 与 retire trace 观测面"
  ```

### 任务 3：显式化 architectural commit boundary 与 speculative side-effect 合同

**文件：**
- 创建：`myCPU/src/exec/pipeline_commit_boundary.h`
- 创建：`myCPU/src/exec/pipeline_commit_boundary.cpp`
- 创建：`myCPU/tests/host/pipeline_speculation_contracts_smoke.cpp`
- 修改：`myCPU/src/isa/effects.h`
- 修改：`myCPU/src/exec/functional_backend.cpp`
- 修改：`myCPU/src/exec/pipeline_backend.h`
- 修改：`myCPU/src/exec/pipeline_backend.cpp`
- 修改：`myCPU/tests/host/backend_differential_smoke.cpp`
- 修改：`myCPU/tests/host/pipeline_backend_smoke.cpp`
- 修改：`myCPU/Makefile`

- [x] **步骤 1：先写失败的 speculative contract smoke**

  在 `pipeline_speculation_contracts_smoke.cpp` 里至少补这几类场景：

  - 被 branch mispredict squash 的 younger RAM store 不会真正写入内存
  - 被 squash 的 younger MMIO store 不会真正命中设备
  - CSR 写只在 commit boundary 之后对后续 architected 观察生效
  - interrupt / fetch fault / trap-return 的可见性继续保持 precise

- [x] **步骤 2：运行测试验证失败**

  运行：`cd myCPU && make test-host-pipeline_speculation_contracts_smoke`

  预期：

  - FAIL
  - 旧实现没有显式 commit-boundary helper，也没有这组 smoke 所需的 side-effect contract

- [x] **步骤 3：把 side effects 收口到 commit boundary helper**

  这一轮不要求真正接入 `ROB`，但要先把合同做对：

  - `effects.h` 里把“可投机结果”和“必须在 commit 生效的动作”边界补清楚
  - `PipelineBackend` 不再把 CSR / redirect / TLB flush / halt / store side effect 直接散落在各处
  - `FunctionalBackend` 和 `PipelineBackend` 尽量复用同一份 commit-boundary apply helper

  可以考虑把接口收口成：

  ```cpp
  struct CommitBoundaryInput {
      uint64_t pc{0};
      InsnEffects effects{};
  };

  struct CommitBoundaryResult {
      bool retired{false};
      bool trap_flush{false};
      bool redirect{false};
      uint64_t next_pc{0};
  };
  ```

- [x] **步骤 4：运行针对性验证**

  运行：

  - `cd myCPU && make test-host-pipeline_speculation_contracts_smoke`
  - `cd myCPU && make test-host-backend_differential_smoke`
  - `cd myCPU && make test-host-pipeline_backend_smoke`
  - `cd myCPU && make test-pipeline`

  预期：

  - PASS
  - squash younger side effects、fault / interrupt precision 和现有 pipeline differential 主干都继续成立

- [x] **步骤 5：Commit**

  ```bash
  git add myCPU/src/exec/pipeline_commit_boundary.h myCPU/src/exec/pipeline_commit_boundary.cpp myCPU/tests/host/pipeline_speculation_contracts_smoke.cpp myCPU/src/isa/effects.h myCPU/src/exec/functional_backend.cpp myCPU/src/exec/pipeline_backend.h myCPU/src/exec/pipeline_backend.cpp myCPU/tests/host/backend_differential_smoke.cpp myCPU/tests/host/pipeline_backend_smoke.cpp myCPU/Makefile
  git commit -m "refactor(pipeline): 显式化 commit boundary 与投机 side-effect 合同"
  ```

### 任务 4：拆 `PipelineBackend` 单体，形成可替换状态与 hazard helper

**文件：**
- 创建：`myCPU/src/exec/pipeline_core_state.h`
- 创建：`myCPU/src/exec/pipeline_core_state.cpp`
- 创建：`myCPU/src/exec/pipeline_hazards.h`
- 创建：`myCPU/src/exec/pipeline_hazards.cpp`
- 修改：`myCPU/src/exec/pipeline_backend.h`
- 修改：`myCPU/src/exec/pipeline_backend.cpp`
- 修改：`myCPU/src/exec/pipeline_types.h`
- 修改：`myCPU/tests/host/pipeline_backend_smoke.cpp`

- [x] **步骤 1：先跑当前 pipeline 基线，固定 refactor 保护网**

  运行：

  - `cd myCPU && make test-host-pipeline_backend_smoke`
  - `cd myCPU && make test-host-backend_differential_smoke`
  - `cd myCPU && make test-pipeline`

  预期：

  - PASS
  - 作为接下来纯结构 refactor 的保护网

- [x] **步骤 2：下沉核心状态载体**

  先把这些状态从 `PipelineBackend` 类本体里移走：

  - `if_id / id_ex / ex_mem / mem_wb`
  - `fetch_pc`
  - `pending_fetch_fault`
  - `redirect_pending / redirect_target`
  - `last_cycle_*`
  - `sequence counter / retire trace`

  目标是让 `PipelineBackend` 只保留 orchestration ownership，而不是继续同时承载所有数据结构。

- [x] **步骤 3：下沉 hazard / forwarding helper**

  把当前这些 helper 从 `PipelineBackend` 中拆出去：

  - `reads_rs1 / reads_rs2`
  - `inflight_rd`
  - `has_decode_hazard`
  - `forward_operand_from_slot`
  - `resolve_ex_operand`

  保持行为不变，不在这一轮偷带 `OoO` 语义变化。

- [x] **步骤 4：运行 refactor 验证**

  运行：

  - `cd myCPU && make test-host-pipeline_backend_smoke`
  - `cd myCPU && make test-host-backend_differential_smoke`
  - `cd myCPU && make test-pipeline`

  预期：

  - PASS
  - 行为不变，但 `PipelineBackend` 已从“单体五级实现文件”变成更适合后续 `ROB / rename / LSQ` 接线的 orchestration shell

- [x] **步骤 5：Commit**

  ```bash
  git add myCPU/src/exec/pipeline_core_state.h myCPU/src/exec/pipeline_core_state.cpp myCPU/src/exec/pipeline_hazards.h myCPU/src/exec/pipeline_hazards.cpp myCPU/src/exec/pipeline_backend.h myCPU/src/exec/pipeline_backend.cpp myCPU/src/exec/pipeline_types.h myCPU/tests/host/pipeline_backend_smoke.cpp
  git commit -m "refactor(pipeline): 拆分 core state 与 hazard helper"
  ```

### 任务 5：引入未接线的 `rename_map / ROB / LSQ` helper 与独立门禁

**文件：**
- 创建：`myCPU/src/exec/rename_map.h`
- 创建：`myCPU/src/exec/rename_map.cpp`
- 创建：`myCPU/src/exec/reorder_buffer.h`
- 创建：`myCPU/src/exec/reorder_buffer.cpp`
- 创建：`myCPU/src/exec/load_store_queue.h`
- 创建：`myCPU/src/exec/load_store_queue.cpp`
- 创建：`myCPU/tests/host/rename_map_smoke.cpp`
- 创建：`myCPU/tests/host/reorder_buffer_smoke.cpp`
- 创建：`myCPU/tests/host/load_store_queue_smoke.cpp`
- 修改：`myCPU/Makefile`
- 修改：`myCPU/AGENTS.md`

- [x] **步骤 1：先写失败的 helper smoke**

  至少补以下最小行为：

  - `rename_map`
    - allocate physical register
    - commit architectural map
    - checkpoint / rollback
  - `reorder_buffer`
    - allocate entry
    - mark-ready
    - in-order commit
    - flush younger than sequence
  - `load_store_queue`
    - enqueue load / store
    - record address-ready / data-ready
    - retire store
    - flush younger entry
    - MMIO / non-speculative flag

- [x] **步骤 2：运行测试验证失败**

  运行：

  - `cd myCPU && make test-host-rename_map_smoke`
  - `cd myCPU && make test-host-reorder_buffer_smoke`
  - `cd myCPU && make test-host-load_store_queue_smoke`

  预期：

  - FAIL
  - 缺少 helper 文件或接口

- [x] **步骤 3：实现未接线 helper**

  首轮接口建议至少落成：

  ```cpp
  class RenameMap {
  public:
      RenameCheckpoint checkpoint() const;
      uint16_t map_source(uint8_t arch) const;
      uint16_t rename_dest(uint8_t arch);
      void commit_dest(uint8_t arch, uint16_t phys);
      void rollback(const RenameCheckpoint& checkpoint);
  };
  ```

  ```cpp
  class ReorderBuffer {
  public:
      RobIndex allocate(const RobAllocate& entry);
      void mark_ready(RobIndex index, const RobReady& ready);
      std::optional<RobEntry> peek_head() const;
      void commit_head();
      void flush_younger_than(uint64_t sequence_id);
  };
  ```

  ```cpp
  class LoadStoreQueue {
  public:
      LsqIndex enqueue_load(const LsqLoadRequest& req);
      LsqIndex enqueue_store(const LsqStoreRequest& req);
      void mark_address_ready(LsqIndex index, uint64_t addr);
      void mark_data_ready(LsqIndex index, uint64_t value);
      void flush_younger_than(uint64_t sequence_id);
  };
  ```

  这一轮明确不把它们接到 `PipelineBackend` 主路径，只要求：

  - 接口稳定
  - 行为可测试
  - 后续 OoO 接线时不再需要从零发明数据结构

- [x] **步骤 4：运行 helper 验证并纳入门禁**

  运行：

  - `cd myCPU && make test-host-rename_map_smoke`
  - `cd myCPU && make test-host-reorder_buffer_smoke`
  - `cd myCPU && make test-host-load_store_queue_smoke`
  - `cd myCPU && make test-pipeline`

  预期：

  - PASS
  - helper 独立稳定，且不会破坏当前主线

- [x] **步骤 5：Commit**

  ```bash
  git add myCPU/src/exec/rename_map.h myCPU/src/exec/rename_map.cpp myCPU/src/exec/reorder_buffer.h myCPU/src/exec/reorder_buffer.cpp myCPU/src/exec/load_store_queue.h myCPU/src/exec/load_store_queue.cpp myCPU/tests/host/rename_map_smoke.cpp myCPU/tests/host/reorder_buffer_smoke.cpp myCPU/tests/host/load_store_queue_smoke.cpp myCPU/Makefile myCPU/AGENTS.md
  git commit -m "feat(phase3): 引入 rename ROB LSQ 预备 helper"
  ```

### 任务 6：完成 readiness 回写并切换到可直接开工的大块 OoO 状态

**文件：**
- 修改：`docs/status/mainline_status.md`
- 修改：`docs/index.md`
- 修改：`myCPU/AGENTS.md`
- 修改：`docs/plan/phase3_ooo_readiness_plan.md`

- [x] **步骤 1：确认 readiness 条件逐项满足**

  收尾前逐项核对：

  - 设计文档已齐
  - retire trace / sequence 已有
  - commit boundary 已显式化
  - `PipelineBackend` 已完成结构拆分
  - `rename_map / ROB / LSQ` helper 已独立存在并有 smoke

- [x] **步骤 2：运行全量验证基线**

  运行：

  - `cd myCPU && make test`
  - `cd myCPU && make test-pipeline`
  - `cd frontend && node --test`

  预期：

  - 全部 PASS
  - 不因为 OoO-ready 前置重构破坏现有 Phase 1 / Phase 2 主门禁

- [x] **步骤 3：回写主线状态与计划完成态**

  完成本计划后，必须同步：

  - 在 `mainline_status.md` 中写明“已具备直接启动大块 `OoO / rename / ROB / LSQ` 的前置条件”
  - 在 `myCPU/AGENTS.md` 中补 `Phase 3-B/C` 的最新基线说明
  - 把本文档头部改为“已完成”
  - 勾完全部 checklist

- [x] **步骤 4：Commit**

  ```bash
  git add docs/status/mainline_status.md docs/index.md myCPU/AGENTS.md docs/plan/phase3_ooo_readiness_plan.md
  git commit -m "docs(phase3): 回写 OoO-ready 前置准备完成态"
  ```

## 完成态回写要求

- 全部 checklist 必须勾完。
- 文件头必须改成“已完成”或等价完成态说明。
- 对应 `status` 文档必须增加：
  - 完成结果摘要
  - 关键历史节点
  - 仍然有效的剩余风险（如果有）
- 完成本计划之后，应单独新建下一份实现计划，用于真正的大块 `OoO / rename / ROB / LSQ` 接线，而不是继续在本文档上叠加后续任务。
