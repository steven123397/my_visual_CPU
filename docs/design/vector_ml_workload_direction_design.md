# 向量扩展与 ML workload 统一设计

## 文档定位

本文档是当前仓库关于 `向量扩展 + ML workload` 的统一设计来源，用于取代此前按 `V0 / V1`、`V2`、`V3`、`V4` 分拆维护的多份阶段文档。

它重点回答：

- 这条线为什么是长期候选主线之一
- 当前 `V-lite` 的最小 ISA / 状态 / workload / pipeline 合同是什么
- `debug/frontend` 目前如何只读观察这条线
- 这条线与后续 `Phase 4` 的衔接边界在哪里

本文档不承担实时进度更新。当前是否已经落地、当前优先级和下一步，以对应 `status` 文档为准。

## 关联文档

- 状态文档：
  - [../status/mainline_status.md](../status/mainline_status.md)
- 相关设计：
  - [debug_frontend_integration.md](debug_frontend_integration.md)
  - [phase4_preparation_design.md](phase4_preparation_design.md)
- 已完成计划归档：
  - [../plan/history_plan.md#vector-v0-v1-plan](../plan/history_plan.md#vector-v0-v1-plan)
  - [../plan/history_plan.md#vector-v2-plan](../plan/history_plan.md#vector-v2-plan)
  - [../plan/history_plan.md#vector-v3-plan](../plan/history_plan.md#vector-v3-plan)
  - [../plan/history_plan.md#vector-v3-hardening-v4-design-plan](../plan/history_plan.md#vector-v3-hardening-v4-design-plan)
  - [../plan/history_plan.md#vector-v4-plan](../plan/history_plan.md#vector-v4-plan)

## 背景与问题

当前仓库已经是一个已可运行的模拟器原型。`functional` 负责 reference 真值，`pipeline` 负责执行模型实验，guest runtime 已能支撑最小 supervisor / kernel bring-up，`debug/frontend` 也已经形成一条教学演示闭环。

在这个基础上，仓库需要一条能继续提供高信号 workload 的长期主线。直接抢跑更重的 `Phase 4`（`cache / DMA / multicore / coherence`）会过早放大平台状态空间；相比之下，先沿“向量扩展 + ML workload”这条 workload-guided、ISA-first 路线推进，更符合当前 reference-first、小步验证的方法论。

但随着 `V-lite` 从最小 ISA 走到最小 guest workload、再走到最小 vector-aware `pipeline`，原先按阶段拆开的设计文档已经开始重复。当前更健康的做法，是把仍然有效的长期边界收口成一份统一设计，把实时进度与阶段完成态留给 `status` 和 `history_plan`。

## 目标

- 把当前 `V-lite` 的长期有效设计边界收口成一份统一文档。
- 维持“共享语义优先、`functional` reference path 优先、workload 驱动后续扩展”的主线方法。
- 明确当前稳定存在的最小 ISA / 状态 / workload / pipeline / 观测面。
- 为后续继续做 bug-driven hardening、workload 观察，以及与 `Phase 4` 的衔接提供单一设计来源。

## 非目标

- 不把当前路线定义成完整 RVV 兼容实现。
- 不在当前边界里引入 mask、predication、segment、gather / scatter、浮点向量或完整 ML runtime。
- 不把 `frontend` 扩成通用向量调试器、通用模型可视化器或性能分析器。
- 不在当前阶段抢跑向量 load/store 的独立 pipeline memory path、vector LSQ、vector rename、lane 模型或更重 memory speculation。
- 不把更重 `Phase 4` 当成这条线的默认下一步。

## 当前统一设计边界

### 1. `V-lite` ISA 与状态合同

当前第一代向量能力保持为最小 `V-lite` 子集：

- 32 个向量寄存器 `v0..v31`
- 每个向量寄存器固定 16 B（128-bit）
- 最小配置状态：
  - `sew_bytes`：当前元素宽度，仅允许 `1 / 2 / 4 / 8`
  - `vl`：当前活跃元素数，要求 `0 <= vl <= 16 / sew_bytes`

编码面当前固定为：

| major opcode | 组合 | 含义 |
|---|---|---|
| `0x57` | `funct3 = 7`，`funct7[6:3] = 0b1000` | `vsetcfg` |
| `0x57` | `funct3 = 0`，`funct7 = 0x00` | `vadd.vv` |
| `0x57` | `funct3 = 0`，`funct7 = 0x20` | `vmul.vv` |
| `0x57` | `funct3 = 0`，`funct7 = 0x21` | `vmax.vv` |
| `0x57` | `funct3 = 0`，`funct7 = 0x22` | `vdot.vv` |
| `0x07` | `funct3 = 0` | `vle.v` |
| `0x27` | `funct3 = 0` | `vse.v` |

其中：

- `vsetcfg` 只改向量配置，不写 GPR，也不触发访存。
- `vle.v / vse.v` 使用连续 little-endian span，字节数等于 `vl * sew_bytes`。
- `vadd.vv / vmul.vv / vmax.vv` 只处理前 `vl` 个元素，非活跃部分清零。
- `vdot.vv` 采用“reduction 结果写回向量寄存器低 64 bit”的收窄形态，用于 `dot / GEMM / Conv` 早期验证。

### 2. 共享语义与 backend 合同

当前向量语义仍然只来自共享 `InstructionSemantics + VectorRequest`：

- `functional` backend 是唯一 reference 真值来源。
- `pipeline` 只能消费共享语义层产出的 `VectorRequest`，不允许复制第二套向量解释器。
- architected 向量状态仍由 commit boundary 顺序落地，不额外平行创造新的架构提交语义。

这条边界确保：

- ISA 语义不会因为 backend 不同而分叉。
- 后续无论继续做 workload 还是继续做 `pipeline` 收口，仍然能回到同一事实来源。

### 3. 当前 vector memory 合同

当前向量访存仍保持连续 span 的保守模型，但已经补上更窄的 fail-closed 边界：

- `vle.v / vse.v` 在真正逐字节执行前，会先对整段 span 做预校验。
- 如果 span 落到 live `MMIO`、非 RAM 区域或翻译失败，当前会直接返回 `access fault`。
- fault 发生时，不允许留下 UART 输入被提前消费、UART 输出 / `IER` 被改写，或 RAM partial write 这类副作用。

也就是说，当前向量 memory path 还没有进入独立 `pipeline` memory machinery，但已经收口到“保守、可解释、无额外副作用”的 reference 合同。

### 4. 当前 workload 与 guest 闭环

当前这条线已经不是单条指令实验，而是形成了完整 workload 阶梯：

- host smoke：
  - `vector_vlite_smoke`
  - `vector_backend_smoke`
  - `vector_operator_smoke`
  - `vector_cnn_smoke`
  - `vector_pipeline_smoke`
- guest demo：
  - `guest_vector_demo`，成功 marker 为 `V2OK`
  - `guest_vector_cnn_demo`，成功 marker 为 `V3OK`

当前最小 CNN-style workload 固定为 `conv -> relu`：

- 输入：`[2, -1, 3, 4, -2, 1]`
- 卷积核：`[1, 0, -1, 2]`
- `conv` 输出：`[7, -9, 7]`
- `relu` 输出：`[7, 0, 7]`

这里的定位仍然是“固定 workload 的教学 / 验证闭环”，不是完整模型执行器。

### 5. 当前 `pipeline` 边界（`V4`）

当前 `V4` 已经把 non-memory vector ALU 从统一 serializing fallback 中收窄出来，但仍保持非常克制：

- `vadd.vv / vmul.vv / vmax.vv / vdot.vv`
  - 可以进入最小 vector-aware execute path
  - execute 先 materialize 结果
  - commit 再顺序落地 architected vector state
- `vsetcfg / vle.v / vse.v`
  - 仍继续按保守 serializing 指令处理

当前 `vector_state_busy` 的边界也已经收窄到：

- pending serializing vector 指令仍阻塞 younger vector ALU
- direct older source dependency 若尚未 materialize，则继续阻塞
- 若 older non-memory vector ALU 结果已经 materialize，即使它还被更老 scalar ROB head 挡住 commit，direct dependent younger vector ALU 也可以先完成 execute

当前仍明确不做：

- 向量 load/store 的独立 pipeline memory path
- vector LSQ
- vector rename / phys file
- lane / latency 模型
- 更重的 memory speculation

### 6. 当前观测面与 frontend 关系

当前 `debug/frontend` 对这条线的支持保持只读、教学化边界：

- `DebugSnapshot` 会暴露：
  - `sew_bytes`
  - `vl`
  - 32 个 16-byte 原始向量寄存器 dump
- 浏览器端可以直接展示：
  - workload 导览
  - 向量指令 `config / memory / ALU` 高亮
  - `SEW / VL + v0..v31` 的最小可视化
  - 固定 `conv -> relu` 专题卡
  - 当前 `vector_state_busy` / serializing guard 的边界提示

但这些都只建立在已有只读快照之上，不构成新的执行语义面，也不把浏览器扩成完整向量调试器。

## 演进摘要（历史台阶，当前已合并收口）

为避免继续分散维护，当前把原先几份阶段文档的长期有效内容统一吸收到本文档；历史台阶只保留摘要：

- `V0 / V1`：冻结最小 `V-lite` ISA / 状态合同，并接入 shared semantics 与 `functional` reference path。
- `V2`：建立 `dot / GEMM / Conv / ReLU` 的 host workload 回归，并新增独立 `guest_vector_demo`。
- `V3`：把 guest workload 推进到固定 `conv -> relu` 的 `guest_vector_cnn_demo`。
- `V3 hardening`：补 mixed `SEW / VL` 链路和全负 `relu` 零钳位回归。
- `V4`：让 non-memory vector ALU 脱离统一 serializing fallback，形成最小 vector-aware `pipeline`。
- `V4 hardening`：收窄 direct dependency 与 serializing guard 的执行边界，并补更像真实依赖链的 host smoke。

阶段级执行过程和完成态，统一以 `status` 文档与 `history_plan` 为准。

## 下一步与 `Phase 4` 的衔接

当前更健康的下一步，仍然不是顺势扩到 `Pool / FC`、向量 load/store path 或完整 `Phase 4`，而是：

1. 继续围绕已落地的 `V4` 边界做 bug-driven hardening。
2. 继续用现有 workload 做更窄的观察与回归补洞。
3. 只有当 workload 真实暴露出 memory hierarchy 信号时，才把后续准备项转交给 [phase4_preparation_design.md](phase4_preparation_design.md) 中定义的 `P4-prep-*` 路线。

换句话说：当前向量主线并不替代 `Phase 4`，而是为“未来到底该做哪一刀 `Phase 4`”提供更高信号的依据。

## 验证思路

当前这条线至少应持续守住：

- `cd myCPU && make test`
- `cd myCPU && make test-pipeline`

如果改动集中在向量 reference / workload / pipeline 边界，至少额外关注：

- `cd myCPU && make test-host-vector_vlite_smoke`
- `cd myCPU && make test-host-vector_operator_smoke`
- `cd myCPU && make test-host-vector_cnn_smoke`
- `cd myCPU && make test-host-vector_pipeline_smoke`
- `cd myCPU && make test-guest-vector_demo`
- `cd myCPU && make test-guest-vector_cnn_demo`

## 风险与取舍

- 继续保持最小 `V-lite`，会推迟完整 RVV、浮点向量和更复杂 kernel，但能显著降低状态空间和验证成本。
- 把向量访存继续保守在 commit-boundary 语义和 serializing config / memory path 上，会牺牲一部分“更像真实硬件”的表现，但能避免现在就把 LSQ、memory ordering 和向量提交一起放大。
- 把前端限制在教学式可视化，会让高级调试能力继续受限，但更符合当前仓库“可观察、可讲解、可回归”的定位。

## 当前有效性说明

- 当前有效 / 历史语境：当前有效，作为 `向量扩展 + ML workload` 的统一设计边界。
- 当前实现进度、当前优先级、后续是否重开更重向量子线或 `Phase 4`，以 [../status/mainline_status.md](../status/mainline_status.md) 为准。
