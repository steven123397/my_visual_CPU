# Phase 3-A 分支预测增强设计

## 文档定位

本文档用于说明在 `phase2-stable` 基线之后，如何以最小可控切片开启 `Phase 3` 的第一轮工程工作：在保持当前 `pipeline` 仍为 in-order 执行模型的前提下，引入分支预测增强与最小可观察性。

本文档重点定义：

- 为什么 `Phase 3` 第一轮只收口到分支预测增强
- 这一轮的目标与非目标
- 与当前 `pipeline`、`debug/frontend`、最小可交互 Monitor OS 路线的边界
- 首轮验证口径与冲突控制方式

本文档不承担实时进度更新。当前进展请写入对应 `status` 文档；若后续正式启动该路线，再新增对应 `plan` 文档。

## 关联文档

- 状态文档：
  - [status/mainline_status.md](/home/liangjiaqi/projects/my_visual_CPU/docs/status/mainline_status.md)
- 相关计划：
  - 当前暂无；后续若正式启动实现，再新增对应 `plan` 文档并回链本文档。

## 背景与问题

当前仓库已经不是纯设计稿，而是一个已可运行的模拟器原型。`phase2-stable` 冻结点之前，主线已经完成 `pipeline core` 正式接入、`debug/frontend` 教学演示链路接入，以及一轮系统性的 `Phase 1` 稳定化与 `Phase 2` 验证补强。当前 `pipeline` 的高风险差分主干与快照 / 协议门禁已经基本形成闭环，适合在不打破既有收口的前提下，开始讨论更高级微架构。

根文档对 `Phase 3` 的高层目标是“在可测试前提下推进预测、重命名、`ROB`、`LSQ` 等高级微架构”。但如果第一轮就同时引入预测、重命名、`ROB`、`LSQ`，会直接撞上当前刚冻结好的 `pipeline` 差分门禁、`debug/frontend` 可观察性边界，以及后续计划中的最小可交互 Monitor OS 路线。这样会把 `Phase 3` 从“可测试的高级微架构前进”变成“高风险大分支”，不符合当前主线强调的小步收口。

因此，`Phase 3` 第一轮明确收口为 `Phase 3-A`：只做分支预测增强，仍保持 in-order pipeline。这样可以让 `Phase 3` 方向真正启动，同时把结构和验证风险控制在当前主线可承受范围内，并为后续 `B / C` 阶段的重命名、`ROB`、`LSQ` 与初步 `OoO` 留出清晰演进路径。

## 目标

- 在当前 `pipeline` 上引入最小但真实可验证的分支预测能力，而不是停留在“永远顺序取指 + redirect”模型。
- 保持当前执行模型仍为 in-order，不引入重命名、`ROB`、`LSQ` 或乱序提交。
- 保持 `functional + shared InstructionSemantics` 作为唯一 ISA 语义真值来源；预测只影响取指方向与性能行为，不影响 architected 语义来源。
- 为预测器补最小可观察性，使 host-side smoke / differential 和后续教学演示能够看到 predictor 的核心状态变化。
- 把 `Phase 3-A` 的前端 ownership 明确收口在 snapshot / 协议层，不直接主导前端 UI 改造，避免与最小可交互 Monitor OS 路线冲突。

## 非目标

- 不引入寄存器重命名、物理寄存器文件、free list。
- 不引入 `ROB`、乱序提交或精确异常恢复的新提交模型。
- 不引入 `LSQ`、store-to-load forwarding、memory disambiguation。
- 不把当前 `pipeline` 从 5-stage in-order 后端改造成最小 `OoO` 核心。
- 不直接改造前端桌面壳、终端窗口或其交互模型。
- 不为了 predictor 演示而重做现有 `debug/frontend` 的主界面结构。

## 约束与边界

- `functional` 的行为定义继续优先于 `pipeline`；预测器只能影响 `pipeline` 内部取指与 redirect 行为，不能成为新的语义来源。
- 当前 `pipeline` 的 trap / interrupt / CSR / MMIO / privilege transition 合同已形成高风险主干门禁；`Phase 3-A` 不得为了预测器而削弱这些门禁。
- 分支预测失败只能通过现有 flush / redirect 机制恢复，不得顺手引入新的提交语义。
- 预测器的可观察性优先收口到 `BackendDebugSnapshot` / `DebugSnapshot` 与协议字段，不直接要求前端 UI 一起重构。
- 当前前端 ownership 默认归最小可交互 Monitor OS 路线；若 `Phase 3-A` 需要 UI 变化，必须限定为最小、可并行、低冲突的增强，而不是主导 `frontend` 结构。
- 首轮预测器命中率、容量、策略不追求“最优”，只追求：结构清晰、行为可解释、回归可验证。

## 方案

### 结构设计

`Phase 3-A` 的结构目标不是重写 `pipeline`，而是在现有 5-stage backend 外围补上一层最小 predictor 子系统：

```text
PipelineBackend
  -> fetch PC selection
  -> predictor query
  -> predicted next PC
  -> normal IF/ID/EX/MEM/WB flow
  -> branch resolved in execute/redirect point
  -> predictor update
  -> existing flush / redirect path
```

这意味着首轮重点集中在两处：

- 取指前：根据当前 PC 查询 predictor，得到 predicted direction / target
- 分支决议后：根据真实结果更新 predictor，并在 mispredict 时走现有 flush / redirect

现有 `pipeline_backend.cpp` 中与 fetch、redirect、flush、commit-boundary interrupt 相关的主逻辑仍然保留。`Phase 3-A` 不应把 predictor 逻辑四散塞进 backend 各处，而应尽量形成独立、可测试的预测模块或 predictor state 子结构。

### 接口 / 数据 / 契约

#### 1. 预测器内部合同

首轮不要求复杂 predictor 组合，但至少需要明确以下最小内部语义：

- 查询接口：给定 fetch PC，返回“是否预测跳转”与“预测目标地址”
- 更新接口：给定分支 PC、真实 taken/not-taken、真实 target，更新 predictor 状态
- 清理接口：reset 时 predictor 状态回到已知初始值

实现上可以从简单策略起步，例如静态 predict-not-taken、再逐步切到最小动态 predictor。但本文档不把“必须使用哪一种具体 predictor”写死，因为首轮重点是结构边界和验证闭环，而不是算法竞赛。

#### 2. 对现有 pipeline 的影响合同

首轮允许 predictor 改变：

- fetch 阶段的 next PC 选择
- mispredict 时的 flush / redirect 触发频率
- backend debug snapshot 中与预测相关的可观察字段

首轮不允许 predictor 改变：

- 指令提交顺序
- trap / interrupt 的提交边界
- `functional` 的参考行为
- 共享 `InstructionSemantics` 的 ISA 语义

#### 3. debug / snapshot 合同

`Phase 3-A` 允许补“激进型最小可观察性”，但这里的“激进”指 snapshot / 协议字段可以比 Phase 2 更多，不代表它拥有前端 UI ownership。

首轮建议至少暴露：

- predictor 类型或模式名
- 最近一次预测是否命中
- 最近一次预测 PC
- 最近一次预测目标
- 最近一次 mispredict redirect 信息

如果需要，还可以暴露很小的 predictor 统计计数，例如：

- total predictions
- correct predictions
- mispredictions

但不应在首轮就把整个 predictor table 转成前端可视化矩阵；这类展示容易造成与 Monitor OS 路线在 `frontend` 层的高冲突。

#### 4. 与最小可交互 Monitor OS 路线的合同

并行开发时，默认边界如下：

- Monitor OS 路线拥有 `frontend` 的桌面壳、终端窗口和输入链路
- `Phase 3-A` 拥有 predictor 内部实现、host-side 回归和 snapshot 字段
- 如果 `Phase 3-A` 需要 UI 可见性，应尽量通过现有 debug 面板的小字段扩展或延后到 OS 分支合并后再接

这个边界的目的不是限制 `Phase 3-A` 的演示能力，而是避免两条线同时重做 `frontend/app/*`。

### 验证思路

`Phase 3-A` 的验证重点不是“性能跑分”，而是“预测增强没有破坏既有合同，并且自身行为可观察、可复现”。

验证至少应覆盖 4 层：

1. predictor 层
   - 针对 predictor 查询 / 更新 / reset 的最小单元或 host-side smoke

2. pipeline 行为层
   - 扩 `pipeline_backend_smoke`，覆盖：
   - predict-hit 路径
   - mispredict 后 flush / redirect 路径
   - predictor reset 后回到已知状态

3. differential 层
   - 继续维护 `functional vs pipeline` 的 architected 一致性
   - 新增 predictor 后，既有 trap / interrupt / privilege / MMIO differential 不得回归
   - 如新增 predictor 相关场景，应重点验证“预测改变性能路径，但不改变最终架构结果”

4. debug / 协议层
   - `debug_cli_smoke` 或同类 host-side smoke 应能看到新增 predictor 字段
   - 如果首轮补最小 UI 字段，也应由 `frontend` 测试门禁守住其稳定性

基线验证仍至少包括：

- `cd myCPU && make test`
- `cd myCPU && make test-pipeline`
- `cd frontend && node --test`

如果后续为 `Phase 3-A` 新增专门验证入口，建议使用类似：

- `cd myCPU && make test-host-predictor_smoke`

## 风险与取舍

- 先做预测增强而不做重命名 / `ROB` / `LSQ`，意味着第一轮性能收益有限，也会留下一个“结构上仍是 in-order”的过渡形态；但这是有意取舍，用来换取更小的改动面和更好的回归控制。
- 允许“激进型最小可观察性”，能提升教学演示和调试价值，但如果直接把它扩成前端 UI 大改，会与 Monitor OS 路线形成高冲突区，因此本文档明确把 ownership 优先收口在 snapshot / 协议。
- 如果 predictor 设计得过于复杂，会快速侵蚀当前 `pipeline_backend.cpp` 的清晰度；因此首轮应优先把 predictor 逻辑抽成独立 state / helper，而不是把策略细节散落到 fetch / execute / redirect 各分支里。
- `Phase 3-A` 若不能在现有差分门禁下保持 architected 一致性，就不应继续推进到 `B / C`；因此首轮成功标准不是“预测器看起来跑起来了”，而是“它在当前回归体系下站得住”。

## 当前有效性说明

- 当前有效 / 历史语境：当前有效，作为 `phase2-stable` 之后 `Phase 3` 第一轮工作的结构边界说明。
- 当前正式进展以 [status/mainline_status.md](/home/liangjiaqi/projects/my_visual_CPU/docs/status/mainline_status.md) 为准；若后续为 `Phase 3-A` 建立专门 `status` 文档，再以对应文档承载实时进度。
