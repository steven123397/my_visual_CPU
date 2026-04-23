# `NPU / TPU-like` AI accelerator Wave 1 实现计划

> **文档状态：** 执行中，任务 1-5 已完成（`2026-04-23` 已完成方向 design / status / wave 1 plan；同日已完成任务 1：`DMA-ready` memory contract，任务 2：静态 graph package 与 tensor golden model，任务 3：AI accelerator 控制面与 MMIO 设备骨架，任务 4：scratchpad + DMA/load-store engine 与第一版 `timed-simple` DMA timing，以及任务 5：静态子图调度器、代表性 compute path 与第一版 `timed-simple` compute timing）
>
> **面向 AI 代理的工作者：** 推荐使用 `superpowers:subagent-driven-development` 或 `superpowers:executing-plans` 逐任务实现此计划。步骤使用复选框（`- [ ]`）语法跟踪进度。

## 文档定位

本文档用于记录独立 `MMIO NPU / TPU-like` AI accelerator 的 Wave 1 如何落地，包括：

- wave 1 的文件分解、责任边界和先后顺序
- 从 `DMA-ready` 基座到代表性推理闭环的最小实施路径
- 每个任务应修改哪些文件、增加哪些测试、用什么验证命令守住结果

本文档不回答“当前优先级是不是已经切到这条线”；那部分口径以对应 `status` 文档为准。

## 关联文档

- 来源设计：
  - [../design/npu_tpu_accelerator_direction_design.md](../design/npu_tpu_accelerator_direction_design.md)
  - [../design/future_expansion_roadmap_design.md](../design/future_expansion_roadmap_design.md)
- 目标状态：
  - [../status/npu_tpu_accelerator_status.md](../status/npu_tpu_accelerator_status.md)
  - [../status/project_priority_roadmap.md](../status/project_priority_roadmap.md)

## 目标

- 把独立 `MMIO NPU / TPU-like` AI accelerator 从纯设计推进到可执行的 Wave 1 foundation。
- 先补 `DMA-ready` memory contract、graph package 与设备控制 / 数据面，再接代表性 `CNN` 和 `GEMM / Transformer-like` 推理闭环。
- 在 wave 1 中补上一层 `timed-simple` 模拟时序 / profile 口径，用 `simulated cycles` 而不是宿主机 wall-clock 判断结构收益。
- 在不污染当前 CPU reference path 和 `xv6 / Linux` 主线的前提下，形成后续可单独扩展的 AI accelerator 子系统。

## 完成定义

- `Bus` 侧已经具备最小 `DMA-ready` initiator / transaction contract，并有独立门禁。
- 已有统一的静态 graph package 格式、descriptor reject matrix 和 tensor golden model。
- 已有独立 `MMIO` AI accelerator 设备骨架，具备：
  - capability / status / fault / perf counter
  - submission / completion queue
  - doorbell / interrupt
- 已有最小 `timed-simple` 时序 / profile 基线，至少能稳定输出：
  - `device_cycles`
  - `dma_cycles`
  - `compute_cycles`
  - `stall_cycles`
  - `bytes_moved`
  - `retired_ops`
- 已有独立 `scratchpad + DMA/load-store engine` 数据面。
- 已有代表性的 compute path：
  - 一条 `CNN` inference smoke
  - 一条 `GEMM / Transformer-like` inference smoke
- host harness 与 guest driver 已共用同一套 graph package / queue ABI，并至少各有一条 smoke。
- AI accelerator 的性能比较口径已经固定为 `simulated cycles`，而不是宿主机秒数；host profile 至少能给出和 CPU / vector baseline 对比的基础数据。
- `docs/status/npu_tpu_accelerator_status.md`、`docs/design/npu_tpu_accelerator_direction_design.md` 与 `docs/index.md` 已同步到完成态口径。

## 文件分解

- `myCPU/src/mem/dma_transaction.{h,cpp}`
  设备主动发起内存访问的统一 transaction / result contract。
- `myCPU/src/devices/ai_graph_package.{h,cpp}`
  静态 graph package header、tensor metadata、op descriptor、dependency 与 memory plan parser / validator。
- `myCPU/src/devices/tensor_golden_ops.{h,cpp}`
  `INT8 / INT16 / FP16 / BF16` 代表性 tensor op 的 golden model。
- `myCPU/src/devices/ai_accelerator.{h,cpp}`
  独立 AI accelerator 顶层设备、MMIO 控制面、fault / capability / queue 接线。
- `myCPU/src/devices/ai_submission_queue.{h,cpp}`
  submission / completion ring、doorbell、token 生命周期。
- `myCPU/src/devices/ai_dma_engine.{h,cpp}`
  `system RAM <-> scratchpad` 的 DMA / tiled transfer。
- `myCPU/src/devices/ai_scratchpad.{h,cpp}`
  设备本地 tile / accumulator buffer 管理。
- `myCPU/src/devices/ai_graph_scheduler.{h,cpp}`
  静态子图 ready 判断、执行次序与 completion 汇总。
- `myCPU/src/devices/ai_compute_gemm.{h,cpp}`
  `GEMM / matmul` 族代表性 compute path。
- `myCPU/src/devices/ai_compute_conv.{h,cpp}`
  `conv2d` 代表性 compute path。
- `myCPU/src/devices/ai_compute_elementwise.{h,cpp}`
  `eltwise / pool / reduce / layout transform` 代表性 compute path。
- `myCPU/workloads/ai_proto/*`
  host 侧 graph package 生成、固定 workload profile 与打包工具。
- `myCPU/guest/include/ai_accel.h`
  guest driver 对外 ABI。
- `myCPU/guest/kernel/ai_accel.c`
  guest 侧最小 queue / doorbell / interrupt / profile 驱动。
- `myCPU/guest/ai_accel_demo/*`
  guest 侧最小推理 demo。
- `myCPU/tests/unit/*`
  contract / parser / MMIO / DMA / scratchpad 单元测试。
- `myCPU/tests/host/*`
  host-side graph submit、CNN、GEMM、profile 与 guest smoke。

## 任务

### 任务 1：`DMA-ready` memory contract

**文件：**
- 创建：
  - `myCPU/src/mem/dma_transaction.h`
  - `myCPU/src/mem/dma_transaction.cpp`
  - `myCPU/tests/unit/dma_transaction_contract.cpp`
- 修改：
  - `myCPU/src/mem/bus.h`
  - `myCPU/src/mem/bus.cpp`
  - `myCPU/Makefile`
- 说明：
  - 本轮最小实现复用了现有 `memory_region` 属性合同；`myCPU/src/mem/memory_region.h`、`myCPU/src/devices/device.h` 与 `myCPU/src/platform/machine.cpp` 评估后无需改动。

- [x] **步骤 1：** 冻结设备主动访存的最小 contract：initiator、direction、burst、side-effect、fault result 与可见性规则。
- [x] **步骤 2：** 在 `Bus` 上新增保守 `dma_read / dma_write`（或等价）入口，保持 CPU 访存路径不变。
- [x] **步骤 3：** 明确 `RAM / MMIO / unmapped` 在 DMA 路径上的 fail-closed 语义，尤其是 live `MMIO` 与 side effect 行为。
- [x] **步骤 4：** 补 unit 测试，覆盖 `RAM` 正向、`MMIO` 拒绝、`unmapped` fault、burst 边界和 partial-failure 语义。
- [x] **步骤 5：** 运行：
  - `cd myCPU && make test-unit-dma_transaction_contract`
  - `cd myCPU && make test-unit-bus_region_contract`
  - `cd myCPU && make test`

### 任务 2：静态 graph package 与 tensor golden model

**文件：**
- 创建：
  - `myCPU/src/devices/ai_graph_package.h`
  - `myCPU/src/devices/ai_graph_package.cpp`
  - `myCPU/src/devices/tensor_golden_ops.h`
  - `myCPU/src/devices/tensor_golden_ops.cpp`
  - `myCPU/tests/unit/ai_graph_package.cpp`
  - `myCPU/tests/host/ai_tensor_golden_ops_smoke.cpp`
  - `myCPU/tests/data/ai_accel/`
- 修改：
  - `myCPU/Makefile`

- [x] **步骤 1：** 固定 graph package 的最小 header、tensor table、op descriptor、dependency 与 memory plan 编码。
- [x] **步骤 2：** 定义 reject matrix：非法 dtype、非法 rank / tile、越界 dependency、非法 scratchpad budget、未知 op。
- [x] **步骤 3：** 实现代表性 golden ops：
  - `INT8 / INT16 -> INT32 accumulate` 的 `gemm / conv`
  - `FP16 / BF16 -> FP32 accumulate` 的 `gemm`
  - `eltwise / pool / reduce / layout transform`
- [x] **步骤 4：** 用 fixed tensor vectors 和小矩阵样本建立可重复对拍数据，不依赖随机值作为主门禁。
- [x] **步骤 5：** 运行：
  - `cd myCPU && make test-unit-ai_graph_package`
  - `cd myCPU && make test-host-ai_tensor_golden_ops_smoke`
  - `cd myCPU && make test`

### 任务 3：AI accelerator 控制面与 MMIO 设备骨架

**文件：**
- 创建：
  - `myCPU/src/devices/ai_accelerator.h`
  - `myCPU/src/devices/ai_accelerator.cpp`
  - `myCPU/src/devices/ai_submission_queue.h`
  - `myCPU/src/devices/ai_submission_queue.cpp`
  - `myCPU/tests/unit/ai_accelerator_mmio_contract.cpp`
  - `myCPU/tests/host/ai_accelerator_submit_smoke.cpp`
- 修改：
  - `myCPU/include/platform_mmio.h`
  - `myCPU/src/platform/address_map.h`
  - `myCPU/src/platform/machine.h`
  - `myCPU/src/platform/machine.cpp`
  - `myCPU/src/debug/debug_snapshot.h`
  - `myCPU/src/debug/debug_session.cpp`
  - `myCPU/src/debug/debug_protocol_response.cpp`
  - `myCPU/Makefile`

- [x] **步骤 1：** 定义 capability / status / reset / queue base / doorbell / irq / fault / perf counter 寄存器窗口。
- [x] **步骤 2：** 把 AI accelerator 作为独立设备接进 `Machine`，默认不影响现有 board profile。
- [x] **步骤 3：** 先接 submission / completion ring skeleton、doorbell、token 流转与中断骨架，不提前耦合 compute engine。
- [x] **步骤 4：** 在 debug snapshot / protocol 里新增只读控制面观测：queue depth、doorbell count、last fault、completion count。
- [x] **步骤 5：** 运行：
  - `cd myCPU && make test-unit-ai_accelerator_mmio_contract`
  - `cd myCPU && make test-host-ai_accelerator_submit_smoke`
  - `cd myCPU && make test`
  - `cd myCPU && make test-pipeline`

### 任务 4：scratchpad 与 DMA / load-store engine

**文件：**
- 创建：
  - `myCPU/src/devices/ai_dma_engine.h`
  - `myCPU/src/devices/ai_dma_engine.cpp`
  - `myCPU/src/devices/ai_scratchpad.h`
  - `myCPU/src/devices/ai_scratchpad.cpp`
  - `myCPU/tests/unit/ai_dma_engine.cpp`
  - `myCPU/tests/unit/ai_scratchpad.cpp`
  - `myCPU/tests/host/ai_accelerator_dma_smoke.cpp`
- 修改：
  - `myCPU/src/devices/ai_accelerator.cpp`
  - `myCPU/tests/unit/ai_accelerator_mmio_contract.cpp`
  - `myCPU/tests/host/ai_accelerator_submit_smoke.cpp`
  - `myCPU/Makefile`

- 说明：
  - 本轮 task 4 实际无需改 `ai_submission_queue` 或 `Bus`；现有 `dma_transaction + Bus::dma_read()/dma_write()` 合同已足够承接设备内 `scratchpad` 与 `DMA/load-store` engine。

- [x] **步骤 1：** 固定 scratchpad / accumulator / temporary buffer 的地址空间与生命周期，避免把局部状态散落到顶层设备。
- [x] **步骤 2：** 实现 `system RAM <-> scratchpad` 的 tiled DMA / load-store engine，并复用任务 1 的 `dma_transaction` contract。
- [x] **步骤 3：** 给 DMA path 接入第一版 `timed-simple` 周期开销：至少固定 `setup_cycles`、`dma_bytes_per_cycle` 与 `DMA load/store` 分项计数，不把数据搬运继续建模成“立即完成”。
- [x] **步骤 4：** 明确并测试 `scratchpad_overflow`、DMA fault、非法 tile shape 与 partial transfer fail-closed 语义。
- [x] **步骤 5：** 在 host smoke 中锁住“先搬运、后计算、再回写”的最小数据面闭环，并验证 DMA 相关计数器单调增长。
- [x] **步骤 6：** 运行：
  - `cd myCPU && make test-unit-ai_dma_engine`
  - `cd myCPU && make test-unit-ai_scratchpad`
  - `cd myCPU && make test-host-ai_accelerator_dma_smoke`
  - `cd myCPU && make test-unit-ai_accelerator_mmio_contract`
  - `cd myCPU && make test-host-ai_accelerator_submit_smoke`
  - `cd myCPU && make test`

### 任务 5：静态子图调度器与代表性 compute path

**文件：**
- 创建：
  - `myCPU/src/devices/ai_graph_scheduler.h`
  - `myCPU/src/devices/ai_graph_scheduler.cpp`
  - `myCPU/src/devices/ai_compute_gemm.h`
  - `myCPU/src/devices/ai_compute_gemm.cpp`
  - `myCPU/src/devices/ai_compute_conv.h`
  - `myCPU/src/devices/ai_compute_conv.cpp`
  - `myCPU/src/devices/ai_compute_elementwise.h`
  - `myCPU/src/devices/ai_compute_elementwise.cpp`
  - `myCPU/tests/host/ai_accelerator_cnn_smoke.cpp`
  - `myCPU/tests/host/ai_accelerator_gemm_smoke.cpp`
- 修改：
  - `myCPU/src/devices/ai_accelerator.cpp`
  - `myCPU/src/devices/ai_graph_package.cpp`
  - `myCPU/src/devices/ai_dma_engine.cpp`
  - `myCPU/Makefile`

- 说明：
  - 本轮 task 5 实际无需改 `ai_graph_package.cpp` 或 `ai_dma_engine.cpp`；现有 graph package / DMA 合同已足够承接第一版静态调度器与 compute path。
  - 第一版 `timed-simple` compute timing 当前明确采用 `DMA + compute` **不重叠** 的保守语义；`stall_cycles` 已作为只读计数器接线，但本轮代表性 workload 下仍保持为 `0`。

- [x] **步骤 1：** 在调度器中只支持静态 shape、静态 dependency、离线 memory plan，不提前引入动态 shape。
- [x] **步骤 2：** 先接一组最小 compute primitive：`gemm`、`conv2d`、`eltwise(relu)`、`pool / reduce`、`layout transform`。
- [x] **步骤 3：** 给 compute path 接入 `timed-simple` compute 周期模型，至少明确 `retired_ops / MACs / tile` 到 `compute_cycles` 的映射，以及 `DMA + compute` 是否允许重叠。
- [x] **步骤 4：** `v1` 只要求代表性闭环，而不是所有 op × 所有 dtype 铺满；先锁住：
  - 一条 quantized `CNN`
  - 一条 semi-precision `GEMM / matmul-family`
- [x] **步骤 5：** 在 host smoke 中固化 `CNN` 和 `GEMM / Transformer-like` 两类固定 workload 的输出、fault 行为和基础 timing counter 语义。
- [x] **步骤 6：** 运行：
  - `cd myCPU && make test-host-ai_accelerator_cnn_smoke`
  - `cd myCPU && make test-host-ai_accelerator_gemm_smoke`
  - `cd myCPU && make test`

### 任务 6：host graph packaging 与 workload 入口

**文件：**
- 创建：
  - `myCPU/workloads/ai_proto/profile.mk`
  - `myCPU/workloads/ai_proto/pack_graph.py`
  - `myCPU/workloads/ai_proto/README.md`
  - `myCPU/tests/host/ai_accelerator_profile_smoke.cpp`
- 修改：
  - `myCPU/workloads/common.mk`
  - `myCPU/Makefile`
  - `myCPU/src/main.cpp`
  - `myCPU/src/platform/machine.cpp`

- [ ] **步骤 1：** 把固定 `CNN` 与 `GEMM / matmul-family` workload lower 成 graph package，而不是让 host test 手写 ad-hoc descriptor。
- [ ] **步骤 2：** 复用现有 `workloads/` 体系接入独立 `ai_proto` profile，不另起并行脚本目录。
- [ ] **步骤 3：** 给 host 侧入口补最小 profile / summary 输出，至少能看到 package、`device_cycles`、`dma_cycles`、`compute_cycles`、`bytes_moved`、`retired_ops` 和基本 fault / progress。
- [ ] **步骤 4：** 如果已有 CPU / vector baseline，就在同一模拟周期口径下输出最小 speedup summary；没有 baseline 时至少输出可对比的 raw counters，不回退到 wall-clock。
- [ ] **步骤 5：** 补 host smoke，锁住 packaging 输出与 profile 行为的兼容性。
- [ ] **步骤 6：** 运行：
  - `cd myCPU && make test-host-ai_accelerator_profile_smoke`
  - `cd myCPU && make test`

### 任务 7：guest driver、demo 与 debug/profile 收尾

**文件：**
- 创建：
  - `myCPU/guest/include/ai_accel.h`
  - `myCPU/guest/kernel/ai_accel.c`
  - `myCPU/guest/ai_accel_demo/start.S`
  - `myCPU/guest/ai_accel_demo/main.c`
  - `myCPU/tests/host/ai_accel_guest_smoke.cpp`
- 修改：
  - `myCPU/guest/include/platform_drivers.inc`
  - `myCPU/guest/include/kernel_runtime.h`
  - `myCPU/guest/kernel/kernel_runtime.c`
  - `myCPU/src/debug/debug_snapshot.h`
  - `myCPU/src/debug/debug_protocol_response.cpp`
  - `myCPU/tests/host/debug_cli_smoke.cpp`
  - `myCPU/Makefile`

- [ ] **步骤 1：** 为 guest 侧定义最小 driver ABI：buffer、queue、doorbell、interrupt、fault 读取与 profile 读取。
- [ ] **步骤 2：** 新增一个固定输入、固定 graph package 的 `guest/ai_accel_demo`，只证明 guest 能提交、等待并读回结果。
- [ ] **步骤 3：** 在 debug snapshot / CLI 中补 AI accelerator 只读观测：queue depth、DMA bytes、scratchpad occupancy、engine busy、last fault，以及 `device_cycles / dma_cycles / compute_cycles / stall_cycles`。
- [ ] **步骤 4：** 通过 host smoke 与 debug CLI smoke 锁住 guest 提交和 profile 可见性，不扩大 frontend UI 范围。
- [ ] **步骤 5：** 运行：
  - `cd myCPU && make test-host-ai_accel_guest_smoke`
  - `cd myCPU && make test-host-debug_cli_smoke`
  - `cd myCPU && make test`
  - `cd myCPU && make test-pipeline`

## 完成态回写要求

- 全部 checklist 必须勾完。
- [../status/npu_tpu_accelerator_status.md](../status/npu_tpu_accelerator_status.md) 必须增加：
  - 完成结果摘要
  - 关键历史节点
  - 仍然有效的剩余风险
- [../status/project_priority_roadmap.md](../status/project_priority_roadmap.md) 需要同步更新：
  - 这条线是否仍只是候选方向
  - 是否已经形成可切主线的基础
- 需要把“完成时间 + 完成内容 + 必要时的一两句过程摘要”追加到 [history_plan.md](history_plan.md)。
- 归档完成后，删除原计划文件，不再长期保留完成态 checklist。
