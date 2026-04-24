# `NPU / TPU-like` AI 加速器方向设计

## 文档定位

本文档用于说明未来 `NPU / TPU-like` AI 加速器方向的正式设计边界，重点回答下面 3 个问题：

- 为什么当前仓库不能把现有 `V-lite` 线直接等同于完整 AI 加速器
- 如果走独立设备路线，当前 `v1` 与后续 `v2+` 的结构、接口、执行与内存边界应该如何定义
- 为了让这条线继续可落地，Wave 1 已补齐什么，Wave 2 和更远期还应补哪些结构

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
- 已完成计划：
  - [../plan/history_plan.md#npu-tpu-accelerator-wave1-plan](../plan/history_plan.md#npu-tpu-accelerator-wave1-plan)
  - [../plan/history_plan.md#npu-tpu-accelerator-wave2-plan](../plan/history_plan.md#npu-tpu-accelerator-wave2-plan)

## 背景与问题

当前仓库已经是一个已可运行的模拟器原型。`V-lite` 路线已经完成从最小 ISA、固定 `conv -> relu` guest workload，到最小 vector-aware `pipeline` 的闭环，这条线证明了仓库已经具备承接 ML workload 的基础能力，也为后续的 profile、观察和 `Phase 4` 判断提供了第一批真实信号。

但这条线在设计上仍然应和“CPU 侧向量能力 + 教学式 workload 闭环”区分开。Wave 1 已经补齐真正 AI 加速器依赖的第一批独立设备边界：异步提交、descriptor / queue 合同、显式 `DMA`、片上 `scratchpad / tile buffer`、子图级执行器、host / guest 共用的设备 ABI，以及面向 `CNN` 与 `GEMM / Transformer-like` 推理的代表性算子与 dtype 合同。后续设计重点不再是证明入口能否接通，而是继续细化 timing、overlap、queue overhead 与更真实 workload 证据。

如果继续把这件事理解为“沿 `V-lite` 再补一些向量指令”，很容易把 CPU ISA 扩展、vector-aware `pipeline`、DMA-ready memory contract 和 AI 设备软件栈混成一件事。对当前仓库来说，更健康的做法是把这条线明确建模为一条新的未来方向：独立挂在 `Bus` 上的 `MMIO` AI 加速器设备。CPU 继续负责程序控制、buffer 生命周期和 doorbell / interrupt，而 tensor 执行、数据搬运和片上存储由设备内部统一负责。

## 目标

- 把“独立 `MMIO` AI 加速器设备”定义为与现有 `V-lite` 并行的正式未来方向，而不是继续膨胀 CPU 向量语义。
- 定义 `v1` 的正式边界：推理优先、静态 shape、静态子图、`scratchpad + DMA`、host / guest 双入口、统一 descriptor / queue / completion 合同。
- 统一收口 `v1` 的最小算子族、dtype family、执行流和故障语义。
- 明确 Wave 1 已补齐的项目基座，以及后续仍需深化的 `timed-simple`、overlap、buffer ownership 与 profile 观测面。
- 让这条线可以同时覆盖 `CNN` 与 `GEMM / MLP / Transformer-like` 两类推理 workload，但不把 `v1` 直接扩成完整训练栈或完整图编译器。
- 把动态 shape 作为 `v2+` 的正式目标：设备侧应逐步承担 bounded runtime shape 解析、op ready 判断、tile 调度和 fault 归因，而不是长期依赖 host 为每个 shape 重新离线生成静态包。
- 把训练前向 + 反向作为更远期正式目标：先在推理路径、动态 shape 和 profile 口径稳定后，再逐步引入 backward primitive、activation/gradient buffer 生命周期与训练专用 fault / profile 语义。

## 非目标

- 不把本方向定义成 `GPU / SIMT` 子系统；`warp`、thread block、shared memory 和图形/通用并行执行模型不在本文档范围内。
- 不把本方向定义成完整 `RVV` 或 CPU 紧耦合 tensor 指令扩展。
- 不在 Wave 2 中承诺完整动态 shape runtime；Wave 2 只允许先落 bounded dynamic shape 的最小合同和代表性闭环。
- 不在 Wave 2 中实现训练、反向传播、optimizer 或梯度同步；训练前向 + 反向是 `v2+ / v3` 方向，必须在后续计划中单独拆分。
- 不在 `v1` 中承诺完整深度学习编译器、完整 kernel library 或 Linux 驱动完成态。
- 不把本文档理解成当前主线的即时实施指令；它只是未来候选方向的正式设计来源。

## 约束与边界

- 设备接入方式固定为：独立 `MMIO` 设备挂在 `Bus` 上，沿当前 `Machine -> Bus -> Device` 边界接入。
- CPU reference path 与 `InstructionSemantics` 不承担 AI 设备语义来源；这条线必须保持为独立设备合同。
- host harness 与 guest driver 必须共用同一套设备 ABI：相同的 `descriptor / queue / doorbell / completion / fault` 语义，不允许分叉成两套设备行为。
- Wave 1 / `v1` 只支持静态 shape、离线准备好的静态子图；设备运行时不做图级动态重写。
- `v2+` 的动态 shape 必须是 bounded dynamic shape：graph package 先声明最大 rank / max dims / scratchpad 上限，submission 或 runtime shape table 再给出本次实际 dims；设备只在边界内做运行时调度，超界必须 fail-closed。
- 动态 shape 的设备侧职责应包括：读取 runtime shape、校验 tensor byte size、选择静态 op 依赖内的 tile 次序、更新 per-op profile，并在 shape / memory plan 不一致时给出稳定 fault。
- `v1` 采用显式 `scratchpad / tile buffer + DMA/load-store engine`，而不是把所有 tensor 访问退化成普通 RAM 连续 load/store。
- `v1` 默认不假设 CPU 与设备之间存在透明 cache coherence；host / guest 通过 buffer 生命周期、queue 提交点和 completion 点建立可见性合同。
- 本方向与当前 `Phase 4` 的关系应理解为“消费并推动更克制的 `DMA-ready` 准备项”，而不是直接宣布 cache / DMA / multicore 已进入正式实施阶段。
- 本方向的性能口径固定以模拟器内部的 `simulated cycles` 为准；宿主机 wall-clock 只用于开发调试，不作为“AI 是否被加速”的正式判断依据。

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

#### 2.1. 动态 shape 演进目标

动态 shape 已经被提升为正式后续目标，但它不应被理解为“设备马上支持任意动态图”。当前推荐的演进顺序是：

1. **bounded dynamic shape**
   - graph package 声明最大 rank / max dims / tensor role / scratchpad budget
   - submission 提供 runtime shape table
   - 设备在 max bounds 内计算本次 tensor byte size 与 tile 次序
2. **动态 batch / 动态 sequence length**
   - 优先服务 `GEMM / MLP / Transformer-like` 的 batch 或 sequence 变化
   - 不在第一刀处理任意 rank 变化或动态 op list
3. **设备侧 runtime 调度**
   - 调度器根据 runtime dims 做 ready 判断、tile 次序和 per-op profile
   - host 仍负责离线 graph lowering，但不再为每个 shape 生成不同静态 graph package
4. **更通用动态图**
   - 只有在 bounded dynamic shape 和 profile 证据稳定后再评估
   - 不把它塞进 Wave 2 的完成定义

这条边界的关键是：动态 shape 是正式目标，但必须先被压成可验证、可 fail-closed 的受限合同。

#### 2.2. 训练前向 + 反向远期目标

当前主场景仍是推理优先；训练支持是更远期方向。正式目标应拆成 3 层，而不是一次性进入完整训练栈：

1. **训练前向**
   - 复用推理 compute path，但需要保留 activation / intermediate buffer
   - profile 需要区分 inference forward 与 training forward
2. **训练反向**
   - 增加 backward primitive，例如 `gemm_backward`、`conv2d_backward_input`、`conv2d_backward_weight`、`activation_backward`
   - graph package 需要表达 gradient tensor role、activation save policy 与 accumulator buffer 生命周期
3. **训练 step**
   - optimizer、梯度同步、mixed precision loss scaling 和 checkpoint 不进入早期完成定义
   - 如果未来需要，必须另开训练专项设计 / 计划

因此，训练“要作为未来目标保留”，但 Wave 2 不直接实现反向传播；Wave 2 最多为 profile、tensor role 和 buffer ownership 留出不破坏推理 ABI 的演进余地。

#### 3. dtype 合同

`v1` 的设备合同从第一天起统一支持两类 dtype family：

| family | 输入元素 | 累加类型 | 当前设计定位 |
|------|------|------|------|
| quantized | `INT8 / INT16` | `INT32` | `v1` 正式支持 |
| semi-precision | `FP16 / BF16` | `FP32` | `v1` 正式支持 |
| low-bit quantized | `INT4` | `INT32` | `v2+` 候选，需先稳定 INT8 / tile profile |

为了避免范围失控，`v1` 的完成定义不要求“所有 op × 所有 dtype” 全矩阵齐备，而要求：

- 两个 dtype family 都有统一的 descriptor 编码与 capability 描述
- 两个 family 都至少各有一条代表性的端到端 inference 闭环
- 所有已声明支持的 op / dtype 组合，都有明确的 overflow、rounding、accumulate 和 fault 语义

也就是说，`v1` 的“统一 family”是设备 ABI 的正式合同；实现节奏上允许先用更窄的代表性 workload 把路径接通，再逐步铺开完整矩阵。

`F/D` CPU 浮点扩展不作为 AI accelerator 的硬前置。设备内 `FP16 / BF16 -> FP32` 是独立设备语义；CPU `F/D` 更适合作为 ISA correctness、Linux 用户态与 CPU-only baseline 的单独路线推进，不能为了 AI accelerator 直接绕过 `InstructionSemantics` 的单一真值来源。

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

#### 5. 时序与性能建模

这条线如果未来要被表述为“AI accelerator”，就不能只停留在功能正确。当前仓库是模拟器，宿主机 wall-clock 受到编译选项、机器负载和实现细节影响，本身没有稳定性能含义；因此 `v1` 必须补一层设备内部的模拟时序模型，并把性能判断统一收口到 `simulated cycles`。

`v1` 不追求 `RTL` 级精确时序，更合适的目标是一个可解释、可回归、可调参的 `timed-simple` 模型。它至少应把一次提交的端到端开销拆成：

- queue / control 开销
- `DMA load` 开销
- compute 开销
- `DMA store` 开销
- completion / interrupt 开销

代表性的参数化模型建议如下：

- `DMA cycles = setup_cycles + ceil(bytes / dma_bytes_per_cycle)`
- `compute cycles = ceil(retired_ops / effective_ops_per_cycle)`，其中 `retired_ops` 对 `GEMM / conv` 可进一步落成 `MAC` 或 tile 粒度公式
- 如果设计允许 `DMA + compute` 重叠，应明确 overlap 规则；不能重叠的部分统一计入 `stall / wait`

`v1` 需要从第一天起暴露一组和时序绑定的 profile 计数器，至少包括：

- `device_cycles`
- `busy_cycles`
- `dma_cycles`
- `compute_cycles`
- `stall_cycles`
- `bytes_moved`
- `retired_ops`
- `effective_ops_per_cycle`
- `utilization`

这组计数器的目的不是伪装成真实芯片频率，而是让仓库能够在统一模拟时钟下比较不同路径的结构收益。后续若要声明“AI 工作被加速”，正式比较应是：

- `cpu_or_vector_baseline_sim_cycles / ai_accelerator_end_to_end_sim_cycles`

也就是说，这条线的性能结论依赖模拟周期模型，而不是宿主机运行秒数。

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
- 是否支持 bounded dynamic shape
- 是否支持 training-forward / backward family
- 支持的 op mask
- queue 深度

#### 2. submission / completion 合同

submission descriptor 至少包含：

- graph package 地址与大小
- input / output tensor address table
- optional profiling enable
- completion token
- guest / host source tag
- optional runtime shape table 地址与大小（`v2+`）
- optional training mode / profile mode 标志（远期）

completion entry 至少包含：

- completion token
- success / fault code
- retired op count
- bytes moved
- simulated cycles busy / idle
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
- optional max-shape / dynamic-shape section
- optional tensor role 扩展，例如 activation、gradient、saved intermediate
- optional profiling section

这条边界的目的是：把“图如何表示、如何下发、如何回收结果”从设备执行里剥离出来，避免 host / guest 两侧各自发明一套 ad-hoc 协议。

### 项目基座已补齐与仍需深化的结构

Wave 1 已经把本设计从纯方案推进到可执行 foundation。当前有效口径应区分“已接通的最小基座”和“后续仍需深化的结构”，避免把已落地能力误读成待办：

1. **`DMA-ready` initiator / transaction contract**
   已落地最小 `dma_transaction` 与 `Bus::dma_read()/dma_write()`，AI accelerator 可作为设备 initiator 主动访问 system RAM；当前仍只覆盖保守 fail-closed 语义，不代表完整 DMA 子系统、cache coherence 或多设备 DMA 已完成。
2. **设备可见内存与 buffer 生命周期合同**
   host profile 与 guest demo 已能通过 descriptor / tensor buffer / completion 点建立最小可见性合同；后续如果进入更真实 runtime，仍需要更清晰的 pinned buffer、ownership transfer 与多 outstanding submission 语义。
3. **设备异步执行与中断合同**
   `AiAccelerator` 已具备 queue、doorbell、completion queue、PLIC interrupt 与 fault completion；当前仍是单设备、保守队列语义，timeout、queue overhead attribution 和更复杂错误恢复仍未展开。
4. **数值与量化 golden model**
   已新增独立 tensor golden ops，覆盖代表性 `INT8 / INT16 -> INT32` 与 `FP16 / BF16 -> FP32` 路径；后续不是追求一次性全矩阵铺满，而是按新增 op / dtype 明确 rounding、overflow 与 fault 语义。
5. **graph package tooling**
   `workloads/ai_proto/pack_graph.py` 已能生成固定 `CNN` 与 `GEMM / matmul-family` workload 的 graph package、tensor 输入、预期输出与 manifest；它仍是最小离线工具，不是完整编译器或动态图 runtime。
6. **独立 debug / profile 观测面**
   debug snapshot / CLI 与 host profile 已能暴露 queue depth、DMA bytes、scratchpad occupancy、engine busy、retired ops 和 `device / dma / compute / stall` cycles；后续仍需更细的 compute / stall attribution、queue/completion overhead 与 utilization 口径。
7. **模拟时钟 / timing model 合同**
   当前已有第一版 `timed-simple simulated cycles`，正式性能表述不再使用宿主机 wall-clock；但 `DMA + compute` 仍固定 no-overlap，不能据此讨论 tile overlap 或隐藏 queue 开销。
8. **guest 侧最小 driver / runtime 边界**
   已新增 guest `ai_accel` driver 与 `ai_accel_demo`，并补上最小 queue helper；后续如要扩大到 OS driver 或 Linux-facing 设备栈，应另开计划，不把 demo helper 直接膨胀成通用 runtime。
9. **动态 shape 与训练演进边界**
   动态 shape 已经是正式目标，但当前只适合先做 bounded dynamic shape 的最小合同与代表性 `GEMM / FC` 闭环；训练前向 + 反向是更远期目标，当前仅在 ABI / graph package / profile 设计中保留可演进位置。

### 后续算子与 workload 取舍

从 Claude Code 建议中可以采纳的方向，需要按项目当前边界重新排序：

- `Softmax`
  - 如果后续进入 `attention`，它比 `GELU / Sigmoid` 更优先
  - 需要先定义数值稳定、近似策略和 dtype 组合，不应直接作为 Wave 2 第一刀
- `attention`
  - 长期有价值，但第一步应是小规模静态 self-attention block
  - 依赖 `softmax`、动态 sequence length、per-op profile 与更稳定 memory plan
- `INT4`
  - 适合放在 INT8 路径和 tile/profile 稳定之后
  - 需要明确 packing、sign-extension、scale/zero-point 与 fault 语义
- `GELU / Sigmoid`
  - 可作为 activation family 扩展，但短期优先级低于 `relu / softmax`
  - 更适合在 tiny Transformer / MLP workload 明确后补
- `MobileNet`
  - 不建议直接做完整模型
  - 可以未来只取 depthwise / pointwise conv 的前几层作为 workload corpus
- `LeNet / tiny MLP`
  - 比 MobileNet 更适合作为下一批端到端教学 workload
  - 可先在 host `ai_proto` profile 中落地，再考虑 guest / frontend

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
- bounded dynamic shape reject / positive smoke
- training mode fail-closed / future ABI smoke（在真正实现训练前先守住不误执行）

`v1` 的代表性 workload 建议至少锁定两条：

1. `CNN`：
   `conv2d -> relu -> pool -> fc`
2. `Transformer-like`：
   `matmul -> bias / eltwise -> reduce / layout transform`
3. bounded dynamic shape：
   `runtime batch` 或 `runtime M/N/K` 变化的最小 `GEMM / FC`

这里第二条故意不直接承诺完整 attention；`v1` 更关心的是把 matmul-family inference block、DMA / tile 行为和静态 graph 执行器先跑通。

## 风险与取舍

- 把设备做成 graph processor，长期方向更对，但也最容易范围失控；因此 `v1` 必须死守“静态 shape、静态子图、推理优先”这 3 条边界。
- 动态 shape 已经是正式目标，但如果第一刀就做任意动态图，会直接放大 graph parser、memory plan、scratchpad 与调度状态空间；因此必须先从 bounded dynamic shape 开始。
- 训练前向 + 反向是合理远期目标，但它会引入 activation 保存、gradient buffer、反向算子、数值稳定和更大验证矩阵；在推理 profile 与动态 shape 稳定前，不应进入代码实现主线。
- host harness 与 guest driver 双入口能提升系统价值，但也容易造成两套事实来源；这就是为什么本文档强制要求两者共用同一套 graph package 与 queue ABI。
- 同时覆盖 quantized 与 semi-precision family，会显著扩大验证矩阵；因此 `v1` 必须按“统一 ABI + 代表性闭环”收口，而不是追求所有组合一步到位。
- 引入 `scratchpad + DMA` 会把项目推向更重的 memory contract；这条线必须以更克制的 `DMA-ready` 准备项落地，而不是顺势打开完整 cache / coherence。
- 当前已有第一版独立模拟时钟与性能模型，但仍停在保守 `timed-simple`；在没有更细 attribution 或 overlap 建模前，不能回答 tile reuse、DMA overlap、queue 开销隐藏和 compute 吞吐是否真的形成结构收益。
- 当前仓库的主线已经切到 `xv6 / Linux` foundation；如果在没有明确优先级切换的情况下把本方向直接拉成当前主线，很容易重新打散当前的验证闭环。

## 当前有效性说明

- 当前有效：本文档作为独立 `MMIO NPU / TPU-like` AI 加速器方向的正式设计来源。
- 当前这条线仍然只是未来候选方向，不是当前已激活主线；当前优先级判断以 [../status/project_priority_roadmap.md](../status/project_priority_roadmap.md) 与 [../status/mainline_status.md](../status/mainline_status.md) 为准。
- 当前对应的专项状态见 [../status/npu_tpu_accelerator_status.md](../status/npu_tpu_accelerator_status.md)，Wave 1 完成态已归档到 [../plan/history_plan.md#npu-tpu-accelerator-wave1-plan](../plan/history_plan.md#npu-tpu-accelerator-wave1-plan)；后续执行进度应回写到状态文档和新的活跃计划，而不是继续堆在本文档里。
