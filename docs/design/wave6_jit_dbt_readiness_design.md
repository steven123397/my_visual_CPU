# Wave 6 JIT / DBT Readiness 设计

## 文档定位

本文档记录主线 `Wave 6 / JIT / DBT 与 multicore / coherence` 的当前有效入口边界。

它回答：

- 为什么 `Wave 6` 可以在 `Wave 5` 首轮收口后激活。
- 为什么当前阶段先做 hot-path evidence、translation contract 和 prototype guardrail，
  而不是直接实现 JIT engine 或 multicore / coherence。
- 当前已经固定了哪些观察、helper / fallback、preflight 和 first-boundary 合同。
- 什么时候才需要重新启用独立 `plan` 文档。

本文档不记录执行 checklist。当前进度以
[../status/mainline_status.md](../status/mainline_status.md) 为准；已完成历史只保留在
[../plan/history_plan.md](../plan/history_plan.md) 中。

## 关联文档

- 状态文档：
  - [../status/mainline_status.md](../status/mainline_status.md)
- 已完成计划归档：
  - [../plan/history_plan.md](../plan/history_plan.md)
  - [../plan/history_plan.md#mainline-wave6-dbt-translator-ir-v0-plan](../plan/history_plan.md#mainline-wave6-dbt-translator-ir-v0-plan)
- 相关设计：
  - [future_expansion_roadmap_design.md](future_expansion_roadmap_design.md)
  - [xv6_linux_jit_mainline_design.md](xv6_linux_jit_mainline_design.md)
  - [phase3_ooo_execution_model_design.md](phase3_ooo_execution_model_design.md)
  - [pipeline_speculation_contracts.md](pipeline_speculation_contracts.md)
  - [wave5_cache_memory_system_design.md](wave5_cache_memory_system_design.md)

## 背景与问题

当前仓库已经是一个已可运行的模拟器原型。`Wave 5 / cache / memory-system`
已经完成首轮收口：从 memory signal、最小可执行 L1D、显式 opt-in 观察面，到 L1D
边界和 lifecycle guardrail，都已经形成可维护合同。

这给 `Wave 6` 提供了入口，但不能把它理解为“现在可以直接写 JIT 或多核”。`JIT /
DBT` 会把共享 `InstructionSemantics`、profile、debug snapshot、host 执行策略、
fault / trap 语义和未来代码缓存串起来；multicore / coherence 则会同时放大 cache、
atomic、DMA、memory-order 和平台状态空间。如果第一刀直接改执行语义，风险会明显高于当前证据能支撑的范围。

因此 `Wave 6` 当前仍保持 `reference-first`：先固定可重复的 hot-path /
translation candidate 证据、helper / fallback 边界和最小 prototype guardrail，不生成
宿主机器码，不引入长期 block cache，不改变 guest 可见语义。

## 目标

- 激活主线 `Wave 6`，但把当前阶段限定为证据链和原型边界。
- 复用现有 `ExecutionProfile`、debug snapshot、probe 和代表性 workload，形成可验证的
  hot-path / translation candidate 观察合同。
- 固定未来 translator 的输入、输出分类、helper 边界、fault / trap 回退和 invalidation
  口径。
- 用 host-smoke-only prototype 证明共享语义和 commit boundary 可以被 DBT 形态复用。
- 为后续是否实现 JIT / DBT engine 提供证据，而不是用“预期会快”作为动机。
- 保持 `functional` reference path、`pipeline` 和当前 workload guardrail 的行为不变。

## 非目标

- 当前已完成 `DBT translator + IR v0 dry-run`、metadata-only block cache、
  executable memory policy、host code emission v0、executable cache runtime hookup、
  scalar memory helper execution opt-in、reference fallback execution opt-in 和 dispatch
  harness v1 子阶段，但仍不实现默认 JIT backend、persistent cache 或 workload-level
  scheduler。
- 不在默认执行路径申请可执行内存，不把宿主平台相关代码生成接入默认 backend。
- 不改变 `InstructionSemantics` 的 ISA 真值来源定位。
- 不改变 guest 可见 fault / trap / CSR / memory 语义。
- 不接入默认 backend 调度，不创建长期 block lifecycle，也不把 helper-required 指令伪装成
  inlineable。
- 不启动 multicore、coherence、memory consistency 新模型、write-back cache、I-cache 或
  cache maintenance instruction。
- 不把 AI accelerator 后续专项并入 `Wave 6`。

## 激活判断

`Wave 6` 当前满足“可以启动 JIT / DBT 证据链和原型边界阶段”的条件：

- `Linux` checkpoint 已冻结在 `timerfd-one-shot-readback-ok`，不再作为近端无限扩展 blocker。
- `xv6` shell、Linux dummy/probe、pipeline `vector_cnn`、debug CLI 和现有 workload
  已经形成稳定 guardrail。
- `Wave 5` 已把 cache / memory-system 从纯观察推进到默认关闭、可显式启用、可观察和已
  hardening 的最小 L1D 模型。
- 现有 profile / debug / probe 基础足以承载不改语义的 hot-path evidence、
  translation dry-run 和 prototype preflight。

这些条件只支撑证据链和原型边界继续推进；后续默认 JIT backend、长期 block cache、
完整 helper runtime execution、workload-level scheduler 或 multicore / coherence 仍需要新的设计和计划。

## DBT translator + IR v0 dry-run 子阶段

证据链和原型边界首轮收口后，当前已经完成正式 `DBT translator + IR v0 dry-run`，
范围限定为“非执行 translator 前端”：

- 已新增 `dbt_ir`，表达最小纯直线整数 IR：立即数写寄存器、寄存器加立即数、
  寄存器加/减寄存器、逻辑立即数 / 寄存器运算、shift immediate / register、
  signed / unsigned set-less-than、`lui` / `auipc`、RV64 word ops，以及 block fallthrough。
- 已新增 `dbt_translator`，输入只能来自共享 `DbtBlockPlan`；`DbtBlockPlan` 不通过、
  或 IR v0 不支持其中某条 inlineable 指令时，translator 只返回稳定 reject metadata，
  不翻译可内联前缀。
- translator reject metadata 已收口为 typed `DbtRejectKind`，并保留
  `reject_reason` / `boundary_kind` 字符串兼容字段；rejected unit 还记录 reject PC、
  raw instruction 和 typed boundary，供 cache / eval / 后续 helper planning 使用。
- helper planning dry-run 已完成 matrix 扩展：`DbtBlockPlan` 和 `DbtTranslationUnit`
  暴露 typed `DbtHelperPlan`，当前固定 memory load / store、CSR write、atomic 与
  vector helper kind / op、PC、raw instruction 和最小参数；fallback-required 边界不附带
  helper plan。
- helper replay contract dry-run 已完成第一刀：`dbt_helper_replay` 只从 rejected
  helper unit 生成未来 replay 需要的分类和 effect flags，覆盖 scalar memory、CSR、
  atomic 与 vector helper；它不执行 helper，不提交 CPU state，也不生成 helper 前缀 IR。
- IR lowering contract dry-run 已完成第一刀：`dbt_ir_lowering` 只把 ok
  `DbtTranslationUnit` 的 IR v0 降成 backend-neutral lowered ops，固定 operand kind、
  ALU op、XLEN / word width、fallthrough 和 unsupported IR 整体拒绝合同；它不生成
  host code，不申请 executable memory，也不接入 runtime dispatch。
- JIT engine skeleton dry-run / runtime fallback bridge 已完成第一刀：`dbt_jit_engine`
  只串联 metadata cache lookup、block planning、translator、IR lowering、helper replay
  contract、profile hot-path dispatch 和 reference fallback 决策；它不生成 host code，
  不申请 executable memory，不执行 guest code，也不接入默认 backend。
- dispatch result serialization / debug-probe visibility bridge 已完成第一刀：dry-run
  result 只序列化 source、action、cache state、reject/helper 分类、candidate evidence
  和 no-host-code flags；debug CLI 与 probe 只读展示该结果，不把它变成 runtime 调度。
- runtime dispatch contract dry-run 已完成第一刀：`dbt_runtime_dispatch` 只把
  `DbtJitDryRunResult` 转成运行时调度合同，固定 lowered block、helper bridge to
  reference 和 plain reference step 三种非执行路径；它不执行 helper、不调用 reference
  backend step、不修改 CPU state、不申请 executable memory，也不生成 host code。
- executable-cache invalidation enforcement dry-run 已完成第一刀：
  `dbt_executable_cache` 只缓存 lowered-block runtime dispatch contract metadata，并复用
  `DbtInvalidationPlan` 强制删除 overlapping guest store、primary image load、
  `sfence.vma` 等事件命中的 resident contract；它只证明 stale dispatch 会被阻止，
  不保存 host code，不申请 executable memory，也不执行 guest code。
- executable cache runtime hookup 已完成第一刀：`DbtExecutableCacheRuntime` 在 host smoke
  显式 opt-in 时可把已通过 differential guardrail 的 `DbtHostExecutable` 作为 resident
  entry 挂到既有 executable-cache invalidation 合同上；lookup 命中可复用 resident host
  executable，guest store / image / reset / `satp` / `sfence.vma` / region 事件会释放并删除
  resident executable。它不做 persistent cache，不接默认 backend，也不执行 helper runtime。
- reference fallback step bridge dry-run 已完成第一刀：`dbt_reference_fallback`
  把 runtime dispatch contract 的 plain reference step 和 helper-bridge-to-reference
  路径转成 fallback 入口合同，保留 reject / helper metadata 和 no-execution flags；
  它本身不调用 functional backend step，不执行 helper，也不提交 CPU state。
- helper execution bridge 已从 request-only 合同推进到最窄 scalar memory opt-in 执行：
  `dbt_helper_execution_bridge` 仍保留 CSR / atomic / vector 的 request-only metadata，
  但 memory load / store 可以在 host smoke 显式调用时经由 `AddressSpace -> Bus`
  执行，并固定 load GPR commit、store memory commit、trap/fault fallback 和 commit
  boundary；它不生成 host code，不接默认 backend，也不把 CSR / atomic / vector helper
  变成 runtime 执行路径。
- runtime invalidation hook contract 已完成第一刀：`dbt_runtime_invalidation`
  只把 runtime invalidation event 转接到 executable-cache dry-run enforcement，覆盖
  guest store、payload load、primary image load、debug reset、`satp`、`sfence.vma` 和
  region 属性变化；它只阻止 stale dispatch metadata，不提交 CPU state，不生成 host code。
- reference fallback execution bridge 已从 request-only 合同推进到 host-smoke-only
  opt-in 单步执行：`dbt_reference_fallback_execution` 仍先分类 plain reference step、
  helper-bridge reference step、JIT miss 和 trap/fault placeholder，再在显式调用时复用
  `FunctionalBackend::step()` 执行一个 reference step。它覆盖 JIT miss / reject /
  helper-required / differential mismatch 的 reference fallback，不生成 host code，不接默认
  backend，也不改变 functional backend 语义。
- executable memory policy 已完成第一刀：`dbt_executable_memory` 用 POSIX
  `mmap/mprotect/munmap` 固定 allocation / write / seal RX / release 生命周期，拒绝
  zero-size、越界写、seal 后写入和重复释放；它不执行宿主代码。
- host code emission v0 已完成第一刀：`dbt_host_emitter` 只接受成功 lowered 的
  pure integer straight-line block，生成 x86-64 SysV 小函数，并通过 executable memory
  policy 完成 W->RX 生命周期；host smoke 真实调用该函数并与 `dbt_ir_eval` 对齐
  GPR / fallthrough PC。unsupported lowered op、rejected lowering 或缺失 fallthrough
  必须整体拒绝，不暴露可执行前缀 code。
- opt-in runtime harness + differential guardrail 已推进到 dispatch harness v1：
  `dbt_runtime_harness` 只在 host smoke 中显式执行 pure integer straight-line block；
  miss 路径完成 cache lookup、emit-on-miss、execute 和 cache insert，hit 路径执行 resident
  host executable，invalidation 后必须 miss 并重新 emit，不能 stale dispatch。执行路径仍必须
  通过 block plan、translator、IR eval、lowering、host emitter 和 executable memory policy，
  并在提交 CPU state 前与 IR eval differential 对齐。helper / fallback / trap-risk block
  必须拒绝并回到 reference fallback，默认 backend 不启用 JIT。
- translator 可以重新 decode `DbtBlockPlan::dry_run_ir[].raw` 来恢复寄存器、立即数和
  opcode 分类，但不得重新取指，不得提交 CPU state，也不得绕过 `InstructionSemantics`
  的 preflight 结论。
- v0 dry-run 的输出是 `DbtTranslationUnit`，只用于 host smoke 和后续 metadata /
  differential 验证，不接入默认 backend 调度。
- 已新增 `dbt_ir_eval`，只在 host smoke 中解释 IR v0 的寄存器语义，并与 reference
  execution 对比 GPR、fallthrough PC 和 retired instruction count；它不读取 memory、
  不提交 CPU state，也不作为 runtime 执行路径。
- 已新增 `dbt_block_cache`，只缓存 `DbtTranslationUnit` metadata，固定 exact-range
  lookup、hit / miss 计数、rejected unit 不入 cache，以及复用现有 invalidation dry-run
  合同删除 metadata；当前也固定了 invalidation check / examined entries /
  non-invalidating event 计数，以及空 cache 上全局 invalidation 事件的稳定分类。它不保存
  host code，不接入 backend，也不承担 persistent lifecycle。

本阶段完成后，项目可以声称“已有最小 DBT translator / IR dry-run 前端、host code
emission v0、host-smoke-only opt-in executable JIT block、opt-in runtime executable
cache hookup、scalar memory helper opt-in execution、reference fallback opt-in execution
和 dispatch harness v1”。它仍不能声称已有默认 JIT backend、persistent cache、
workload-level scheduler 或完整 helper inline / replay runtime。

## 当前合同

### Hot-path 与观察信号

当前 translation candidate 的输入直接复用 `ExecutionProfile.hot_paths`，排序口径为：

1. `executions` 降序
2. `retired_instructions` 降序
3. `start_pc` 升序
4. `end_pc` 升序

补充观察信号包括：

- `profile.pc_costs[]`：按 guest PC 聚合 retirements、cycles、memory observations、
  memory reads / writes / faults 和 memory bytes。
- `profile.branch_targets[]`：按 branch PC + actual target 聚合 executions 和 redirects。
- `translation-candidate:`、`pc-cost:`、`branch-target:` probe 摘要。

这些字段属于 debug/profile 读侧合同，不是 guest ABI，也不是性能 benchmark 结论。

### Translation 输入合同

未来 translator 的输入必须来自已经存在且可验证的 reference path：

- `ExecutionProfile.hot_paths` 只提供候选区间和排序证据，不是执行授权。
- 指令语义以 `InstructionSemantics` 为唯一 ISA 真值来源；translator 不得复制一套私有
  ISA 解释。
- 单条指令输入应至少能绑定 guest `pc`、raw instruction、decode 后的 `Insn`、当前
  privilege / CSR / translation 相关只读上下文，以及必要时的 `SemanticInputs` 快照。
- Basic block 输入只能从已取指、已解码、未跨越控制流边界的连续 guest PC range 形成；
  遇到不确定长度、decode fault 或 page / fetch fault 时必须停止。

### Translation 输出分类

当前固定输出分类，并允许 `IR v0 dry-run` 定义非执行 IR 格式；仍不定义 host code 格式：

- `inlineable`：未来可在 prototype 或 translator 中内联模拟，但仍必须等价于共享
  `InstructionSemantics`。
- `helper-required`：必须调用 reference helper 或共享 simulator 边界完成，例如 memory、
  CSR、trap-prone、atomic、fence、vector 或 device 相关行为。
- `fallback-required`：必须回到 interpreter / functional reference path，不能进入
  translated block。

`IR v0 dry-run` 只允许覆盖已经由 `DbtBlockPlan` 判定为完整 inlineable 的 pure
straight-line integer 子集。当前覆盖 `addi/add/sub`、逻辑运算、shift、
signed / unsigned set-less-than、`lui` / `auipc` 和 RV64 word ops。memory、CSR、
trap、atomic、vector、control-flow、fence / TLB flush 或 unsupported instruction 都必须
保持 reject，不允许翻译前缀。

`IR semantic differential dry-run` 只允许解释已成功翻译的 IR v0，并把结果同
`InstructionSemantics` / reference 执行对齐。当前比较范围限于更宽 pure integer 子集的
GPR、fallthrough PC 和 retired instruction count；它不是 IR lowering，也不是 helper replay。

`IR lowering contract dry-run` 只允许消费已成功翻译的 IR v0，并输出后端无关的 lowered
ops。当前 lowered ops 描述 `Compute` / `Fallthrough`、operand kind、ALU op、
XLEN / word width 和 word sign-extension；它本身不绑定 x86 / ARM host ISA，也不能读取
或修改 CPU state。遇到 rejected unit 或 unknown IR opcode 时必须整体 reject，并且不得暴露
可消费前缀 lowering。

`host code emission v0` 只允许消费成功 lowered 的 pure integer straight-line block。
当前唯一支持的宿主 ABI 是 x86-64 SysV：
`uint64_t (*)(uint64_t* gpr, uint64_t pc)`。emitter 可以写入传入的 GPR snapshot，
返回 fallthrough PC，但不能访问 simulator `CPU`、`Bus`、memory、CSR、trap controller
或设备状态，也不能成为默认 runtime scheduler。`auipc` / PC operand 必须使用对应
guest 指令 PC，而不是 block entry PC。unsupported lowered op、rejected lowering 或缺失
fallthrough 必须整体 reject，不返回可执行前缀 code。

`opt-in runtime harness` 只允许执行单个已完整通过 block plan、translator、IR eval、
lowering、host emitter 和 executable memory policy 的 pure integer block。它可以把生成的
host code 作用于 GPR snapshot，并在 differential guardrail 成功后提交 GPR、PC 和
retired instruction count；任何 helper-required、fallback-required、trap-risk、
invalidation-risk 或 differential mismatch 都必须拒绝并保留 CPU state 不变。该 harness
只用于 host smoke，不是默认 backend，也不是 workload-level runtime scheduler。

`JIT engine skeleton dry-run` 只允许做调度决策编排：先查 metadata-only cache，未命中时
走 `DbtBlockPlan -> DbtTranslationUnit -> DbtIrLoweringResult`，helper-required unit
生成 helper replay contract，其他 rejected unit 选择 reference fallback；profile 入口只允许
从 `ExecutionProfile.hot_paths` 选择 candidate，并把 source、candidate evidence、no-candidate
fallback 和 cache / lowering 统计暴露出来。当前 skeleton 的输出是决策和统计，不是可执行
block；它不得调用 helper 执行、不得提交 CPU state、不得申请 executable memory、不得生成
host code，也不得成为 runtime scheduler。

`dispatch result serialization / debug-probe visibility bridge` 只允许读出 dry-run 结果：
`DbtJitDryRunSummary`、debug CLI `jit_dispatch` 和 `run_debug_cli_probe.py --jit-dispatch`
必须保持 opt-in；输出字段用于调试和回归，不是 guest ABI，也不能授权 runtime 执行。

`runtime dispatch contract dry-run` 只允许消费已有 `DbtJitDryRunResult`，并输出未来
runtime scheduler 会采用的非执行选择：lowered block、helper bridge to reference 或 plain
reference step。该合同只能描述 `can_enter_lowered_block`、`requires_helper_bridge`、
`reference_step_required`、helper replay flags 和 no-host-code / no-executable-memory /
no-guest-execution 边界；它不得调用 helper、不得执行 reference step、不得提交 CPU state，
也不得成为默认 backend。

`executable-cache invalidation enforcement dry-run` 只允许接收 lowered-block runtime
dispatch contract，并把它作为 future executable cache 的 resident metadata。该 cache
必须拒绝 helper bridge / reference step contract，必须对 overlapping guest store 和全局
image / reset / `satp` / `sfence.vma` / region 属性变化事件强制删除 resident metadata，
并显式计数 stale dispatch prevention；它不保存 host code，不申请 executable memory，
也不得参与真实 dispatch。

`executable cache runtime hookup` 只允许在 host smoke 显式 opt-in 时接收已经生成的
`DbtHostExecutable`。resident entry 必须继续绑定 lowered-block
`DbtRuntimeDispatchContract`，cache miss 路径必须先通过 IR eval differential guardrail
再把 host executable 放入 cache；cache hit 路径仍需重新规划 / 翻译当前 guest block，并用
IR eval differential 校验 resident host executable 的输出后才允许提交 CPU state。runtime
cache 复用现有 invalidation matrix；命中 guest store、image / reset、`satp`、`sfence.vma`
或 region 属性变化时必须释放 executable memory 并移除 entry。它不持久化 host code，不接
默认 backend，不实现 workload-level scheduler，也不执行 helper runtime。

第一版 block 终止条件包括控制流转移、system boundary、decode / fetch / execute fault
风险、跨 page / 权限 / MMIO / side-effect region / self-modifying-code 风险边界，以及
unsupported / illegal instruction。

### Helper 与 fallback 边界

Helper 只能复用已有 simulator 事实来源：

- ISA architected effect 走 `InstructionSemantics` 或由其拆出的共享语义 helper。
- `DbtHelperPlan` 只描述未来 helper 调用合同，不执行 helper，不提交状态，也不允许
  translator 生成 helper 前缀 IR。
- 当前 helper plan matrix 已固定 memory load / store、CSR write、atomic 和 vector
  的 typed metadata；trap-prone、fence / TLB flush 等边界仍保持 reject taxonomy，
  不附带 helper plan。
- 当前 helper replay contract 只把 helper metadata 转成非执行 replay effect flags；
  memory / CSR / atomic / vector 的真实 replay、异常处理、MMIO 观察、reservation 更新和
  vector state 提交仍必须留给未来 runtime helper 设计。
- 当前 helper execution bridge 已允许 scalar memory load / store 在 host smoke 中显式
  opt-in 执行；它固定参数、effect flags、trap fallback 与 commit-boundary 合同，并经由
  `AddressSpace -> Bus` 提交 load / store。CSR / atomic / vector 仍只保留 request-only
  metadata，不实际执行。
- 当前 runtime invalidation hook 只把 runtime event 映射到 executable-cache dry-run
  enforcement；它不直接挂接 RAM / CSR / loader 写路径，也不实现 persistent lifecycle。
- 当前 reference fallback bridge 只把 runtime dispatch contract 转成 reference fallback
  plan；plain reference step 和 helper-bridge-to-reference 的执行仍由独立 opt-in execution
  bridge 承担。
- 当前 reference fallback execution bridge 可以在 host smoke 中显式 opt-in 调用
  `FunctionalBackend::step()` 执行一个 reference step；它区分 JIT miss、helper bridge、
  reject 和 trap/fault placeholder，但不执行 helper，不生成 host code，也不接默认 backend。
- 当前 executable memory policy 只管理宿主内存生命周期和权限切换；host emitter smoke
  和 opt-in runtime harness 可以真实调用生成函数，但默认 backend 仍不接入 JIT。
- 当前 executable cache runtime hookup 只让 host smoke 显式持有 / 复用 / 释放已生成的
  host executable；它仍拒绝 helper / reference contract 入 cache，也不把 cache hit 变成
  默认 runtime dispatch 授权。
- 当前 dispatch harness v1 只存在于 opt-in runtime harness：cache miss 才 emit 并插入，
  cache hit 才执行 resident executable，invalidation 后必须重新 miss / emit；summary /
  stats 只读暴露 hit、miss、emit、exec、fallback、invalidate 和 differential mismatch 计数。
- load / store / fetch / page walk 走 `AddressSpace -> Bus -> Ram/Device` 现有边界。
- trap / interrupt / exception 走既有 trap controller 和 commit boundary。
- debug / profile 继续由现有 backend 记录；translator 不能自行制造 guest 可见状态。

必须 fallback 的情况包括：

- unsupported、illegal、decode 不完整或指令长度不确定。
- 需要改变 privilege / CSR / trap state 且没有已定义 helper commit boundary。
- 访问 MMIO、side-effect region、uncacheable region，或无法证明为普通 RAM。
- data / instruction page fault、permission fault、misaligned fault 或 refill fault。
- `fence`、`sfence.vma`、atomic reservation、vector side effect 或设备交互尚未有专门
  helper 合同。

### Prototype 与 preflight guardrail

当前 `interpreter_dbt_prototype` 只在 host smoke 中显式调用：

- 不申请可执行内存。
- 不生成 host code。
- 不创建 block cache。
- 不接入 `functional` 或 `pipeline` 默认调度。
- 不改变 guest 可见语义。

Prototype 只执行 `pure straight-line inlineable` 指令：每条指令仍通过
`InstructionSemantics` 构造 architected effect，commit 仍通过
`apply_commit_boundary`。

`plan_interpreter_dbt_prototype_block()` 在不提交 CPU architectural state 的前提下扫描候选
block，报告：

- block 是否完全 inlineable。
- 可内联前缀指令数。
- 第一个 helper / fallback 边界的 guest PC。
- `helper-required` 或 `fallback-required` 原因。
- first-boundary `boundary_kind`，例如 `memory-load`、`memory-store`、`control-flow`。

如果 preflight 失败，`run_interpreter_dbt_prototype_block()` 不允许提交任何前缀指令，也不推进
PC、`instret` 或 `cycle`。

### Translation-plan dry-run 与 fallback replay

`plan_interpreter_dbt_prototype_hot_path()` 把 top hot-path candidate 交给 preflight 做
dry-run。`run_debug_cli_probe.py --translation-plan` 只在显式 opt-in 时输出
`translation-plan:` 摘要；默认 probe 不新增该行。

`run_interpreter_dbt_prototype_with_functional_fallback()` 只作为 host-smoke-only helper：

- Preflight 成功时继续走 prototype path。
- Preflight 失败时从 block start 回到 `FunctionalBackend` replay 到 first boundary。
- replay 结果必须与直接 functional reference 的 GPR / PC / `instret` / `cycle` 对齐。

这证明 fallback replay 的状态等价性，但不是 runtime JIT harness，也不是 helper inline。

### Invalidation 合同

当前不创建 block cache，但先固定未来必须触发 invalidation 的来源：

- primary image load、debug reset、payload load 或任意覆盖已翻译代码页的 loader 行为。
- guest 写入已翻译 instruction bytes 所在 RAM page 或 cache line。
- `satp` 变化、`sfence.vma`、TLB flush、page table permission / mapping 变化。
- region 属性变化、MMIO / side-effect 判定变化，或无法继续证明目标是普通 RAM。
- 未来若引入 I-cache、write-back、DMA coherence 或 multicore coherence，必须另开设计把这些事件并入 invalidation matrix。

在真正存在 block cache 之前，以上只是合同：默认执行路径不做 invalidation 动作，也不产生可执行 host code。

当前 `dbt_block_cache` 只把这些合同用于 host-smoke-only metadata cache dry-run：缓存
成功翻译的 `DbtTranslationUnit`，按 guest block range lookup，并在 dry-run invalidation
事件命中时删除 metadata 条目。invalidation matrix 已覆盖 empty cache 分类、disjoint
range、overlapping guest store、primary image load、debug reset、`satp` write、
`sfence.vma` 和 region 属性变化等 metadata 删除合同。它仍不是 runtime cache，不持久化，
不缓存 executable host code，也不承担 helper replay 或 scheduler 职责。

当前 `DbtExecutableCacheRuntime` 只把同一 invalidation matrix 用于 host-smoke-only
executable resident entry：成功缓存后拥有 `DbtHostExecutable` 的 executable memory，
lookup 命中可供 opt-in runtime harness 复用；任何命中 invalidation 的事件都会释放该内存
并删除 entry，阻止 stale dispatch。它不是 persistent cache，也不接默认 backend 或 helper
runtime execution。

## 后续推进口径

在证据链和原型边界阶段，后续窄任务不再单独创建 plan 文档。允许直接推进的任务包括：

- 补充更窄的 profile / probe / debug 只读字段。
- 扩展 first-boundary 分类或 translation-plan dry-run 摘要。
- 增加 host smoke，证明 preflight、fallback replay 或默认路径不变。
- 对代表性 workload 做 opt-in 观察，但不接入默认 backend 调度。

这些任务完成后只需要同步 [../status/mainline_status.md](../status/mainline_status.md)，并跑相应验证。

只有进入以下整块任务时，才重新考虑独立计划文档：

- 真正 JIT engine、runtime DBT translator 或 translator 执行路径接入。
- executable IR lowering、host-specific lowering 或 runtime lowering dispatch。
- 默认 runtime JIT dispatch、persistent host code cache 或 helper replay execution。
- IR 语义扩展或 differential execution。
- 默认 runtime 中的 host code emission、executable memory policy 扩展或宿主平台相关代码生成。
- persistent block cache、block lifecycle 和 invalidation implementation。
- workload-level runtime JIT harness 或 backend scheduler。
- memory / CSR / trap / vector helper 的 runtime replay / inline 策略。
- multicore、coherence 或新的 memory consistency 模型。

`DBT translator + IR v0 dry-run` 已完成并归档；后续如果只是在该 v0 范围内补窄测试，
可以直接更新状态并守住相应 host smoke。若进入默认 runtime scheduler、persistent
block cache 或 workload-level JIT harness，需要再开新的设计 / 计划。

## 验证思路

文档-only 更新至少运行：

- `git diff --check`

触达 `src/exec/*`、`src/debug/*`、profile、probe 或 host smoke 时，优先补最窄门禁：

- `cd myCPU && make test-host-interpreter_dbt_prototype_smoke`
- `cd myCPU && make test-host-dbt_block_plan_smoke`
- `cd myCPU && make test-host-dbt_translator_smoke`
- `cd myCPU && make test-host-dbt_ir_eval_smoke`
- `cd myCPU && make test-host-dbt_ir_lowering_smoke`
- `cd myCPU && make test-host-dbt_host_emitter_smoke`
- `cd myCPU && make test-host-dbt_block_cache_smoke`
- `cd myCPU && make test-host-dbt_executable_cache_smoke`
- `cd myCPU && make test-host-dbt_helper_execution_bridge_smoke`
- `cd myCPU && make test-host-dbt_reference_fallback_execution_smoke`
- `cd myCPU && make test-host-dbt_runtime_harness_smoke`
- `cd myCPU && make test-host-dbt_runtime_invalidation_smoke`
- `cd myCPU && make test-host-run_debug_cli_probe`
- `cd myCPU && make test-host-execution_profile_smoke`
- `cd myCPU && make test-host-debug_cli_smoke`

行为边界或共享执行路径有改动时，还要守住：

- `cd myCPU && make test`
- `cd myCPU && make test-pipeline`

默认门禁不应启用 JIT / DBT，不应执行 prototype，不应改变 guest 可见语义。

## 风险与取舍

- 当前阶段看起来不像“真正 JIT”，但它能防止在没有热点证据和回退合同前过早引入执行语义分叉。
- hot-path、pc-cost、branch-target、translation-plan 和 jit-dispatch 输出只应定位为
  debug/probe 观察合同，不是 guest ABI。
- Prototype 很窄，只覆盖 straight-line inlineable block；这是为了先证明 shared semantics +
  commit boundary 可以被 DBT 形态复用，而不是提前承诺完整 JIT。
- Preflight 主动拒绝 helper 或 control-flow 边界，会让 prototype 短期看起来保守，但能避免在没有 precise replay 和 block stitching 前提交错误前缀状态。
- Fallback replay 只证明能从 block start 回到 functional reference 并到达 first boundary；它仍不代表 prototype 已经具备 memory helper、branch stitching 或 runtime fallback 调度能力。
- First-boundary taxonomy 能说明候选块为什么被拒绝，但不代表 helper 可以 inline、block lifecycle 可以持久化，或 runtime scheduler 已经存在。
- Multicore / coherence 与 JIT 同属 `Wave 6`，但不应同刀启动；它需要更重的 memory-order 和 DMA / cache 交界验证。

## 当前有效性说明

- 当前有效：本文档作为主线 `Wave 6 / JIT / DBT` 的证据链、translation contract、
  原型边界和 `DBT translator + IR v0 dry-run` 完成态入口。
- 已完成计划：[../plan/history_plan.md#mainline-wave6-jit-execution-layer-plan](../plan/history_plan.md#mainline-wave6-jit-execution-layer-plan)
- 当前已经完成 hot-path candidate、per-PC / branch-target 观察、translation contract、
  host-smoke-only prototype、preflight guardrail、opt-in translation-plan dry-run、functional
  fallback replay 等价性、first-boundary taxonomy、共享 `DbtBlockPlan` analyzer，以及
  非执行 `dbt_ir` / `dbt_translator` v0 dry-run、translator reject taxonomy、
  helper planning dry-run / matrix 扩展、helper replay contract dry-run、
  IR lowering contract dry-run、JIT engine skeleton dry-run / runtime fallback bridge、
  profile hot-path dispatch dry-run、dispatch result serialization / debug-probe visibility bridge、
  runtime dispatch contract dry-run、executable-cache invalidation enforcement dry-run、
  reference fallback step bridge dry-run、helper execution bridge contract dry-run、
  runtime invalidation hook contract、
  reference fallback execution bridge、
  executable memory policy、
  host code emission v0、
  opt-in runtime harness + differential guardrail、
  executable cache runtime hookup、
  scalar memory helper opt-in execution、
  reference fallback opt-in execution、
  dispatch harness v1、
  runtime harness summary / stats、
  `dbt_ir_eval` semantic differential dry-run 的更宽整数覆盖，以及 metadata-only
  `dbt_block_cache` dry-run / invalidation matrix hardening。
- 当前仍未启动默认 JIT backend、persistent cache、完整 helper runtime execution、
  workload-level scheduler、multicore / coherence、write-back cache、I-cache 和 cache
  maintenance instruction。
- 当前阶段后续微任务不再单独创建 plan 文档；只有进入真正 JIT engine 或其他整块执行面时，才重新启用独立计划文档。
