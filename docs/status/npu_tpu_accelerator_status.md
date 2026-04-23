# `NPU / TPU-like` AI accelerator 状态

## 文档定位

本文档用于跟踪独立 `MMIO NPU / TPU-like` AI accelerator 方向的当前状态：

- 当前已经明确到什么程度
- 当前仍然有效的风险 / 限制是什么
- 如果后续真的启动，这条线应先从哪一批基础工作开始

本文档不记录完整执行 checklist；执行拆分和逐项步骤统一写入 `plan` 文档。

## 关联文档

- 相关设计：
  - [../design/npu_tpu_accelerator_direction_design.md](../design/npu_tpu_accelerator_direction_design.md)
  - [../design/future_expansion_roadmap_design.md](../design/future_expansion_roadmap_design.md)
  - [../design/phase4_preparation_design.md](../design/phase4_preparation_design.md)
- 当前计划：
  - [../plan/npu_tpu_accelerator_wave1_plan.md](../plan/npu_tpu_accelerator_wave1_plan.md)
- 已完成计划：
  - 当前暂无；完成后统一归档到 [../plan/history_plan.md](../plan/history_plan.md)

## 目标 / 主题

这份状态文档跟踪的不是“CPU 侧再补一些向量指令”，而是一条独立未来方向：把当前仓库继续推进到独立 `MMIO` AI accelerator 设备。它的目标是为后续 `CNN` 与 `GEMM / Transformer-like` 推理提供更贴近真实 `NPU / TPU` 的设备级结构边界，同时不污染当前 CPU reference path 与已激活的 `xv6 / Linux` 主线。

## 当前状态

- `2026-04-23` 已把这条线收口成正式设计文档 [../design/npu_tpu_accelerator_direction_design.md](../design/npu_tpu_accelerator_direction_design.md)，并明确它采用独立 `MMIO` 设备路线，而不是 CPU 紧耦合 tensor 指令扩展。
- `2026-04-23` 同日已完成 wave 1 的任务 1：`DMA-ready` memory contract。
  - 已新增 `myCPU/src/mem/dma_transaction.{h,cpp}`，冻结 `initiator / direction / burst / fault / transferred_bytes` 最小合同。
  - `Bus` 已新增显式 `dma_read / dma_write` 结果 API，并保留 `dma_load_bytes / dma_store_bytes` 作为兼容层，现有 `virtio` 路径无需改写即可继续工作。
  - 当前 DMA 路径的保守语义已经固定为：先做 span 级预校验；`unmapped`、跨 region 边界、side-effect `MMIO`、非 `dma_visible` region 与不支持 burst 的 region 都会 fail-closed；设备中途报错时会显式带回 `transferred_bytes`。
- `2026-04-23` 同日已完成 wave 1 的任务 2：静态 graph package 与 tensor golden model。
  - 已新增 `myCPU/src/devices/ai_graph_package.{h,cpp}`，固定最小 header、tensor table、op descriptor、dependency 与 memory plan 编码。
  - graph package parser / validator 已覆盖非法 dtype、非法 rank / tile、越界 dependency、scratchpad budget 超限与未知 opcode 的 reject matrix。
  - 已新增 `myCPU/src/devices/tensor_golden_ops.{h,cpp}`，覆盖 `INT8 / INT16 -> INT32` 的 `gemm / conv`、`FP16 / BF16 -> FP32` 的 `gemm`，以及 `relu / max-pool / reduce-sum / transpose` 固定向量样本。
- `2026-04-23` 同日已完成 wave 1 的任务 3：AI accelerator 控制面与 MMIO 设备骨架。
  - 已新增 `myCPU/src/devices/ai_submission_queue.{h,cpp}`，固定 submission descriptor、completion entry、ring head/tail、token 与 DMA 读写队列的最小 ABI。
  - 已新增 `myCPU/src/devices/ai_accelerator.{h,cpp}`，提供 capability / status / reset / queue base / doorbell / IRQ / fault / perf counter 的 MMIO 控制窗口。
  - `Machine` 已默认挂载独立 `AI accelerator` MMIO 设备，设备完成后会通过独立 PLIC source 触发 completion / fault interrupt；当前仍不耦合 scratchpad、DMA engine 或 compute engine。
  - debug snapshot / JSON protocol 已新增只读控制面观测：`queue_depth`、`doorbell_count`、`last_fault` 与 `completion_count`。
- 当前对 AI accelerator 的性能口径已经进一步收口为：后续统一采用 `timed-simple` 的 `simulated cycles` 模型评估结构收益，不用宿主机 wall-clock 表述“是否加速”。
- 当前任务 3 的 `perf counter window` 仍主要是控制面占位；设备提交后仍接近“功能上立即完成”，还不是具备真实延迟语义的时序模型。
- `v1` 的正式设计边界已经冻结为：
  - 推理优先，不把训练放进 `v1` 完成定义
  - 只支持静态 shape、离线准备好的静态子图
  - 设备内采用 `scratchpad + DMA/load-store engine`
  - host harness 与 guest driver 共用同一套 `descriptor / queue / doorbell / completion` ABI
  - 性能比较统一落在 `simulated cycles`，而不是 wall-clock 秒数
- `v1` 的 workload 目标已经固定为同时覆盖：
  - `CNN`
  - `GEMM / MLP / Transformer-like` matmul-family inference block
- `v1` 的 dtype family 也已在设计层锁定为两组统一合同：
  - `INT8 / INT16 -> INT32 accumulate`
  - `FP16 / BF16 -> FP32 accumulate`
- 当前这条线已经有三块已落地代码与测试门禁：`DMA-ready` memory contract、静态 graph package / tensor golden model，以及独立 AI accelerator 控制面 / MMIO 设备骨架。
- 当前任务 4 到任务 7 仍未开始；scratchpad / DMA engine、compute path、guest driver 与 host profile 入口都还没有落地。
- 当前已形成的 wave 1 落地顺序是：
  - 先补 `DMA-ready` initiator / transaction contract
  - 再定义 graph package 与 tensor golden model
  - 然后接设备控制面、数据面和代表性 compute path
  - 最后再接 host / guest 入口与 debug/profile 观测

## 关键历史节点

- `2026-04-11`
  - `V4` 向量路径完成一轮更窄的 direct dependency hardening，证明仓库已经具备承接更真实 ML workload 的 CPU-side 基线。
- `2026-04-12`
  - `P4-prep-1` 完成，`Bus / memory_region` 已成为统一事实来源，为后续 `DMA-ready` 方向留下准备性入口。
- `2026-04-23`
  - 独立 `MMIO NPU / TPU-like` AI accelerator 方向完成 design / status / wave 1 plan 首轮收口。
  - 同日补充时序口径：明确后续采用 `timed-simple` 模拟时钟 / 周期模型，把 `DMA`、compute、stall 与 completion 开销统一纳入 `simulated cycles`。
  - 同日完成 wave 1 任务 1，并通过：
    - `cd myCPU && make test-unit-dma_transaction_contract`
    - `cd myCPU && make test-unit-bus_region_contract`
    - `cd myCPU && make test`
  - 同日完成 wave 1 任务 2，并通过：
    - `cd myCPU && make test-unit-ai_graph_package`
    - `cd myCPU && make test-host-ai_tensor_golden_ops_smoke`
    - `cd myCPU && make test`
  - 同日完成 wave 1 任务 3，并通过：
    - `cd myCPU && make test-unit-ai_accelerator_mmio_contract`
    - `cd myCPU && make test-host-ai_accelerator_submit_smoke`
    - `cd myCPU && make test`
    - `cd myCPU && make test-pipeline`

## 当前仍然有效的风险 / 限制

- 当前主线已经切到 `xv6 / Linux` foundation；这条 AI accelerator 线虽然已经建模，但不是当前已激活主线。
- `DMA-ready` contract 已有第一版，但还只是 `Bus` 级最小合同；后续 `scratchpad/DMA engine`、graph package、guest ABI 与 profile 观测仍未接上。
- graph package、tensor golden model 与独立 AI accelerator 控制面已有第一版，但当前还没有 scratchpad / DMA engine、compute engine、host graph packaging tooling 或 guest driver ABI。
- 当前还没有独立设备时序模型；在任务 4 / 5 落地前，这条线只能证明功能上完成 offload，不能正式证明 `DMA overlap`、tile reuse 或 compute throughput 已形成可比较的性能收益。
- 同时覆盖 quantized 与 semi-precision family 会明显放大验证矩阵；后续实施时必须坚持“统一 ABI + 代表性闭环”，不能一开始就追求全矩阵算子铺满。
- 如果把这条线和当前 `xv6 / Linux` 主线混在同一轮里推进，很容易打散已有回归与 ownership 边界。

## 下一步

1. 继续执行 wave 1 的任务 4：接 scratchpad 与 DMA / load-store engine，并在这一层开始引入 `timed-simple` 的 `DMA load/store` 周期模型。
2. 任务 4 完成后，再接静态调度器与代表性 compute path，并把 `retired_ops / MACs / tile` 映射到 `compute_cycles`，明确 `DMA + compute` overlap 语义。
3. 继续按“控制面 -> 数据面 + 基础 timing -> 代表性 compute path + compute timing -> host/guest 接入 -> profile/debug”顺序推进 wave 1，不颠倒实施顺序。

## 验证基线

- 当前已落地并验证的门禁：
  - `cd myCPU && make test-unit-dma_transaction_contract`
  - `cd myCPU && make test-unit-bus_region_contract`
  - `cd myCPU && make test-unit-ai_graph_package`
  - `cd myCPU && make test-unit-ai_accelerator_mmio_contract`
  - `cd myCPU && make test-host-ai_tensor_golden_ops_smoke`
  - `cd myCPU && make test-host-ai_accelerator_submit_smoke`
  - `cd myCPU && make test`
  - `cd myCPU && make test-pipeline`
- 后续继续扩到设备控制面、debug 或 guest/runtime 路径时，再额外守住：
  - `cd myCPU && make test-pipeline`
- wave 1 预期新增的代表性验证至少包括：
  - `cd myCPU && make test-unit-dma_transaction_contract`
  - `cd myCPU && make test-unit-ai_graph_package`
  - `cd myCPU && make test-unit-ai_accelerator_mmio_contract`
  - `cd myCPU && make test-host-ai_accelerator_cnn_smoke`
  - `cd myCPU && make test-host-ai_accelerator_gemm_smoke`
  - `cd myCPU && make test-host-ai_accelerator_profile_smoke`
