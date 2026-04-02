# Pipeline 投机执行与提交契约设计

## 文档定位

本文档用于定义当前 `pipeline` 在后续进入 `Phase 3-B/C` 之前，关于投机执行、architectural commit boundary 与 side effect 可见性的正式契约。

它重点回答：

- precise exception / interrupt 的观察边界是什么
- 哪些结果可以 speculative 产生，哪些动作必须到 commit 才能生效
- MMIO、CSR、`mret/sret`、`sfence.vma`、TLB flush、RAM store 在投机与 squash 下应如何处理
- `LSQ` 第一轮允许和不允许做什么

本文档不承担实时进度更新。当前实现情况请以 [status/mainline_status.md](../status/mainline_status.md) 与对应计划文档为准。

## 关联文档

- 状态文档：
  - [status/mainline_status.md](../status/mainline_status.md)
- 相关计划：
  - [plan/phase3_ooo_execution_plan.md](../plan/phase3_ooo_execution_plan.md)
  - [plan/phase3_ooo_readiness_plan.md](../plan/phase3_ooo_readiness_plan.md)

## 背景与问题

当前 `pipeline` 已经具备 in-order 的 flush / redirect、trap / interrupt、CSR、MMIO 与 Sv39 路径，也已有最小分支预测增强。下一步一旦引入 `rename`、`ROB`、`LSQ` 和更明确的投机执行，就不能再只靠“当前 backend 大致这么做”来维持正确性，而必须先把 architected 可见性的边界写清楚。

如果不先定义投机与提交契约，后续很容易出现几类反复出 bug 的问题：

- younger 指令在被 squash 之后仍然污染 RAM、MMIO 或 CSR
- interrupt / trap 的可见性跑到错误的时序边界
- `mret/sret`、`sfence.vma` 或 TLB flush 在错误阶段生效
- `LSQ` 首轮实现时默认引入过激的 speculation，破坏现有 reference contract

因此，这份文档的目的不是把所有未来细节一次写死，而是先把“哪些动作必须等到 commit、哪些副作用绝不能在投机阶段泄漏”收口成正式 contract。

## 目标

- 定义 speculative result 与 architected side effect 的边界。
- 定义 precise exception / interrupt 在 `pipeline` 中的统一观察口径。
- 定义 MMIO、RAM store、CSR、trap-return、TLB / `sfence.vma` 在 commit 之前和之后的可见性。
- 为后续 `LSQ` 首轮实现划清允许与禁止的行为。
- 让 host smoke、differential、debug snapshot 与后续 OoO 接线可以共享同一套 contract。

## 非目标

- 不在本文档中定义完整的 superscalar issue / replay / recovery 策略。
- 不要求首轮 `LSQ` 支持复杂 memory disambiguation。
- 不把 `functional` 变成 speculative backend；它继续只提供 architected 真值。
- 不规定具体 predictor 算法或具体 `ROB` / `LSQ` 容量。

## 约束与边界

- architected state 的最终定义仍由 `functional + shared InstructionSemantics` 给出；投机只存在于 `pipeline` 内部执行与暂存层。
- “可 speculative 产生”不等于“对外已生效”。只要尚未跨过 commit boundary，结果都应可被 squash 而不留下 architected 痕迹。
- interrupt、exception、trap-return、CSR 写、MMIO side effect、TLB flush 都必须围绕统一的 commit boundary 理解，而不是围绕局部阶段便利理解。

## 方案

### 1. speculative result 与 architected side effect 的边界

在 `Phase 3-B/C` 语境下，backend 内部允许提前形成以下 speculative result：

- ALU 结果
- branch resolved result
- load 命中的读取结果
- CSR 指令计算出的“待写入值”
- store 的地址 / 数据准备状态

但以下动作默认必须在 architected commit boundary 才能生效：

- GPR architected mapping 切换
- CSR 真正写入
- RAM store 真正落内存
- MMIO load / store 对设备产生可观察效果
- halt
- `mret/sret` privilege / PC 切换
- `sfence.vma` / `satp` 驱动的 TLB flush

换句话说，backend 可以提前“算出结果”，但不能提前“承诺副作用”。

### 2. precise exception / interrupt 合同

当前和后续 `pipeline` 都必须继续满足 precise exception / interrupt：

- architecturally older 的 fault / trap 之前，younger 指令不得留下可见副作用。
- interrupt 只能在 architecturally precise 的 commit boundary 被观察和递送。
- 如果某条指令在 retirement 之前被 squash，它的异常、CSR 写、store、MMIO side effect 都应一并失效。
- `trap / fault` 恢复必须按 retire 顺序裁剪 younger 指令，而不是按“谁先执行完”裁剪。

这组规则的直接含义是：

- younger squashed store 不得落到 RAM / MMIO
- younger squashed CSR 写不得被后续 architected 读观察到
- branch mispredict / trap flush 之后，retire trace 中不得出现已被裁掉的 younger 指令

### 3. RAM store 合同

对于普通 RAM store，首轮 `Phase 3-B/C` 合同如下：

- store 可以提前计算地址与数据，并进入 `LSQ` 或等价暂存结构。
- store 在 commit 之前不得真正写入 RAM。
- 如果 store 在 commit 前被 squash，RAM 内容必须保持旧值。
- store commit 之后，其结果必须立刻对后续 architected 观察生效。

这意味着首轮 `LSQ` 最小 contract 必须至少支持：

- store enqueue
- address-ready / data-ready 跟踪
- commit 时落内存
- squash 时丢弃 younger store

### 4. MMIO 合同

MMIO 默认比 RAM 更严格：

- MMIO load / store 默认不得在 commit 前对设备生效。
- 被 squash 的 younger MMIO store 不得真正命中设备。
- 首轮 `LSQ` 不允许把 MMIO 当成可自由投机的普通 memory 请求。
- 如果需要保留某些当前 in-order 路径的行为，也应通过 commit-boundary helper 统一解释，而不是在执行阶段绕开 contract。

对当前仓库而言，这条规则尤其重要，因为 UART / CLINT / PLIC / storage 都已经是现有 correctness 与教学演示链路的一部分，不能让 speculative 访问污染设备状态。

### 5. CSR、trap-return 与 halt 合同

CSR 指令在投机阶段最多只能形成“待提交写入”：

- CSR 写只在 commit boundary 之后对后续 architected 观察生效。
- 被 squash 的 younger CSR 写不得影响后续 interrupt serviceability、privilege view 或 debug snapshot。
- `mret/sret` 的 privilege / EPC / PC 切换只在 commit boundary 生效。
- halt 也只在 commit 边界生效；被 squash 的 younger halt 不得提前停机。

这样可以保持当前 backend differential 对 trap-return、interrupt 与 CSR 时序的已有门禁继续成立。

### 6. `sfence.vma`、`satp` 与 TLB flush 合同

TLB / address-space 相关动作也必须按 architected commit boundary 理解：

- `satp` 写入只有在 commit 后才成为新的 architected address-space 配置。
- `sfence.vma` 的 flush 只有在 commit 后才真正对后续 architected fetch/load/store 可见。
- 被 squash 的 younger `satp` / `sfence.vma` 不得污染 TLB 或 page-walk 可见性。

这条规则的目标不是限制内部实现，而是保证对外始终能解释成“只有退休后的地址空间变更才算数”。

### 7. LSQ 第一轮允许与不允许的行为

首轮 `LSQ` 允许：

- 区分 load / store entry
- 记录 sequence / age
- 分别跟踪 address-ready 与 data-ready
- 在 commit 时退休 store
- flush younger entry
- 标记 MMIO / non-speculative 请求

首轮 `LSQ` 不允许默认引入：

- 激进 memory disambiguation
- 未经明确 contract 支持的 speculative MMIO
- 为了性能而放弃 precise exception / interrupt
- 未经验证的 store-to-load forwarding 组合爆炸

换言之，首轮 `LSQ` 的目标是“提供正确边界”，不是“提供尽量多的 memory speculation”。

### 8. debug / verification 合同

为了让这套 contract 真正可维护，相关验证面应至少能观察到：

- sequence / retire order
- commit-boundary 之后才生效的 CSR / trap-return / flush 行为
- squashed younger store / MMIO 不会泄漏副作用
- interrupt / fetch fault / trap-return 仍保持 precise

这意味着后续 host smoke / differential / debug snapshot 至少需要围绕这些边界建立直接断言，而不能只靠“最后程序输出对了”来间接证明。

## 风险与取舍

- 把 MMIO 一律视为 non-speculative，会限制首轮 `LSQ` 的表面灵活度，但这是必要取舍，因为当前仓库已经把多个设备状态直接暴露给 guest 回归和 debug/frontend。
- 把大量动作都推迟到 commit boundary，会让 backend 的内部实现比纯 in-order 更繁琐，但这是 precise exception 与可调试性的必要成本。
- 本文档刻意不展开复杂 memory speculation 细节，是为了避免当前主线在尚未具备基础结构前就过早承诺高风险行为。

## 当前有效性说明

- 当前有效 / 历史语境：当前有效，作为 `Phase 3-B/C` readiness 阶段的投机执行与 commit contract 说明。
- 当前实现结果、测试门禁与当前 `rename / ROB / LSQ` 接线进度，以 [status/mainline_status.md](../status/mainline_status.md)、[plan/phase3_ooo_execution_plan.md](../plan/phase3_ooo_execution_plan.md) 与 [plan/phase3_ooo_readiness_plan.md](../plan/phase3_ooo_readiness_plan.md) 为准。
