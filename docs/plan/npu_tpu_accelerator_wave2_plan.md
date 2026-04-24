# `NPU / TPU-like` AI accelerator Wave 2 实现计划

> **文档状态：** 执行中
>
> **面向 AI 代理的工作者：** 必需子技能：使用 `superpowers:subagent-driven-development`（推荐）或 `superpowers:executing-plans` 逐任务实现此计划。步骤使用复选框（`- [ ]`）语法跟踪进度。

## 文档定位

本文档用于记录独立 `MMIO NPU / TPU-like` AI accelerator Wave 2 如何落地。

Wave 2 不重新打开 Wave 1 已经完成的设备基础设施，也不直接跳到完整动态图、完整训练栈、完整 `attention` 或 MobileNet。它的目标是先把 Wave 1 的功能闭环推进到更可观察、更可评估、更适合承接后续动态 shape / 训练路线的状态。

## 关联文档

- 来源设计：
  - [../design/npu_tpu_accelerator_direction_design.md](../design/npu_tpu_accelerator_direction_design.md)
  - [../design/future_expansion_roadmap_design.md](../design/future_expansion_roadmap_design.md)
  - [../design/phase4_preparation_design.md](../design/phase4_preparation_design.md)
- 目标状态：
  - [../status/npu_tpu_accelerator_status.md](../status/npu_tpu_accelerator_status.md)
- 已完成计划：
  - [history_plan.md#npu-tpu-accelerator-wave1-plan](history_plan.md#npu-tpu-accelerator-wave1-plan)

## 目标

- 补齐 Wave 1 缺少的 profile attribution：queue/control/completion、busy/utilization、effective ops per cycle，以及 per-op / per-tile 观测。
- 增加一条比当前 `cnn / gemm` 更接近小模型的 host workload：固定 `conv -> relu -> pool -> fc` 或 tiny MLP 闭环。
- 落下 bounded dynamic shape 的第一刀：设备在受限 max-shape 合同内解析 runtime shape，并至少跑通一条动态 `GEMM / FC` profile。
- 把训练前向 + 反向作为远期目标写入 ABI / graph package / profile 的演进边界，但 Wave 2 不实现反向传播或 optimizer。
- 继续保持 AI accelerator 作为独立 `MMIO` 设备，不把实现反向混入 CPU `InstructionSemantics` 或当前 `xv6 / Linux` 主线。

## 非目标

- 不实现完整动态 shape runtime，不支持动态 op list、任意 rank 变化或设备内 graph rewrite。
- 不实现训练反向传播、optimizer、梯度同步、checkpoint 或 mixed precision loss scaling。
- 不实现完整 `Softmax / attention / MobileNet`，也不把 `INT4 / GELU / Sigmoid` 放进 Wave 2 完成定义。
- 不实现 CPU `F/D` 浮点扩展；这应作为单独 ISA correctness / Linux-facing 计划处理。
- 不扩大 frontend UI；Wave 2 只补 debug snapshot / JSON / CLI 可观察性，为后续 UI 做数据准备。

## 完成定义

- `AiAccelerator` 可稳定暴露并回归：
  - `busy_cycles`
  - `queue_cycles`
  - `completion_cycles`
  - `effective_ops_per_cycle`
  - `utilization`
  - per-op `retired_ops / compute_cycles / stall_cycles`
- host profile 输出继续只使用 `simulated cycles`，并能展示新增 attribution。
- `workloads/ai_proto` 新增一条固定 tiny model profile，并有对应 expected output。
- graph package / descriptor 具备 bounded dynamic shape 的最小编码与 reject matrix。
- 设备至少跑通一条 bounded dynamic `GEMM / FC` 正向 profile；超出 max shape、runtime shape 缺失或 memory plan 不匹配必须 fail-closed。
- 训练相关字段只作为 future-safe contract 或显式 reject 出现，不会被误执行。
- 状态、索引、计划归档规则保持同步。

## 文件分解

- `myCPU/src/devices/ai_accelerator.{h,cpp}`
  扩展 timing / queue / completion / utilization 计数器，保持 MMIO 控制面兼容。
- `myCPU/src/devices/ai_graph_scheduler.{h,cpp}`
  产出 per-op / per-tile profile summary，并为 bounded dynamic shape 使用 runtime dims。
- `myCPU/src/devices/ai_graph_package.{h,cpp}`
  增加 shape mode、max-shape / runtime-shape 合同、training mode 保留字段与 reject matrix。
- `myCPU/src/devices/ai_compute_gemm.{h,cpp}`、`myCPU/src/devices/ai_compute_conv.{h,cpp}`、`myCPU/src/devices/ai_compute_elementwise.{h,cpp}`
  只按需要接入 per-op profile 和 bounded shape 参数，不扩大新算子面。
- `myCPU/src/debug/debug_snapshot.h`、`myCPU/src/debug/debug_protocol_response.cpp`
  暴露新增 AI accelerator profile attribution。
- `myCPU/src/platform/machine.{h,cpp}`、`myCPU/src/main.cpp`
  扩展 `--ai-profile-manifest` summary 输出。
- `myCPU/workloads/ai_proto/pack_graph.py`、`myCPU/workloads/ai_proto/profile.mk`、`myCPU/workloads/ai_proto/README.md`
  新增 tiny model 与 bounded dynamic GEMM profile。
- `myCPU/tests/unit/ai_graph_package.cpp`
  补 bounded dynamic shape 与 training future 字段的 parser / reject matrix。
- `myCPU/tests/host/ai_accelerator_profile_smoke.cpp`
  锁住新增 profile summary、tiny model 和 malformed manifest 行为。
- `myCPU/tests/host/ai_accelerator_cnn_smoke.cpp`、`myCPU/tests/host/ai_accelerator_gemm_smoke.cpp`
  按触达范围补 per-op/tile counter 回归。
- `myCPU/tests/host/debug_cli_smoke.cpp`
  锁住 debug CLI 下新增 AI profile 字段可见性。
- `docs/status/npu_tpu_accelerator_status.md`
  回写 Wave 2 进度、风险和完成态。
- `docs/index.md`
  保持当前活跃计划入口同步。

## 任务

### 任务 1：profile attribution 与 MMIO / debug 观测

**文件：**
- 修改：
  - `myCPU/src/devices/ai_accelerator.h`
  - `myCPU/src/devices/ai_accelerator.cpp`
  - `myCPU/src/debug/debug_snapshot.h`
  - `myCPU/src/debug/debug_protocol_response.cpp`
  - `myCPU/src/platform/machine.cpp`
  - `myCPU/tests/unit/ai_accelerator_mmio_contract.cpp`
  - `myCPU/tests/host/ai_accelerator_profile_smoke.cpp`
  - `myCPU/tests/host/debug_cli_smoke.cpp`

- [x] **步骤 1：** 先在 `ai_accelerator_mmio_contract` 和 `ai_accelerator_profile_smoke` 中补失败期望，要求新增 `busy_cycles / queue_cycles / completion_cycles / utilization / effective_ops_per_cycle` 可读且语义稳定。
- [x] **步骤 2：** 在 `AiAccelerator` 中增加 counter 存储与 MMIO 只读窗口；`reset` 必须清零，fault completion 也必须有稳定归因。
- [x] **步骤 3：** 在 host profile summary 中输出新增 attribution，继续保持 `baseline=none`，不引入 wall-clock。
- [x] **步骤 4：** 在 debug snapshot / JSON response 中暴露新增字段，并更新 `debug_cli_smoke`。
- [x] **步骤 5：** 运行：
  - `cd myCPU && make test-unit-ai_accelerator_mmio_contract`
  - `cd myCPU && make test-host-ai_accelerator_profile_smoke`
  - `cd myCPU && make test-host-debug_cli_smoke`

### 任务 2：per-op / per-tile profile summary

**文件：**
- 修改：
  - `myCPU/src/devices/ai_graph_scheduler.h`
  - `myCPU/src/devices/ai_graph_scheduler.cpp`
  - `myCPU/src/devices/ai_accelerator.h`
  - `myCPU/src/devices/ai_accelerator.cpp`
  - `myCPU/tests/host/ai_accelerator_cnn_smoke.cpp`
  - `myCPU/tests/host/ai_accelerator_gemm_smoke.cpp`

- [ ] **步骤 1：** 在 host smoke 中补 per-op profile 期望：至少能区分 `conv / relu / pool / gemm` 的 `retired_ops` 与 `compute_cycles`。
- [ ] **步骤 2：** 在 `AiGraphScheduler` 的 execution result 中增加 per-op summary；第一版只需要固定长度或 `std::vector` host-side summary，不急着暴露复杂 trace。
- [ ] **步骤 3：** 将 per-op summary 聚合到 `AiAccelerator` completion/profile 统计，不改变现有 completion entry ABI。
- [ ] **步骤 4：** 对 tile 先暴露 `tile_count / scratchpad_peak_bytes` 这类聚合字段，不实现 MAC 阵列热力图。
- [ ] **步骤 5：** 运行：
  - `cd myCPU && make test-host-ai_accelerator_cnn_smoke`
  - `cd myCPU && make test-host-ai_accelerator_gemm_smoke`

### 任务 3：tiny model host workload

**文件：**
- 修改：
  - `myCPU/workloads/ai_proto/pack_graph.py`
  - `myCPU/workloads/ai_proto/profile.mk`
  - `myCPU/workloads/ai_proto/README.md`
  - `myCPU/tests/host/ai_accelerator_profile_smoke.cpp`

- [ ] **步骤 1：** 在 `pack_graph.py` 中新增固定 tiny model workload，优先选择 `conv -> relu -> pool -> fc`；`fc` 可复用 GEMM，不新增独立 op。
- [ ] **步骤 2：** 为 tiny model 写固定输入、权重和 expected output，避免随机数据成为主门禁。
- [ ] **步骤 3：** 让 `make run-workload WORKLOAD_NAME=ai_proto AI_PROTO_WORKLOAD=tiny_model` 可以生成 manifest 并运行 profile。
- [ ] **步骤 4：** 在 `ai_accelerator_profile_smoke` 中锁住 tiny model summary、输出文件和新增 attribution。
- [ ] **步骤 5：** 运行：
  - `cd myCPU && make test-host-ai_accelerator_profile_smoke`
  - `cd myCPU && make run-workload WORKLOAD_NAME=ai_proto AI_PROTO_WORKLOAD=tiny_model`

### 任务 4：bounded dynamic shape 合同与 reject matrix

**文件：**
- 修改：
  - `myCPU/src/devices/ai_graph_package.h`
  - `myCPU/src/devices/ai_graph_package.cpp`
  - `myCPU/src/devices/ai_submission_queue.h`
  - `myCPU/src/devices/ai_submission_queue.cpp`
  - `myCPU/tests/unit/ai_graph_package.cpp`
  - `myCPU/tests/unit/ai_accelerator_mmio_contract.cpp`

- [ ] **步骤 1：** 在 `ai_graph_package` 单测中先补红灯：`dynamic_bounded` package 必须声明 max dims / max tensor bytes / scratchpad budget；缺字段、越界 rank、runtime dims 超界都必须 reject。
- [ ] **步骤 2：** 给 graph package 增加 `shape_mode` 与 max-shape section；保持默认 `static` package 编码兼容。
- [ ] **步骤 3：** 在 submission descriptor 或 profile manifest 层增加 runtime shape table 的最小入口；如果 descriptor ABI 不能无损扩展，则优先通过 graph package optional section 或 profile manifest 传递，不破坏 48-byte descriptor 静态断言。
- [ ] **步骤 4：** 增加 training mode / backward op 的显式 fail-closed 测试：Wave 2 可以识别保留字段，但不得误执行训练 op。
- [ ] **步骤 5：** 运行：
  - `cd myCPU && make test-unit-ai_graph_package`
  - `cd myCPU && make test-unit-ai_accelerator_mmio_contract`

### 任务 5：bounded dynamic `GEMM / FC` 正向闭环

**文件：**
- 修改：
  - `myCPU/src/devices/ai_graph_scheduler.cpp`
  - `myCPU/src/devices/ai_compute_gemm.cpp`
  - `myCPU/src/devices/ai_accelerator.cpp`
  - `myCPU/workloads/ai_proto/pack_graph.py`
  - `myCPU/tests/host/ai_accelerator_gemm_smoke.cpp`
  - `myCPU/tests/host/ai_accelerator_profile_smoke.cpp`

- [ ] **步骤 1：** 先在 host smoke 中补动态 `GEMM / FC` 样本：同一 graph package 在两个 runtime shape 下产生不同但可预期的输出。
- [ ] **步骤 2：** 在 scheduler 中根据 runtime dims 计算本次 tensor byte size、retired ops 和 compute cycles；超出 max dims 或 memory plan 不覆盖时返回稳定 fault。
- [ ] **步骤 3：** 在 GEMM compute path 中使用 runtime dims，不改变静态 GEMM 的既有行为。
- [ ] **步骤 4：** 在 `ai_proto` 增加 `dynamic_gemm` profile，summary 必须显示 runtime dims 与 dynamic shape mode。
- [ ] **步骤 5：** 运行：
  - `cd myCPU && make test-host-ai_accelerator_gemm_smoke`
  - `cd myCPU && make test-host-ai_accelerator_profile_smoke`

### 任务 6：状态回写与总验证

**文件：**
- 修改：
  - `docs/status/npu_tpu_accelerator_status.md`
  - `docs/index.md`
  - `docs/plan/npu_tpu_accelerator_wave2_plan.md`

- [ ] **步骤 1：** 回写 Wave 2 当前完成情况、仍不做完整动态 shape / training 的边界，以及新增验证基线。
- [ ] **步骤 2：** 检查 `docs/index.md` 的 AI accelerator 专题入口仍包含当前活跃计划和 Wave 1 归档。
- [ ] **步骤 3：** 运行最窄验证：
  - `cd myCPU && make test-unit-ai_graph_package`
  - `cd myCPU && make test-unit-ai_accelerator_mmio_contract`
  - `cd myCPU && make test-host-ai_accelerator_cnn_smoke`
  - `cd myCPU && make test-host-ai_accelerator_gemm_smoke`
  - `cd myCPU && make test-host-ai_accelerator_profile_smoke`
  - `cd myCPU && make test-host-debug_cli_smoke`
- [ ] **步骤 4：** 如果本轮触及 `src/devices/*`、`src/platform/machine.cpp`、`src/debug/*` 或 `guest/*`，最终补跑：
  - `cd myCPU && make test`
  - `cd myCPU && make test-pipeline`

## 完成态回写要求

- 全部 checklist 必须勾完。
- [../status/npu_tpu_accelerator_status.md](../status/npu_tpu_accelerator_status.md) 必须增加：
  - Wave 2 完成结果摘要
  - 新增 profile / dynamic shape 能力
  - 仍然有效的剩余风险，尤其是完整动态 shape、训练反向、`Softmax / attention` 与 frontend visualization
- 需要把“完成时间 + 完成内容 + 必要时的一两句过程摘要”追加到 [history_plan.md](history_plan.md)。
- 归档完成后，删除原计划文件，不再长期保留完成态 checklist。
