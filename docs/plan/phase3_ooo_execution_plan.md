# Phase 3-B/C OoO 接线实现计划

> **文档状态：** 执行中

> **面向 AI 代理的工作者：** 必需子技能：使用 `superpowers:executing-plans` 在当前会话中按任务推进，或在明确授权后使用 `superpowers:subagent-driven-development` 分任务执行。步骤使用复选框（`- [ ]`）语法跟踪进度。

**目标：** 在保持单发射、共享 ISA 真值来源和顺序退休的前提下，把当前 `pipeline` 从 in-order 5-stage 后端推进到首轮 `rename + ROB + LSQ` 已接线的 `Phase 3-B/C` 完成态。

**架构：** 本计划分两段推进。先完成 `Phase 3-B` 的 `rename + ROB` 最小接线，把 decode/rename、物理寄存器结果暂存、ROB head commit 和 branch/trap rollback 接到主路径；再完成 `Phase 3-C` 的 `LSQ` 最小接线，让 load/store 进入统一队列、store 仅在 commit 时落内存、MMIO 继续保持 non-speculative 合同。整个过程不改变 `functional + shared InstructionSemantics` 作为唯一 architected 真值来源的定位。

**技术栈：** C++17、GNU Make、host-side smoke / differential、asm / guest regression、Node `--test`、RISC-V 交叉工具链。

## 文档定位

本文档用于把当前已经批准的 `Phase 3-B/C` 设计，收口成一份可直接执行的大块 `OoO / rename / ROB / LSQ` 实现计划。

它重点回答：

- 首轮 `Phase 3-B` 和 `Phase 3-C` 应按什么切片落地。
- 每一轮需要修改哪些核心文件、补哪些门禁。
- 在不破坏现有 precise exception / interrupt / MMIO / CSR 合同的前提下，`pipeline` 主路径应如何演进。

本文档只回答“怎么落地”。当前实时结果和完成态回写以 [status/mainline_status.md](../status/mainline_status.md) 为准。

## 2026-04-02 执行补充

- `LSQ` 的 load-after-store 顺序合同已继续细化为「按 `sequence_id + address/data-ready + address overlap` 判定」的形态，不再继续保留最初那种对非重叠 younger load 也一并施加的保守阻塞。
- `guest_supervisor_demo` 的 `pipeline` 长 guest 路径在当前 `Phase 3-B/C` 形态下，本机串行实测约为 `6.18 s`；对照 `538ebf9` 基线约为 `6.33 s`。因此本轮同步把 `PIPELINE_SUPERVISOR_GUEST_TEST_TIMEOUT` 调整到 `8 s`，避免沿用更早阶段的过紧预算。

## 关联文档

- 来源设计：
  - [design/phase3_ooo_execution_model_design.md](../design/phase3_ooo_execution_model_design.md)
  - [design/pipeline_speculation_contracts.md](../design/pipeline_speculation_contracts.md)
  - [plan/phase3_minimal_ooo_execute_plan.md](./phase3_minimal_ooo_execute_plan.md)
- 目标状态：
  - [status/mainline_status.md](../status/mainline_status.md)

## 2026-04-03 执行补充

- [plan/phase3_minimal_ooo_execute_plan.md](./phase3_minimal_ooo_execute_plan.md) 已完成；当前 `pipeline` 已把“最小真实 OoO execute”接到主路径。
- `ROB head` 现在已成为真实退休源头，backend 不再依赖 `mem_wb` 单槽才能退休；younger ALU 可以在 older RAM / faulting memory access 未完成时先写 `phys_regs + ROB ready`，architected side effect 仍只会在顺序 commit 时生效。
- 当前 memory execute 已收口成一条最小独立路径：RAM / faulting access 会形成最小 OoO 完成窗口；已知 MMIO load 继续维持 non-speculative 执行，以守住 `clint_split_access`、UART / PLIC / CLINT 等现有设备合同。
- 这意味着 `Phase 3-B/C` 的基础任务已经不再卡在“是不是仍然近似顺序 execute”；剩余工作重点已转向 bug-driven hardening、是否继续扩 issue / replay / memory speculation，以及是否进入更激进的下一轮微架构能力。

## 目标

- 完成 `Phase 3-B` 的 `rename + ROB` 最小接线，让 GPR 结果不再直接依赖 architected register file 写回。
- 完成 `Phase 3-C` 的 `LSQ` 最小接线，让 load / store 进入统一内存队列，store 仅在 commit 时真正生效。
- 保持当前 `pipeline` 仍为单发射、顺序退休后端，不把 superscalar、cache、复杂 replay 和激进 memory speculation 混入本轮。
- 继续守住当前 `pipeline_speculation_contracts` 文档定义的 precise exception / interrupt / MMIO / CSR / trap-return / TLB flush 合同。
- 保持现有 asm / host / guest / debug 门禁继续可运行，并补上首轮 `rename / ROB / LSQ` 相关的持久回归。

## 完成定义

- `PipelineBackend` 已在 decode 侧完成 `rename + ROB allocate`，stage / queue / commit 路径都能携带 source / destination physical tag 和 `ROB` 元数据。
- 新增一个可独立测试的物理寄存器文件 helper，并由 `rename_map`、`ROB`、`pipeline` 主路径共同使用。
- ROB head 成为 GPR architected mapping 切换和 commit-visible result 生效的唯一入口；mispredict、trap 和 interrupt flush 会一并回滚 younger rename / ROB / phys-state。
- `LSQ` 已接入主路径，load / store 进入队列管理；RAM store 仅在 commit 时写入，MMIO 继续按 non-speculative 规则处理。
- `debug_snapshot / debug_protocol` 已能暴露首轮 `ROB / LSQ` 最小观测面，相关 smoke 已接入。
- 以下验证通过：
  - `cd myCPU && make test-host-physical_register_file_smoke`
  - `cd myCPU && make test-host-rename_map_smoke`
  - `cd myCPU && make test-host-reorder_buffer_smoke`
  - `cd myCPU && make test-host-load_store_queue_smoke`
  - `cd myCPU && make test-host-pipeline_rename_commit_smoke`
  - `cd myCPU && make test-host-pipeline_speculation_contracts_smoke`
  - `cd myCPU && make test-host-backend_differential_smoke`
  - `cd myCPU && make test-host-debug_cli_smoke`
  - `cd myCPU && make test-pipeline`
  - `cd myCPU && make test`
  - `cd frontend && node --test`

## 文件结构

### 计划内新增文件

- `myCPU/src/exec/physical_register_file.h`
  物理寄存器文件接口，承接 speculative value、ready bit、checkpoint / rollback 所需的最小操作。
- `myCPU/src/exec/physical_register_file.cpp`
  物理寄存器文件实现。
- `myCPU/tests/host/physical_register_file_smoke.cpp`
  物理寄存器文件独立 smoke。
- `myCPU/tests/host/pipeline_rename_commit_smoke.cpp`
  `Phase 3-B` 的最小 host smoke，直接守住 `rename + ROB commit` 的主合同。

### 计划内重点修改文件

- `myCPU/src/exec/rename_map.h`
- `myCPU/src/exec/rename_map.cpp`
- `myCPU/src/exec/reorder_buffer.h`
- `myCPU/src/exec/reorder_buffer.cpp`
- `myCPU/src/exec/load_store_queue.h`
- `myCPU/src/exec/load_store_queue.cpp`
- `myCPU/src/exec/pipeline_types.h`
- `myCPU/src/exec/pipeline_core_state.h`
- `myCPU/src/exec/pipeline_core_state.cpp`
- `myCPU/src/exec/pipeline_backend.h`
- `myCPU/src/exec/pipeline_backend.cpp`
- `myCPU/src/exec/pipeline_hazards.h`
- `myCPU/src/exec/pipeline_hazards.cpp`
- `myCPU/src/debug/debug_snapshot.h`
- `myCPU/src/debug/debug_protocol.cpp`
- `myCPU/tests/host/rename_map_smoke.cpp`
- `myCPU/tests/host/reorder_buffer_smoke.cpp`
- `myCPU/tests/host/load_store_queue_smoke.cpp`
- `myCPU/tests/host/pipeline_backend_smoke.cpp`
- `myCPU/tests/host/pipeline_speculation_contracts_smoke.cpp`
- `myCPU/tests/host/backend_differential_smoke.cpp`
- `myCPU/tests/host/debug_cli_smoke.cpp`
- `myCPU/Makefile`
- `myCPU/AGENTS.md`
- `docs/status/mainline_status.md`

### 本轮明确不改

- 不改 `functional` backend 的 ISA 真值职责。
- 不把本轮扩成 superscalar、cache hierarchy、DMA 或 multicore 计划。
- 不主动扩 `debug/frontend` 的功能面；只被动兼容新的 `ROB / LSQ` 最小快照字段。
- 不把 guest runtime、`kernel_alpha`、`interactive_os` 的功能改动混入本轮。

## 任务

### 任务 1：补齐物理寄存器文件与 `rename / ROB` 接口

**文件：**
- 创建：`myCPU/src/exec/physical_register_file.h`
- 创建：`myCPU/src/exec/physical_register_file.cpp`
- 创建：`myCPU/tests/host/physical_register_file_smoke.cpp`
- 修改：`myCPU/src/exec/rename_map.h`
- 修改：`myCPU/src/exec/rename_map.cpp`
- 修改：`myCPU/src/exec/reorder_buffer.h`
- 修改：`myCPU/src/exec/reorder_buffer.cpp`
- 修改：`myCPU/tests/host/rename_map_smoke.cpp`
- 修改：`myCPU/tests/host/reorder_buffer_smoke.cpp`
- 修改：`myCPU/Makefile`

- [x] **步骤 1：先写失败的 helper smoke。**
  需要覆盖以下边界：
  - `physical_register_file` 能写入 speculative value、查询 ready 状态并按 checkpoint 回滚。
  - `rename_map` 在 `rename_dest()` 后能保留旧的 architected / speculative mapping，供 ROB commit 或 rollback 使用。
  - `reorder_buffer` entry 需要携带 old phys、checkpoint 或等价 rollback 元数据，而不是只记录 destination phys。

- [x] **步骤 2：运行失败测试，确认红灯成立。**
  运行：
  - `cd myCPU && make test-host-physical_register_file_smoke`
  - `cd myCPU && make test-host-rename_map_smoke`
  - `cd myCPU && make test-host-reorder_buffer_smoke`
  预期：因为 helper 尚未接齐接口或行为不满足新断言而失败。

- [x] **步骤 3：写最小实现代码。**
  实现要求：
  - `physical_register_file` 只承接首轮 `Phase 3-B` 必需能力：`read / write / ready / checkpoint / rollback / reset`。
  - `rename_map` 提供 old phys 返还或等价查询接口，避免 pipeline 主路径自己猜测旧映射。
  - `reorder_buffer` 扩成真正能承载 commit/rollback 所需信息的 entry，但不要提前塞入未用到的复杂调度字段。

- [x] **步骤 4：重跑 helper smoke，确认绿灯。**
  运行：
  - `cd myCPU && make test-host-physical_register_file_smoke`
  - `cd myCPU && make test-host-rename_map_smoke`
  - `cd myCPU && make test-host-reorder_buffer_smoke`

- [x] **步骤 5：提交一个聚焦 commit。**

### 任务 2：接通 `Phase 3-B` 的 decode/rename、phys operand 与 ROB head commit

**文件：**
- 创建：`myCPU/tests/host/pipeline_rename_commit_smoke.cpp`
- 修改：`myCPU/src/exec/pipeline_types.h`
- 修改：`myCPU/src/exec/pipeline_core_state.h`
- 修改：`myCPU/src/exec/pipeline_core_state.cpp`
- 修改：`myCPU/src/exec/pipeline_backend.h`
- 修改：`myCPU/src/exec/pipeline_backend.cpp`
- 修改：`myCPU/src/exec/pipeline_hazards.h`
- 修改：`myCPU/src/exec/pipeline_hazards.cpp`
- 修改：`myCPU/tests/host/pipeline_backend_smoke.cpp`
- 修改：`myCPU/tests/host/backend_differential_smoke.cpp`
- 修改：`myCPU/Makefile`

- [x] **步骤 1：先写失败的 `Phase 3-B` smoke。**
  至少覆盖：
  - younger 指令可通过 renamed source 读到 older 尚未 architecturally commit 的结果。
  - destination 结果在 ROB head commit 前，对 architected GPR 观察仍不可见。
  - ROB head 退休后，architected mapping 和 `instret` 顺序保持一致。

- [x] **步骤 2：运行失败测试验证红灯。**
  运行：
  - `cd myCPU && make test-host-pipeline_rename_commit_smoke`
  - `cd myCPU && make test-host-pipeline_backend_smoke`
  预期：当前 pipeline 仍直接读写 architected GPR，无法满足新合同。

- [x] **步骤 3：写最小 `Phase 3-B` 接线代码。**
  实现要求：
  - decode 侧完成 `rename + ROB allocate`。
  - stage slot 携带 source phys、destination phys、ROB index 和必要 checkpoint 引用。
  - execute / memory 结果先进入 phys file 和 ROB ready 状态。
  - head commit 成为 architected mapping 切换的唯一入口。
  - 仍保持单发射、近似顺序 execute，不在本任务引入真正的 memory OoO。

- [x] **步骤 4：重跑 smoke 与差分门禁。**
  运行：
  - `cd myCPU && make test-host-pipeline_rename_commit_smoke`
  - `cd myCPU && make test-host-pipeline_backend_smoke`
  - `cd myCPU && make test-host-backend_differential_smoke`

- [ ] **步骤 5：提交一个聚焦 commit。**

### 任务 3：接通分支 checkpoint、mispredict / trap rollback 与 precise younger squash

**文件：**
- 修改：`myCPU/src/exec/rename_map.h`
- 修改：`myCPU/src/exec/rename_map.cpp`
- 修改：`myCPU/src/exec/reorder_buffer.h`
- 修改：`myCPU/src/exec/reorder_buffer.cpp`
- 修改：`myCPU/src/exec/pipeline_core_state.h`
- 修改：`myCPU/src/exec/pipeline_core_state.cpp`
- 修改：`myCPU/src/exec/pipeline_backend.cpp`
- 修改：`myCPU/tests/host/pipeline_speculation_contracts_smoke.cpp`
- 修改：`myCPU/tests/host/backend_differential_smoke.cpp`
- 修改：`myCPU/tests/host/debug_cli_smoke.cpp`

- [x] **步骤 1：先写失败的 rollback 合同测试。**
  至少覆盖：
  - branch mispredict 后，younger renamed destination 与 ROB entry 会被回滚。
  - trap / fetch fault / trap-return flush 后，younger phys-state 不会泄漏到 architected 观察。
  - retire trace 中不出现已经被 squash 的 younger 指令。

- [x] **步骤 2：运行失败测试验证红灯。**
  运行：
  - `cd myCPU && make test-host-pipeline_speculation_contracts_smoke`
  - `cd myCPU && make test-host-backend_differential_smoke`
  - `cd myCPU && make test-host-debug_cli_smoke`

- [x] **步骤 3：写最小 rollback 实现。**
  实现要求：
  - branch checkpoint 只做首轮最小能力，不扩展为多级复杂恢复。
  - mispredict、trap、interrupt precision 都沿统一 flush 路径回滚 rename / ROB / phys-state / LSQ younger entry。
  - 被 squash 的 younger CSR / halt / trap-return / store / MMIO side effect 继续保持不可见。

- [x] **步骤 4：重跑 speculation / debug 门禁。**
  运行：
  - `cd myCPU && make test-host-pipeline_speculation_contracts_smoke`
  - `cd myCPU && make test-host-backend_differential_smoke`
  - `cd myCPU && make test-host-debug_cli_smoke`

- [ ] **步骤 5：提交一个聚焦 commit。**

### 任务 4：接通 `Phase 3-C` 的 `LSQ` 最小主路径

**文件：**
- 修改：`myCPU/src/exec/load_store_queue.h`
- 修改：`myCPU/src/exec/load_store_queue.cpp`
- 修改：`myCPU/src/exec/pipeline_types.h`
- 修改：`myCPU/src/exec/pipeline_backend.cpp`
- 修改：`myCPU/tests/host/load_store_queue_smoke.cpp`
- 修改：`myCPU/tests/host/pipeline_speculation_contracts_smoke.cpp`
- 修改：`myCPU/tests/host/backend_differential_smoke.cpp`

- [x] **步骤 1：先写失败的 `LSQ` 合同测试。**
  至少覆盖：
  - load / store entry 能携带 sequence、address-ready、data-ready 与 MMIO / non-speculative 标记。
  - RAM store 在 commit 前不可见，commit 后立即可见。
  - 被 squash 的 younger store 不落 RAM / MMIO。
  - MMIO 请求继续按 non-speculative 路径处理。

- [x] **步骤 2：运行失败测试验证红灯。**
  运行：
  - `cd myCPU && make test-host-load_store_queue_smoke`
  - `cd myCPU && make test-host-pipeline_speculation_contracts_smoke`
  - `cd myCPU && make test-host-backend_differential_smoke`

- [x] **步骤 3：写最小 `LSQ` 接线代码。**
  实现要求：
  - load / store 在主路径进入 `LSQ`。
  - load 结果继续走 ROB / phys-state，而不是直接写 architected GPR。
  - store 仅在 commit boundary 时调用 memory / MMIO apply。
  - 首轮不引入激进 memory disambiguation 或复杂 replay。

- [x] **步骤 4：重跑 `LSQ` 与 speculation 门禁。**
  运行：
  - `cd myCPU && make test-host-load_store_queue_smoke`
  - `cd myCPU && make test-host-pipeline_speculation_contracts_smoke`
  - `cd myCPU && make test-host-backend_differential_smoke`

- [ ] **步骤 5：提交一个聚焦 commit。**

### 任务 5：补齐 `ROB / LSQ` 观测面、正式文档与总门禁

**文件：**
- 修改：`myCPU/src/debug/debug_snapshot.h`
- 修改：`myCPU/src/debug/debug_protocol.cpp`
- 修改：`myCPU/tests/host/debug_cli_smoke.cpp`
- 修改：`myCPU/AGENTS.md`
- 修改：`docs/status/mainline_status.md`

- [x] **步骤 1：先写失败的 debug / protocol 断言。**
  至少覆盖：
  - `debug_snapshot` 能暴露 `ROB` / `LSQ` 的最小队列深度、head sequence 或等价调试字段。
  - `debug_cli` JSON 输出与 snapshot 透传字段保持一致。

- [x] **步骤 2：运行失败测试验证红灯。**
  运行：
  - `cd myCPU && make test-host-debug_cli_smoke`

- [x] **步骤 3：写最小观测面与文档回写。**
  实现要求：
  - 只暴露首轮调试真正需要的字段，不把 snapshot 扩成调度器内部实现转储。
  - 回写 `myCPU/AGENTS.md` 与 `docs/status/mainline_status.md` 的当前实现边界、验证基线和剩余风险。

- [x] **步骤 4：运行总门禁。**
  运行：
  - `cd myCPU && make test-host-physical_register_file_smoke`
  - `cd myCPU && make test-host-rename_map_smoke`
  - `cd myCPU && make test-host-reorder_buffer_smoke`
  - `cd myCPU && make test-host-load_store_queue_smoke`
  - `cd myCPU && make test-host-pipeline_rename_commit_smoke`
  - `cd myCPU && make test-host-pipeline_speculation_contracts_smoke`
  - `cd myCPU && make test-host-backend_differential_smoke`
  - `cd myCPU && make test-host-debug_cli_smoke`
  - `cd myCPU && make test-pipeline`
  - `cd myCPU && make test`
  - `cd frontend && node --test`

- [x] **步骤 5：提交收尾 commit，并把本计划勾到最新进度。**

## 完成态回写要求

- 全部 checklist 必须勾完。
- 文件头必须改成“已完成”或等价完成态说明。
- [status/mainline_status.md](../status/mainline_status.md) 必须回写：
  - 本轮 `Phase 3-B/C` 的已落地结果；
  - 当前 `pipeline` 执行模型边界；
  - 仍然有效的剩余风险；
  - 下一步是否进入更激进的 OoO execute / replay 工作。
