# `V-lite` V3 设计：固定 `conv -> relu` guest demo

## 文档定位

本文档用于把 `V-lite` 在 `V2` 之后的下一刀收窄成一个更像最小 CNN inference、但仍保持工程边界克制的 `V3`：

- 在 guest 侧把单个算子回归推进成固定 `conv -> relu` 的链路闭环
- 保持这条线继续独立于当前 `guest/kernel/*` 主干，不反向污染 Phase 1 基线
- 明确 `V3` 仍然服务于 reference-first 的 workload 验证，而不是提前扩成 vector-aware `pipeline` 或更完整 runtime

本文档不承担实时进度更新。当前主线状态、活跃计划进度，以及是否已经进入实现收口，以对应 `status` 与 `plan` 文档为准。

## 关联文档

- 上层方向设计：
  - [vector_ml_workload_direction_design.md](vector_ml_workload_direction_design.md)
  - [vector_v2_operator_guest_design.md](vector_v2_operator_guest_design.md)
- 状态文档：
  - [../status/mainline_status.md](../status/mainline_status.md)
  - [../status/project_priority_roadmap.md](../status/project_priority_roadmap.md)
- 已完成计划归档：
  - [../plan/history_plan.md#vector-v3-plan](../plan/history_plan.md#vector-v3-plan)
  - [../plan/history_plan.md#vector-v3-hardening-v4-design-plan](../plan/history_plan.md#vector-v3-hardening-v4-design-plan)
  - [../plan/history_plan.md#vector-v2-plan](../plan/history_plan.md#vector-v2-plan)

## 背景与问题

当前仓库已经是一个已可运行的模拟器原型。`V-lite` 的 `V0 / V1` 已经把最小向量状态、共享语义和 `pipeline` serializing fallback 接回主线，`V2` 也已经补齐 host 侧 `dot / GEMM / Conv / ReLU` workload smoke，以及独立最小 `guest/vector_demo` 闭环。

但 `V2` 仍然主要回答“单个算子是否正确”和“最小 guest 向量程序是否能稳定跑通”。它还没有回答下一个更关键的问题：当这些算子在 guest 侧按固定顺序拼成最小网络片段时，当前 `V-lite`、ELF 加载路径和两种 backend 是否仍能稳定形成闭环。

因此，`V3` 的健康首刀不应该直接跳到更大的 `conv -> relu -> pool -> fc` 网络，也不应该把 guest runtime、模型加载或 vector-aware `pipeline` 一起放大，而应该先补一条固定 `conv -> relu` guest demo，作为最小 CNN inference demo 的前置台阶。

## 目标

- 新增一条独立的固定 `conv -> relu` guest demo，作为 `V3` 的最小 CNN-style 闭环。
- 保持 `V2` 现有 host workload smoke 和独立 `guest/vector_demo` 不变，不把 `V3` 变成覆盖式重写。
- 用固定输入、固定权重和固定期望输出，验证 `conv` 结果经过 `relu` 后的最终 guest 侧输出。
- 保持 `pipeline` 在 `V3` 中仍是正确的 serializing fallback，不引入 vector-aware 资源或更重执行模型。
- 为后续是否值得继续扩到 `pool / fc` 或更完整最小 CNN inference demo 提供更直接证据。

## 非目标

- 不在 `V3` 中引入 `Pool`、`FC`、模型文件加载、权重文件加载或更完整 guest runtime。
- 不在 `V3` 中改现有 `V-lite` ISA 面，也不为了更像 CNN 而新增额外向量原语。
- 不在 `V3` 中引入 vector-aware `pipeline`、向量 forwarding、向量 LSQ 或 lane 级资源建模。
- 不把 `V3` demo 接进 `guest/kernel/*`、`supervisor_demo`、`kernel_alpha` 或 `interactive_os` 主线。
- 不把 `V3` 包装成性能研究入口；本轮仍只回答 correctness 和 workload 形态问题。

## 约束与边界

- `V3` 必须继续建立在共享 `InstructionSemantics` 与现有 `V-lite` 指令子集之上。
- `V3` 的 guest demo 应独立于 `V2` 的 `guest/vector_demo`，避免把“最小算子闭环”和“最小 CNN-style 闭环”揉进同一 demo。
- `V3` 继续复用现有 guest 平台基础设施，例如 `platform.S`、UART 输出和关机路径，但不新增新的 guest 公共 API。
- `conv` 与 `relu` 都应由现有 `V-lite` 子集表达：`conv` 继续依赖 `vle + vdot`，`relu` 继续依赖 `vmax.vv`。
- 当前 `Makefile` 里 `vector_vlite_smoke` 与 `vector_backend_smoke` 只有泛化 `test-host-%` 入口；本轮顺手补显式 alias 仅属于工程卫生改动，不改变任何测试语义。

## 方案

### 结构设计

`V3` 保持两条线分离：

1. **保留 `V2` 既有回归**
   - `vector_operator_smoke` 继续负责算子级 host workload 验证。
   - `guest/vector_demo` 继续负责最小独立 guest 向量程序。

2. **新增 `V3` 固定 `conv -> relu` guest demo**
   - 新增独立 `guest/vector_cnn_demo` 目录。
   - 采用最小 `_start + main + link.ld` 形式，与 `V2` demo 同样克制。
   - 程序内部先完成固定 `conv`，再对 `conv` 输出做 `relu`，最后比对固定期望结果并输出 marker。

这样可以把：

- `V2` 定位为“算子 + 最小 guest 闭环”
- `V3` 定位为“最小 CNN-style guest 链路闭环”

保持成两个清晰台阶，而不是不断把同一个 demo 往更重方向堆。

### 接口 / 数据 / 契约

#### demo 形态

`V3` 的 guest demo 采用如下形态：

- 固定输入向量与固定卷积核。
- 先完成一个最小 1D `conv`，得到固定 3 个输出值。
- 再以向量方式对这 3 个输出做 `relu`。
- 用固定期望结果逐字节比对，成功输出 `V3OK`，失败输出 `V3X`。

推荐固定结果为：

- `conv` 输出：`[7, -9, 7]`
- `relu` 输出：`[7, 0, 7]`

这样可以复用 `V2` 已经站稳的 `conv` 样本，又能用 `relu` 把它拼成一个真正的两阶段 workload。

#### 文件边界

建议新增：

- `myCPU/guest/vector_cnn_demo/start.S`
- `myCPU/guest/vector_cnn_demo/main.S`
- `myCPU/guest/vector_cnn_demo/link.ld`

以及对应 `Makefile` 目标：

- `guest/vector_cnn_demo.elf`
- `test-guest-vector_cnn_demo`
- `test-pipeline-guest-vector_cnn_demo`

这条线应与 `guest/vector_demo` 并存，而不是覆盖它。

#### backend 合同

`pipeline` 在 `V3` 里继续保持 `V2` 的 serializing fallback：

- 所有向量指令继续按 serializing 指令处理。
- 不新增向量资源建模。
- 不把 `V3` demo 的通过误读为 vector-aware `pipeline` 已经完成。

## 验证思路

`V3` 的验证建议分 3 层：

1. **工程卫生 alias**
   - `cd myCPU && make test-host-vector_vlite_smoke`
   - `cd myCPU && make test-host-vector_backend_smoke`

2. **新增 guest `conv -> relu` demo**
   - `cd myCPU && make test-guest-vector_cnn_demo`
   - `cd myCPU && make test-pipeline-guest-vector_cnn_demo`

3. **守住现有主门禁**
   - `cd myCPU && make test`
   - `cd myCPU && make test-pipeline`

## 风险与取舍

- 先把 `V3` 收窄成固定 `conv -> relu`，意味着 `pool / fc` 继续延后，但这样可以避免把数据编排、网络结构和 guest runtime 问题过早混在一起。
- 新增独立 `guest/vector_cnn_demo` 会多一条轻量 demo 路径，但能显著降低对 `V2` 与 Phase 1 guest 主线的扰动。
- 当前 `V3` 仍然不回答性能问题；这会让路线继续偏 correctness-first，但更符合当前项目的 reference-first 方法论。

## 当前有效性说明

- 当前有效 / 历史语境：当前有效，作为 `V-lite` `V3` 首刀的设计边界。
- 当前是否已经进入实现、后续是否继续沿这条线扩展，以 [../status/mainline_status.md](../status/mainline_status.md)、[../status/project_priority_roadmap.md](../status/project_priority_roadmap.md) 与 [../plan/history_plan.md#vector-v3-plan](../plan/history_plan.md#vector-v3-plan) 为准。
