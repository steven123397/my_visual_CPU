# Wave 6 JIT / DBT Readiness 设计

## 文档定位

本文档记录主线 `Wave 6 / JIT / DBT 与 multicore / coherence` 的当前有效入口边界。

它回答：

- 为什么 `Wave 6` 可以在 `Wave 5` 首轮收口后激活。
- 为什么第一刀选择 `JIT / DBT hot-path evidence`，而不是直接实现 JIT engine 或 multicore / coherence。
- `Wave 6 Slice A / B / C / D / E / F` 已经固定了哪些观察、translation contract 与 prototype
  guardrail，哪些内容仍必须留在后续切片。

本文档不记录执行 checklist。当前进度以
[../status/mainline_status.md](../status/mainline_status.md) 和计划归档为准。

## 关联文档

- 状态文档：
  - [../status/mainline_status.md](../status/mainline_status.md)
- 当前计划：
  - 暂无主线活跃计划；继续推进 `Wave 6` 下一刀前先新建 `docs/plan/` 计划。
- 已完成计划归档：
  - [../plan/history_plan.md#mainline-wave6-jit-dbt-fallback-equivalence-slice-f-plan](../plan/history_plan.md#mainline-wave6-jit-dbt-fallback-equivalence-slice-f-plan)
  - [../plan/history_plan.md#mainline-wave6-jit-dbt-translation-plan-slice-e-plan](../plan/history_plan.md#mainline-wave6-jit-dbt-translation-plan-slice-e-plan)
  - [../plan/history_plan.md#mainline-wave6-jit-dbt-prototype-guardrail-slice-d-plan](../plan/history_plan.md#mainline-wave6-jit-dbt-prototype-guardrail-slice-d-plan)
  - [../plan/history_plan.md#mainline-wave6-jit-dbt-observation-and-slice-c-plan](../plan/history_plan.md#mainline-wave6-jit-dbt-observation-and-slice-c-plan)
  - [../plan/history_plan.md#mainline-wave6-jit-dbt-translation-contract-slice-b-plan](../plan/history_plan.md#mainline-wave6-jit-dbt-translation-contract-slice-b-plan)
  - [../plan/history_plan.md#mainline-wave6-jit-dbt-hot-path-evidence-slice-a-plan](../plan/history_plan.md#mainline-wave6-jit-dbt-hot-path-evidence-slice-a-plan)
  - [../plan/history_plan.md#mainline-wave5-closeout-wave6-readiness-plan](../plan/history_plan.md#mainline-wave5-closeout-wave6-readiness-plan)
  - [../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-f-l1d-lifecycle-guardrail-plan](../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-f-l1d-lifecycle-guardrail-plan)
- 相关设计：
  - [future_expansion_roadmap_design.md](future_expansion_roadmap_design.md)
  - [xv6_linux_jit_mainline_design.md](xv6_linux_jit_mainline_design.md)
  - [phase3_ooo_execution_model_design.md](phase3_ooo_execution_model_design.md)
  - [pipeline_speculation_contracts.md](pipeline_speculation_contracts.md)
  - [wave5_cache_memory_system_design.md](wave5_cache_memory_system_design.md)

## 背景与问题

当前仓库已经是一个已可运行的模拟器原型。`Wave 5 / cache / memory-system`
已经完成 `Slice A ~ F`：从 memory signal、最小可执行 L1D、显式 opt-in
观察面，到 L1D 边界和 lifecycle guardrail，都已经形成首轮可维护合同。

这给 `Wave 6` 提供了入口，但不能把它理解为“现在可以直接写 JIT 或多核”。`JIT /
DBT` 会把 shared `InstructionSemantics`、profile、debug snapshot、host 执行策略、
fault / trap 语义和未来代码缓存串起来；multicore / coherence 则会同时放大 cache、
atomic、DMA、memory-order 和平台状态空间。如果第一刀直接改执行语义，风险会明显高于当前证据能支撑的范围。

因此 `Wave 6` 的第一刀必须继续 `reference-first`：先固定可重复的 hot-path /
translation candidate 证据，只做观察和候选区间定义，不生成宿主机器码，不引入 block
cache，不改变 guest 可见语义。

## 目标

- 激活主线 `Wave 6`，但把第一刀限定为 `JIT / DBT hot-path evidence`。
- 复用现有 `ExecutionProfile`、debug snapshot、probe 和代表性 workload，形成可验证的
  hot-path / translation candidate 观察合同。
- 在不改 simulator 行为的前提下，为后续 opt-in prototype 固定 translation contract：
  translator 输入、输出分类、helper 边界、fault / trap 回退和 invalidation 口径。
- 为后续是否实现 JIT / DBT engine 提供证据，而不是用“预期会快”作为动机。
- 保持 `functional` reference path、`pipeline` 和当前 workload guardrail 的行为不变。

## 非目标

- 不在 `Slice A / B / C / D / E / F` 实现 JIT engine、DBT translator、IR、block cache 或 host code emission。
- 不申请可执行内存，不引入宿主平台相关代码生成。
- 不改变 `InstructionSemantics` 的 ISA 真值来源定位。
- 不改变 guest 可见 fault / trap / CSR / memory 语义。
- `Slice C` 的 prototype 不接入默认 backend 调度，不创建长期 block cache，不把
  helper-required 指令伪装成 inlineable。
- `Slice D` 只扩 prototype preflight / lifecycle guardrail，不执行 helper，不 replay
  fallback，不接 workload-level runtime harness。
- `Slice E` 只做 opt-in translation-plan dry-run，不执行 prototype，不接默认 backend
  调度，不把 `translation-plan` 写成长期 debug snapshot schema。
- `Slice F` 只做 host-smoke-only functional fallback replay 等价性，不把 fallback
  replay 接成 runtime 调度器，不执行 prototype helper。
- 不启动 multicore、coherence、memory consistency 新模型、write-back cache、I-cache 或
  cache maintenance instruction。
- 不把 AI accelerator 后续专项并入 `Wave 6`。

## 激活判断

`Wave 6` 当前满足“可以启动第一刀 evidence slice”的条件：

- `Linux` checkpoint 已冻结在 `timerfd-one-shot-readback-ok`，不再作为近端无限扩展 blocker。
- `xv6` shell、Linux dummy/probe、pipeline `vector_cnn`、debug CLI 和现有 workload
  已经形成稳定 guardrail。
- `Wave 5` 已把 cache / memory-system 从纯观察推进到默认关闭、可显式启用、可观察和已
  hardening 的最小 L1D 模型。
- 现有 profile / debug / probe 基础足以承载一刀不改语义的 hot-path evidence。

这些条件已经支撑 `Wave 6 Slice A` 完成证据收集和候选区间合同，支撑
`Slice B` 在文档层固定 translation contract，也支撑 `Slice C` 以显式 host smoke
形式落地一个 interpreter-assisted prototype，支撑 `Slice D` 对 prototype 的
preflight / lifecycle guardrail 做窄扩展，并支撑 `Slice E` 把 top hot-path candidate
接到 opt-in dry-run translation plan，支撑 `Slice F` 证明 first-boundary fallback
可以从 block start 回到 functional reference。后续 JIT engine、host code emission、长期
block cache 或 multicore / coherence 仍需要新的设计和计划。

## 方案

### Slice A：JIT / DBT hot-path evidence

第一刀只回答“哪些代码区间值得未来翻译”，不回答“如何翻译”。

允许范围：

- 复用现有 execution/profile 数据，固定可重复的 hot-path candidate 口径。
- 在 debug/probe 或 host smoke 中输出只读 candidate 摘要。
- 以代表性 workload 验证 candidate 输出稳定、默认路径行为不变。
- 保持输出可降级：没有足够证据时应输出空候选或低置信度候选，而不是制造假热点。

禁止范围：

- 不创建 JIT block cache。
- 不生成或执行宿主代码。
- 不把 hot-path candidate 写成 guest ABI 或 debug ABI 的破坏性变更。
- 不把 pipeline speculation、L1D counters 或 AI accelerator timing 混成同一个性能结论。

### Slice A 收口结果

当前 `Slice A / JIT DBT hot-path evidence` 已完成，结果归档见
[../plan/history_plan.md#mainline-wave6-jit-dbt-hot-path-evidence-slice-a-plan](../plan/history_plan.md#mainline-wave6-jit-dbt-hot-path-evidence-slice-a-plan)。
本轮结论是：

- 现有 `ExecutionProfile` 已具备第一刀需要的 PC range、branch、syscall、trap、
  memory-region 和 `shadow_cache` 统计入口。
- `translation-candidate` 的第一版输入直接复用 `profile.hot_paths`，排序口径为
  `executions` 降序、`retired_instructions` 降序、`start_pc` 升序、`end_pc` 升序。
- `run_debug_cli_probe.py` 新增 probe 文本摘要：
  `translation-candidate: start=... end=... executions=... retired=...`。
- 没有 hot path、没有重复执行或 retired 计数为空时，probe 输出
  `translation-candidate: none reason=...`，避免把低证据路径写成假热点。
- 本轮不扩 debug JSON schema，不引入 guest ABI，不启用 JIT / DBT 执行路径。

`Slice A` 收口时留下的缺口：

- per-PC memory cost、branch target 热度、cycle cost、translation invalidation 和
  helper / fault 回退合同当时还没有固定。
- 这些缺口不阻塞 `Slice A` 的 evidence 合同；其中 translation invalidation、
  helper / fault 回退口径已在下面的 `Slice B` 设计中收口为文档 contract。

### Slice B：translation contract design

`Slice B` 只回答“未来 translator 应该接收什么、允许产出什么、什么时候必须 helper
或 fallback”，仍不回答“如何生成和执行宿主代码”。

#### Translator 输入合同

未来 translator 的输入必须来自已经存在且可验证的 reference path：

- `ExecutionProfile.hot_paths` 只提供候选区间和排序证据，不是执行授权。
- 指令语义以 `InstructionSemantics` 为唯一 ISA 真值来源；translator 不得复制一套
  私有 ISA 解释。
- 单条指令输入应至少能绑定：
  - guest `pc`
  - 原始指令字节或 raw instruction
  - decode 后的 `Insn`
  - 当前 privilege / CSR / translation 相关只读上下文
  - 必要时的 `SemanticInputs` 快照
- basic block 输入只能从已取指、已解码、未跨越控制流边界的连续 guest PC range
  形成；遇到不确定长度、decode fault 或 page / fetch fault 时必须停止。

候选排序继续沿用 `Slice A` 的口径：`executions -> retired_instructions ->
start_pc -> end_pc`。`Slice B` 不新增长期 debug JSON schema，也不把候选结果写成
guest ABI。

#### Translator 输出分类

`Slice B` 固定的是输出分类，不是实际 IR 或宿主代码格式。未来 translator 对每条指令
或 block 只能给出以下分类：

- `inlineable`：可在未来 prototype 中内联模拟，但仍必须等价于共享
  `InstructionSemantics`。
- `helper-required`：必须调用 reference helper 或共享 simulator 边界完成，例如
  memory、CSR、trap-prone、atomic、fence、vector 或 device 相关行为。
- `fallback-required`：必须回到 interpreter / functional reference path，不能进入
  translated block。

第一版 block 终止条件：

- 任意控制流转移、`ecall`、`ebreak`、`mret`、`sret`、trap return 或未知 system 指令。
- 任意 decode / fetch / execute 可能产生精确 fault 且当前没有 helper 合同覆盖的指令。
- 任意跨 page、权限、MMIO、side-effect region 或 self-modifying-code 风险边界。
- 任意 unsupported / illegal instruction。

#### Helper 与 fallback 边界

helper 只能复用已有 simulator 事实来源：

- ISA architected effect 走 `InstructionSemantics` 或由其拆出的共享语义 helper。
- load / store / fetch / page walk 走 `AddressSpace -> Bus -> Ram/Device` 现有边界。
- trap / interrupt / exception 走既有 trap controller 和 commit boundary。
- debug / profile 继续由现有 backend 记录；translator 不能自行制造 guest 可见状态。

必须 fallback 的情况：

- unsupported、illegal、decode 不完整或指令长度不确定。
- 需要改变 privilege / CSR / trap state 且没有已定义 helper commit boundary。
- 访问 MMIO、side-effect region、uncacheable region，或无法证明为普通 RAM。
- data / instruction page fault、permission fault、misaligned fault 或 refill fault。
- `fence`、`sfence.vma`、atomic reservation、vector side effect 或设备交互尚未有
  专门 helper 合同。

#### Memory / fault / trap 合同

memory 访问不允许因为未来 JIT / DBT 被重新解释：

- 普通 RAM load / store 即使未来被优化，也必须通过 `AddressSpace` 的权限、
  translation、region 和 fault 判断。
- MMIO、side-effect region、设备寄存器和 DMA 可见状态必须 helper / fallback，不允许
  speculative reorder 或 host-side 直读直写。
- page walk、PTE A/D 更新、TLB 命中 / 失效、`satp` 变化和 `sfence.vma` 均保留在
  reference/helper 边界。
- trap / fault 必须保持 precise：一旦 translated block 内任意指令发生 fault，
  architectural state 必须等价于 functional reference path 在同一 guest PC 处停止。
- atomic / reservation / fence 在 multicore / memory-order 设计完成前只能 helper 或
  fallback，不能被普通 RAM 快路径吞掉。

#### Invalidation 合同

`Slice B` 不创建 block cache，但先固定未来必须触发 invalidation 的来源：

- primary image load、debug reset、payload load 或任意覆盖已翻译代码页的 loader 行为。
- guest 写入已翻译 instruction bytes 所在 RAM page 或 cache line。
- `satp` 变化、`sfence.vma`、TLB flush、page table permission / mapping 变化。
- region 属性变化、MMIO / side-effect 判定变化，或无法继续证明目标是普通 RAM。
- 未来若引入 I-cache、write-back、DMA coherence 或 multicore coherence，必须另开设计
  把这些事件并入 invalidation matrix。

在真正存在 block cache 之前，以上只是 contract：默认执行路径不做 invalidation
动作，也不产生可执行 host code。

### Slice B 收口结果

当前 `Slice B / translation contract design` 已完成，结果归档见
[../plan/history_plan.md#mainline-wave6-jit-dbt-translation-contract-slice-b-plan](../plan/history_plan.md#mainline-wave6-jit-dbt-translation-contract-slice-b-plan)。
本轮结论是：

- `Slice A` 的 candidate 观察合同足以作为 translator 输入候选，但不能直接授权执行。
- 第一版 translation contract 以 `InstructionSemantics`、`SemanticInputs`、
  `AddressSpace`、trap / commit boundary 和现有 profile 为事实来源。
- 输出只固定为 `inlineable`、`helper-required`、`fallback-required` 三类，不定义 IR
  或 host code 形态。
- memory、CSR、trap、atomic、fence、MMIO、page walk 和 invalidation 均采用保守
  helper / fallback 合同。
- 本轮不改代码，不启用 JIT / DBT 执行路径，不改变 guest 可见语义。

仍延期到后续切片的缺口：

- 还没有 IR、translation cache、block lifecycle、host code emission 或 executable
  memory policy。
- 还没有 memory / CSR / trap / branch / vector helper prototype 证明 helper/fallback
  组合与 functional reference path 结果等价。

### Slice C 前置观察合同：per-PC cost 与 branch target heat

进入 prototype 前先补齐了 3 个窄观察信号：

- `profile.pc_costs[]`
  - 按 guest PC 聚合 `retirements`、`cycles`、`memory_observations`、`memory_reads`、
    `memory_writes`、`memory_faults` 和 `memory_bytes`。
  - `cycles` 是 retire-side 观察成本：functional path 通常每条 retired PC 记 1；
    pipeline path 会把两次 retire 之间的间隔归到当前退休 PC，作为 hot-path 取样信号，
    不等同于微架构性能模型。
  - memory cost 只来自已经存在的 `ExecutionMemoryObservation`，仍通过
    `AddressSpace -> Bus -> Ram/Device` 的现有观测边界。
- `profile.branch_targets[]`
  - 按 branch PC + actual target 聚合 `executions` 和 `redirects`，用于判断同一个
    branch PC 的 target heat。
  - 当前只记录实际退休的 control-flow 指令，不预测、不改写分支行为。
- `run_debug_cli_probe.py`
  - 输出 `pc-cost:` 和 `branch-target:` top 摘要，作为 Slice C 入口前的只读 evidence。

这些字段属于 debug/profile 读侧合同，不是 guest ABI，也不授权默认启用 JIT / DBT。

### Slice C：interpreter-assisted DBT prototype

`Slice C` 的第一版 prototype 只在 host smoke 中显式调用
`run_interpreter_dbt_prototype_block()`。它不是 JIT engine：

- 不申请可执行内存。
- 不生成 host code。
- 不创建 block cache。
- 不接入 `functional` 或 `pipeline` 默认调度。
- 不改变 guest 可见语义。

prototype 只执行 `pure straight-line inlineable` 指令：

- 每条指令仍通过 `InstructionSemantics` 构造 architected effect。
- commit 仍通过 `apply_commit_boundary`。
- block 内每条成功退休指令按 functional path 一样推进 `instret` 和 `cycle`。
- memory、CSR、trap、atomic、vector、control-flow、system boundary 或 unsupported
  instruction 统一返回 `helper-required` / `fallback-required`，不执行 helper。

### Slice C 收口结果

当前 `Slice C / observation + interpreter-assisted DBT prototype` 已完成，结果归档见
[../plan/history_plan.md#mainline-wave6-jit-dbt-observation-and-slice-c-plan](../plan/history_plan.md#mainline-wave6-jit-dbt-observation-and-slice-c-plan)。
本轮结论是：

- `ExecutionProfile` 新增 `pc_costs` 和 `branch_targets`，debug JSON 与 probe 文本均有
  最小观察合同。
- `interpreter_dbt_prototype` 只执行 pure straight-line inlineable block，并用 host
  smoke 证明简单整数块与 functional backend 的 GPR / PC / `instret` / `cycle`
  结果等价。
- memory 指令在 prototype 中明确 fallback 为 `helper-required`，不走假内联。
- 默认 `make test` / `make test-pipeline` 仍不会启用 JIT / DBT 执行路径。

仍延期到后续切片的缺口：

- prototype 尚不处理 memory helper、CSR/system helper、branch block stitching、
  trap/fault precise replay、vector helper 或 block lifecycle persistence。
- 仍无 IR、translation cache、host code emission、executable memory policy。
- `pc_costs.cycles` 只是 retire-side 观察成本，不是稳定性能模型或 benchmark 结论。

### Slice D：prototype preflight / lifecycle guardrail

`Slice D` 只把 `interpreter_dbt_prototype` 从“执行时遇到边界再 fallback”收窄为“执行前先
plan / preflight 整个候选 block”。

新增合同：

- `plan_interpreter_dbt_prototype_block()` 在不提交 CPU architectural state 的前提下扫描
  candidate block，返回：
  - block 是否完全 inlineable；
  - 可内联前缀指令数；
  - 第一个 helper / fallback 边界的 guest PC；
  - `helper-required` 或 `fallback-required` 原因。
- `run_interpreter_dbt_prototype_block()` 必须先调用 preflight；如果 preflight 失败，
  不允许提交任何前缀指令，也不推进 PC、`instret` 或 `cycle`。
- memory、CSR、atomic、vector 等仍是 `helper-required`。
- control-flow 指令即使当前分支路径可能不 redirect，也统一是 `fallback-required`，
  避免 prototype 在没有 block stitching / precise replay 之前吞掉控制流边界。

这个 preflight 只是 prototype 生命周期 guardrail，不是 translator cache，也不是 block
cache。它不持久化 plan，不做 invalidation，不执行 helper，不生成 host code。

### Slice D 收口结果

当前 `Slice D / prototype guardrail expansion` 已完成，结果归档见
[../plan/history_plan.md#mainline-wave6-jit-dbt-prototype-guardrail-slice-d-plan](../plan/history_plan.md#mainline-wave6-jit-dbt-prototype-guardrail-slice-d-plan)。
本轮结论是：

- `interpreter_dbt_prototype` 新增 `InterpreterDbtPrototypePlan` 与
  `plan_interpreter_dbt_prototype_block()`，把 block preflight 结果显式暴露给 host
  smoke。
- pure straight-line inlineable block 仍可执行，并保持与 functional backend 的
  GPR / PC / `instret` / `cycle` 结果等价。
- `addi + lw` 这类中途遇到 helper-required 的候选块会在执行前整体拒绝，不再提交前缀
  `addi`。
- `addi + jal` 这类中途遇到 control-flow boundary 的候选块会在执行前整体拒绝，并以
  `fallback-required` 报告 first fallback PC。
- 默认 `make test` / `make test-pipeline` 仍不会启用 JIT / DBT 执行路径。

仍延期到后续切片的缺口：

- 仍未执行 memory / CSR / trap / vector helper，也未做 fallback replay。
- 仍没有 workload-level opt-in runtime harness、block lifecycle persistence、
  invalidation matrix implementation、IR、translation cache、host code emission 或
  executable memory policy。

### Slice E：translation plan dry-run / opt-in harness

`Slice E` 只回答“当前 profile top candidate 如果交给 prototype preflight，会得到什么
plan 结果”。它仍不执行 prototype，也不进入默认 backend 调度。

新增合同：

- `plan_interpreter_dbt_prototype_hot_path()` 复用 `Slice A` 的候选排序口径：
  `executions -> retired_instructions -> start_pc -> end_pc`。
- 没有 hot path、重复次数不足或 retired 计数为空时，返回
  `no-hot-paths` / `insufficient-repetition` / `empty-hot-path`，不触发 block
  preflight。
- 有足够候选证据时，只把候选区间交给 `plan_interpreter_dbt_prototype_block()` 做
  dry-run preflight；返回 `inlineable`、`helper-required` 或 `fallback-required`
  观察结果。
- debug CLI 新增 opt-in `translation_plan` command，`run_debug_cli_probe.py` 只有在
  显式传入 `--translation-plan` 时才发出该 command 并输出 `translation-plan:`
  文本摘要。

这个 dry-run command 是 probe / debug harness 的观察合同，不是 guest ABI，也不是
长期 debug snapshot schema。它不会执行候选块，不提交 CPU state，不推进 PC、`instret`
或 `cycle`。

### Slice E 收口结果

当前 `Slice E / translation plan dry-run` 已完成，结果归档见
[../plan/history_plan.md#mainline-wave6-jit-dbt-translation-plan-slice-e-plan](../plan/history_plan.md#mainline-wave6-jit-dbt-translation-plan-slice-e-plan)。
本轮结论是：

- top hot-path candidate 现在可以通过 `plan_interpreter_dbt_prototype_hot_path()`
  进入 Slice D preflight，排序口径继续复用 `Slice A`。
- `run_debug_cli_probe.py --translation-plan` 可显式输出
  `translation-plan: none / inlineable / fallback` 摘要；默认 probe 不新增该行。
- 真实 flat probe 已覆盖 `addi + lw + loop` 这类 first memory boundary，dry-run
  会报告 `helper-required`，且不提交前缀状态。
- 默认 `make test` / `make test-pipeline` 仍不会启用 JIT / DBT 执行路径。

仍延期到后续切片的缺口：

- 仍未执行 memory / CSR / trap / vector helper，也未做 fallback replay。
- 仍没有 persistent block lifecycle、invalidation matrix implementation、IR、
  translation cache、host code emission 或 executable memory policy。
- `translation-plan:` 当前只是 opt-in probe 文本和 debug-cli response，不是长期
  debug snapshot ABI。

### Slice F：functional fallback replay equivalence

`Slice F` 只回答“preflight 拒绝候选块之后，是否能从 block start 回到 functional
reference，把 inlineable prefix 和 first boundary 一起提交到等价状态”。它不是 helper
内联，也不是 runtime JIT harness。

新增合同：

- `run_interpreter_dbt_prototype_with_functional_fallback()` 是显式 host smoke helper。
- preflight 成功时继续复用 `run_interpreter_dbt_prototype_block()`，并报告
  `used_fallback=false`。
- preflight 失败时，不允许先提交 prototype 前缀；helper 会从当前 block start 创建
  `FunctionalBackend`，执行 `inlineable_instructions + 1` 步，即执行到 first
  helper / fallback boundary。
- replay 结果报告 `used_fallback=true`、first fallback PC 和 fallback reason。

这个 helper 只证明 fallback replay 的状态等价性，不创建长期 block lifecycle，不持久化
translation plan，不做 invalidation，不生成 host code。

### Slice F 收口结果

当前 `Slice F / fallback equivalence` 已完成，结果归档见
[../plan/history_plan.md#mainline-wave6-jit-dbt-fallback-equivalence-slice-f-plan](../plan/history_plan.md#mainline-wave6-jit-dbt-fallback-equivalence-slice-f-plan)。
本轮结论是：

- pure inlineable block 仍走 prototype path，`used_fallback=false`。
- `addi + lw` 这类 helper-required boundary 会用 functional fallback replay 从 block
  start 执行到 first boundary，并与直接 functional reference 的 GPR / PC /
  `instret` / `cycle` 结果等价。
- `addi + jal` 这类 control-flow boundary 会用 functional fallback replay 从 block
  start 执行到 first boundary，并与直接 functional reference 的 GPR / PC /
  `instret` / `cycle` 结果等价。
- 默认 `make test` / `make test-pipeline` 仍不会启用 JIT / DBT 执行路径。

仍延期到后续切片的缺口：

- 仍未在 prototype 内执行 memory / CSR / trap / vector helper。
- fallback replay 仍只是 host-smoke-only helper，不是 workload runtime 调度器。
- 仍没有 persistent block lifecycle、invalidation matrix implementation、IR、
  translation cache、host code emission 或 executable memory policy。

### 后续切片候选

后续只有在 `Slice A / B / C / D / E / F` 给出稳定证据、contract、最小 prototype、
preflight guardrail、dry-run 观察和 fallback replay 等价性后，才允许继续拆分：

- `Slice G / block lifecycle observation or helper boundary taxonomy`
  - 只允许继续补更窄 block lifecycle 观察、helper boundary 分类或 workload-level
    opt-in 观测；仍不得直接进入 host code emission 或长期 block cache。
- `multicore / coherence readiness`
  - 必须另开设计，先补 atomic、memory-order、DMA / cache 交界和 verification matrix。

## 验证思路

`Slice A` 的验证重点是“不改变语义 + 输出可重复证据”：

- 先补最窄 host / probe 回归，固定 candidate 摘要格式和空候选 fallback。
- 覆盖至少一个代表性 workload 的 hot-path signal。
- 守住默认门禁：
  - `cd myCPU && make test`
  - `cd myCPU && make test-pipeline`
- 若触达 frontend，再补：
  - `cd frontend && node --test`

`Slice B` 是文档 contract 切片，验证重点是“合同不越界 + 默认路径不启用 JIT / DBT”：

- 复核 shared `InstructionSemantics`、backend 和 `AddressSpace` 现有边界后再写 contract。
- 文档只定义分类和 fallback，不新增执行路径或 debug schema。
- 至少运行：
  - `git diff --check`
- 如果后续切片触达 `src/debug/*`、`src/exec/*` 或 `tests/host/*`，再按
  [../../myCPU/AGENTS.md](../../myCPU/AGENTS.md) 补 `make test` / `make test-pipeline`
  和对应窄门禁。

`Slice C` 的验证重点是“只在 opt-in host smoke 内证明等价，不改变默认路径”：

- `make test-host-execution_profile_smoke`
- `make test-host-run_debug_cli_probe`
- `make test-host-interpreter_dbt_prototype_smoke`
- `make test`
- `make test-pipeline`

`Slice D` 的验证重点是“preflight 失败不提交前缀指令，默认路径仍不启用 JIT / DBT”：

- `make test-host-interpreter_dbt_prototype_smoke`
- `make test`
- `make test-pipeline`

`Slice E` 的验证重点是“translation plan 只做 opt-in dry-run，默认 probe 不新增输出”：

- `make test-host-interpreter_dbt_prototype_smoke`
- `make test-host-run_debug_cli_probe`
- `make test-host-debug_cli_smoke`
- `make test`
- `make test-pipeline`

`Slice F` 的验证重点是“fallback replay 只在显式 host smoke helper 中发生，默认路径仍不启用 JIT / DBT”：

- `make test-host-interpreter_dbt_prototype_smoke`
- `make test-host-run_debug_cli_probe`
- `make test-host-debug_cli_smoke`
- `make test`
- `make test-pipeline`

## 风险与取舍

- 第一刀只做 evidence，看起来不像“真正 JIT”；但这能防止在没有热点证据和回退合同前过早引入执行语义分叉。
- hot-path 统计如果过早设计成长期 ABI，会约束后续实现；因此 `Slice A` 输出应定位为 debug/probe 观察合同，不是 guest ABI。
- `Slice B` 只做 contract，看起来仍不像“真正 JIT”；但它先把 helper、fallback 和
  invalidation 边界写清楚，避免后续 prototype 把 guest 可见语义复制到私有路径。
- `Slice C` 的 prototype 很窄，只覆盖 straight-line inlineable block；这是为了先证明
  shared semantics + commit boundary 可以被 DBT 形态复用，而不是提前承诺完整 JIT。
- `Slice D` 继续保持很窄：它只增加 preflight / lifecycle guardrail，并主动拒绝含
  helper 或 control-flow 边界的候选块；这会让 prototype 短期看起来更保守，但能避免
  在没有 precise replay 和 block stitching 前提交错误前缀状态。
- `Slice E` 只把 hot-path candidate 与 preflight 接成 dry-run 观察。它可以让后续
  更早看见真实 workload 的 first boundary 分布，但仍不能被理解为 runtime JIT
  harness 或 block cache。
- `Slice F` 只证明 fallback replay 可以从 block start 回到 functional reference 并到达
  first boundary；它仍不代表 prototype 已经具备 memory helper、branch stitching 或
  runtime fallback 调度能力。
- multicore / coherence 与 JIT 同属 `Wave 6`，但不应同刀启动；它需要更重的 memory-order 和 DMA / cache 交界验证。

## 当前有效性说明

- 当前有效：本文档作为主线 `Wave 6 / JIT / DBT` 首轮 readiness、Slice A 观察合同
  与 Slice B / C / D / E / F 设计入口。
- `Slice A / JIT DBT hot-path evidence`、`Slice B / translation contract design` 与
  `Slice C / observation + interpreter-assisted DBT prototype`、`Slice D / prototype
  guardrail expansion`、`Slice E / translation plan dry-run`、`Slice F / fallback
  equivalence` 均已完成并归档；当前暂无主线活跃计划。
- `Wave 6` 已激活，但当前只完成候选观察合同、translation contract、host-smoke-only
  prototype、preflight guardrail、opt-in dry-run translation plan 与 host-smoke-only
  fallback replay 等价性；JIT engine、
  host code emission、长期 block cache、multicore /
  coherence、write-back cache、I-cache 和 cache maintenance instruction 仍未启动。
