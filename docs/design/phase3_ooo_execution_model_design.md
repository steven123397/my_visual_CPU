# Phase 3-B/C OoO 执行模型设计

## 文档定位

本文档用于说明在当前 `Phase 3-A` 分支预测增强已经落地之后，后续真正进入 `rename / ROB / LSQ / OoO execute` 之前，首轮 `Phase 3-B/C` 应采用什么执行模型边界。

它重点回答：

- 首个“大块 OoO”到底接到什么程度
- 哪些能力属于本轮目标，哪些明确不做
- 当前 in-order `pipeline` 还存在哪些结构障碍
- `rename / ROB / LSQ` 应按什么顺序接线

本文档不承担实时进度更新。当前推进情况请以 [status/mainline_status.md](../status/mainline_status.md) 与对应计划文档为准。

## 关联文档

- 状态文档：
  - [status/mainline_status.md](../status/mainline_status.md)
  - [status/project_priority_roadmap.md](../status/project_priority_roadmap.md)
- 相关计划：
  - [plan/history_plan.md#phase3-ooo-execution-plan](../plan/history_plan.md#phase3-ooo-execution-plan)
  - [plan/history_plan.md#phase3-ooo-readiness-plan](../plan/history_plan.md#phase3-ooo-readiness-plan)

## 背景与问题

当前仓库已经是一个已可运行的模拟器原型。`functional` reference path、`pipeline` backend、`debug/frontend` 教学演示链路，以及 `Phase 3-A` 的最小分支预测增强都已经落地。现在真正的问题不再是“能不能继续往高级微架构方向走”，而是“下一步应该以什么最小切片进入 `rename / ROB / LSQ / OoO`，同时不破坏现有 correctness 和可观察性基线”。

如果直接在当前 `PipelineBackend` 上混合引入 `rename`、`ROB`、`LSQ`、更复杂的 speculate / flush / replay 规则，很容易把现有 `pipeline` 主路径、debug 快照、差分门禁和 trap / interrupt 精确边界一起卷进一次高风险大改。这样会让 `Phase 3` 从“可测试地推进高级微架构”退化成“边界未定义的后端重写”。

因此，`Phase 3-B/C` 的首要目标不是尽快把所有 OoO 部件都接上，而是先明确第一轮执行模型的完成态：继续保持单发射、统一 ISA 真值来源和 in-order retire，只把执行与结果缓存逐步从当前 in-order 5-stage 后端推进到“可以安全承载 `rename / ROB / LSQ`”的形态。

## 目标

- 在不改变 `functional + shared InstructionSemantics` 为唯一 ISA 真值来源的前提下，为首轮 `Phase 3-B/C` 定义清晰、可测试、可演进的执行模型。
- 明确首轮 `OoO` 仍保持单发射 fetch / decode 与 in-order retire，不把 superscalar、cache、复杂 memory speculation 一起混入。
- 明确 `rename`、`ROB`、`LSQ` 的接线顺序与相互依赖，避免实现阶段边做边改模型。
- 为 precise exception、interrupt、MMIO、CSR、`mret/sret`、`sfence.vma` 等高风险边界预留统一 contract，而不是让这些语义散落在 backend 分支里。
- 让后续“真正的大块 OoO 接线”成为一轮结构清晰的实现任务，而不是再反过来补定义和补边界。

## 非目标

- 不在首轮 `Phase 3-B/C` 中引入 superscalar fetch / decode / issue。
- 不在首轮中引入 cache hierarchy、DMA、multicore 或一致性模型。
- 不在首轮中引入激进的 memory disambiguation、load speculation replay storm 或复杂 predictor 组合。
- 不让 `functional` 变成新的微架构实验场；它继续只负责 reference 真值。
- 不把 `debug/frontend` 扩成断点、条件暂停或任意镜像加载的通用调试器。

## 约束与边界

- `functional` 与共享 `InstructionSemantics` 继续定义 architected 语义；`Phase 3-B/C` 只改变 `pipeline` 的调度、暂存和提交模型。
- 首轮 `Phase 3-B/C` 仍应保持单条指令按程序序进入 rename / ROB / LSQ，architected state 按程序序退休。
- 所有 trap、interrupt、CSR、MMIO、TLB / `sfence.vma` 可见性，都必须继续沿“architectural commit boundary”解释，不得在 speculate 阶段偷偷生效。
- 当前 `pipeline` 的 debug snapshot、host smoke、differential 和前端协议已是正式门禁；首轮设计必须允许这些门禁继续维护，而不是先拆掉再重建。

## 方案

### 当前 in-order backend 的结构障碍

当前 `PipelineBackend` 已能正确处理 5-stage in-order、forwarding、load-use hazard、flush / redirect、trap / interrupt 和最小分支预测，但它仍保留几类会阻碍 OoO 接线的结构问题：

- 年龄顺序和退休记录主要隐含在 backend 内部流程里，而不是稳定、显式的数据面。
- architected side effect 与局部阶段行为仍有耦合，commit boundary 还没有被抽成独立 contract。
- stage state、redirect、pending fault、hazard / forwarding helper 仍然高度集中在单体 backend 实现文件里。
- `rename`、`ROB`、`LSQ` 所需的数据结构和接口目前不存在；如果在正式接线时才一起发明，会把结构设计和行为实现混在一次改动里。

因此，首轮 `Phase 3-B/C` 的合理入口不是直接“把当前五级后端改成 OoO”，而是先把这些结构障碍拆掉，再进入真正的接线阶段。

### Phase 3-B：rename + ROB 最小接线

`Phase 3-B` 的目标，是在保持单发射和 in-order retire 的前提下，把“执行结果先进入 ROB、architected state 在 head commit 时生效”这一层结构搭起来。

这一阶段建议明确采用：

- 单发射 fetch / decode / rename
- 物理寄存器重命名
- in-order ROB retire
- 分支 checkpoint / rollback
- 仍不引入真正的 load/store 乱序完成

首轮 `Phase 3-B` 里，分支 checkpoint 是值得保留的最小能力，因为没有 checkpoint，分支错误恢复会继续和现有 in-order backend 的 flush 状态强耦合，不利于后续扩展；但这组 checkpoint 只需要满足“单发射、单分支恢复、最小 rollback”，不需要上复杂的多级恢复策略。

这一步完成后，backend 的主语义应变成：

- decode/rename 分配 sequence / ROB slot / destination phys reg
- execute 产出 speculative result
- result 标记 ROB ready
- 只有 ROB head 在满足 commit 条件后，architected state 才真正变化

### Phase 3-C：LSQ + OoO execute 最小接线

`Phase 3-C` 再在 `Phase 3-B` 的基础上接入 `LSQ` 和最小 `OoO execute`，但仍然保持首轮工程边界克制：

- ALU 指令可在不破坏顺序退休的前提下提前完成
- load / store 进入 LSQ 管理
- store 只在 commit 时真正落到 RAM / MMIO
- MMIO load / store 维持非投机规则
- 第一轮不追求 aggressive memory speculation

也就是说，`Phase 3-C` 的首个完成态不是“功能尽量多”，而是“执行可以乱序，退休仍然精确，memory 语义和设备 side effect 仍然守得住”。

### 建议接线顺序

后续真正进入实现时，建议顺序如下：

1. 先补 sequence / retire trace 与 commit boundary contract。
2. 再把 `PipelineBackend` 的状态和 helper 拆成可替换组件。
3. 再引入未接线的 `rename_map / ROB / LSQ` helper，并用单元 / smoke 守住接口。
4. 开始 `Phase 3-B`：接 `rename + ROB`，仍维持顺序 execute 或近似顺序 execute。
5. 最后做 `Phase 3-C`：接 `LSQ + OoO execute`，继续保持 in-order retire 和 precise exception。

这个顺序的核心是：先让结构与契约可测试，再让执行模型变复杂。

### 完成定义

当首轮 `Phase 3-B/C` 进入正式实现时，“可以开始”的前提应是：

- 已有明确的 sequence / retire trace 观测面。
- 已有显式的 architectural commit boundary helper。
- `PipelineBackend` 已不再是难以替换的单体。
- `rename_map / ROB / LSQ` 已存在稳定接口与独立门禁。
- 投机执行下的 precise exception / interrupt / MMIO / CSR / TLB 合同已经写成正式设计。

达到这几个条件之后，下一份实现计划才应真正去接“大块 `OoO / rename / ROB / LSQ`”。

## 风险与取舍

- 明确要求单发射和 in-order retire，会让首轮 `Phase 3-B/C` 的性能野心受到约束，但这是有意取舍，用来换取更低的验证风险和更清晰的架构边界。
- 先补结构与 contract，再做真正 OoO 接线，会延后“看到更多微架构行为”的时间点，但能显著降低大分支返工概率。
- 引入 branch checkpoint 但不同时引入复杂 speculation，会留下一个过渡形态；但这个过渡形态对当前仓库是合理的，因为它能保持与现有 debug / differential 基线兼容。

## 当前有效性说明

- 当前有效 / 历史语境：当前有效，作为 `Phase 3-A` 之后首轮 `Phase 3-B/C` 的执行模型设计边界。
- 当前实时进展、已完成的首轮收口结果以及下一步是否继续扩更激进的 issue / replay / memory speculation，以 [status/mainline_status.md](../status/mainline_status.md)、[status/project_priority_roadmap.md](../status/project_priority_roadmap.md)、[plan/history_plan.md#phase3-ooo-execution-plan](../plan/history_plan.md#phase3-ooo-execution-plan) 与 [plan/history_plan.md#phase3-ooo-readiness-plan](../plan/history_plan.md#phase3-ooo-readiness-plan) 为准。
