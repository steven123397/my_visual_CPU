# `NPU / TPU-like` AI 加速器方向设计

## 文档定位

本文档用于说明未来 `NPU / TPU-like` AI 加速器方向的正式设计边界，重点回答下面 3 个问题：

- 为什么当前仓库不能把现有 `V-lite` 线直接等同于完整 AI 加速器
- 如果走独立设备路线，`v1` 的结构、接口、执行与内存边界应该如何定义
- 为了让这条线真正可落地，当前项目基座还需要补哪些结构

本文档不承担实时进度更新。当前主线、当前优先级和是否正式启动本方向，以对应 `status` 文档为准。

## 关联文档

- 状态文档：
  - [../status/mainline_status.md](../status/mainline_status.md)
  - [../status/project_priority_roadmap.md](../status/project_priority_roadmap.md)
  - [../status/npu_tpu_accelerator_status.md](../status/npu_tpu_accelerator_status.md)
- 相关设计：
  - [future_expansion_roadmap_design.md](future_expansion_roadmap_design.md)
  - [vector_ml_workload_direction_design.md](vector_ml_workload_direction_design.md)
  - [phase4_preparation_design.md](phase4_preparation_design.md)
  - [platform_mmio_contract.md](platform_mmio_contract.md)
- 当前计划：
  - [../plan/npu_tpu_accelerator_wave1_plan.md](../plan/npu_tpu_accelerator_wave1_plan.md)

## 背景与问题

当前仓库已经是一个已可运行的模拟器原型。`V-lite` 路线已经完成从最小 ISA、固定 `conv -> relu` guest workload，到最小 vector-aware `pipeline` 的闭环，这条线证明了仓库已经具备承接 ML workload 的基础能力，也为后续的 profile、观察和 `Phase 4` 判断提供了第一批真实信号。

但这条线目前仍然是“CPU 侧向量能力 + 教学式 workload 闭环”，而不是完整的 `NPU / TPU-like` AI 加速器。当前仍然缺少真正 AI 加速器会依赖的独立设备边界：异步提交、descriptor / queue 合同、显式 `DMA`、片上 `scratchpad / tile buffer`、子图级执行器、host / guest 共用的设备 ABI，以及面向 `CNN` 与 `GEMM / Transformer-like` 推理的更系统算子与 dtype 合同。

如果继续把这件事理解为“沿 `V-lite` 再补一些向量指令”，很容易把 CPU ISA 扩展、vector-aware `pipeline`、DMA-ready memory contract 和 AI 设备软件栈混成一件事。对当前仓库来说，更健康的做法是把这条线明确建模为一条新的未来方向：独立挂在 `Bus` 上的 `MMIO` AI 加速器设备。CPU 继续负责程序控制、buffer 生命周期和 doorbell / interrupt，而 tensor 执行、数据搬运和片上存储由设备内部统一负责。

## 目标

- 把“独立 `MMIO` AI 加速器设备”定义为与现有 `V-lite` 并行的正式未来方向，而不是继续膨胀 CPU 向量语义。
- 定义 `v1` 的正式边界：推理优先、静态 shape、静态子图、`scratchpad + DMA`、host / guest 双入口、统一 descriptor / queue / completion 合同。
- 统一收口 `v1` 的最小算子族、dtype family、执行流和故障语义。
- 明确项目基座需要补齐的结构，包括 `DMA-ready` memory contract、设备异步执行 ABI、图包格式、golden model 与 profile 观测面。
- 让这条线可以同时覆盖 `CNN` 与 `GEMM / MLP / Transformer-like` 两类推理 workload，但不把 `v1` 直接扩成完整训练栈或完整图编译器。

## 非目标

- 不把本方向定义成 `GPU / SIMT` 子系统；`warp`、thread block、shared memory 和图形/通用并行执行模型不在本文档范围内。
- 不把本方向定义成完整 `RVV` 或 CPU 紧耦合 tensor 指令扩展。
- 不在 `v1` 中承诺训练、反向传播、optimizer、梯度同步或动态图执行。
- 不在 `v1` 中承诺动态 shape；动态 batch、动态 sequence length 与更通用 runtime 只作为后续演进方向保留。
- 不在 `v1` 中承诺完整深度学习编译器、完整 kernel library 或 Linux 驱动完成态。
- 不把本文档理解成当前主线的即时实施指令；它只是未来候选方向的正式设计来源。

## 约束与边界

- 设备接入方式固定为：独立 `MMIO` 设备挂在 `Bus` 上，沿当前 `Machine -> Bus -> Device` 边界接入。
- CPU reference path 与 `InstructionSemantics` 不承担 AI 设备语义来源；这条线必须保持为独立设备合同。
- host harness 与 guest driver 必须共用同一套设备 ABI：相同的 `descriptor / queue / doorbell / completion / fault` 语义，不允许分叉成两套设备行为。
- `v1` 只支持静态 shape、离线准备好的静态子图；设备运行时不做图级动态重写。
- `v1` 采用显式 `scratchpad / tile buffer + DMA/load-store engine`，而不是把所有 tensor 访问退化成普通 RAM 连续 load/store。
- `v1` 默认不假设 CPU 与设备之间存在透明 cache coherence；host / guest 通过 buffer 生命周期、queue 提交点和 completion 点建立可见性合同。
- 本方向与当前 `Phase 4` 的关系应理解为“消费并推动更克制的 `DMA-ready` 准备项”，而不是直接宣布 cache / DMA / multicore 已进入正式实施阶段。

## 方案

### 结构设计

#### 1. 总体结构

`v1` 的推荐结构如下：

```text
CPU / Host
  -> MMIO registers
  -> submission queue in system RAM
  -> doorbell

AI Accelerator Device
  -> queue manager
  -> graph package parser
  -> dependency-aware graph scheduler
  -> DMA / load-store engine
  -> scratchpad / tile buffer manager
  -> tensor compute engines
     - GEMM / matmul engine
     - conv engine
     - eltwise / pool / reduce engine
  -> completion queue + interrupt
  -> fault / perf counters
```

推荐把设备主体理解成 5 层：

1. **控制层**
   负责 `MMIO` 寄存器、queue 状态、doorbell、interrupt、fault status 和 capability 暴露。
2. **提交与解析层**
   负责从系统 RAM 读取 submission descriptor，解析 graph package、tensor metadata、dependency 与 memory plan。
3. **调度层**
   负责子图内 op 的 ready 判断、tile 次序、DMA / compute overlap，以及 completion 写回。
4. **数据搬运层**
   负责 `system RAM <-> scratchpad` 的 DMA / tiled transfer，不把片上数据复用逻辑散落到 compute engine 里。
5. **计算层**
   负责 `GEMM / conv / eltwise / reduce / pool / layout transform` 这类最小 AI primitive。

#### 2. `v1` 计算模型

`v1` 不做“任意 runtime graph”，只做静态子图执行器。设备接收的是仓库自定义的 `graph package`，它至少包含：

- package header
- tensor table
- constant / weight segment metadata
- op list
- dependency edges
- memory plan
- scratchpad / tile hint
- output write-back policy

`v1` 的最小 op family 建议收口为：

- `tensor_load`
- `tensor_store`
- `tile_move`
- `gemm`
- `conv2d`
- `eltwise`
- `pool_reduce`
- `layout_transform`

这里的关键不是一次性承诺完整算子库，而是先定义一组能同时承接两类 workload 的共同 primitive：

- `CNN` 路径：`conv2d + eltwise(relu) + pool_reduce + gemm`
- `Transformer-like` 路径：`gemm + eltwise + reduce + layout_transform`

`v1` 对 `Transformer-like` 的支持应理解为“最小可证明有效的 matmul-family 推理 block”，而不是完整 attention / softmax / KV-cache 系统。后者可以在后续阶段再单独扩。

#### 3. dtype 合同

`v1` 的设备合同从第一天起统一支持两类 dtype family：

| family | 输入元素 | 累加类型 | 当前设计定位 |
|------|------|------|------|
| quantized | `INT8 / INT16` | `INT32` | `v1` 正式支持 |
| semi-precision | `FP16 / BF16` | `FP32` | `v1` 正式支持 |

为了避免范围失控，`v1` 的完成定义不要求“所有 op × 所有 dtype” 全矩阵齐备，而要求：

- 两个 dtype family 都有统一的 descriptor 编码与 capability 描述
- 两个 family 都至少各有一条代表性的端到端 inference 闭环
- 所有已声明支持的 op / dtype 组合，都有明确的 overflow、rounding、accumulate 和 fault 语义

也就是说，`v1` 的“统一 family”是设备 ABI 的正式合同；实现节奏上允许先用更窄的代表性 workload 把路径接通，再逐步铺开完整矩阵。

#### 4. 内存与数据搬运模型

`v1` 的内存模型必须显式地区分 3 类空间：

1. **system RAM**
   存放 graph package、输入输出 tensor、常量和权重的主副本。
2. **scratchpad / tile buffer**
   设备本地显式管理的片上存储，用于 tile staging、局部重用和 DMA / compute overlap。
3. **accumulator / temporary buffer**
   对更宽累加结果、部分和或 layout transform 的中间态提供隔离空间。

设计上不推荐把 `v1` 简化成“设备直接对系统 RAM 做大跨度随机访问”，原因很明确：

- 这不符合真正 `NPU / TPU-like` 设备的性能建模方式
- 无法形成 tile reuse / DMA overlap 的结构边界
- 后续很难自然扩到更真实的本地存储、权重缓存或多 engine 调度

因此，`v1` 要求 graph package 显式描述搬运与执行边界：

- 哪些 tensor tile 需要装入 scratchpad
- 哪些结果需要保留在本地继续消费
- 哪些输出必须回写系统 RAM
- 哪些 op 之间允许 `DMA + compute` 重叠

### 接口 / 数据 / 契约

#### 1. MMIO 设备合同

`v1` 至少需要以下寄存器窗口：

- `version / capability`
- `control / status / reset`
- `submit_queue_base / size / head / tail`
- `complete_queue_base / size / head / tail`
- `doorbell`
- `irq_status / irq_mask`
- `fault_code / fault_detail`
- `perf_counter_window`

其中 capability 至少要能描述：

- 支持的 dtype family
- 最大 graph package 大小
- 最大 scratchpad 容量
- 最大 tensor rank / tile shape
- 支持的 op mask
- queue 深度

#### 2. submission / completion 合同

submission descriptor 至少包含：

- graph package 地址与大小
- input / output tensor address table
- optional profiling enable
- completion token
- guest / host source tag

completion entry 至少包含：

- completion token
- success / fault code
- retired op count
- bytes moved
- cycles busy / idle
- optional profile summary pointer

推荐的 fault code 至少覆盖：

- `invalid_descriptor`
- `unsupported_dtype`
- `illegal_op`
- `dma_fault`
- `scratchpad_overflow`
- `execution_fault`
- `timeout`

#### 3. graph package 合同

graph package 是 `v1` 的统一软件入口，不管来自 host harness 还是 guest driver，都必须落成相同的静态包格式。建议至少包含：

- package header
- tensor metadata table
- constant / weight segment table
- op descriptor table
- dependency table
- memory plan
- scratchpad budget hint
- optional profiling section

这条边界的目的是：把“图如何表示、如何下发、如何回收结果”从设备执行里剥离出来，避免 host / guest 两侧各自发明一套 ad-hoc 协议。

### 项目基座需要补齐的结构

如果未来真的要实现本设计，当前仓库至少还需要补齐下面这些基座能力：

1. **`DMA-ready` initiator / transaction contract**
   当前 [phase4_preparation_design.md](phase4_preparation_design.md) 已完成 `P4-prep-1` 的 `memory_region` 收口，但还缺更进一步的 initiator / transaction 边界。独立 AI 设备要安全地发起 DMA，需要 `Bus` 明确支持“设备主动访问系统内存”的合同，而不是只假设 CPU 单向访存。
2. **设备可见内存与 buffer 生命周期合同**
   host 和 guest 都需要可被设备消费的 descriptor / tensor buffer。后续需要至少一层 pinned / device-visible buffer 合同，以及 queue 提交点、completion 点与 buffer ownership 的明确规则。
3. **设备异步执行与中断合同**
   现有设备路径更多偏同步或轻量事件；真正的 graph processor 需要更稳定的 queue、doorbell、completion queue、interrupt 与超时 / 错误恢复语义。
4. **数值与量化 golden model**
   当前向量线的 reference 语义主要服务 CPU 指令。独立 AI 设备需要自己的 golden tensor op 模型，覆盖 `INT8 / INT16 / FP16 / BF16`、累加、量化、rounding 与 saturation 规则。
5. **graph package tooling**
   仓库需要一层最小离线工具，把固定 `CNN` 或 `Transformer-like` 推理 block lower 成设备可消费的静态 graph package；这不等于完整编译器，但它是 `v1` 的必要载体。
6. **独立 debug / profile 观测面**
   仅靠现有 CPU / vector snapshot 不足以观察 AI 设备。后续至少需要 queue depth、DMA bytes、scratchpad occupancy、engine utilization、stall reason 和 retired op 计数。
7. **guest 侧最小 driver / runtime 边界**
   如果未来希望这条线不只停留在 host harness，就需要最小 guest driver ABI：buffer、queue、doorbell、interrupt、fault 上报与 profile 读取。

### 验证思路

本方向的验证不能只复用现有 `vector_*_smoke`，而需要形成独立的设备级验证矩阵。

至少应包括：

- golden tensor op 单元测试
- graph package parser / descriptor reject 测试
- `MMIO` queue / doorbell / completion 合同测试
- DMA / scratchpad 边界与 fault 测试
- host-side graph submission smoke
- guest-side driver submission smoke
- 代表性 `CNN` inference smoke
- 代表性 `GEMM / MLP / Transformer-like` inference smoke
- profile counter / fault summary smoke

`v1` 的代表性 workload 建议至少锁定两条：

1. `CNN`：
   `conv2d -> relu -> pool -> fc`
2. `Transformer-like`：
   `matmul -> bias / eltwise -> reduce / layout transform`

这里第二条故意不直接承诺完整 attention；`v1` 更关心的是把 matmul-family inference block、DMA / tile 行为和静态 graph 执行器先跑通。

## 风险与取舍

- 把设备做成 graph processor，长期方向更对，但也最容易范围失控；因此 `v1` 必须死守“静态 shape、静态子图、推理优先”这 3 条边界。
- host harness 与 guest driver 双入口能提升系统价值，但也容易造成两套事实来源；这就是为什么本文档强制要求两者共用同一套 graph package 与 queue ABI。
- 同时覆盖 quantized 与 semi-precision family，会显著扩大验证矩阵；因此 `v1` 必须按“统一 ABI + 代表性闭环”收口，而不是追求所有组合一步到位。
- 引入 `scratchpad + DMA` 会把项目推向更重的 memory contract；这条线必须以更克制的 `DMA-ready` 准备项落地，而不是顺势打开完整 cache / coherence。
- 当前仓库的主线已经切到 `xv6 / Linux` foundation；如果在没有明确优先级切换的情况下把本方向直接拉成当前主线，很容易重新打散当前的验证闭环。

## 当前有效性说明

- 当前有效：本文档作为独立 `MMIO NPU / TPU-like` AI 加速器方向的正式设计来源。
- 当前这条线仍然只是未来候选方向，不是当前已激活主线；当前优先级判断以 [../status/project_priority_roadmap.md](../status/project_priority_roadmap.md) 与 [../status/mainline_status.md](../status/mainline_status.md) 为准。
- 当前对应的专项状态与 wave 1 计划分别见 [../status/npu_tpu_accelerator_status.md](../status/npu_tpu_accelerator_status.md) 与 [../plan/npu_tpu_accelerator_wave1_plan.md](../plan/npu_tpu_accelerator_wave1_plan.md)；后续执行进度应回写到它们，而不是继续堆在本文档里。
