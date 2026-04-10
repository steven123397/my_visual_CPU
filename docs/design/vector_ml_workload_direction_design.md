# 向量扩展与 ML workload 演进设计

## 文档定位

本文档用于说明：在当前仓库已经形成 `functional` reference path、`pipeline` backend、最小 guest runtime 与 `kernel_alpha` bring-up 基线之后，为什么把“向量扩展 + ML workload”列为一个可长期演进的主线方向，以及这条路线在未来应按什么边界和顺序推进。

它重点回答：

- 这条线为什么值得作为长期主线候选，而不是短期 demo
- 第一代应该做什么，不做什么
- 它与当前 `Phase 2 / 3 / 4` 的关系是什么
- 当它与更重的 `Phase 4` 工作相冲突时，应先推进哪一边

本文档不承担实时进度更新。当前主线状态、当前优先级和是否已经进入实施阶段，以对应 `status` 文档为准。

## 关联文档

- 状态文档：
  - [status/mainline_status.md](../status/mainline_status.md)
  - [status/project_priority_roadmap.md](../status/project_priority_roadmap.md)
- 已完成计划归档：
  - [../plan/history_plan.md#vector-v0-v1-plan](../plan/history_plan.md#vector-v0-v1-plan)
  - [../plan/history_plan.md#vector-v2-plan](../plan/history_plan.md#vector-v2-plan)
  - [../plan/history_plan.md#vector-v3-plan](../plan/history_plan.md#vector-v3-plan)
- 相关细化设计：
  - [vector_vlite_v0_v1_design.md](vector_vlite_v0_v1_design.md)
  - [vector_v2_operator_guest_design.md](vector_v2_operator_guest_design.md)
  - [vector_v3_minimal_cnn_guest_design.md](vector_v3_minimal_cnn_guest_design.md)
  - [vector_v4_minimal_vector_pipeline_design.md](vector_v4_minimal_vector_pipeline_design.md)

## 背景与问题

当前仓库已经是一个已可运行的模拟器原型：`functional` 负责 reference 真值，`pipeline` 负责执行模型实验，guest runtime 已能支撑最小 supervisor / kernel bring-up，`debug/frontend` 也已形成一条教学演示闭环。当前主线的重点仍然是 reference correctness、bug-driven hardening，以及把已经接入的 `pipeline` / debug / guest 能力维持在可验证范围内。

在这个基础上，后续如果要再开启一条有长期结构收益的新主线，继续直接放大更重的 `Phase 4`（cache hierarchy、DMA、multicore、一致性）并不是最健康的起点。原因不是这些方向没有价值，而是：在当前仍缺少向量化 workload、缺少可用于判断带宽 / 局部性 / 数据并行收益的真实负载之前，过早扩大 `Phase 4` 的平台状态空间，信号往往不足，验证成本却会明显上升。

相比之下，把“向量扩展 + ML workload”定义为长期候选主线更符合当前仓库的结构优势：它既能继续沿统一 ISA 语义来源、reference-first、可测试推进的现有方法论演进，又能用 CNN / GEMM / Conv 这类真实 workload 为后续是否值得重开更深的 `pipeline` 或 `Phase 4` 提供更直接的证据。

## 目标

- 把“向量扩展 + ML workload”定义为一个长期可演进的主线方向，而不是一次性的 CNN demo。
- 采用 workload-guided、ISA-first 的推进方式：由真实 workload 约束设计，但实现顺序仍以 reference path 和共享语义层为先。
- 第一代优先支持最小整数向量能力，并围绕 ML kernel 所需的核心算子逐步接线。
- 明确这条线与 `Phase 2 / 3 / 4` 的衔接关系，避免未来把向量扩展、cache、DMA、multicore 等多个增长点混在同一轮实现里。
- 为后续“向量语义 → 算子验证 → 最小 CNN demo → vector-aware pipeline → workload-driven memory hierarchy”提供清晰顺序。

## 非目标

- 不把这条路线定义成 GPU、SIMT 或图形渲染方向。
- 不在第一代就追求完整 RVV、完整浮点向量、完整 ML runtime 或通用深度学习框架。
- 不在向量 reference path 尚未形成前，先抢跑更重的 `Phase 4` cache / DMA / multicore / coherence。
- 不为了尽快跑通一个网络 demo，临时堆叠 ad-hoc ISA、guest API 或 pipeline 特判。
- 不让 `pipeline` 先于 `functional` 成为向量语义的事实来源。

## 约束与边界

- 所有向量语义都必须优先落在共享 ISA 语义层与 `functional` reference path，不得先在 `pipeline` 中另起一套解释。
- 第一代应优先采用“最小可长期扩展”的 `V-lite` 思路，而不是直接引入完整 RVV 兼容面。
- 第一代 workload 采用“先 kernel、后 network”的推进方式：先验证算子与数据路径，再拼最小 CNN。
- 第一代数据路线优先考虑整数 / 定点（例如 `int8` 输入与权重、`int32` 累加）；浮点和更完整的 ML 能力留待后续明确收益后再扩。
- 这条线在进入真实实施前，仍然服从当前主线的 bug-driven hardening、reference correctness 和已有 guest/debug 门禁，不得反客为主。

## 方案

### 总体路线定位

这条线的定位不是“新产品面”，而是“后续长期主线候选之一”。它的第一完成态，不是高性能 AI 平台，也不是完整 GPU 模拟器，而是：

- 一个带有最小向量扩展的 RISC-V 平台级模拟器
- 一个能稳定运行 ML kernel 与最小 CNN 推理闭环的可验证环境
- 一个后续可继续外推到 vector-aware pipeline、cache / DMA、乃至更系统 workload 研究的平台

因此，这条线本质上是从“标量 RISC-V 模拟器”往“可支撑向量计算与小型 ML workload 的平台级模拟器”演进。

### 第一代 workload 北极星

第一代不要把目标直接写成“跑一个完整 CNN 模型”，而应先拆成更窄的工作负载阶梯：

1. `dot-product / MAC`
2. `small GEMM`
3. `Conv2D`
4. `ReLU / MaxPool`
5. `conv -> relu -> pool -> fc` 这类最小网络链路

也就是说，先把 CNN 理解成一组会反复出现的核心算子，而不是一套重量级模型框架。这样做有两个好处：

- 更容易把问题锁定在 ISA、访存和执行模型上，而不是被模型格式、预处理、文件加载等外围问题带偏。
- 后续无论是继续留在 CNN，还是外推到更广的线性代数 / DSP / ML workload，这组 kernel 都仍然有长期价值。

### 第一代向量扩展边界

第一代建议采用最小 `V-lite` 路线，能力只覆盖 ML kernel 必需子集：

- 一组独立的向量寄存器状态
- 最小的向量长度 / 元素宽度配置能力
- 连续向量 `load/store`
- 元素级整数 `add / mul / max`
- widening accumulate、dot-product 或等价的 MAC 支撑
- 与最小 kernel 验证相匹配的 trap / 非法编码 / 边界回归

第一代明确不做：

- gather / scatter
- mask / predication 全能力面
- segment load/store
- 完整浮点向量
- 追求与完整 RVV 一次性对齐的广泛编码面

这样做的取舍是：先用最小有结构收益的子集站稳 reference path 和 workload 验证，再决定后续是否向更 RVV-like 的方向收敛。

### 建议推进顺序

#### V0：设计冻结与 workload 收窄

先完成设计层收口，明确：

- 第一代只做 `V-lite`
- 第一代只做整数 / 定点主线
- 第一代 workload 只围绕 kernel 阶梯与最小 CNN 闭环

这一阶段的重点不是写代码，而是避免未来把“向量 ISA”“CNN demo”“pipeline 扩展”“Phase 4”混成同一轮大任务。

#### V1：`functional` reference path 接入向量语义

第一轮实现只动 shared semantics 与 `functional` reference path，包括：

- 向量状态定义
- 最小配置与非法编码边界
- 向量访存和核心算子语义
- host-side smoke / differential / regression

只有当这条线在 `functional` 上站稳，后续 guest、CNN kernel 和 `pipeline` 路线才有可靠真值来源。

#### V2：kernel 级验证与最小 guest 程序

在 `functional` 稳定后，`V2` 首刀先做算子级验证，再做最小 guest 程序：

- `dot / MAC`
- `small GEMM`
- `Conv`
- `ReLU`

`Pool` 在当前 `V-lite` 子集里缺少自然、稳定的表达方式，因此明确延后到后续更合适的原语设计阶段；`V2` 首刀不把它塞进来。

这一阶段仍应优先把问题留在算子与 guest 最小闭环里，不要马上扩大到更完整 runtime。

#### V3：最小 CNN inference demo

只有当 kernel 级链路足够稳定后，再把这些算子拼成一个极小 CNN：

- 固定输入
- 固定权重
- 固定输出或 checksum

第一代 CNN demo 的意义是验证“向量 ISA + guest runtime + workload”闭环，而不是追求复杂模型能力。

#### V4：vector-aware pipeline

在 reference path、kernel 和最小网络都稳定后，再评估 `pipeline` 是否要跟进：

- 向量指令的执行资源
- lane / 延迟 / 结构冲突
- 向量 load/store 与 memory path 行为
- 对 `stall_reason`、快照和 regression 的最小扩展

顺序必须继续保持：`functional` 先、`pipeline` 后。

#### V5：由向量 workload 驱动的 `Phase 4`

当且仅当向量 workload 已经稳定存在，并且真实暴露出 memory hierarchy、DMA、scratchpad、multicore 或一致性方面的热点时，再进入更重的 `Phase 4` 工作。

这样 `Phase 4` 才不是“先做一个很大的系统，再找负载去解释它”，而是“由真实 workload 证明其必要性之后再扩”。

### 与 `Phase 2 / 3 / 4` 的关系

这条线与现有阶段规划并不冲突，但对应位置要明确：

- 第一代向量语义和最小 kernel 闭环，更接近 `Phase 2` 之后“继续扩 execution / ISA 能力”的自然延伸。
- vector-aware pipeline 属于 `Phase 3` 范围，应在 reference path 和 kernel 验证之后再做。
- cache、DMA、multicore 和一致性仍属于 `Phase 4`，但应由向量 workload 的真实访问模式与性能瓶颈来驱动，而不是抢在 workload 之前独立放大。

换句话说：这条线不是替代 `Phase 4`，而是为未来哪些 `Phase 4` 工作值得做、应该先做哪一刀，提供更高信号的依据。

### 与更重 `Phase 4` 的冲突与优先级

如果未来需要在“开启向量扩展 + ML workload 主线”和“直接做更重的 `Phase 4`”之间二选一，默认应先做前者。

原因如下：

- **与 cache / DMA 的冲突**：在还没有向量 workload 之前，cache 层级、DMA 或 scratchpad 的收益判断缺少真实访问模式；先做只会扩大状态空间和验证面。
- **与 multicore / coherence 的冲突**：多核与一致性会把平台状态、调试快照和验证复杂度显著放大；在单核向量 workload 还没站稳前，收益通常不足。
- **与当前主线方法论的冲突**：仓库当前最有价值的做法是小步推进、reference-first、由真实 bug 或 workload 驱动更重结构；直接抢跑更大 `Phase 4` 更容易偏离这条方法论。

例外情况只保留给少数“有独立结构收益、且不提前扩大外部语义面”的准备性工作，例如：为未来 DMA 预留更清晰的 bus / memory contract，或补更好的观测面。但这种准备性工作不应被包装成完整 `Phase 4` 启动。

### 验证思路

这条线的验证应分层推进：

1. 向量 ISA 与非法编码的 host 单测 / smoke
2. 算子级 host workload 回归（dot / GEMM / Conv）
3. 最小 guest 向量程序
4. 最小 CNN inference demo
5. `pipeline` 跟进后的 host / guest / debug 回归
6. 只有在进入 `Phase 4` 相关工作后，才继续补更深的带宽 / 层次结构验证

验证顺序同样应保持“先 correctness，后 workload，最后才是更重微架构”。

## 风险与取舍

- 先做 `V-lite` 而不是完整 RVV，会牺牲一部分规范兼容性，但可以显著降低第一代复杂度，并让 workload 驱动后续是否继续扩面。
- 先做整数 / 定点，会推迟浮点 CNN 或更广 ML 场景，但更符合当前仓库的结构和验证成本。
- 把更重 `Phase 4` 延后到 workload 驱动之后，会延缓 cache / DMA / multicore 研究启动时间，但能避免在信号不足时放大系统复杂度。
- 先 kernel、后 network，看起来没有“直接跑模型”那么热闹，但长期更稳，也更符合现有 reference-first 的项目方法论。

## 当前有效性说明

- 当前有效 / 历史语境：当前有效，作为“向量扩展 + ML workload”长期主线候选的方向设计边界。
- 当前是否已经进入实施、近期主线是否已经因此调整，以及它与当前 bug-driven hardening / `Phase 4` 的优先级关系，以 [status/mainline_status.md](../status/mainline_status.md) 和 [status/project_priority_roadmap.md](../status/project_priority_roadmap.md) 为准。
