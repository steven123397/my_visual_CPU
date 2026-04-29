# xv6 / Linux / JIT 主线切换设计

## 文档定位

本文档用于说明：

- 为什么当前要把 `xv6-riscv` 这条标准 OS bring-up 路径作为主线近端执行波次
- 为什么这次推进不能再按“一次性最小跑通”来组织
- 如何把 `xv6`、后续 `Linux`、以及更远的 `JIT / 动态二进制翻译` 放进同一条可持续扩展的工程路径里

本文档不承担实时进度更新。当前进展、风险和下一步以对应 `status` 文档为准。

## 关联文档

- 状态文档：
  - [../status/mainline_status.md](../status/mainline_status.md)
- 相关计划：
  - [../plan/history_plan.md#mainline-wave6-jit-dbt-fallback-equivalence-slice-f-plan](../plan/history_plan.md#mainline-wave6-jit-dbt-fallback-equivalence-slice-f-plan)
  - [../plan/history_plan.md#mainline-wave6-jit-dbt-translation-plan-slice-e-plan](../plan/history_plan.md#mainline-wave6-jit-dbt-translation-plan-slice-e-plan)
  - [../plan/history_plan.md#mainline-wave6-jit-dbt-prototype-guardrail-slice-d-plan](../plan/history_plan.md#mainline-wave6-jit-dbt-prototype-guardrail-slice-d-plan)
  - [../plan/history_plan.md#mainline-wave6-jit-dbt-observation-and-slice-c-plan](../plan/history_plan.md#mainline-wave6-jit-dbt-observation-and-slice-c-plan)
  - [../plan/history_plan.md#mainline-wave6-jit-dbt-translation-contract-slice-b-plan](../plan/history_plan.md#mainline-wave6-jit-dbt-translation-contract-slice-b-plan)
  - [../plan/history_plan.md#mainline-wave6-jit-dbt-hot-path-evidence-slice-a-plan](../plan/history_plan.md#mainline-wave6-jit-dbt-hot-path-evidence-slice-a-plan)
  - [../plan/history_plan.md#xv6-linux-jit-wave1-plan](../plan/history_plan.md#xv6-linux-jit-wave1-plan)
  - [../plan/history_plan.md](../plan/history_plan.md)
- 来源设计：
  - [future_expansion_roadmap_design.md](future_expansion_roadmap_design.md)
  - [phase4_preparation_design.md](phase4_preparation_design.md)
  - [vector_ml_workload_direction_design.md](vector_ml_workload_direction_design.md)

## 背景与问题

`future_expansion_roadmap_design.md` 现在已经改成主线长期排期设计。此前仓库的默认推进顺序仍是围绕 `V4` hardening 和 `P4-prep-2` 可行性判断继续收口，但当前已经明确决定：要把“标准 OS bring-up + 后续 Linux / JIT 预留”作为主线近端执行波次来推进。

这意味着当前不再只是“评估要不要做 `xv6-riscv`”，而是要把 `xv6-riscv` 作为近端牵引目标，并让当前波次的结构决策直接服务后续 `Linux` 与 `JIT / 动态二进制翻译`。因此，本轮不能继续接受“一次性最小跑通、以后再推倒重来”的短寿命实现；一旦某个抽象是迟早都要引入的，就应该优先选择可被 `xv6 -> Linux -> JIT / DBT` 复用的形态。

与此同时，当前并行 guardrail workstream 已经落地的护栏不能丢。`kernel_alpha`、`debug/frontend`、`pipeline`、`V4`、`P4-prep-1` 和既有回归矩阵仍然是现阶段最关键的稳定性护栏。推进近端主线波次不等于放弃它们，而是要让它们成为这条主线的 correctness 基线、观测基础和回归支架。

## 目标

- 把 `RV64A + CSR / privilege 补全 + virtio 平台 + xv6-riscv bring-up` 确立为当前主线的近端执行路径。
- 在实现上避免只服务单一 `xv6` 场景的短寿命 hack，优先采用后续 `Linux` 与 `JIT / DBT` 可以继续复用的结构边界。
- 保持 `reference-first` 原则：共享 `InstructionSemantics + functional backend` 仍是 ISA 真值来源，`pipeline` 与未来 `JIT` 都复用同一份语义事实来源。
- 保留并行 guardrail workstream：`V4`、`P4-prep-1`、现有 debug/guest/pipeline 回归不被新主线反向污染。
- 用低交叉依赖的 4 条 workstream 支撑 4 个独立 worktree / 对话并行推进。

## 非目标

- 本轮不直接承诺跑起 `Linux`。
- 本轮不直接实现完整 `JIT / 动态二进制翻译`。
- 本轮不顺势重开更激进的 `Phase 3 issue / replay / speculation`。
- 本轮不因为 `xv6` bring-up 而牺牲既有 `kernel_alpha`、`interactive_os`、`V4` 或调试链路的 correctness 门禁。
- 本轮不为了追求“大而全”预先补齐所有 `F / D / C` 或多核 / coherence。

## 约束与边界

- `reference-first` 不变：任何 ISA / CSR / 平台语义都先在共享语义层与 `functional` 后端站稳，再决定 `pipeline` 与未来 `JIT` 如何消费。
- 非必要不取“一次性最小实现”，但也不允许为远期目标提前引入没有当前落脚点的空抽象。只有那些已被 `xv6`、后续 `Linux` 或未来 `JIT` 明确需要的公共层，才应本轮引入。
- `virtio` 不能写成只服务 `virtio-blk` 的一次性分支逻辑，应直接拆出可扩展到 `virtio-console` / `virtio-net` 的 transport / queue / device backend 分层。
- 外部 guest workload 不能写成只服务某个 `xv6_demo` 的一条 Makefile 特判，应抽成后续可以容纳 `Linux` workload 的外部 guest harness / board profile。
- 观测面不能只做一次性调试打印，应优先形成未来 `JIT / DBT` 可复用的 profile / trace / hot-path 信号。
- 多 agent 并行时，shared docs 由协调者维护；各 worktree 默认不改 `docs/status/*`、`docs/index.md` 或共享设计文档正文，只在对话里回报阻塞与结论。

## 方案

### 结构设计

本轮按 4 条 workstream 组织：

| Workstream | 目标 | 主要落点 | 面向未来的结构收益 |
|------|------|------|------|
| A：ISA / privilege foundation | 落地 `RV64A` 与按需 CSR / privilege contract 补全 | `decode / instruction_semantics / cpu / trap / csr` | 为 `xv6`、`Linux`、未来 `JIT` 提供稳定语义底座 |
| B：platform / virtio foundation | 引入可扩展的 `virtio-mmio + virtqueue + virtio-blk` 平台层 | `devices / mem / platform` | 为 `xv6` 现在、`Linux` 后续，以及更多 virtio 设备留统一入口 |
| C：external guest workload harness | 把 `xv6-riscv` 接入成外部 workload，并搭出可复用的 board / boot / smoke harness | `external workload tree + Makefile + machine/run glue` | 为未来 `Linux` 保留一致的 workload 接入方式 |
| D：observation / profile foundation | 继续保留并行 guardrail，并补上面向 `Linux / JIT` 的观测与 profiling 合同 | `debug / exec / tests` | 为后续 hot-path 识别、cache 观测、JIT 候选区间定位提供基础 |

这 4 条线的顺序关系是：

1. A 和 B 可以先并行启动。
2. C 依赖 A/B 的第一轮 contract 站稳之后再大步推进，但它可以先做 gap audit、workload harness 和 boot path 盘点。
3. D 全程并行，但它不反向拥有 A/B/C 的语义修复权，只提供观测、profile 和 guardrail 支撑。

### 接口 / 数据 / 契约

#### 1. ISA 语义层继续单一事实来源

- `InstructionSemantics` 仍然是 ISA 真值来源。
- `RV64A` 不应在 `functional`、`pipeline`、未来 `JIT` 里各写一套语义。
- 本轮应直接定义稳定的原子访存 / reservation contract，使 `functional`、保守 `pipeline` 路径，以及未来 `JIT / DBT` 都能复用同一份 architected 规则。

建议的边界是：

- 共享语义层回答“这条原子指令的 architected 效果是什么”。
- backend 决定“这条语义以什么执行策略落地”，例如：
  - `functional`：直接原子完成。
  - `pipeline`：允许先用保守串行化路径，但 contract 不另起一套。
  - `JIT / DBT`：未来可基于同一 contract 做 block translation / helper call。

#### 2. `virtio` 采用可扩展 transport 分层

`virtio` 本轮虽然第一刀是 `virtio-blk`，但结构上不应写成“在 `Machine` 里再塞一个特殊块设备”。建议直接拆成：

- `virtio_mmio_transport`
- `virtqueue / vring`
- `virtio_device` 抽象
- `virtio_blk_device` 作为首个 backend

这样做的理由是：

- `xv6` 当前先消费 `virtio-blk`
- `Linux` 后续大概率还会继续需要更多 `virtio` 设备
- 如果未来引入 `JIT / DBT`，设备模型最好仍保持宿主可调用、调试可观测的统一接口，而不是散落在 `Machine` 与若干 if/else 中

#### 3. 外部 guest workload 采用“板级 profile + workload harness”

`xv6-riscv` 不应被写成一条硬编码 demo。建议引入：

- 外部 workload 源目录（例如 vendored tree 或固定同步入口）
- 板级 / 平台 profile
- 单独的 build glue / run glue / smoke target

也就是说，本轮真正引入的不是“一个 `xv6_demo`”，而是“外部 guest workload 接入机制”，`xv6-riscv` 只是第一个使用者。后续如果接 `Linux`，仍复用同一入口。

#### 4. observation / profile 采用稳定读侧合同

为后续 `Linux` 与 `JIT / DBT` 做准备，本轮观测面不应只停留在 ad-hoc log。建议优先把以下读侧信号收口成稳定合同：

- hot basic block / branch / trap / syscall 频率
- workload 层级的 memory-region 访问分布
- 对 `xv6`、`vector_cnn_demo`、`kernel_alpha` 等代表性 workload 的 profile 入口
- debug / CLI 可消费、但不会反向影响执行语义的 profile snapshot

这样后续无论是做 `P4-prep-2`、cache 评估，还是做 `JIT` 候选热路径选择，都有现成基础。

### 验证思路

- Workstream A：新增 `RV64A` asm / host smoke，必要时补更窄的 Spike differential 子集；守住 `make test` 和相关 CSR / trap / privilege 回归。
- Workstream B：新增 `virtio-mmio` / `virtqueue` / `virtio-blk` unit / host smoke；守住 `bus / device / platform` 相关现有门禁。
- Workstream C：新增 `xv6` boot smoke / harness smoke；在接通前先形成明确 gap audit，避免盲目调试。
- Workstream D：扩展 `debug_cli_smoke`、代表性 workload smoke 和 profile 观测回归；继续守住 `V4`、`interactive_os`、`kernel_alpha` 等并行 guardrail。
- 主线整体验证仍以 `cd myCPU && make test`、`cd myCPU && make test-pipeline` 和 `cd frontend && node --test` 为底线，再按 workstream 触达路径补窄门禁。

## 风险与取舍

- `xv6` 会暴露大量 CSR / trap / device 细节缺口，短期内工作量不可预测；因此必须把 A/B/C 分开，而不是让 bring-up 对话顺手乱修 simulator 全域。
- “不做短寿命最小实现”会提升本轮抽象成本；因此必须严格限定在那些确定会被 `xv6 -> Linux -> JIT` 复用的公共层，不为远期目标提前造空框架。
- `virtio`、外部 workload harness、profile 合同都会引入新边界；如果 ownership 不清晰，多 agent 会互相踩文件。必须通过 worktree / branch 和文件 ownership 管理交叉风险。
- 并行 guardrail 一旦失守，主线推进会把 reference path 稳定性一起拖下水；因此 D 线不是“可有可无的锦上添花”，而是这轮激进推进的保护层。

## 当前有效性说明

- 当前有效：自 `2026-04-21` 起，本文档作为“当前已激活的 `xv6 / Linux / JIT` 主线切换设计”。
- 当前结果以 [../status/mainline_status.md](../status/mainline_status.md) 为准。
- 当前执行计划已归档到 [../plan/history_plan.md#xv6-linux-jit-wave1-plan](../plan/history_plan.md#xv6-linux-jit-wave1-plan)。
- 当前 `Wave 6` 已激活，JIT / DBT 证据链和原型边界首轮已完成。当前结果只固定
  hot-path candidate、translation contract、host-smoke-only prototype、preflight
  guardrail、opt-in translation-plan dry-run、functional fallback replay 等价性和
  first-boundary taxonomy；不实现 host code emission、长期 block cache 或改变 guest
  可见语义。后续同类窄观察和原型边界补洞不再单独创建 plan 文档；只有进入真正
  JIT engine、host code、persistent block cache、runtime scheduler 或 multicore /
  coherence 等整块任务时，才重新启用独立计划文档。
