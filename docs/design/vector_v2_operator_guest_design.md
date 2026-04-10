# `V-lite` V2 设计：算子回归与最小 guest 闭环

## 文档定位

本文档用于把 `V-lite` 在 `V0 / V1` 之后的下一刀收窄成一个可健康推进、又不会过早放大系统复杂度的 `V2`：

- 在 host 侧建立更接近 ML kernel 的算子级回归
- 在 guest 侧补上一条独立、最小、可回归的向量程序闭环
- 明确 `V2` 仍然只服务于 reference-first 的正确性与 workload 验证，不提前扩成新的 guest runtime 或 vector-aware `pipeline`

本文档不承担实时进度更新。当前主线状态、当前计划是否完成，以及下一步是否继续推进更完整 CNN 闭环，以对应 `status` 文档为准。

## 关联文档

- 上层方向设计：
  - [vector_ml_workload_direction_design.md](vector_ml_workload_direction_design.md)
  - [vector_vlite_v0_v1_design.md](vector_vlite_v0_v1_design.md)
- 状态文档：
  - [../status/mainline_status.md](../status/mainline_status.md)
  - [../status/project_priority_roadmap.md](../status/project_priority_roadmap.md)
- 已完成计划归档：
  - [../plan/history_plan.md#vector-v2-plan](../plan/history_plan.md#vector-v2-plan)

## 背景与问题

当前仓库已经是一个已可运行的模拟器原型，`V-lite` 的 `V0 / V1` 也已经把最小向量状态、共享语义、`functional` reference path、最小 host smoke，以及 `pipeline` 的正确 serializing fallback 接回主线。这说明“向量指令能被解码、执行并在两种 backend 下得到一致的最终架构态”这一层已经成立。

但这还不是 `V2`。`V0 / V1` 里的 smoke 仍然主要停留在“单条或少量指令是否正确”，还没有把 `dot / GEMM / Conv / ReLU` 这类更接近 ML kernel 的操作组织成稳定回归，也没有补上一条真正从 ELF 加载、执行到输出的最小 guest 向量程序。缺少这一层时，后续无论是讨论更完整 CNN、guest runtime 接口，还是 vector-aware `pipeline`，都还缺少足够扎实的中间落脚点。

因此，`V2` 的健康首刀不应该是继续扩 ISA 面，也不应该是把 guest runtime 或 `pipeline` 一起放大，而应该先补“算子级 workload 验证 + 最小 guest 闭环”这两个结构收益最直接的中间层。

## 目标

- 在 host 侧新增一组更贴近 ML kernel 的向量算子回归，而不是只停留在单条指令 smoke。
- 用现有 `V-lite` 子集站稳第一批 workload：`dot`、小 `GEMM`、小 `Conv` 和 `ReLU`。
- 新增一条独立的最小 guest 向量 demo，验证 ELF 加载、程序执行、向量访存与结果自检闭环。
- 保持 `pipeline` 在 `V2` 中仍是正确的 serializing fallback，不引入 vector-aware rename、LSQ 或执行资源。
- 为后续最小 CNN inference demo 提供稳定的前置台阶。

## 非目标

- 不在 `V2` 中引入新的向量 ISA 指令。
- 不在 `V2` 中扩成新的 guest supervisor runtime、kernel runtime API 或 monitor 命令集。
- 不在 `V2` 中引入 vector-aware `pipeline`、向量 forwarding、向量 LSQ 或 lane 级资源建模。
- 不在 `V2` 中追求完整 `Conv + ReLU + Pool + FC` 网络链路。
- 不为了补 `Pool` 而临时加入 shuffle、reduction max 或其他超出当前 `V-lite` 边界的新原语。

## 约束与边界

- 所有 workload 都必须继续建立在共享 `InstructionSemantics` 与现有 `V-lite` 指令子集之上，不得为测试方便新增 backend 私有捷径。
- `V2` 的 host 算子回归优先走“手写向量程序 + 标量 reference 结果比较”，而不是把新逻辑塞进 production helper。
- `V2` 的 guest demo 必须保持独立、最小，不挤进当前 `guest/kernel/*` 主干，也不和 `supervisor_demo` / `kernel_alpha` 共用一套更重 bring-up。
- `V2` 的 guest demo 应复用现有 guest 平台基础设施（如 `platform.S`、UART 输出、关机路径），但不新增新的 guest 公共 API 面。
- `Pool` 在当前 ISA 子集下缺少自然、稳定的表达方式；`V2` 首刀明确延后 `Pool`，避免为了补齐 checklist 而牵出额外 ISA 扩面。

## 方案

### 结构设计

`V2` 分成两层闭环，但都保持在最小切片：

1. **host 算子级回归**
   - 新增一条专门的 `vector_operator_smoke`。
   - 通过 hand-written 向量程序覆盖更完整的数据搬运和算子组合，而不是只测单条指令。
   - 同时比较：
     - `functional` 最终状态
     - `pipeline` 最终状态
     - 标量 reference 期望结果

2. **独立最小 guest 向量 demo**
   - 新增独立 `guest/vector_demo` 目录。
   - 以最小 `_start + main + link.ld` 形式加载为 ELF。
   - 程序内部完成固定小 workload、自检输出和关机。
   - 保证它是一条可进入 `make test` / `make test-pipeline` 的稳定 guest 门禁。

这两层各自回答不同问题：

- host 算子回归回答“当前 `V-lite` 是否已经足以稳定表达第一批 ML kernel 原语”。
- guest demo 回答“这组原语是否已经能穿过真实 guest 装载与执行路径形成闭环”。

### 接口 / 数据 / 契约

#### host 算子回归范围

`V2` 首刀建议固定为 4 组 workload：

1. `int8 dot`
2. `int32 2x2 GEMM`
3. `int8 1D Conv`
4. `int16 ReLU`

其中：

- `dot` 直接验证 `vdot.vv` 的基本可用性。
- `GEMM` 验证重复 `load + dot + store` 组合，以及不同输入/输出地址编排。
- `Conv` 验证窗口滑动下的重复 `vle + vdot` 模式，这是后续 CNN 的更直接前驱。
- `ReLU` 用 `vmax.vv` 对零向量取 max，验证简单激活函数路径。

`Pool` 在当前 `V-lite` 里既没有自然的 lane 内 reduction，也没有 shuffle / permute 支撑。`V2` 首刀明确不把它塞进来；后续若真要补，应等到有明确的新原语设计，而不是在 `V2` 里用非常绕的标量旁路硬拼。

#### guest demo 形态

独立 guest demo 采用下面的克制边界：

- 只依赖 `guest/lib/platform.S`、最小 UART 输出和关机路径。
- 不依赖当前 `guest/kernel/*` 中的复杂 bring-up、VM、trap runtime 或进程/用户态层次。
- 向量指令通过 raw encoding（例如 `.word` 宏）进入程序，不要求交叉工具链原生识别当前 `V-lite` 助记符。
- 程序内部用固定输入、固定权重和固定期望结果做自检，成功输出固定 marker，失败输出错误 marker 并关机。

这样可以保证：

- `V2` 不会反向污染当前 guest runtime 主线。
- ELF loader、CPU 执行路径、向量访存与程序自检都能真正跑一遍。
- 后续若要扩成最小 CNN demo，也可以在这条独立 guest demo 线上继续长，而不是从 `kernel_alpha` 或 `interactive_os` 里拆改。

#### backend 合同

`pipeline` 在 `V2` 仍继续沿用 `V1` 的 serializing fallback：

- 所有向量指令继续作为 serializing 指令处理。
- 不新增向量资源冲突建模。
- 不把 guest demo 的通过包装成“vector-aware pipeline 已完成”。

`V2` 的 backend 验证重点仍然是“功能正确且最终态一致”，不是吞吐或 stall 模型。

### 验证思路

`V2` 首刀的验证分 3 层：

1. **新增 host workload smoke**
   - `cd myCPU && make test-host-vector_operator_smoke`
   - 对 `functional / pipeline / expected result` 做三方比对。

2. **新增 guest vector demo**
   - `cd myCPU && make test-guest-vector_demo`
   - `cd myCPU && make test-pipeline-guest-vector_demo`
   - 通过固定 marker 验证 end-to-end 闭环。

3. **守住现有主门禁**
   - `cd myCPU && make test`
   - `cd myCPU && make test-pipeline`

## 风险与取舍

- 先做 `dot / GEMM / Conv / ReLU`，会把 `Pool` 留到后面，但这样能避免为了补一个不自然的算子而提前扩 ISA。
- 独立 guest demo 不复用当前 guest runtime 主线，会多一条较轻的 demo 路径，但能明显降低对现有 Phase 1 基线的扰动。
- `pipeline` 继续保持 serializing fallback，会让 `V2` 依然偏 correctness-first，而不是性能研究；这是有意识的取舍，避免现在就把 `V2` 和 `V4` 混起来。

## 当前有效性说明

- 当前有效 / 历史语境：当前有效，作为 `V-lite` `V2` 首刀的设计边界。
- 当前结果是否已落地、相关计划是否已经完成，以 [../status/mainline_status.md](../status/mainline_status.md) 和 [../plan/history_plan.md#vector-v2-plan](../plan/history_plan.md#vector-v2-plan) 为准。
