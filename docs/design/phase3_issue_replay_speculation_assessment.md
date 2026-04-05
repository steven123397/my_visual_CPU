# `Phase 3` issue / replay / speculation 后续取舍设计

## 文档定位

本文档用于回答一个当前已经真正开放、且需要先收口清楚的问题：在 decode 级 `BlockedByUnresolvedStore` 最小收窄已经落地之后，是否值得继续在当前基线上主动扩更激进的 `issue / replay / speculation`。

本文档重点回答：

- 当前基线已经做到什么程度
- 继续扩更激进行为的真实收益和真实成本分别是什么
- 当前主线应不应该继续主动推进这条线
- 如果未来重开，这一轮最小切片应该先落在哪

本文档不承担实时进度更新。当前主线状态与下一步，以 [status/mainline_status.md](../status/mainline_status.md) 和 [status/project_priority_roadmap.md](../status/project_priority_roadmap.md) 为准。

## 关联文档

- 状态文档：
  - [status/mainline_status.md](../status/mainline_status.md)
  - [status/project_priority_roadmap.md](../status/project_priority_roadmap.md)
- 相关设计：
  - [phase3_ooo_execution_model_design.md](phase3_ooo_execution_model_design.md)
  - [blocked_by_unresolved_store_boundary.md](blocked_by_unresolved_store_boundary.md)
  - [pipeline_speculation_contracts.md](pipeline_speculation_contracts.md)
- 已完成计划归档：
  - [plan/history_plan.md#phase3-ooo-execution-plan](../plan/history_plan.md#phase3-ooo-execution-plan)
  - [plan/history_plan.md#phase3-blocked-by-unresolved-store-boundary-plan](../plan/history_plan.md#phase3-blocked-by-unresolved-store-boundary-plan)

## 背景与当前基线

当前仓库已经是一个已可运行的模拟器原型。`Phase 3-B/C` 的首轮目标也已经达成：`pipeline` 已接上最小 `rename + ROB + LSQ +` 真实 `OoO execute` 主路径，同时继续保持单发射、顺序退休、MMIO non-speculative 和统一 commit boundary。

decode 级 `BlockedByUnresolvedStore` 的第一轮专项收窄也已经完成：

- `BlockedByUnresolvedStore` 现在只保留给 older store 地址未知场景。
- older store 地址已知但 data 未 ready 时，非重叠 younger load 不再被全局阻塞。
- 重叠场景继续暴露 `BlockedByOverlappingStore`。
- 晚到的 older store 地址如果与已放行的 younger load 重叠，则继续走现有 `ReplayRequired` + coarse replay flush。

因此，当前真正的问题已经不再是“这条 decode 边界该怎么定义”，而是“在这条边界已经收窄后，继续往更激进的 `issue / replay / speculation` 扩，到底值不值得”。

## 当前实现对继续扩的真实约束

### 1. decode 级 load 分类仍然直接决定前端是否停住

当前年轻 load 的阻塞判断仍然发生在 decode 入口；一旦 `classify_load()` 返回阻塞状态，`step_id()` 会直接把整个前端停住，而不是把这条 load 放进独立 issue 队列后继续让后面的独立指令前进。

这意味着：即使继续放宽 memory speculation，如果没有先把 decode / rename 和后续 issue 解耦，很多潜在收益仍然拿不到。

### 2. memory execute 资源仍然是单通道

当前 `pipeline` 仍只有一个 `ex_mem` memory 路径。load / store 只要命中这条路径占用，就会继续反压后续 memory 指令。

这意味着：在没有先扩 issue 组织方式和 memory execute 资源之前，单纯让更多 load“先发出去”，可兑现的吞吐提升很有限。

### 3. replay / recovery 仍然是 coarse rollback 到 committed boundary

当前 automatic replay 直接复用现有 committed rollback + flush 主路径。一旦命中 `ReplayRequired`，backend 会整体回滚到 committed boundary，而不是只回放必要的 younger 指令或单条 violating load。

这条路径当前是合理的，因为它简单、可解释、已被 smoke 和 debug snapshot 守住；但如果继续主动扩大 replay 触发面，就等于主动放大整机级 flush 频率和调试复杂度。

### 4. 当前 `LSQ` 合同仍然是有意保守的最小形态

当前 `LSQ` 只支持：

- `BlockedByUnresolvedStore`
- `BlockedByOverlappingStore`
- `ReplayRequired`
- `RAM-only`、full-cover store-to-load forwarding

它并没有进入 partial merge、复杂多 store disambiguation、细粒度 violation bookkeeping 或 speculative MMIO。继续扩大 `issue / replay / speculation`，很快就会越过“现有合同补洞”边界，进入一轮新的 `LSQ` / recovery 设计。

### 5. 当前可观察性和门禁成本已经接近这条基线的合理上限

当前 `pipeline` 已经把 `LSQ` 状态、replay flush、rollback、MMIO non-speculative、commit boundary 和 debug snapshot 一起接成稳定门禁。继续扩更激进行为，不会只多几条测试，而是会显著抬高：

- host smoke 的组合面
- debug snapshot 的解释负担
- `functional` vs `pipeline` 差分与 precise contract 的维护成本

对当前主线目标来说，这部分复杂度是真成本，不是“顺手就能补”的小尾巴。

## 方案比较

### 方案 A：把当前基线视为 `Phase 3` 这一轮的够用完成态

- 做法：不再主动继续扩更激进的 `issue / replay / speculation`，后续只做 bug-driven hardening。
- 优点：最符合当前主线优先级，风险最低，不会把 reference correctness、guest 基线和 debug 可观察性重新拖进高风险大改。
- 缺点：会保留一部分可见但尚未兑现的并行度。

### 方案 B：当前不主动扩，但把未来重开条件和最小切片写清楚

- 做法：主线结论仍然是不主动扩；同时明确哪些证据出现时才值得重开，以及如果重开，第一刀应该切在哪里。
- 优点：既能结束当前反复摇摆的“要不要继续扩”讨论，又不会把未来路线彻底写死。
- 缺点：短期没有新的微架构行为增量，更多是决策收口。

### 方案 C：现在就继续主动扩更激进的 `issue / replay / speculation`

- 做法：开始放宽未知地址 load 放行、引入更细的 replay / recovery，或继续把 issue 组织方式做得更激进。
- 优点：理论上能继续释放一部分 memory-side 并行度。
- 缺点：在当前 decode 级阻塞、单 memory execute 通道和 coarse rollback 仍然存在的前提下，边际收益很可能小于实现与验证成本。

## 推荐结论

当前推荐采用方案 B。

它的工程含义其实很直接：

- 当前主线不再主动继续扩更激进的 `issue / replay / speculation`。
- 当前 `Phase 3` 的最小 `rename + ROB + LSQ +` 真实 `OoO execute`，连同最近完成的 decode 级 `BlockedByUnresolvedStore` 收窄，已经构成一条“够用且可维护”的阶段性基线。
- 后续如果没有新的真实 workload 证据或明确研究目标，不应为了继续“往前推一点微架构行为”而主动打开新一轮恢复 / 验证复杂度。

换句话说，当前最值得做的不是继续追求更多 speculate，而是把已经落地的基线继续稳住。

## 为什么当前不值得主动继续扩

1. 继续放宽 unknown-address load speculation 的主要收益，要以更高 replay 频率为代价；而当前 replay 仍然是整机级 coarse flush，这个代价偏大。
2. 当前 decode 仍会被 blocking load 直接卡住；如果先不做 issue decoupling，只加更激进 speculation，收益会被前端背压吃掉一大截。
3. 当前 memory execute 路径仍然单通道，进一步放行 memory op 的可兑现收益有限。
4. 当前项目主线优先级仍然是 reference correctness、guest/kernel 基线和 bug-driven hardening，而不是主动追求下一档 memory-side 并行度。

## 未来重开条件

只有在出现以下任一条件时，才值得重新打开这条线：

1. 真实 guest / kernel workload 已经证明：当前 stall 热点主要来自这条 memory-order 保守边界，而不是其他更低成本问题。
2. 项目明确新增了“需要展示或研究更激进 issue / replay / memory disambiguation 行为”的目标，而不只是想继续把 `Phase 3` 往前推一点。
3. 可以接受随之而来的结构性工作：更细粒度 rollback / replay、更多可观察性字段，以及更厚的 host smoke / differential 门禁。

## 如果未来重开，第一刀应该切在哪里

如果未来真的重开，第一优先级不应是“直接放宽 unknown-address load speculation”，而应优先评估更窄的 issue decoupling 切片，例如：

- 让被 older store 阻塞的 load 不再直接冻结 decode / rename。
- 把“前端继续发射独立 younger ALU”与“memory 指令暂时等待可 issue 条件”拆开。
- 继续复用现有 commit boundary、MMIO non-speculative 和 precise rollback 合同，不同时扩大多条高风险边界。

原因很简单：在当前实现里，真正限制收益兑现的，首先是 decode 级背压，而不是 replay 机制还不够激进。

只有在 issue decoupling 已经证明有收益之后，才值得进一步评估：

- 是否需要更细粒度的 selective replay
- 是否值得放宽 unknown-address load speculation
- 是否要继续扩大 `LSQ` 的 disambiguation / forwarding 行为

## 非目标

- 当前不新开主动实现计划。
- 当前不承诺新的 issue queue、reservation station 或 selective replay 设计细节。
- 当前不改变 MMIO non-speculative、顺序退休和 architected commit boundary 的既有口径。
- 当前不把这份取舍文档写成未来必做路线图。

## 当前有效性说明

- 当前有效 / 历史语境：当前有效，作为 decode 级 `BlockedByUnresolvedStore` 第一轮收窄之后，`Phase 3` 是否继续扩更激进 `issue / replay / speculation` 的主线判断。
- 当前实时状态与后续是否重开，以 [status/mainline_status.md](../status/mainline_status.md) 和 [status/project_priority_roadmap.md](../status/project_priority_roadmap.md) 为准。
