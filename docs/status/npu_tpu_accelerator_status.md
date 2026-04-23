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
- `2026-04-23` 同日已完成 wave 1 的任务 4：scratchpad 与 DMA / load-store engine。
  - 已新增 `myCPU/src/devices/ai_scratchpad.{h,cpp}`，把 `scratchpad / accumulator / temporary` 三段本地缓冲收口到统一地址空间与生命周期。
  - 已新增 `myCPU/src/devices/ai_dma_engine.{h,cpp}`，把 `system RAM <-> scratchpad` 的 DMA / load-store path 收口成独立 engine，并继续复用任务 1 的 `dma_transaction` / `Bus::dma_read()/dma_write()` 合同。
  - `AiAccelerator` 当前已从 doorbell 立即完成切到异步 `tick()` 推进：submission 会先进入设备内数据面状态机，再按 `DMA load -> placeholder compute barrier -> DMA store` 完成 completion / IRQ。
  - 第一版 `timed-simple` timing 已接到 DMA path：当前已固定 `setup_cycles`、`dma_bytes_per_cycle`，并可通过 MMIO 读取 `device_cycles`、`dma_cycles`、`dma_load_cycles`、`dma_store_cycles`、`dma_load_bytes` 与 `dma_store_bytes`。
  - host / unit 回归已补上 `scratchpad_overflow`、DMA fault、partial transfer fail-closed，以及 `DMA` 相关计数器单调增长；当前 host smoke 也已锁住最小 `load -> store` 数据面闭环。
- `2026-04-23` 同日已完成 wave 1 的任务 5：静态子图调度器与代表性 compute path。
  - 已新增 `myCPU/src/devices/ai_graph_scheduler.{h,cpp}`，按静态 dependency 做最小 ready 判断与顺序执行；当前只支持静态 shape、静态 dependency 和离线 memory plan，不提前引入动态 shape。
  - 已新增 `myCPU/src/devices/ai_compute_gemm.{h,cpp}`、`myCPU/src/devices/ai_compute_conv.{h,cpp}` 与 `myCPU/src/devices/ai_compute_elementwise.{h,cpp}`，落下 `gemm / conv2d / relu / pool / reduce / layout transform` 这组 Wave 1 代表性 compute primitive，并继续复用 `tensor_golden_ops` 作为 reference model。
  - `AiAccelerator` 当前已从 `DMA load -> placeholder compute barrier -> DMA store` 收口成 `DMA load -> static graph compute -> DMA store`，completion 也已带回真实 `retired_ops`。
  - 第一版 `timed-simple` compute timing 已接线：当前 `device_cycles` 已不再等同于 `dma_cycles`，并新增 `compute_cycles / stall_cycles` MMIO 只读计数器；其保守语义已固定为 `DMA + compute` **不重叠**，因此当前代表性 workload 下 `stall_cycles` 仍为 `0`。
  - 已新增 `myCPU/tests/host/ai_accelerator_cnn_smoke.cpp` 与 `myCPU/tests/host/ai_accelerator_gemm_smoke.cpp`，分别锁住 quantized `CNN` 与 semi-precision `GEMM / matmul-family` 的输出、fault 行为，以及基础 timing counter 语义。
- 当前对 AI accelerator 的性能口径已经进一步收口为：后续统一采用 `timed-simple` 的 `simulated cycles` 模型评估结构收益，不用宿主机 wall-clock 表述“是否加速”。
- 当前控制面里的 `perf counter window` 已经覆盖 `device_cycles / dma_cycles / compute_cycles / stall_cycles / dma_load_bytes / dma_store_bytes`，completion 也已带回 `retired_ops`；但 `completion overhead`、更细的 compute / stall attribution 和 `DMA + compute overlap` 仍未展开。
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
- 当前这条线已经有五块已落地代码与测试门禁：`DMA-ready` memory contract、静态 graph package / tensor golden model、独立 AI accelerator 控制面 / MMIO 设备骨架、独立 `scratchpad + DMA/load-store engine + timed-simple DMA timing`，以及静态调度器 + 代表性 compute path + 第一版 `timed-simple` compute timing。
- 当前任务 6 到任务 7 仍未开始；host graph packaging tooling、guest driver ABI 与 host profile 入口都还没有落地。
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
  - 同日完成 wave 1 任务 4，并通过：
    - `cd myCPU && make test-unit-ai_dma_engine`
    - `cd myCPU && make test-unit-ai_scratchpad`
    - `cd myCPU && make test-host-ai_accelerator_dma_smoke`
    - `cd myCPU && make test-unit-ai_accelerator_mmio_contract`
    - `cd myCPU && make test-host-ai_accelerator_submit_smoke`
    - `cd myCPU && make test`
    - `cd myCPU && make test-pipeline`
  - 同日完成 wave 1 任务 5，并通过：
    - `cd myCPU && make test-host-ai_accelerator_cnn_smoke`
    - `cd myCPU && make test-host-ai_accelerator_gemm_smoke`
    - `cd myCPU && make test-unit-ai_accelerator_mmio_contract`
    - `cd myCPU && make test`

## 当前仍然有效的风险 / 限制

- 当前主线已经切到 `xv6 / Linux` foundation；这条 AI accelerator 线虽然已经建模，但不是当前已激活主线。
- `DMA-ready` contract、graph package、tensor golden model、独立 AI accelerator 控制面、`scratchpad/DMA engine` 与第一版 compute engine 当前都已有实现；但 host graph packaging tooling、guest driver ABI 与更完整的 profile / debug 观测仍未接上。
- 当前已经有独立设备时序模型的第二刀，但仍只停在 `timed-simple`：`DMA + compute` 当前固定为 no-overlap，`stall_cycles` 也还没有展开成更细 attribution，因此这条线仍不能拿来讨论更激进的 tile overlap、queue 开销隐藏或更细颗粒度吞吐模型。
- 同时覆盖 quantized 与 semi-precision family 会明显放大验证矩阵；后续实施时必须坚持“统一 ABI + 代表性闭环”，不能一开始就追求全矩阵算子铺满。
- 如果把这条线和当前 `xv6 / Linux` 主线混在同一轮里推进，很容易打散已有回归与 ownership 边界。

## 下一步

1. 继续执行 wave 1 的任务 6：补 host graph packaging 与 workload/profile 入口，把固定 `CNN` 与 `GEMM / matmul-family` workload 从 ad-hoc host test 收口成独立 packaging/summary 入口。
2. 继续按“控制面 -> 数据面 + 基础 timing -> 代表性 compute path + compute timing -> host/guest 接入 -> profile/debug”顺序推进 wave 1，不颠倒实施顺序。
3. 下一轮仍保持边界收窄：先围绕已落地的静态 scheduler + compute path 收口 host/profile 入口与最小 debug 观测，不把这条线反向混入 CPU ISA reference path，也不顺手扩到训练、动态图或更重的 overlap / performance 模型。

## 验证基线

- 当前已落地并验证的门禁：
  - `cd myCPU && make test-unit-dma_transaction_contract`
  - `cd myCPU && make test-unit-bus_region_contract`
  - `cd myCPU && make test-unit-ai_graph_package`
  - `cd myCPU && make test-unit-ai_accelerator_mmio_contract`
  - `cd myCPU && make test-unit-ai_dma_engine`
  - `cd myCPU && make test-unit-ai_scratchpad`
  - `cd myCPU && make test-host-ai_tensor_golden_ops_smoke`
  - `cd myCPU && make test-host-ai_accelerator_submit_smoke`
  - `cd myCPU && make test-host-ai_accelerator_dma_smoke`
  - `cd myCPU && make test-host-ai_accelerator_cnn_smoke`
  - `cd myCPU && make test-host-ai_accelerator_gemm_smoke`
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
