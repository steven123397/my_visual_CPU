# `BlockedByUnresolvedStore` 串行化边界设计

## 文档定位

本文档用于说明当前 `Phase 3` 中 decode 级 `BlockedByUnresolvedStore` 应如何收口为更明确的串行化边界，以及为什么这一步应先于更激进的 issue / replay / memory speculation 扩展。这里的“decode 级”特指 decode 入口对 load 分类的放行边界，以及这条分类结果在后续 issue 前沿用的同一判定语义。

本文档重点回答：

- 当前 `BlockedByUnresolvedStore` 为什么过于保守
- decode 级到底应在什么条件下阻塞年轻 load
- 这一步允许和不允许扩大到什么程度
- 需要守住哪些现有 `LSQ`、commit-boundary 和 debug 可观察性合同

本文档不承担实时进度更新。当前推进情况请以 [status/mainline_status.md](../status/mainline_status.md) 与相关状态文档为准。

## 关联文档

- 状态文档：
  - [status/mainline_status.md](../status/mainline_status.md)
  - [status/project_priority_roadmap.md](../status/project_priority_roadmap.md)
- 相关计划：
  - [plan/history_plan.md#phase3-blocked-by-unresolved-store-boundary-plan](../plan/history_plan.md#phase3-blocked-by-unresolved-store-boundary-plan)
- 相关设计：
  - [phase3_ooo_execution_model_design.md](phase3_ooo_execution_model_design.md)
  - [pipeline_speculation_contracts.md](pipeline_speculation_contracts.md)

## 背景与问题

当前仓库已经是一个已可运行的模拟器原型。`Phase 3-B/C` 首轮最小真实 `OoO execute` 已经接上 `rename + ROB + LSQ` 主路径，也已经把 `BlockedByUnresolvedStore`、`BlockedByOverlappingStore` 和 `ReplayRequired` 这些 `LSQ` 状态显式暴露到 host smoke 与 debug snapshot。

现在真正开放的问题不是抽象的“要不要继续做 memory speculation”，而是当前 decode 级 load 分类对老 store 未解析状态的处理仍然过于保守。现有 `classify_load()` 逻辑里，只要存在更老 store 且其 `address` 或 `data` 尚未 ready，年轻 load 就会被直接标记为 `BlockedByUnresolvedStore`。这意味着即便老 store 的地址已经明确、而年轻 load 明显不重叠，也仍然会被这条 decode 级串行化边界挡住。

这条边界之所以需要单列成专项问题，是因为它正处在当前 `Phase 3` 最敏感的交界处：一边连着 `LSQ` 的最小 correctness contract，另一边连着更激进的 issue / replay / disambiguation 演进方向。如果不先把这条边界收口清楚，后续任何更激进的内存推测都会建立在模糊前提上。

## 目标

- 把 `BlockedByUnresolvedStore` 收口为更窄、更可解释的 decode 级阻塞条件。
- 保持当前 `LSQ` 状态机、host smoke 和 debug snapshot 的核心观测面连续可用。
- 在不引入更激进 memory speculation 的前提下，消除“地址已知但仍全局阻塞”的明显过保守边界。
- 继续守住 RAM / MMIO、trap / interrupt、rollback 和 commit boundary 的现有合同。
- 为后续是否继续扩 issue / replay / memory disambiguation 提供更清晰的基线。

## 非目标

- 不在这一步引入“地址未知也先发射，靠后续 violation 检测统一 replay”的更激进行为。
- 不在这一步扩展新的 `LSQ` 状态种类或更复杂的 replay 风暴治理策略。
- 不改变 MMIO non-speculative、顺序退休或 architected commit boundary 的既有口径。
- 不把这一步扩成完整 memory disambiguation 设计。

## 约束与边界

- `functional + shared InstructionSemantics` 继续是唯一 ISA 真值来源；本专项只调整 `pipeline` 内部 decode 级 load 分类对阻塞边界的定义。
- `ReplayRequired` 继续表示“已经放行的 load 后来被确认需要重放”；本专项不主动扩大它的触发范围。
- `BlockedByOverlappingStore` 继续表示“更老 store 地址已知、但顺序上尚未允许，且与 load 明确重叠”。
- `BlockedByUnresolvedStore` 在本专项之后应只表示“更老 store 的地址尚未知，因而当前无法判断是否冲突”。
- MMIO load / store 继续维持 non-speculative 规则；本专项不把 MMIO 推向更激进行为。
- debug snapshot 中现有 `lsq_load_state / lsq_load_sequence_id / lsq_store_sequence_id` 观测面应继续保持可解释。

## 方案

### 当前问题的最小抽象

当前 decode 级 load 分类逻辑把两类情况混在了同一个 `BlockedByUnresolvedStore` 里：

1. 更老 store 的地址未知，确实无法判断与年轻 load 是否冲突。
2. 更老 store 的地址已知、数据未知，但当前实现仍然把年轻 load 一律挡住。

第一类是必要保守；第二类则过宽。因为一旦更老 store 的地址已知，当前 decode 级分类已经具备最基本的冲突判断条件：

- 如果年轻 load 与该 store 不重叠，就没有必要继续把它归入“未解析 store 阻塞”。
- 如果年轻 load 与该 store 重叠，则应更准确地落到 `BlockedByOverlappingStore`，而不是继续使用“未解析”这类更宽泛的标签。

因此，本专项的核心不是新增一种复杂机制，而是把当前过宽的保守边界拆回更准确的两类语义。

### 推荐方案：收窄到“未知地址阻塞”

本专项推荐采用如下规则：

- 如果存在更老 store，且其地址尚未知，则年轻 load 继续返回 `BlockedByUnresolvedStore`。
- 如果更老 store 地址已知但数据未 ready：
  - 与年轻 load 不重叠：不再因 `BlockedByUnresolvedStore` 被 decode / issue 阻塞。
  - 与年轻 load 重叠：继续返回 `BlockedByOverlappingStore`。
- 如果更老 store 地址、数据都已 ready，但 `order_ready` 尚未满足：
  - 维持当前 `BlockedByOverlappingStore` 逻辑。
- `ReplayRequired` 继续只承接“已放行 load 后来被确认与更老 store 冲突”的既有路径，不在本专项中新增更激进触发面。

这条规则的直接含义是：`BlockedByUnresolvedStore` 的名字重新与它的真实语义对齐，不再偷偷承载“地址已知但仍然全局阻塞”的额外保守行为。

### 当前建议：先按这条边界落最小实现

当前主线建议先按上面的边界收口最小实现与回归，再根据结果判断是否值得继续扩大到更激进的 issue / replay / speculation。本文档描述的是当前推荐方向，不表示后续不会因实现验证结果而继续细化。

### 接口 / 数据 / 契约

对当前 `LSQ` 而言，本专项不要求新增新的长期状态机类型，而是要求把现有状态映射解释得更清楚：

- `BlockedByUnresolvedStore`
  - 仅在 older store 地址未知时返回。
  - 它表达的是“无法判断冲突”，而不是“older store 任一字段未 ready”。
- `BlockedByOverlappingStore`
  - 在 older store 地址已知且与 load 重叠、但顺序上仍不可放行时返回。
  - older store 的 `data_ready` 是否满足，不再成为“非重叠 load 也被阻塞”的理由。
- `ReplayRequired`
  - 仍然只表达“已经越过 decode 级分类边界、后续被确认需要重放的 load”。

因此，当前专项更像是 `classify_load()` 判定条件的重排和语义收口，而不是对外 contract 的大改版。

### 与现有 commit-boundary 合同的关系

本专项只收窄 decode / issue 阻塞边界，不改变以下 contract：

- store 仍只在 commit boundary 真正落 RAM / MMIO。
- MMIO 仍然 non-speculative。
- trap / interrupt / branch-mispredict flush 仍需统一回滚 younger speculative state。
- debug snapshot 继续暴露当前可见 `LSQ` 状态，而不是隐藏在 backend 内部。

换句话说，这一步只是在“哪些 load 可以更早进入执行窗口”上做最小收窄，而不是让 architected side effect 提前生效。

### 为什么不在这一步直接走更激进的 replay / speculation

更激进的方案，是即使 older store 地址未知，也允许年轻 load 先发射，再通过后续 violation 检测与 replay 来兜底。这种方案理论上吞吐更高，但它会同时扩大以下风险面：

- `ReplayRequired` 的触发条件和频率显著增加。
- `LSQ` 需要更重的 violation 检测与后续恢复语义。
- 当前 host smoke 和 debug snapshot 的解释复杂度会上升。
- 更容易把当前问题从“收窄串行化边界”直接放大成“重新设计 memory disambiguation”。

而当前主线文档和优先级路线图已经明确要求：先把 decode 级 `BlockedByUnresolvedStore` 边界单列成专项问题，再决定是否继续做更激进的 issue / replay / speculation。因此，本专项有意不跨过这条线。

## 验证思路

本专项的验证应优先围绕现有 `LSQ` 和 `pipeline` smoke 做最小补洞，而不是新增大而宽的 mega-smoke。

至少需要直接守住：

- older store 地址未知时，年轻 load 继续得到 `BlockedByUnresolvedStore`。
- older store 地址已知、数据未 ready、且与年轻 load 不重叠时，年轻 load 不再被 `BlockedByUnresolvedStore` 挡住。
- older store 地址已知、数据未 ready、且与年轻 load 重叠时，年轻 load 得到 `BlockedByOverlappingStore`。
- 现有 replay-needed、rollback、commit-boundary 与 MMIO non-speculative 合同不回退。
- debug snapshot 中 `lsq_load_state` 的可解释性保持一致，不出现“状态名和真实阻塞原因脱节”的倒退。

如果后续进入实现，优先补 `tests/host/load_store_queue_smoke.cpp` 的最小行为矩阵，再按需补 `tests/host/pipeline_speculation_contracts_smoke.cpp` 的专项断言。

## 风险与取舍

- 这一步只收窄到“未知地址阻塞”，不会一下子释放所有潜在并行度；但这是有意取舍，用来避免在当前主线时机过早引入更重的 replay / speculation 复杂度。
- 如果后续发现“地址已知但数据未 ready”的非重叠放行仍会触发新的隐藏合同问题，应优先补更窄回归，而不是立刻扩大设计范围。
- 保持现有状态枚举不扩张，有助于维持 debug / smoke 连续性；代价是短期内部分内部原因仍需通过测试命名和文档语义来精确表达。

## 当前有效性说明

- 当前有效 / 历史语境：当前有效，作为 `Phase 3` 下一步 decode 级 `BlockedByUnresolvedStore` 串行化边界的专项设计。
- 当前实时推进情况，以及后续是否继续扩 issue / replay / memory disambiguation，以 [status/mainline_status.md](../status/mainline_status.md) 和 [status/project_priority_roadmap.md](../status/project_priority_roadmap.md) 为准。
