# `V-lite` V4 设计：最小 vector-aware pipeline 首刀

## 文档定位

本文档用于把 `V-lite` 在 `V3` 之后的下一刀收窄成一个有明确结构收益、但仍保持范围克制的 `V4`：

- 不再让所有向量指令都退化成 `pipeline` 里的统一 serializing system 路径
- 先只让 non-memory vector ALU 指令进入一条最小 vector-aware execute 路径
- 继续把 `functional` reference path 作为唯一向量语义真值来源，不在 `pipeline` 中复制一套独立语义

本文档不承担实时进度更新。当前主线状态、活跃计划与实现进度，以对应 `status` 和 `plan` 文档为准。

## 关联文档

- 上层方向设计：
  - [vector_ml_workload_direction_design.md](vector_ml_workload_direction_design.md)
  - [vector_v3_minimal_cnn_guest_design.md](vector_v3_minimal_cnn_guest_design.md)
- 状态文档：
  - [../status/mainline_status.md](../status/mainline_status.md)
  - [../status/project_priority_roadmap.md](../status/project_priority_roadmap.md)
- 已完成计划归档：
  - [../plan/history_plan.md#vector-v4-plan](../plan/history_plan.md#vector-v4-plan)
  - [../plan/history_plan.md#vector-v3-hardening-v4-design-plan](../plan/history_plan.md#vector-v3-hardening-v4-design-plan)
  - [../plan/history_plan.md#vector-v3-plan](../plan/history_plan.md#vector-v3-plan)

## 背景与问题

当前仓库已经是一个已可运行的模拟器原型。`V-lite` 的 `V0 / V1`、`V2` 与 `V3` 都已经站稳：shared semantics、最小 host smoke、独立 guest vector demo，以及固定 `conv -> relu` 的最小 CNN-style guest 闭环都已接通。

但 `pipeline` 侧目前仍把所有向量指令都视为 serializing system 类指令。这保证了 correctness，却也意味着：只要 workload 稍微变得更像真实向量程序，当前 `pipeline` 就无法区分“需要保守处理的向量配置 / 访存指令”和“其实可以先做最小独立执行的纯向量 ALU 指令”。

因此，`V4` 的健康首刀不应该直接去做 lane 建模、vector LSQ、向量 forwarding 或更重的 memory speculation，而应该先把最小 non-memory vector ALU 路径从“统一 serializing fallback”里拆出来，形成第一条真正的 vector-aware pipeline 边界。

## 目标

- 先让 `vadd.vv`、`vmul.vv`、`vmax.vv`、`vdot.vv` 进入 `pipeline` 的最小独立执行路径。
- 保持 architected 向量语义继续来自共享 `InstructionSemantics + VectorRequest`，不在 `pipeline` 中复制解释逻辑。
- 保持 `vsetcfg`、`vle.v`、`vse.v` 在 `V4` 首刀中继续走保守 serializing 路径。
- 为后续是否值得继续扩到向量访存、lane/latency 或更细粒度结构冲突提供更干净的结构基础和观测面。

## 非目标

- 不在 `V4` 首刀中实现 vector load/store 的独立 pipeline memory path。
- 不在 `V4` 首刀中新增 vector LSQ、向量 forwarding、lane 级并行度、latency model 或专门的 scoreboard。
- 不在 `V4` 首刀中重开更激进的 issue/replay/speculation。
- 不在 `V4` 首刀中扩 `V-lite` ISA 面。
- 不把 `V4` 误读成“向量性能研究入口”；本轮仍然优先回答结构边界和 correctness 问题。

## 约束与边界

- `functional` backend 继续是向量语义真值来源；`pipeline` 只能消费共享语义层产出的 `VectorRequest`。
- 只有 non-memory vector ALU 指令可以在 `V4` 首刀中脱离 serializing fallback；`vsetcfg / vle.v / vse.v` 继续保守处理。
- `pipeline` 对向量结果的暂存与提交，应尽量复用现有 `ROB` / commit boundary 的顺序退休边界，而不是平行创造一套新的 architected 提交语义。
- 本轮应优先引入最小可观测性：至少能区分“vector execute in-flight”和“继续走 serializing fallback”的最小状态，不扩大更重 debug 协议面。

## 方案

### 结构设计

`V4` 首刀把向量指令分成两类：

1. **继续 serializing 的指令**
   - `vsetcfg`
   - `vle.v`
   - `vse.v`

2. **进入最小 vector-aware execute 的指令**
   - `vadd.vv`
   - `vmul.vv`
   - `vmax.vv`
   - `vdot.vv`

第一类继续沿现有 commit-boundary vector apply 路径，保证保守 correctness；第二类则在 `pipeline execute` 中获得一条最小独立处理路径，但仍通过共享 `VectorRequest` 驱动结果生成。

### 接口 / 数据 / 契约

- decode / frontend 侧需要把“所有 vector opcode 都是 serializing system”收窄成“只有 vector config / memory 指令是 serializing”。
- execute 侧需要新增最小 vector ALU 路径，用于对 non-memory `VectorRequest` 形成 speculative result。
- commit boundary 仍然负责顺序退休；但对 non-memory vector ALU 指令，commit 时不应再次重算语义，而应只做最小提交动作。
- 当前首刀可以接受保守 hazard 边界，但不必把它永远固定成“任何 older vector pending 都阻塞所有 younger vector ALU”：
  - pending 的 `vsetcfg / vle.v / vse.v` 仍应继续阻塞 younger vector ALU
  - direct older source dependency 如果已经 materialize 成 result payload，则允许 younger non-memory vector ALU 以这份 payload 作为 execute 输入
  - 本轮只接受这种最小的 source patch-through，不扩成通用 vector rename / vector forwarding / lane network

### 验证思路

`V4` 首刀的验证建议分 3 层：

1. host smoke
   - 纯向量 ALU 指令在 `pipeline` 下不再被统一归入 serializing system fallback
   - ready older vector producer 被更老 scalar ROB head 挡住 commit 时，direct dependent younger vector ALU 仍能以前驱 materialized result 完成 execute
   - pending older vector config / memory 指令继续阻塞 younger vector dependency chain
   - `functional` / `pipeline` 最终向量状态与内存结果继续一致
2. workload smoke
   - 现有 `vector_operator_smoke`
   - 现有 `vector_cnn_smoke`
3. 总门禁
   - `cd myCPU && make test`
   - `cd myCPU && make test-pipeline`

## 风险与取舍

- 首刀只做 non-memory vector ALU，意味着向量访存仍然保守，但这样可以避免把 memory ordering、LSQ 和向量状态提交问题一次性混在一起。
- 保留 shared semantics 可以降低重复实现风险，但也要求 `pipeline` 首刀在数据接线和提交边界上保持克制。
- 如果后续真实 workload 没有显示出继续扩 lane / latency / vector memory path 的收益，这个首刀也仍然有独立结构价值：它至少把“全部 vector = serializing system”这条过宽边界收窄了。

## 当前有效性说明

- 当前有效 / 历史语境：当前有效，作为 `V-lite` `V4` 首刀的设计边界。
- 当前实现语境补充：`V4` 首刀落地后又补了一轮很窄的 hardening；当前 `vector_state_busy` 已从“只要有任何 older vector pending 就全局阻塞”收窄为“pending serializing vector 仍阻塞；direct older source dependency 若尚未 materialize 则阻塞；若结果已 materialize，则 younger non-memory vector ALU 可用这份结果完成 execute”。
- 当前是否已经进入实现、做到哪一步，以 [../status/mainline_status.md](../status/mainline_status.md)、[../status/project_priority_roadmap.md](../status/project_priority_roadmap.md) 与 [../plan/history_plan.md#vector-v4-plan](../plan/history_plan.md#vector-v4-plan) 为准。
