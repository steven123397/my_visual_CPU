# `V-lite` V0 / V1 设计

## 文档定位

本文档用于把“向量扩展 + ML workload”方向里的 `V0` / `V1` 两个最前置阶段收窄成可直接实施的设计边界：

- `V0`：补齐第一代 `V-lite` 的 ISA / 状态 / 执行合同，冻结最小实现面
- `V1`：只把这组合同接到 shared semantics、`functional` reference path，以及最小 host 验证闭环

本文档不覆盖 `V2` 之后的 guest kernel、最小 CNN demo、vector-aware `pipeline` 或 workload-driven `Phase 4` 扩面；这些仍以后续设计与计划为准。

## 关联文档

- 上层方向设计：
  - [vector_ml_workload_direction_design.md](vector_ml_workload_direction_design.md)
- 后续设计：
  - [vector_v2_operator_guest_design.md](vector_v2_operator_guest_design.md)
- 相关状态：
  - [../status/mainline_status.md](../status/mainline_status.md)
  - [../status/project_priority_roadmap.md](../status/project_priority_roadmap.md)
- 已完成计划归档：
  - [../plan/history_plan.md#vector-v0-v1-plan](../plan/history_plan.md#vector-v0-v1-plan)
  - [../plan/history_plan.md#vector-v2-plan](../plan/history_plan.md#vector-v2-plan)

## 背景

当前仓库已经是一个已可运行的模拟器原型，但现有 ISA / guest / `pipeline` 仍全部围绕标量 RV64I / RV64M 展开。若要把“向量扩展 + ML workload”真正从方向判断推进到工程实现，第一刀不能直接跳到 guest runtime、CNN demo 或 vector-aware `pipeline`，而应先把第一代 `V-lite` 的最小合同冻结下来，并只在 shared semantics 与 `functional` reference path 上站稳。

这一步的目标不是“完整 RVV 兼容”，也不是“高性能向量后端”，而是建立一个后续能继续长出来、同时又不会立刻放大状态空间和验证成本的最小向量基线。

## 目标

- 冻结第一代 `V-lite` 的最小架构状态、编码面和执行语义。
- 把第一代范围严格限制在 shared semantics、`functional` reference path 和 host-side 回归。
- 明确 `pipeline` 在 `V1` 中只承担“正确的 serializing fallback”，不承担 vector-aware 资源模型。
- 为后续 `V2` 算子验证保留足够的计算与数据搬运能力。

## 非目标

- 不追求与完整 RVV 的编码兼容或 CSR 兼容。
- 不在 `V1` 中引入 mask、predication、segment、gather/scatter、浮点向量。
- 不在 `V1` 中引入 guest kernel API、最小 CNN demo 或模型文件加载。
- 不在 `V1` 中把 `pipeline` 扩成 lane-aware、vector rename、vector LSQ 或新执行资源。
- 不把向量指令变成第二套 backend 私有语义。

## 总体方案

### 状态模型

第一代 `V-lite` 采用固定宽度、最小可扩展的状态：

- 32 个向量寄存器 `v0..v31`
- 每个向量寄存器固定为 16 B（128-bit）
- 一组最小配置状态：
  - `sew_bytes`：当前元素宽度，仅允许 `1 / 2 / 4 / 8`
  - `vl`：当前活跃元素个数，要求 `1 <= vl <= 16 / sew_bytes`

`CoreState` 在 reset 后的默认状态为：

- 全部向量寄存器清零
- `sew_bytes = 1`
- `vl = 0`

也就是说，reset 后的向量算子默认是“零活跃元素”状态；真正进入可用配置，必须显式执行一次 `vsetcfg`。

### 编码面

第一代只保留 3 个 major opcode：

- `0x57`：向量配置与向量 ALU
- `0x07`：连续向量 load
- `0x27`：连续向量 store

其中：

- `0x57` / `funct3 = 7` / `funct7[6:3] = 0b1000` 表示 `vsetcfg`
  - `funct7[2:0]` 编码 `sew_bytes`
  - `rs2 + 1` 编码 `vl`
- `0x57` / `funct3 = 0` 表示 `vv` 算子
  - `funct7 = 0x00`：`vadd.vv`
  - `funct7 = 0x20`：`vmul.vv`
  - `funct7 = 0x21`：`vmax.vv`
  - `funct7 = 0x22`：`vdot.vv`
- `0x07` / `funct3 = 0` 表示 `vle.v`
- `0x27` / `funct3 = 0` 表示 `vse.v`

这里故意保留“RVV-like，但不完整 RVV”的收窄：未来若要继续向更像 RVV 的方向收敛，major opcode 不需要重选；但当前也不为兼容完整规范提前背上更大复杂度。

### 指令语义

#### `vsetcfg`

`vsetcfg` 只修改向量配置状态，不写 GPR，不触发访存。

非法条件：

- `sew_bytes` 不是 `1 / 2 / 4 / 8`
- `vl` 超出 `16 / sew_bytes`
- 编码落入未保留的 `funct7` / `funct3` 组合

#### `vle.v` / `vse.v`

- 地址计算方式仍是 `base(rs1) + imm`
- 访存总字节数为 `vl * sew_bytes`
- 访存顺序按 little-endian 连续字节流处理
- `vle.v` 只覆盖目标寄存器的前 `vl * sew_bytes` 字节，剩余字节清零
- `vse.v` 只把源寄存器前 `vl * sew_bytes` 字节写回内存
- 当 `vl == 0` 时，`vle.v` / `vse.v` 为 no-op

#### `vadd.vv`

- 逐元素、按当前 `sew_bytes` 做模 `2^(8 * sew_bytes)` 加法
- 只处理前 `vl` 个元素
- 结果写回 `vd`
- 非活跃部分清零

#### `vmul.vv`

- 逐元素、按当前 `sew_bytes` 做低位截断乘法
- 只处理前 `vl` 个元素
- 结果写回 `vd`
- 非活跃部分清零

#### `vmax.vv`

- 逐元素按有符号整数比较取 max
- 只处理前 `vl` 个元素
- 结果写回 `vd`
- 非活跃部分清零

#### `vdot.vv`

第一代把 `vdot.vv` 定义成“向量 reduction 结果写回向量寄存器”的收窄形态：

- 对前 `vl` 个元素做有符号乘加
- 内部累加使用 `__int128`
- 最终结果只保留低 64 bit
- 结果写入 `vd` 的低 8 B
- `vd` 其余字节清零

也就是说，`vdot.vv` 当前不是 GPR 写回算子，也不是完整 widening vector accumulate；它的定位只是给 `V2` 的 dot / GEMM / Conv 早期验证提供一个足够稳定、又不会立刻牵扯标量 rename / forwarding 合同的 reduction 原语。

### 执行合同

#### shared semantics / `functional`

向量指令仍由共享 `InstructionSemantics` 统一分派，但第一代不把大块向量数据直接塞进 `InsnEffects`；`InsnEffects` 只携带一个小型 `VectorRequest`：

- 请求种类（config / load / store / add / mul / max / dot）
- 相关向量寄存器编号
- 计算好的访存地址（如果有）
- 需要的配置值（如果有）

真正的向量状态修改与向量访存，在 commit boundary 统一执行。这样有两个好处：

- `functional` 与后续 backend 可以继续共享同一份 architected 语义请求
- 第一代无需在 effect 层携带整块 128-bit * 32 的中间态，也不用为 `pipeline` 提前引入向量 forwarding / rename

#### `pipeline` 的 `V1` 边界

`V1` 不做 vector-aware `pipeline`，但为了不让未来在 `pipeline` backend 下直接跑向量程序时出现明显错误，当前允许一个最小 fallback：

- 所有向量指令都被视为 serializing instruction
- 只有当对应 ROB 项已经来到 head 时，它们才允许进入执行 / 提交边界
- 向量状态仍只在 commit boundary 修改
- 向量 load/store 不进入现有 LSQ / memory issue machinery

这意味着：

- 当前 `pipeline` 只保证“architecturally correct but serialized”
- 不保证任何 lane 级吞吐、vector load/store overlap、vector hazard 观测
- 未来若进入 `V4`，再单独设计 vector-aware pipeline

### 模块边界

建议按下面边界落地：

- `src/arch/vector_state.*`
  - 向量寄存器文件与最小配置状态
- `src/arch/core_state.*`
  - 持有 `VectorState`，提供 reset / accessor
- `src/exec/vector_ops.*`
  - 向量编码校验、`VectorRequest` 构造、commit-boundary 执行 helper
- `src/isa/effects.h`
  - 为 shared semantics 增加 `VectorRequest`
- `src/isa/instruction_semantics.cpp`
  - 接入向量 opcode 分派
- `src/decode.c`
  - 为 `0x07` / `0x27` 补齐立即数解码
- `src/exec/pipeline_commit_boundary.cpp`
  - 接入 `VectorRequest` 执行
- `src/exec/pipeline_backend_frontend.cpp`
  - 把向量指令纳入 serializing 判定

第一代不需要新增新的 CSR 文件、独立 `VectorCPU` facade 或 guest runtime helper。

## 验证方案

`V0 / V1` 的最小验证闭环只保留两层：

1. `tests/host/vector_vlite_smoke.cpp`
   - 配置合法 / 非法
   - `vle.v` / `vse.v`
   - `vadd.vv` / `vmul.vv` / `vmax.vv`
   - `vdot.vv`
2. `tests/host/vector_backend_smoke.cpp`
   - 用一小段 hand-written 程序比较 `functional` 与 `pipeline(serialized fallback)` 的最终向量 / 内存状态

除此之外，仍继续守住仓库全局基线：

- `cd myCPU && make test`
- `cd myCPU && make test-pipeline`

## 风险与取舍

- 把向量访存延后到 commit boundary，会让 `pipeline` 在 `V1` 中显得比标量路径更保守，但这是有意识的收窄，用来避免现在就把 LSQ / memory path 扩成第二条复杂主线。
- `vdot.vv` 当前结果写回向量寄存器低 64 bit，而不是 GPR 或 widening vector accumulator；这牺牲了一部分易用性，但换来更小的 rename / forwarding 影响面。
- 固定 128-bit `VLEN` 会限制第一代规模，但足以支撑早期 int8 / int16 workload 验证，也更利于后续 host smoke 保持窄而稳定。

## 当前有效性说明

- 当前有效 / 历史语境：当前有效，作为 `V-lite` `V0 / V1` 的最小工程设计边界。
- 当前是否已经落地，以及后续是否需要继续扩到 `V2`，以 [../status/mainline_status.md](../status/mainline_status.md) 和 [../plan/history_plan.md#vector-v0-v1-plan](../plan/history_plan.md#vector-v0-v1-plan) 为准。
