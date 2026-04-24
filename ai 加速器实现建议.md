

### 第一层：必须的基础设施（硬前置）

| 缺口 | 为什么需要 | 现有基础 |
| :--- | :--- | :--- |
| F/D 浮点扩展 | AI workload 的核心数据类型是浮点；没有 FP16/FP32 连推理都跑不起来 | 当前 ISA 只有 RV64IM，完全没有浮点 |
| DMA 引擎 | 加速器需要独立搬数据，不能靠 CPU 逐字节搬运 | 当前 Bus 只有 CPU 主动读写，没有 bus mastering / DMA |
| 中断驱动的异步完成通知 | 加速器完成计算后需要通知 CPU | PLIC 已有，但需要新增 source wiring |
| 命令队列 / 描述符机制 | CPU 向加速器提交任务的标准接口 | VirtIO 的 virtqueue 已经是一个现成的描述符环模型，可以复用或参考 |

---

### 第二层：加速器核心模块（新建）

| 模块 | 说明 |
| :--- | :--- |
| Scratchpad SRAM | 加速器私有的高带宽存储（几 KB ~ 几十 KB），存放当前 tile 的权重和激活值 |
| MAC 阵列 / 矩阵引擎 | 核心计算单元，支持 GEMM / Conv2D，可配置为 NxN systolic 或向量 MAC |
| 数据类型引擎 | FP16/BF16/INT8/INT4 的混合精度运算 + 量化/反量化 |
| Activation 单元 | 硬件 ReLU/GELU/Sigmoid，紧跟在 MAC 后做 fused 激活 |
| Tile 控制器 | 自动把大矩阵拆成 scratchpad 能容纳的 tile，驱动 DMA 搬运 + 计算流水 |
| 命令处理器 | 从命令队列中取描述符、解析、调度到计算单元 |

---

### 第三层：软件栈与验证

| 项目 | 说明 |
| :--- | :--- |
| 驱动层 | guest 侧的加速器驱动：初始化、提交命令、等待完成 |
| 算子库 | GEMM / Conv2D / Pooling / Softmax 等基础算子的 tile 策略 |
| 模型编排 | 能串联多个算子跑一个完整小模型（如 LeNet / 小 MLP） |
| 可视化 | 在前端展示加速器状态：scratchpad 使用、MAC 利用率、DMA 传输 |
| 差分验证 | 加速器结果与纯软件 reference 逐 bit 对比 |

一个最小可行的切片建议
  如果要启动，我建议的最小第一刀是：

  Phase 1: 最小 MMIO 加速器原型
  ├── 在 Bus 上新增一个 NpuDevice (MMIO mapped)
  ├── 4 个 MMIO 寄存器：CMD, STATUS, SRC_ADDR, DST_ADDR
  ├── 支持 1 个操作：8x8 INT8 矩阵乘
  ├── 内部有一个 64-byte scratchpad
  ├── CPU 写 CMD 触发计算，轮询 STATUS 等完成
  ├── functional 后端先跑通 reference 语义
  └── 一个 host smoke test 验证正确性

  Phase 2: DMA + 中断 + 更大矩阵
  ├── 加 DMA：加速器自己从 RAM 搬数据到 scratchpad
  ├── 加 PLIC 中断：完成后通知 CPU，不用轮询
  ├── 支持 tiled GEMM：任意大小矩阵自动分块
  └── 补 FP16 数据类型

  Phase 3: 完整 CNN pipeline
  ├── Conv2D + ReLU + Pooling + FC 全硬件化
  ├── 算子可串联（fused execution）
  ├── guest 驱动 + 模型编排
  └── 前端可视化


  我把图片里的内容转换成了标准 Markdown 格式，完整保留了层级、表格和细节：

---

最终形态：一个可观察、可教学、可运行真实模型的 RISC-V + AI 加速器 SoC 模拟器

### 1. 能跑什么

| 层级         | 能力                                                         | 具体示例                                                     |
| ------------ | ------------------------------------------------------------ | ------------------------------------------------------------ |
| 基础算子     | 硬件加速的 GEMM / Conv2D / Depthwise Conv / Pooling / FC / Softmax / 激活函数 | 单层 Conv2D 在加速器上 < 1ms 模拟时间完成                   |
| 经典小模型   | 完整推理，端到端从加载权重到输出结果                         | LeNet-5 手写数字识别、小 MLP 分类器、MobileNet-v1 的前几层    |
| 量化推理     | INT8 / INT4 量化模型的完整推理链路                           | 用 PyTorch 导出量化模型 → 转成加速器可消费的权重格式 → 在模拟器里跑推理 |
| 注意力机制   | 矩阵乘 + Softmax + 矩阵乘 的 fused pipeline                  | 小规模 Transformer 的单层 self-attention（如 seq_len=64，d_model=128） |
| 多算子串联   | 加速器内部 fused execution，减少 CPU-加速器往返              | Conv → BatchNorm → ReLU → Pool 一次提交、一次完成       |

一句话概括： 你的模拟器将能从 guest OS (xv6) 里加载一个训练好的小型神经网络，通过驱动程序提交给硬件加速器，完成完整推理，并输出分类结果——整个过程对 CPU来说就像调用一个块设备一样自然。

我先帮你把图里的信息整理成 Markdown 格式，并对架构做简要说明：

---

### 2. 架构上达到什么水平

**最终的 SoC 架构图（文字版说明）：**

```text
┌─────────────────────────────────────────────────────────────────────────┐
│                              RISC-V SoC                                 │
│                                                                         │
│  ┌──────────────────┐                    ┌─────────────────────────┐    │
│  │ RV64IM CPU       │                    │ UART                   │    │
│  │ +V-lite          │◄────System Bus────►│ CLINT                   │    │
│  │ +F/D             │      (MMIO)        │ PLIC                   │    │
│  └──────────────────┘                    │ VirtIO-blk             │    │
│         │                                │ NPU Device (MMIO)       │    │
│         │                                └─────────────────────────┘    │
│         │ DMA                                                              │
│         │                        ┌───────────────────────────────┐       │
│         └───────────────────────►│       AI Accelerator          │       │
│                                  │ ┌─────────────────────────┐   │       │
│                                  │ │ Cmd Queue               │   │       │
│                                  │ │ MAC Array (NxN)         │   │       │
│                                  │ │ Scratchpad SRAM         │   │       │
│                                  │ │ Activation Unit         │   │       │
│                                  │ └─────────────────────────┘   │       │
│                                  └───────────────────────────────┘       │
│         ↓                                                                 │
│  ┌──────────────────┐                                                    │
│  │   RAM (DDR)      │                                                    │
│  └──────────────────┘                                                    │
└─────────────────────────────────────────────────────────────────────────┘
```
架构核心模块解读
1.  **核心 CPU**：`RV64IM +V-lite +F/D`，即基础整数指令集加上浮点（F/D）和精简向量扩展（V-lite），为 AI 计算提供基础数据处理能力。
2.  **外设总线**：通过 `System Bus (MMIO)` 挂载标准外设（UART/CLINT/PLIC/VirtIO-blk）和 `NPU Device`，CPU 可以通过内存映射 I/O 控制 AI 加速器。
3.  **AI 加速器**：
    - `Cmd Queue`：接收 CPU 下发的任务描述符
    - `MAC Array (NxN)`：矩阵乘核心计算单元
    - `Scratchpad SRAM`：本地高速缓存，减少对 DDR 的访问延迟
    - `Activation Unit`：集成激活函数等后处理操作
4.  **数据通路**：`DMA` 负责在 CPU、AI 加速器和 `RAM (DDR)` 之间搬运数据，避免 CPU 逐字节搬运的瓶颈。

这个形态对标的是真实芯片中的：

| 你的模拟器        | 对标真实硬件                          |
| ----------------- | ------------------------------------- |
| NPU Device (MMIO) | Google Edge TPU / Apple ANE / 寒武纪 MLU |
| MAC Array         | TPU 的 systolic array / GPU 的 tensor core |
| Scratchpad        | TPU 的 unified buffer / NPU 的片上 SRAM |
| DMA Engine        | 真实 SoC 中的 DMAC                    |
| Cmd Queue         | PCIe command ring / 加速器 mailbox   |
| CPU + NPU 协作    | 手机 SoC 里 CPU 调度 NPU 的真实工作模式 |

---

### 3. 可观察性与教学价值
这是你的项目相对于 QEMU/Spike 的核心差异化优势——不只是能跑，而是能看见里面发生了什么：

| 可视化维度      | 展示内容                                                         |
| --------------- | ---------------------------------------------------------------- |
| 数据流          | 权重/激活值从 RAM → DMA → Scratchpad → MAC → 输出的完整路径动画 |
| MAC 利用率      | NxN 阵列中每个 MAC 单元每周期的忙/闲状态热力图                   |
| Tiling 策略     | 大矩阵如何被拆分成 scratchpad 能容纳的小块，每块的计算顺序      |
| 精度对比        | FP32 vs FP16 vs INT8 同一模型的输出差异可视化                    |
| CPU-NPU 交互    | 命令提交 → 等待 → 中断返回的时序图                               |
| Pipeline 气泡   | DMA 搬运和计算重叠度，暴露带宽瓶颈                               |
| 能效估算        | 基于 MAC 操作数和内存访问次数的粗粒度能效比较                   |

教学场景举例：
  ▎ "为什么 TPU 要用 systolic array？"
  ▎ → 在模拟器中切换 NxN 大小，直接观察 MAC 利用率的变化

  ▎ "为什么量化很重要？"
  ▎ → 同一个模型，FP32 vs INT8，观察 scratchpad 能装下的 tile 大小差 4 倍

  ▎ "为什么需要 DMA？"
  ▎ → 关闭 DMA 用 CPU 搬数据，观察加速器 95% 时间在等数据

---
我已经把这张图片的内容转换成了标准 Markdown 格式，完整保留了表格结构：

---

### 4. 项目定位的变化

| 维度           | 现在                               | 做完之后                                                                 |
| -------------- | ---------------------------------- | ------------------------------------------------------------------------ |
| 项目性质       | RISC-V CPU 模拟器 + 教学原型        | **RISC-V AI SoC 模拟器**                                                 |
| 竞品对比       | 简化版 QEMU / Spike                | 没有直接竞品——开源社区里没有"可视化 + 可观察的 RISC-V AI 加速器模拟器"   |
| 论文/展示价值  | "我做了一个 CPU 模拟器"            | "我做了一个完整的异构计算平台，能直观展示 AI 推理在硬件上的执行过程"     |
| 覆盖面         | 计算机体系结构课程                 | 体系结构 + AI 芯片设计 + 异构计算 + 编译器/runtime                       |
| 扩展空间       | 更多 ISA / OS                      | 多加速器调度、加速器间互联、编译器 backend                               |

---

### 5. 一个端到端 demo 的样子

  最终做完后，一个典型的使用场景是：

   1. 编译 guest 程序（含 NPU 驱动 + LeNet 模型）
  riscv64-unknown-elf-gcc -o lenet_demo lenet_demo.c npu_driver.c

   2. 启动模拟器，加载 xv6 + demo 程序
  ./mycpu --board mycpu_virt --workload xv6 --npu-config 8x8-int8

   3. 在 xv6 shell 里运行
  $ lenet_demo mnist_test_image.bin
  Loading weights to NPU scratchpad... done (DMA: 2.3KB, 45 cycles)
  Running inference...
    Conv1: 28x28x1 -> 24x24x6  [NPU: 1200 cycles, MAC util: 87%]
    Pool1: 24x24x6 -> 12x12x6  [NPU: 180 cycles]
    Conv2: 12x12x6 -> 8x8x16   [NPU: 2400 cycles, MAC util: 91%]
    Pool2: 8x8x16  -> 4x4x16   [NPU: 120 cycles]
    FC1:   256 -> 120           [NPU: 960 cycles, MAC util: 78%]
    FC2:   120 -> 84            [NPU: 320 cycles]
    FC3:   84  -> 10            [NPU: 40 cycles]
  Result: digit = 7 (confidence: 0.97)
  Total NPU cycles: 5220  |  Total CPU cycles: 890
  Speedup vs CPU-only: ~23x

   4. 浏览器前端实时展示每一层的数据流、MAC 热力图和 tiling 策略

---

6. 现实预期

  需要诚实地说：

  - 模拟速度 不会快——这是 cycle-level 模拟器，不是真硬件，跑 LeNet
  可能需要几秒到几十秒的主机时间。但目标本来就不是性能，而是可观察性。
  - 模型规模 上限大约在 LeNet / 小 MLP / MobileNet 前几层这个量级。跑 ResNet-50 或 LLM 不现实，也不是目标。
  - 与真实 NPU 的差距 在于没有真实的时序模型（布线延迟、SRAM bank conflict 等），但功能语义和架构抽象是对齐的。

  核心价值不在于跑多快或跑多大，而在于：让人能看见 AI 推理在硬件上到底是怎么一步步发生的。 这在开源社区里目前没有对标项目。