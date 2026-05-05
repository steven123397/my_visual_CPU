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
  - [../design/post_wave7_ai_user_tasks_npu_performance_design.md](../design/post_wave7_ai_user_tasks_npu_performance_design.md)
  - [../design/phase4_preparation_design.md](../design/phase4_preparation_design.md)
- 当前活跃计划：
  - [../plan/post_wave7_ai_user_tasks_npu_performance_plan.md](../plan/post_wave7_ai_user_tasks_npu_performance_plan.md)
- 已完成计划：
  - [../plan/history_plan.md#mainline-wave4-ai-accelerator-slices-plan](../plan/history_plan.md#mainline-wave4-ai-accelerator-slices-plan)
  - [../plan/history_plan.md#npu-tpu-accelerator-wave3-plan](../plan/history_plan.md#npu-tpu-accelerator-wave3-plan)
  - [../plan/history_plan.md#npu-tpu-accelerator-wave1-plan](../plan/history_plan.md#npu-tpu-accelerator-wave1-plan)
  - [../plan/history_plan.md#npu-tpu-accelerator-wave2-plan](../plan/history_plan.md#npu-tpu-accelerator-wave2-plan)

## 目标 / 主题

这份状态文档跟踪的不是“CPU 侧再补一些向量指令”，而是一条已经排入主线后续 wave 的独立设备路线：把当前仓库继续推进到独立 `MMIO` AI accelerator 设备。它的目标是为后续 `CNN` 与 `GEMM / Transformer-like` 推理提供更贴近真实 `NPU / TPU` 的设备级结构边界，同时不污染当前 CPU reference path 与当前近端的 `xv6 / Linux` 主线推进。

## 当前状态

- `2026-05-02` 已把 `Wave 7` 阶段性收口之后的 AI accelerator 后续方向正式收口为
  `用户自定义 AI 任务 + 更接近商用 NPU 的性能模型` 新主线，并补齐独立
  `design / plan` 入口；当前实时推进仍由本文档承接。
- `2026-05-02` 同日已把这条新主线的第一刀进一步收窄为：
  先落 host-side 受限 `task spec importer`，第一批 contract 固定为
  `bounded_dynamic_gemm_v1 -> dynamic_bounded GEMM graph package`，并在 importer 内附带
  最小 automatic memory plan helper；当前不把性能模型第一刀放到它之前。
- `2026-05-02` 同日又沿同一 importer 路线继续落下一刀：
  `bounded_dynamic_cnn_v1 -> dynamic_bounded CNN graph package` 现已接通，固定覆盖
  `conv2d -> eltwise_relu -> layout_transpose -> reduce_sum`、`3x3/4x4` 受限输入、
  `2x2` 固定 kernel、runtime shape table、expected output 与 host profile 路径；
  automatic memory plan helper 也已升级为 `16B` 对齐顺序 scratchpad 分配，以对齐现有
  `dynamic_cnn` contract。
- `2026-05-02` 同日还把这套 task-spec lower / serialize 逻辑抽成共享 host-side 模块
  `myCPU/workloads/ai_proto/task_spec_lowering.py`；`pack_graph.py` 现在只保留 CLI /
  固定 workload 入口，frontend 也继续复用同一条 host 打包路径。
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
- `2026-04-23` 同日已完成 wave 1 的任务 6：host graph packaging 与 workload/profile 入口。
  - 已新增 `myCPU/workloads/ai_proto/profile.mk`、`myCPU/workloads/ai_proto/pack_graph.py` 与 `myCPU/workloads/ai_proto/README.md`，把固定 `CNN` 与 `GEMM / matmul-family` workload 收口成可重复生成的 `graph.bin + tensor inputs + expected output + manifest` 打包入口。
  - `myCPU/workloads/common.mk` 与 `myCPU/Makefile` 当前已支持 `debug-cli` 与 `ai-profile` 两类 workload run mode；`ai_proto` 复用现有 `workloads/` 体系，不另起并行脚本目录，也不再依赖 guest image / debug probe 路径。
  - `myCPU/src/platform/machine.{h,cpp}` 当前已新增 `run_ai_profile_manifest()`，并通过 `myCPU/src/main.cpp` 暴露 `--ai-profile-manifest` CLI 入口；host summary 统一输出 `baseline=none` 和 `timed-simple` 原始计数器，包括 `device_cycles / dma_cycles / compute_cycles / stall_cycles / bytes_moved / retired_ops`，不回退到宿主机 wall-clock。
  - 已新增 `myCPU/tests/host/ai_accelerator_profile_smoke.cpp`，锁住 packaging 产物格式、`run-workload WORKLOAD_NAME=ai_proto` 干跑命令，以及 `cnn / gemm` 两条代表性 workload 的 profile summary 与输出结果。
- `2026-04-23` 同日已完成 wave 1 的任务 7：guest driver、guest demo 与 debug/profile 收尾。
  - 已新增 `myCPU/guest/include/ai_accel.h` 与 `myCPU/guest/kernel/ai_accel.c`，把 guest 与 host 共用的 `descriptor / queue / doorbell / completion / counter` ABI 收口为独立最小 driver。
  - 已新增 `myCPU/guest/ai_accel_demo/start.S` 与 `myCPU/guest/ai_accel_demo/main.c`，固定一条最小推理闭环：guest 会提交固定 graph package、等待 AI completion interrupt、读回结果与计数器，并以 `KMVAI` 作为成功输出。
  - `myCPU/src/debug/debug_snapshot.h` 与 `myCPU/src/debug/debug_protocol_response.cpp` 当前已补齐 AI accelerator 只读观测：`engine_busy`、`scratchpad_occupancy_bytes`、`dma_load_bytes`、`dma_store_bytes`、`device_cycles`、`dma_cycles`、`compute_cycles` 与 `stall_cycles`；`myCPU/tests/host/debug_cli_smoke.cpp` 也已锁住 `debug-cli` 下的最终可见性。
  - `myCPU/tests/host/ai_accel_guest_smoke.cpp`、`make test-guest-ai_accel_demo` 与 `make test-pipeline-guest-ai_accel_demo` 当前已一起守住 guest 侧 submit / completion / counter ABI 闭环。
- `2026-04-23` 同日已开始 wave 1 完成态后的第一刀 hardening：`ai-profile manifest` 当前已显式要求 `format=ai_proto_manifest_v1`，并拒绝重复的单值 key（`format / name / graph_package / max_ticks / source_tag`），避免 host profile 入口对 malformed-input fail-open。
- `2026-04-23` 同日继续完成 wave 1 hardening 第二刀：guest `ai_accel` driver 当前已新增最小 queue helper，显式拦住 `NULL / zero / >max` queue 参数，并把 submit / completion ring 的 head-tail 推进、completion tail 回退与 overflow 检查收口到统一 guest-side 合同；`ai_accel_demo` 也已改成复用这组 helper，不再假设 completion 永远固定落在槽位 `0`。
- 当前对 AI accelerator 的性能口径已经进一步收口为：后续统一采用 `timed-simple` 的 `simulated cycles` 模型评估结构收益，不用宿主机 wall-clock 表述“是否加速”。
- 当前控制面里的 `perf counter window` 已经覆盖 `device_cycles / dma_cycles / compute_cycles / stall_cycles / busy_cycles / queue_cycles / completion_cycles / effective_ops_per_cycle / utilization / dma_load_bytes / dma_store_bytes`，completion 也已带回 `retired_ops`；Wave 2 任务 2 的 host-side per-op / per-tile profile summary 也已经落地，但 `DMA + compute overlap` 与更细粒度的 tile 热力图仍未展开。
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
- 当前这条线的 wave 1 七个任务都已落地：`DMA-ready` memory contract、静态 graph package / tensor golden model、独立 AI accelerator 控制面 / MMIO 设备骨架、独立 `scratchpad + DMA/load-store engine + timed-simple DMA timing`、静态调度器 + 代表性 compute path + 第一版 `timed-simple` compute timing、host packaging/profile 入口，以及 guest driver / guest demo / debug-profile 收尾。
- 当前已形成的 wave 1 落地顺序是：
  - 先补 `DMA-ready` initiator / transaction contract
  - 再定义 graph package 与 tensor golden model
  - 然后接设备控制面、数据面和代表性 compute path
  - 最后再接 host / guest 入口与 debug/profile 观测
- `2026-04-24` 已把动态 shape 和训练支持纳入正式后续设计边界，并完成 Wave 2 全部任务，相关完成态已归档到 [../plan/history_plan.md#npu-tpu-accelerator-wave2-plan](../plan/history_plan.md#npu-tpu-accelerator-wave2-plan)。
  - 动态 shape 当前被定义为 `v2+` 正式目标，但 Wave 2 只做 bounded dynamic shape 的最小合同与代表性 `GEMM / FC` 闭环，不做任意动态图。
  - 训练前向 + 反向当前被定义为更远期正式目标，但 Wave 2 不实现反向传播、optimizer 或梯度同步，只在 ABI / graph package / profile 设计中保留演进位置。
  - Claude Code 建议中的 `Softmax / attention / INT4 / GELU / Sigmoid / MobileNet` 已被重新排序：`Softmax / tiny attention` 属于后续 matmul-family 扩展，`INT4` 等待 INT8 与 tile profile 稳定，MobileNet 只考虑未来前几层或 depthwise / pointwise 子集。
- `2026-04-24` 同日已完成 Wave 2 任务 1：profile attribution 与 MMIO / debug 观测第一刀。
  - `AiAccelerator` 当前新增 `busy_cycles / queue_cycles / completion_cycles / effective_ops_per_cycle / utilization` 只读 MMIO 窗口，并在 reset 与 fault completion 路径上保持稳定归因。
  - host `--ai-profile-manifest` summary 继续使用 `baseline=none` 与 simulated cycles，同时输出新增 attribution，不引入宿主机 wall-clock。
  - debug snapshot / JSON response 已同步暴露新增字段，`debug_cli_smoke` 已锁住 guest `ai_accel_demo` 完成后的最终观测面。
- `2026-04-24` 同日继续完成 Wave 2 任务 2：per-op / per-tile profile summary 第一刀。
  - `AiGraphScheduler` 的 execution result 当前已新增 host-side per-op summary，按 op 顺序稳定暴露 `op_index / opcode / retired_ops / compute_cycles / stall_cycles / tile_count`。
  - `AiAccelerator` 当前已把最近一次成功 compute 的 per-op summary 收口成独立 profile 统计，并新增第一版 tile 聚合字段 `tile_count / scratchpad_peak_bytes`；现有 completion entry ABI 继续保持不变。
  - `ai_accelerator_cnn_smoke` 与 `ai_accelerator_gemm_smoke` 当前已分别锁住 `conv / relu / layout_transpose / reduce_sum`、`gemm / pool_max` 的分项归因，以及 fault completion 后 summary 稳定不漂移的合同。
- `2026-04-24` 同日继续完成 Wave 2 任务 3：tiny model host workload。
  - `workloads/ai_proto/pack_graph.py` 当前已新增 `tiny_model` packer，固定生成一条更像小模型 block 的 `fp16 gemm -> fp32 relu -> fp32 max-pool` workload。
  - 这一刀没有强行扩到 `conv -> relu -> pool -> fc`：在当前算子面下，`conv / relu` 走 `int32`，`pool` 只接受 `fp32`，而 `gemm` 也还不接受 `fp32` 输入；为了不把任务 3 反向扩大到新的 dtype / op 合同，本轮保持 host workload 收敛。
  - `ai_accelerator_profile_smoke` 与 `make run-workload WORKLOAD_NAME=ai_proto AI_PROTO_WORKLOAD=tiny_model` 当前都已锁住这条 `tiny_model` 的 pack / profile / expected-output 闭环。
- `2026-04-24` 同日继续完成 Wave 2 任务 4：bounded dynamic shape 合同与 reject matrix。
  - `AiGraphPackage` 当前已新增 `shape_mode=dynamic_bounded`、dynamic tensor metadata、training future 保留字段 fail-closed，以及 runtime shape table 校验与 concrete package resolve helper。
  - `AiSubmissionDescriptor` 当前已在不改变 `48-byte` 宽度的前提下新增 `runtime_shape_table_offset` roundtrip；静态 package 带 offset 会 reject，动态 package 缺 offset 也会 fail-closed。
  - `test-unit-ai_graph_package` 与 `test-unit-ai_accelerator_mmio_contract` 当前已锁住动态 graph package parser / reject matrix、runtime shape bound 校验，以及 descriptor ABI roundtrip。
- `2026-04-24` 同日继续完成 Wave 2 任务 5：bounded dynamic `GEMM / FC` 正向闭环第一刀。
  - `AiAccelerator` 当前已能在 doorbell 路径读取 runtime shape table，并把 `dynamic_bounded` package resolve 成本次 submission 的 concrete static package；现阶段先把 runtime dims 接到 `GEMM` 路径，不改变静态 graph 的既有语义。
  - `workloads/ai_proto` 当前已新增 `dynamic_gemm`，manifest 也已支持可选 `runtime_shape_table=` 入口；`--ai-profile-manifest` summary 现在会额外输出 `shape_mode` 与 `runtime_shapes`。
  - `ai_accelerator_gemm_smoke` 当前已锁住同一 dynamic GEMM graph 在两组 runtime shape 下的不同输出、`retired_ops / bytes_moved / tile_count / scratchpad_peak_bytes`，以及缺 runtime shape 时的 fail-closed；`ai_accelerator_profile_smoke` 当前也已锁住 `dynamic_gemm` 的 pack / profile / expected-output 闭环。
- `2026-04-24` 同日后续补上 runtime-shape fail-closed hardening：`parse_ai_runtime_shape_table()` 当前会拒绝非零 reserved byte，dynamic graph extended header reserved 字段也会 fail-closed，`AiAccelerator` 还会拒绝未对齐的 `runtime_shape_table_offset`，避免后续扩展位或错位 runtime shape table 被旧 parser / 设备路径静默吞掉；这些合同已由 `test-unit-ai_graph_package` 与 `test-host-ai_accelerator_gemm_smoke` 锁住。
- `2026-04-24` 同日已完成 Wave 3，并归档到 [../plan/history_plan.md#npu-tpu-accelerator-wave3-plan](../plan/history_plan.md#npu-tpu-accelerator-wave3-plan)。
  - runtime-shape fail-closed matrix 当前已经补齐到 parser / host manifest / device submission 三层：extended header reserved、runtime-shape reserved byte、manifest 缺失/重复 `runtime_shape_table`、runtime shape 文件尺寸错误、rank / dims 超界，以及 `runtime_shape_table_offset` 的 zero / unaligned / overlap / out-of-window / DMA-fault 都已形成最小回归。
  - `Machine::AiProfileRunResult` 与 `--ai-profile-manifest` 当前已稳定暴露 `ai_profile_aggregate` 与 `ai_profile_op` 文本出口，固定输出 `tile_count / scratchpad_peak_bytes / op_count`，以及每个 op 的 `op_index / opcode / retired_ops / compute_cycles / stall_cycles / tile_count`。
  - profile lifecycle 当前也已收口：success 会更新 summary，runtime-shape / execute fault 不会污染上一轮成功 summary，reset 会把 aggregate 与 op summary 清零；这些合同已由 `test-host-ai_accelerator_cnn_smoke`、`test-host-ai_accelerator_gemm_smoke` 与 `test-host-ai_accelerator_profile_smoke` 一起守住。
- `2026-04-29` 已打开主线 Wave 4 的 AI accelerator 三段切片计划。
  - 切片 A 先推进 `bounded dynamic shape` 与动态小模型 workload，把当前只覆盖
    `GEMM / FC-like` 的动态路径扩到现有 op family 的正向或 fail-closed 合同。
  - 切片 B 再推进 profile / timing attribution 与最小 frontend AI accelerator
    观察面，只消费只读 debug / manifest 字段。
  - 切片 C 把 `Softmax + tiny static attention` 作为后段 stretch；如果数值或 op
    合同扩大过快，可以降级到 AI accelerator 后续专项阶段，不阻塞主线 Wave 4 的
    AI accelerator 核心完成。
- `2026-04-29` 主线 Wave 4 / AI accelerator 切片 A 已完成。
  - `bounded dynamic shape` 的 concrete package resolve 现在会重新校验
    `GEMM / conv2d / relu / pool / reduce / layout_transpose` 的最小 shape / dtype
    合同，漏声明 dynamic op tensor、dynamic memory-plan byte mismatch、runtime
    scratchpad overflow 等都会 fail-closed。
  - 设备侧 host smoke 已新增 dynamic `conv2d -> relu -> layout_transpose ->
    reduce_sum` 正向闭环，并锁住 runtime scratchpad overflow 的
    `AI_ACCEL_FAULT_SCRATCHPAD_OVERFLOW` 归因和 fault 后 profile lifecycle 不漂移。
  - `workloads/ai_proto` 已新增 `dynamic_tiny_model`，复用现有
    `fp16 gemm -> fp32 relu -> fp32 max-pool` 算子面，通过 runtime shape table
    运行 `1x3 -> 1x2 -> 1x1` 动态小模型闭环，并进入 host profile / workload 入口。
- `2026-04-29` 主线 Wave 4 / AI accelerator 切片 B 已完成。
  - `timed-simple` profile 现在把 tile setup 归入 `stall_cycles`，`compute_cycles`
    只表达 throughput compute；host profile、per-op summary 与 guest debug counters
    已同步新 attribution。
  - debug snapshot 继续保持 aggregate-only AI accelerator schema，不把 host-side
    itemized op summary 扩大成 MMIO / debug ABI。
  - frontend 已为 `guest_ai_accel_demo` 增加 workload presentation，并在平台组中
    展示 queue、engine busy、scratchpad occupancy、DMA bytes、device / dma /
    compute / stall cycles、effective ops 与 utilization；缺字段时优雅降级。
- `2026-04-29` 主线 Wave 4 / AI accelerator 切片 C stretch 已完成。
  - graph package / validator / serializer 已新增静态 `fp32 -> fp32` row-wise
    `Softmax` op，非法 dtype / rank 继续 fail-closed。
  - compute path 已新增 `Softmax` 和最小 `fp32 -> fp32` GEMM 支撑，用于固定
    `tiny_attention_static` 的 `gemm -> softmax -> gemm` 小闭环。
  - `workloads/ai_proto` 已新增 `tiny_attention_static`，host profile 稳定输出
    `gemm / softmax / gemm` 三段 `ai_profile_op`；该 stretch 不代表完整 attention、
    动态 sequence length、KV-cache、多 head attention 或 Transformer runtime。

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
  - 同日完成 wave 1 任务 6，并通过：
    - `cd myCPU && make test-host-ai_accelerator_profile_smoke`
    - `cd myCPU && make test`
    - `cd myCPU && make test-pipeline`
  - 同日完成 wave 1 任务 7，并通过：
    - `cd myCPU && make test-guest-ai_accel_demo`
    - `cd myCPU && make test-pipeline-guest-ai_accel_demo`
    - `cd myCPU && make test-host-ai_accel_guest_smoke`
    - `cd myCPU && make test-host-debug_cli_smoke`
    - `cd myCPU && make test`
    - `cd myCPU && make test-pipeline`
  - 同日启动 wave 1 hardening 第一刀，并通过：
    - `cd myCPU && make test-host-ai_accelerator_profile_smoke`
  - 同日完成 wave 1 hardening 第二刀，并通过：
    - `cd myCPU && make test-unit-ai_accel_queue`
    - `cd myCPU && make test-guest-ai_accel_demo`
    - `cd myCPU && make test-host-ai_accel_guest_smoke`
- `2026-04-24`
  - 完成 Wave 2 任务 1：新增 profile attribution 与 MMIO / debug 观测第一刀，`busy_cycles / queue_cycles / completion_cycles / effective_ops_per_cycle / utilization` 已进入设备只读窗口、host profile summary 与 debug snapshot。
  - 本轮验证已覆盖：
    - `cd myCPU && make test-unit-ai_accelerator_mmio_contract`
    - `cd myCPU && make test-host-ai_accelerator_profile_smoke`
    - `cd myCPU && make test-host-debug_cli_smoke`
  - 完成 Wave 2 任务 2：新增 host-side per-op / per-tile profile summary，并通过：
    - `cd myCPU && make test-host-ai_accelerator_cnn_smoke`
    - `cd myCPU && make test-host-ai_accelerator_gemm_smoke`
  - 完成 Wave 2 任务 3：新增固定 `tiny_model` host workload，并通过：
    - `cd myCPU && make test-host-ai_accelerator_profile_smoke`
    - `cd myCPU && make run-workload WORKLOAD_NAME=ai_proto AI_PROTO_WORKLOAD=tiny_model`
  - 完成 Wave 2 任务 4：新增 bounded dynamic shape contract / reject matrix；`AiGraphPackage` 现在已支持 `shape_mode=dynamic_bounded`、training future 保留字段 fail-closed、dynamic tensor metadata 与 runtime shape table 校验，`AiSubmissionDescriptor` 也已在不改变 `48-byte` 宽度的前提下暴露 `runtime_shape_table_offset` roundtrip，并通过：
    - `cd myCPU && make test-unit-ai_graph_package`
    - `cd myCPU && make test-unit-ai_accelerator_mmio_contract`
  - 完成 Wave 2 后续 runtime-shape fail-closed hardening：非零 runtime shape table reserved byte、dynamic graph extended header reserved 字段与未对齐 `runtime_shape_table_offset` 现在都会被拒绝，并通过：
    - `cd myCPU && make test-unit-ai_graph_package`
    - `cd myCPU && make test-host-ai_accelerator_gemm_smoke`
  - 完成 Wave 3：runtime-shape fail-closed matrix、host profile manifest 负向矩阵、`--ai-profile-manifest` itemized 文本出口，以及 profile lifecycle 都已收口，并通过：
    - `cd myCPU && make test-unit-ai_graph_package`
    - `cd myCPU && make test-unit-ai_accelerator_mmio_contract`
    - `cd myCPU && make test-host-ai_accelerator_cnn_smoke`
    - `cd myCPU && make test-host-ai_accelerator_gemm_smoke`
    - `cd myCPU && make test-host-ai_accelerator_profile_smoke`
    - `cd myCPU && make test`
    - `cd myCPU && make test-pipeline`

## 当前仍然有效的风险 / 限制

- 当前主线 active wave 已切到 `Wave 4`，本方向重新进入近端推进；但推进范围仍限于
  AI accelerator 的动态 shape、profile、workload 和观察面，不代表真实 cache /
  DMA / multicore 已启动。
- 注意这里的 `Wave 4` 是仓库主线 wave；本状态文档中已完成的 AI accelerator
  Wave 1 / 2 / 3 是本方向局部历史阶段，不能把二者当成同一套编号。
- `DMA-ready` contract、graph package、tensor golden model、独立 AI accelerator 控制面、`scratchpad/DMA engine`、第一版 compute engine、host graph packaging / profile 入口，以及 guest driver / guest demo / debug profile 可观察性当前都已有实现；这条线的剩余限制已经不再是“入口没接上”，而是更细的 timing / overlap / performance 模型还没展开。
- 当前已经有独立设备时序模型的第二刀，但仍只停在 `timed-simple`：`DMA + compute`
  当前固定为 no-overlap，`stall_cycles` 只承担 tile setup 等最小等待归因，因此这条线
  仍不能拿来讨论更激进的 tile overlap、queue 开销隐藏或更细颗粒度吞吐模型。
- 当前 per-op / per-tile profile summary 已经外推到 `--ai-profile-manifest` 的 `ai_profile_aggregate` / `ai_profile_op` 文本出口，但仍没有扩大到 MMIO 或 debug snapshot 的 itemized ABI；当前继续保持“host-side 文本出口 + 设备侧 ABI 不变”的边界。
- 当前 `tiny_model` 已经补上，但它仍是受当前算子面约束的 `gemm -> relu -> pool` block，还不是更完整的 `conv -> relu -> pool -> fc`；如果后续要推进到后者，应先在独立设计里明确 dtype bridge 或更完整的 matmul-family 输入合同，而不是在 Wave 2 的 host workload 里偷开新语义。
- 同时覆盖 quantized 与 semi-precision family 会明显放大验证矩阵；后续实施时必须坚持“统一 ABI + 代表性闭环”，不能一开始就追求全矩阵算子铺满。
- bounded dynamic shape 当前已从 matmul-family 第一刀扩到主线 Wave 4 切片 A 的
  `conv / pool / relu / reduce / layout_transpose` 正向或 fail-closed 合同；
  但它仍是 bounded dynamic shape，不是任意动态图，fault detail 也仍保持最小归因。
- 动态 shape 和训练支持已经是正式远期目标，但仍不能抢跑完整动态图或训练栈；
  当前必须先守住 bounded dynamic shape、profile attribution 和小模型推理闭环。
- `Softmax + tiny static attention` 已作为 Wave 4 stretch 完成，但只覆盖最小静态
  `fp32` row-wise softmax 和极小 attention-like workload；`INT4`、`GELU /
  Sigmoid`、MobileNet、训练前向 / 反向和 Linux-facing NPU driver 都后移到 AI
  accelerator 后续专项阶段。
- `Wave 7` 的 AI accelerator 产品化展示不开放任意用户模型。推荐边界是白名单 demo +
  参数化小模型：用户只能在固定模板内调整小尺寸输入、bounded runtime shape 或已支持
  op 组合，服务器端重新生成 / 校验 graph package、dtype、shape、memory plan 和资源上限。
- `Wave 7` 之后，AI accelerator 新主线可以主动打破当前保守边界：支持用户自己的 AI
  任务，提供公开 graph schema、DSL / importer、graph lowering / compiler、自动 memory
  plan、更多 op / dtype / quantization、Linux-facing driver，并把 timing 从当前
  `timed-simple no-overlap` 推向更接近商用 NPU 的 tile scheduler、DMA + compute overlap、
  multi outstanding queue、buffer ownership、per-op timeline、带宽 / 延迟 / 利用率模型。
- 当前这条 Post-Wave 7 新主线已经明确第一刀采用受限 importer，而不是先做性能模型、
  任意模型上传或完整 runtime；当前 importer 已覆盖最小 `bounded_dynamic_gemm_v1` 与
  最小 `bounded_dynamic_cnn_v1`，但仍不代表更宽 op family 已经开放。
- 如果把这条线和当前 `xv6 / Linux` 主线混在同一轮里推进，很容易打散已有回归与 ownership 边界。

## 下一步

1. 本方向的主线 Wave 4 AI accelerator 切片完成记录统一见
   [../plan/history_plan.md#mainline-wave4-ai-accelerator-slices-plan](../plan/history_plan.md#mainline-wave4-ai-accelerator-slices-plan)。
2. `Wave 7` 展示优先做白名单 demo + 参数化小模型，而不是任意模型上传；前端参数必须经
   host 侧 graph package 生成 / 校验和资源限制后再运行。
3. 当前已新增
   [../design/post_wave7_ai_user_tasks_npu_performance_design.md](../design/post_wave7_ai_user_tasks_npu_performance_design.md)
   与
   [../plan/post_wave7_ai_user_tasks_npu_performance_plan.md](../plan/post_wave7_ai_user_tasks_npu_performance_plan.md)，
   后续应先在这套文档里明确第一刀的用户任务入口、compile / memory plan 和性能模型阶段边界。
4. 当前第一刀实现已进一步收窄为 host-side `bounded_dynamic_gemm_v1` + `bounded_dynamic_cnn_v1`
   task spec importer；共享 lower 模块已经抽出，下一步先守住它们与现有
   `dynamic_gemm` / `dynamic_cnn` 共用 lowering / memory-plan 路径，再逐步扩展到更宽 task kind。
5. 在第一刀实现中，优先保持现有 `dynamic_tiny_model`、`dynamic_gemm`、`dynamic_cnn`、
   `tiny_attention_static`、guest `ai_accel_demo` 和既有 profile / debug 可观察性继续作为稳定 guardrail。

## 验证基线

- 当前已落地并验证的门禁：
  - `cd myCPU && make test-unit-dma_transaction_contract`
  - `cd myCPU && make test-unit-bus_region_contract`
  - `cd myCPU && make test-unit-ai_graph_package`
  - `cd myCPU && make test-unit-ai_accelerator_mmio_contract`
  - `cd myCPU && make test-unit-ai_dma_engine`
  - `cd myCPU && make test-unit-ai_scratchpad`
  - `cd myCPU && make test-unit-ai_accel_queue`
  - `cd myCPU && make test-host-ai_tensor_golden_ops_smoke`
  - `cd myCPU && make test-host-ai_accelerator_submit_smoke`
  - `cd myCPU && make test-host-ai_accelerator_dma_smoke`
  - `cd myCPU && make test-host-ai_accelerator_cnn_smoke`
  - `cd myCPU && make test-host-ai_accelerator_gemm_smoke`
  - `cd myCPU && make test-host-ai_accelerator_profile_smoke`
  - `cd myCPU && make test-host-ai_accel_guest_smoke`
  - `cd myCPU && make test-host-debug_cli_smoke`
  - `cd myCPU && make test-guest-ai_accel_demo`
  - `cd myCPU && make test-pipeline-guest-ai_accel_demo`
  - `cd myCPU && make test`
  - `cd myCPU && make test-pipeline`
- 后续继续扩到设备控制面、debug 或 guest/runtime 路径时，仍应按触达范围补跑 `make test` / `make test-pipeline` 与对应窄门禁。
