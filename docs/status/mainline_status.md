# 主线状态

## 文档定位

本文档是仓库唯一的主线实时状态文档，用于记录：

- 当前稳定快照
- 当前优先级
- active wave 与近端 blocker
- 当前仍然有效的风险 / 限制
- 下一步工作

执行过程、阶段性 checklist 和已完成专项统一归档到
[../plan/history_plan.md](../plan/history_plan.md)。

## 关联文档

- 相关设计：
  - [../design/regression_completion_criteria.md](../design/regression_completion_criteria.md)
  - [../design/phase3_ooo_execution_model_design.md](../design/phase3_ooo_execution_model_design.md)
  - [../design/pipeline_speculation_contracts.md](../design/pipeline_speculation_contracts.md)
  - [../design/phase4_preparation_design.md](../design/phase4_preparation_design.md)
  - [../design/future_expansion_roadmap_design.md](../design/future_expansion_roadmap_design.md)
  - [../design/wave5_cache_memory_system_design.md](../design/wave5_cache_memory_system_design.md)
  - [../design/wave6_jit_dbt_readiness_design.md](../design/wave6_jit_dbt_readiness_design.md)
  - [../design/xv6_linux_jit_mainline_design.md](../design/xv6_linux_jit_mainline_design.md)
- 相关状态：
  - [kernel_alpha_status.md](kernel_alpha_status.md)
  - [npu_tpu_accelerator_status.md](npu_tpu_accelerator_status.md)
  - [code_reself_status.md](code_reself_status.md)
- 已完成计划归档：
  - [../plan/history_plan.md](../plan/history_plan.md)
  - [../plan/history_plan.md#mainline-wave6-dbt-translator-ir-v0-plan](../plan/history_plan.md#mainline-wave6-dbt-translator-ir-v0-plan)
  - [../plan/history_plan.md#mainline-wave5-closeout-wave6-readiness-plan](../plan/history_plan.md#mainline-wave5-closeout-wave6-readiness-plan)
  - [../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-f-l1d-lifecycle-guardrail-plan](../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-f-l1d-lifecycle-guardrail-plan)
  - [../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-e-l1d-frontend-observation-plan](../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-e-l1d-frontend-observation-plan)
  - [../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-d-l1d-hardening-plan](../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-d-l1d-hardening-plan)
  - [../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-c-l1d-observation-guardrail-plan](../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-c-l1d-observation-guardrail-plan)
  - [../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-b-minimal-l1d-plan](../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-b-minimal-l1d-plan)
  - [../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-a-signal-contract-plan](../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-a-signal-contract-plan)
  - [../plan/history_plan.md#mainline-wave4-ai-accelerator-slices-plan](../plan/history_plan.md#mainline-wave4-ai-accelerator-slices-plan)
  - [../plan/history_plan.md#mainline-roadmap-rewrite-and-linux-checkpoint-closure-plan](../plan/history_plan.md#mainline-roadmap-rewrite-and-linux-checkpoint-closure-plan)
  - [../plan/history_plan.md#phase4-prep2-memory-observation-shadow-cache-plan](../plan/history_plan.md#phase4-prep2-memory-observation-shadow-cache-plan)
  - [../plan/history_plan.md#xv6-linux-jit-wave1-plan](../plan/history_plan.md#xv6-linux-jit-wave1-plan)

## 目标 / 主题

当前主线仍围绕 `reference-first`、真实 workload bring-up 和可观察性收口展开。
`Wave 3` 已按真实实现现状收口：Linux fourth-stage checkpoint 线冻结在
`timerfd-one-shot-readback-ok`，后续不再默认继续扩同类 syscall breadth。
主线 `Wave 4` 的 AI accelerator A/B/C 三段切片已经完成并归档。

`Wave 5 / cache / memory-system` 的 `Slice A ~ F` 已完成首轮收口：已有
`shadow_cache` 证据补上一条 pipeline-side `xv6` memory observation guardrail；
最小 L1D 已以默认关闭、显式 opt-in、RAM-only、write-through、no dirty write-back
的方式接入 data load/store；debug/probe/frontend 已有只读观察面；跨 line、store
miss、non-cacheable / side-effect / unmapped / refill fault、atomic、page-walk、
instruction fetch、load / reset / payload lifecycle 等边界也已经 hardening。
这些结果足够把 `Wave 5` 作为首轮 cache / memory-system 收口，但仍不代表
write-back、DMA coherence、multicore、JIT、I-cache 或 cache maintenance
instruction 已启动。

当前 active wave 是 `Wave 6 / JIT / DBT`。它已经完成证据链和原型边界阶段的首轮收口：
hot-path candidate、per-PC / branch-target 观察、translation contract、
host-smoke-only prototype、preflight guardrail、opt-in translation-plan dry-run、
functional fallback replay 等价性、first-boundary taxonomy、typed boundary、
dry-run IR metadata、future block-cache invalidation dry-run 和共享 `dbt_block_plan`
analyzer 入口都已有窄合同与验证。
`DBT translator + IR v0 dry-run` 已完成，当前已有非执行 translator 前端和 typed IR
形状验证。
`IR semantic differential dry-run` 已完成第一刀：`dbt_ir_eval` 只解释已成功翻译的
IR v0，并在 host smoke 中同 reference 执行对齐 GPR、fallthrough PC 和 retired count。
`translator reject taxonomy` 已完成第一刀：`DbtTranslationUnit` 暴露 typed
`DbtRejectKind`、reject PC、raw instruction 和 typed boundary，同时保留原字符串兼容字段。
`helper planning dry-run` 已完成 matrix 扩展：`DbtBlockPlan` / `DbtTranslationUnit`
暴露 typed `DbtHelperPlan`，当前固定 memory load / store、CSR write、atomic 和 vector
helper metadata；fallback-required 边界不附带 helper plan。
`helper replay contract dry-run` 已完成第一刀：`dbt_helper_replay` 只把 rejected helper
unit 分类成未来 replay 需要的 memory / CSR / atomic / vector effect flags，不执行 helper，
不提交 CPU state，也不生成 helper 前缀 IR。
`IR lowering contract dry-run` 已完成第一刀：`dbt_ir_lowering` 把成功翻译的 IR v0
降成 backend-neutral lowered ops，固定 operand kind、ALU op、XLEN / word width、
fallthrough 和 unsupported IR 整体拒绝合同；它不生成宿主代码、不申请 executable memory，
也不接入 runtime dispatch。
`JIT engine skeleton dry-run / runtime fallback bridge` 已完成第一刀：`dbt_jit_engine`
把 metadata cache lookup、`DbtBlockPlan`、translator、IR lowering、helper replay
contract、profile hot-path dispatch 和 reference fallback 选择串成单一 dry-run 入口；
它只返回调度决策和统计，不生成宿主代码、不申请 executable memory、不执行 guest code，
也不成为默认 backend。
`dispatch result serialization / debug-probe visibility bridge` 已完成第一刀：dry-run
result 现在有稳定 summary 结构、`jit-dispatch:` 单行文本、debug CLI
`jit_dispatch` opt-in JSON 事件，以及 `run_debug_cli_probe.py --jit-dispatch` 显式
probe 输出；该观察面仍只读，不驱动 runtime。
`runtime dispatch contract dry-run` 已完成第一刀：`dbt_runtime_dispatch` 只把
`DbtJitDryRunResult` 映射成运行时调度合同，区分 lowered block、helper bridge to
reference 和 plain reference step；它不调用 helper、不执行 reference step、不提交 CPU
state，也不生成宿主代码或申请 executable memory。
`executable-cache invalidation enforcement dry-run` 已完成第一刀：
`dbt_executable_cache` 只接收 lowered-block runtime dispatch contract，并复用现有
invalidation plan 强制删除 guest store / primary image load / TLB flush 等事件命中的
resident contract；它不保存 host code、不申请 executable memory、不执行 guest code。
`reference fallback step bridge dry-run` 已完成第一刀：
`dbt_reference_fallback` 只把 runtime dispatch contract 的 plain reference step 和
helper-bridge-to-reference 路径收成未来 fallback 入口合同，保留 reject / helper metadata
与 no-execution flags；它不调用 functional backend step、不执行 helper、不提交 CPU state。
`helper execution bridge contract dry-run` 已完成第一刀：
`dbt_helper_execution_bridge` 只把 helper replay plan 转成 future helper execution request，
保留 memory / CSR / atomic / vector operands、effect flags、trap fallback 和 commit-boundary
合同；它不执行 helper、不提交 CPU state，也不生成 host code。
`runtime invalidation hook contract` 已完成第一刀：
`dbt_runtime_invalidation` 把 guest store、payload load、primary image load、debug reset、
`satp`、`sfence.vma` 和 region 属性变化映射到 executable-cache dry-run enforcement；
它只阻止 stale dispatch metadata，不提交 CPU state、不生成 host code。
`reference fallback execution bridge` 已完成第一刀：
`dbt_reference_fallback_execution` 把 fallback plan 分类成 plain reference step、
helper-bridge reference step、JIT miss 和 trap/fault placeholder execution request；
它只描述 future reference backend 调用，不执行 reference step、不提交 CPU state。
`executable memory policy` 已完成第一刀：
`dbt_executable_memory` 提供 POSIX allocation / write / seal RX / release 合同，拒绝
zero-size、越界写、seal 后写入和重复释放；当前只管理内存生命周期，不执行宿主代码。
`host code emission v0` 已完成第一刀：
`dbt_host_emitter` 只接受成功 lowered 的 pure integer straight-line block，生成
x86-64 SysV `uint64_t (*)(uint64_t* gpr, uint64_t pc)` 形态的宿主代码，并通过
executable memory policy 完成 W->RX 生命周期；host smoke 用 IR eval differential
验证 GPR / fallthrough PC。unsupported lowered op、rejected lowering 或缺失
fallthrough 都整体拒绝，不返回可执行前缀 code。
`opt-in runtime harness + differential guardrail` 已完成第一刀：
`dbt_runtime_harness` 只在 host smoke 显式调用时执行一个 pure integer straight-line
block，路径必须通过 `DbtBlockPlan -> translator -> IR eval -> lowering -> host emitter ->
executable memory policy`，并在提交 CPU state 前与 IR eval differential 对齐。helper /
fallback / trap-risk block 会拒绝并要求 reference fallback，不执行 host code、不提交状态；
默认 `Machine` backend 仍是 `functional`。
`IR semantic coverage` 已扩到更宽的 pure integer 子集：逻辑运算、shift immediate /
register、signed / unsigned set-less-than、`lui` / `auipc` 和 RV64 word ops 都有
reference differential smoke。
`metadata-only block cache` 已完成第一刀：`dbt_block_cache` 只缓存成功翻译的
`DbtTranslationUnit` metadata，固定 exact-range lookup、hit / miss 计数、rejected unit
不入 cache，以及复用 invalidation dry-run 合同删除 metadata 条目；当前也固定了
invalidation check / examined entries / non-invalidating event 计数和空 cache 全局事件分类。

这些结果只说明“哪些 guest PC 区间值得观察、哪些能进入 IR v0 dry-run、为什么剩余部分
仍不能翻译、回退是否等价、第一拒绝边界是什么”。
当前仍不实现默认 JIT backend、persistent / executable block cache、helper runtime execution、
multicore、coherence 或新的 memory consistency 模型，也不改变 guest 可见语义。

## 当前状态

- 当前仓库已经是一个已可运行的模拟器原型，不是纯设计稿。
- `phase1-stable`（`283aee6`）仍是 Phase 1 冻结参考点。
- `xv6-riscv` 已在真实 `virtio-blk` board path 上稳定到 shell，当前主要承担
  workload guardrail 角色，不再是近端 blocker。
- Linux `linux_proto` 的 repo-generated block-rootfs fourth-stage harness
  已扩展并冻结到：
  - `mycpu linux userland: stage=timerfd-one-shot-readback-ok`
- 这条冻结边界之前，fourth-stage 已连续跨过多类 Linux userland 合同；
  当前 baseline 已不只覆盖文件/映射边界、进程 control-plane、路径 mutation、
  ready-queue 和 ancillary fd passing，还覆盖了：
  - `sendmsg(2)` + `recvmsg(2)` + `SCM_RIGHTS` 的 UNIX ancillary fd passing /
    sender-exit / parent readback 合同
  - `copy_file_range(2)` 的 kernel-side file copy / dst readback 合同
  - `splice(2)` 的 file -> pipe -> file zero-copy transfer / dst readback 合同
  - `statx(2)` 的 relative-dirfd metadata ABI / mask / size-mode readback 合同
  - `inotify_init1(2)` + `inotify_add_watch(2)` + `read(2)` 的 fs-notify queue /
    close-write event 合同
  - `timerfd_create(2)` + `timerfd_settime(2)` + `read(2)` 的 one-shot timer queue /
    expiration readback 合同
- 仓库默认位置仍不携带真实 Linux `Image`；因此 `timerfd` 边界在默认门禁里由
  build/string/probe 合同锁住，真实 `Image + rootfs.ext4` runtime 仍是 opt-in
  验证项。后续如果要把它作为发布级 runtime 断言，必须显式提供 `Image`
  重新跑对应 runtime guardrail。
- `P4-prep-1` 和 `C1 / P4-prep-2 memory observation / shadow cache` 已完成。
  当前稳定的观测 guardrail 主要是：
  - pipeline vector CNN 的 `shadow_cache` RAM baseline
  - functional `xv6` 的稳定 profile / `shadow_cache` baseline
  - functional `linux_proto` dummy-payload observation baseline
- `debug/frontend`、`kernel_alpha` 十条基线、`make test` / `make test-pipeline`
  和现有 workload smoke 都已进入维护态。
- 当前 active wave 是 `Wave 6 / JIT / DBT`，不是继续深挖 Linux checkpoint 的
  `Wave 3`，也不是 AI accelerator 后续专项或完整 multicore / coherence 专项。
- 主线 `Wave 4` 的 AI accelerator 切片 A 已完成：`bounded dynamic shape`
  已从 `dynamic GEMM / FC-like` 扩到现有 op family 的正向或 fail-closed 合同，
  并新增 `dynamic_tiny_model` 动态小模型 workload。
- 主线 `Wave 4` 的 AI accelerator 切片 B 已完成：`timed-simple` profile 现在把
  tile setup 归入 `stall_cycles`，debug snapshot 保持 aggregate-only schema，
  frontend 已为 `guest_ai_accel_demo` 增加 workload presentation 与 AI accelerator
  只读 counters 面板。
- 主线 `Wave 4` 的 AI accelerator 切片 C stretch 已完成：新增静态 `fp32`
  row-wise `Softmax`，并新增 `tiny_attention_static` host workload，固定验证
  `gemm -> softmax -> gemm` 的最小 attention-like profile 闭环。
- 主线 `Wave 5` 的 `Slice A / signal + contract` 已完成：pipeline-side memory
  signal 固定为 `run_debug_cli_probe` 的 `xv6 --backend pipeline` 5000-cycle
  probe；该 guardrail 只证明 memory observation / `shadow_cache` 输出合同，不证明
  pipeline 已完整 boot `xv6`。
- 主线 `Wave 5` 的 `Slice B / minimal executable L1D` 已完成：新增
  `SimpleL1DataCache`，以默认关闭的 `Machine` 开关显式启用，`AddressSpace`
  只在 data load/store 绑定且启用时走 L1D；默认 reference path 不变，且继续
  保持 RAM-only、write-through、no dirty write-back、MMIO / side-effect bypass、
  atomic / fence 保守处理，以及 DMA 不透明 coherence 的边界。
- 主线 `Wave 5` 的 `Slice C / L1D opt-in observation + guardrail` 已完成：默认关闭
  的 L1D 现在有顶层 `l1_data_cache` debug snapshot 只读 counters，
  `run_debug_cli_probe.py --l1d` 可显式打开 L1D 并输出 `l1d-cache:` 摘要；默认
  执行路径不变，既有 `shadow_cache` 字段语义不变。
- 主线 `Wave 5` 的 `Slice D / L1D hardening` 已完成：本轮固定跨 cache line
  store bypass 后失效重叠 line、store miss write-through + no-allocate、fault
  refill 不污染 cache line，以及 non-cacheable / side-effect / unmapped、
  atomic、page-walk、instruction fetch 和默认路径继续 bypass / 默认关闭的合同。
- 主线 `Wave 5` 的 `Slice E / L1D frontend observation` 已完成：frontend 平台组
  新增只读 `L1 data cache` 面板，展示已有顶层 `l1_data_cache` debug snapshot
  counters；缺失字段和默认关闭 snapshot 均有稳定 fallback，默认路径仍不打开 L1D。
- 主线 `Wave 5` 的 `Slice F / L1D lifecycle guardrail` 已完成：opt-in L1D 在
  primary load / reset / payload load 下的 line state 与 counters 生命周期合同已
  固定；payload 覆盖已缓存 RAM line 后会失效对应 L1D line，不扩 cache 功能面。
- 主线 `Wave 5 closeout / Wave 6 readiness` 已完成：`Wave 5` `Slice A ~ F` 作为
  首轮 cache / memory-system 收口；`Wave 6` 正式激活，但第一刀只做 `JIT / DBT`
  hot-path evidence。
- 主线 `Wave 6` 证据链和原型边界阶段已完成首轮收口：`translation-candidate:`、
  `pc-cost:`、`branch-target:`、opt-in `translation-plan:`、host-smoke-only
  `interpreter_dbt_prototype`、preflight 整块拒绝、functional fallback replay 和
  first-boundary `boundary_kind` 都已具备窄观察 / 验证合同；当前补充了代码级
  `InterpreterDbtBoundaryKind`、pure inlineable block 的 dry-run IR op，以及 future
  block-cache invalidation dry-run 分类；并已抽出 `src/exec/dbt_block_plan.{h,cpp}` 作为
  prototype 和未来 translator 可共享的 block analyzer 入口。
- 主线 `Wave 6` 已完成 `DBT translator + IR v0 dry-run`：新增非执行 typed IR 和
  translator dry-run，输入来自 `DbtBlockPlan`，输出 `DbtTranslationUnit`，仍不接入
  默认 runtime。
- 主线 `Wave 6` 已完成 `IR semantic differential dry-run` 第一刀：新增
  `src/exec/dbt_ir_eval.{h,cpp}`，只解释 IR v0 的寄存器语义，并用
  `tests/host/dbt_ir_eval_smoke.cpp` 对齐 reference 执行的 GPR、fallthrough PC 和
  retired instruction count；当前已扩到逻辑运算、shift immediate / register 和
  signed / unsigned set-less-than、`lui` / `auipc` 与 RV64 word ops。
- 主线 `Wave 6` 已完成 `translator reject taxonomy` 第一刀：`DbtTranslationUnit`
  新增 typed `DbtRejectKind`、reject PC 和 raw instruction，translator smoke 固定
  memory helper、control-flow、TLB flush 和 unsupported IR 的 reject metadata。
- 主线 `Wave 6` 已完成 `helper planning dry-run` matrix 扩展：`DbtBlockPlan`
  新增 typed `DbtHelperPlan`，并由 `DbtTranslationUnit` 透传；host smoke 固定 memory
  load / store、CSR write、atomic 和 vector helper 的 kind、PC、raw instruction
  和最小参数。
- 主线 `Wave 6` 已完成 `helper replay contract dry-run` 第一刀：新增
  `src/exec/dbt_helper_replay.{h,cpp}`，只从 rejected `DbtTranslationUnit` 生成
  非执行 replay effect flags，并用 host smoke 固定 scalar memory、CSR、atomic
  和 vector helper 分类。
- 主线 `Wave 6` 已完成 `IR lowering contract dry-run` 第一刀：新增
  `src/exec/dbt_ir_lowering.{h,cpp}`，只把 ok `DbtTranslationUnit` 的 IR v0
  降成 backend-neutral lowered ops，并用 host smoke 固定 rejected unit / unknown IR
  不返回前缀 lowering。
- 主线 `Wave 6` 已完成 `JIT engine skeleton dry-run / runtime fallback bridge`
  第一刀：新增 `src/exec/dbt_jit_engine.{h,cpp}`，只串联 metadata cache lookup、
  block planning、translator、lowering、helper bridge 和 reference fallback 决策；
  当前已补 profile hot-path dispatch dry-run，host smoke 固定 cache hit / miss、
  helper-required bridge、control-flow fallback、profile 无候选 fallback 和不生成
  host code / 不执行 guest code 合同。
- 主线 `Wave 6` 已完成 `dispatch result serialization / debug-probe visibility bridge`
  第一刀：`DbtJitDryRunSummary`、debug CLI `jit_dispatch` 和
  `run_debug_cli_probe.py --jit-dispatch` 暴露 action/source/cache/reject/helper/no-host-code
  字段，仍不接 runtime backend。
- 主线 `Wave 6` 已完成 `runtime dispatch contract dry-run` 第一刀：新增
  `src/exec/dbt_runtime_dispatch.{h,cpp}`，只把 `DbtJitDryRunResult` 分类成
  lowered block、helper bridge to reference 和 plain reference step 三种非执行
  runtime dispatch 合同，并用 host smoke 固定 no-host-code / no-executable-memory /
  no-guest-execution 边界。
- 主线 `Wave 6` 已完成 `executable-cache invalidation enforcement dry-run`
  第一刀：新增 `src/exec/dbt_executable_cache.{h,cpp}`，只缓存 lowered-block dispatch
  contract metadata，并用 host smoke 固定 overlapping guest store / global event
  invalidation enforcement、stale dispatch prevention 计数和 rejected helper/reference
  contract 不入 cache。
- 主线 `Wave 6` 已完成 `reference fallback step bridge dry-run` 第一刀：新增
  `src/exec/dbt_reference_fallback.{h,cpp}`，只把 runtime dispatch contract 转成
  future reference fallback plan，并用 host smoke 固定 plain reference step、
  helper-bridge-to-reference、lowered-block rejection 和 no-execution 边界。
- 主线 `Wave 6` 已完成 `helper execution bridge contract dry-run` 第一刀：新增
  `src/exec/dbt_helper_execution_bridge.{h,cpp}`，只把 helper replay plan 转成
  future helper execution request，并用 host smoke 固定 scalar memory、CSR、atomic、
  vector request metadata 和 no-execution 边界。
- 主线 `Wave 6` 已完成 `runtime invalidation hook contract` 第一刀：新增
  `src/exec/dbt_runtime_invalidation.{h,cpp}`，只把 runtime invalidation event 转接到
  executable-cache dry-run enforcement，并用 host smoke 固定 guest store、payload load、
  image/reset/`satp`/`sfence.vma`/region 事件。
- 主线 `Wave 6` 已完成 `reference fallback execution bridge` 第一刀：新增
  `src/exec/dbt_reference_fallback_execution.{h,cpp}`，只把 fallback plan 转成 future
  reference backend execution request，并用 host smoke 固定 JIT miss、helper bridge、
  trap/fault placeholder 和 no-execution 边界。
- 主线 `Wave 6` 已完成 `executable memory policy` 第一刀：新增
  `src/exec/dbt_executable_memory.{h,cpp}`，用 POSIX `mmap/mprotect/munmap`
  固定 allocation / write / seal RX / release 生命周期，并用 host smoke 固定错误回退合同。
- 主线 `Wave 6` 已完成 `host code emission v0` 第一刀：新增
  `src/exec/dbt_host_emitter.{h,cpp}`，只把 pure integer straight-line lowered block
  发射成 x86-64 SysV 小函数；host smoke 通过真实调用该函数并与 `dbt_ir_eval`
  对齐 GPR / fallthrough PC，同时固定 unsupported lowered op 不暴露前缀 code。
- 主线 `Wave 6` 已完成 `opt-in runtime harness + differential guardrail` 第一刀：
  新增 `src/exec/dbt_runtime_harness.{h,cpp}`，只在 host smoke 中显式执行单个 pure
  integer block；提交 CPU state 前必须和 IR eval differential 对齐，helper block
  保守拒绝并保持状态不变，默认 `Machine` backend 仍是 `functional`。
- 主线 `Wave 6` 已完成 `metadata-only block cache` 第一刀：新增
  `src/exec/dbt_block_cache.{h,cpp}`，只缓存成功翻译的 `DbtTranslationUnit`
  metadata，并用 `tests/host/dbt_block_cache_smoke.cpp` 固定 key / hit / miss /
  rejected insert / invalidation dry-run / invalidation matrix hardening 合同。

## 当前优先级

1. 当前 `Wave 6` 已激活并完成证据链 / 原型边界首轮收口；typed boundary、dry-run
   IR metadata、future block-cache invalidation dry-run、共享 `DbtBlockPlan` analyzer
   入口、
   [DBT translator + IR v0 dry-run](../plan/history_plan.md#mainline-wave6-dbt-translator-ir-v0-plan)、
   IR semantic differential dry-run、更宽 IR semantic coverage、translator reject taxonomy、
   helper planning dry-run / matrix 扩展、helper replay contract dry-run、
   IR lowering contract dry-run、JIT engine skeleton dry-run / runtime fallback bridge、
   profile hot-path dispatch dry-run、dispatch result serialization / debug-probe visibility bridge、
   runtime dispatch contract dry-run、executable-cache invalidation enforcement dry-run、
   reference fallback step bridge dry-run、helper execution bridge contract dry-run、
   runtime invalidation hook contract、reference fallback execution bridge、
   executable memory policy、host code emission v0、
   opt-in runtime harness + differential guardrail，
   以及 metadata-only block cache / invalidation matrix hardening
   第一刀已作为补充合同落地。
   当前执行计划已完成并归档：
   [Wave 6 JIT Execution Layer 实现计划](../plan/history_plan.md#mainline-wave6-jit-execution-layer-plan)。
   下一刀如继续推进，应优先选择 executable cache runtime hookup 或更窄的 helper
   execution opt-in 设计；不得直接进入默认 JIT backend、长期 block cache、multicore、
   coherence 或新的 memory consistency 模型。
2. AI accelerator 的 `INT4 / training / MobileNet / Linux-facing NPU driver /
   real DMA overlap / multi outstanding queue` 等后续专项不得改写主线 `Wave 6`
   定位。
3. Linux `timerfd-one-shot-readback-ok` 作为 `Wave 3` 冻结边界进入守成；除非真实
   runtime 重新暴露新 blocker，不再继续向当前 fourth-stage smoke 追加同类
   `open-fd / mmap / pipe / futex / socketpair / openat2 / pidfd / signalfd /
   renameat2 / eventfd / epoll / sendmsg / recvmsg / SCM_RIGHTS / copy_file_range /
   splice / statx / inotify / timerfd` 微分支。
4. 把 `xv6` shell、Linux probe、`kernel_alpha`、`pipeline`、debug CLI 和
   现有回归矩阵守成稳定 guardrail。
5. 继续积累 `shadow_cache / observation / representative workload` 证据；后续
   cache / DMA / multicore / coherence 或 JIT engine 都必须另开设计/计划并补足验证。

## 关键历史节点

- `2026-04-30`
  - 主线 `Wave 6` 完成并归档 `DBT translator + IR v0 dry-run`：
    [../plan/history_plan.md#mainline-wave6-dbt-translator-ir-v0-plan](../plan/history_plan.md#mainline-wave6-dbt-translator-ir-v0-plan)。
    本轮新增非执行 `dbt_ir` 与 `dbt_translator` 前端，translator 只消费共享
    `DbtBlockPlan`，输出 `DbtTranslationUnit`；helper / fallback plan 和 IR v0 不支持的
    inlineable 指令都会整体 reject，不返回可消费前缀 IR。默认 runtime、JIT engine、
    host code emission、persistent block cache 和 multicore / coherence 仍不启动。
  - 主线 `Wave 6` 完成 `IR semantic differential dry-run` 第一刀：新增非执行
    `dbt_ir_eval`，只解释 IR v0 的寄存器语义；host smoke 覆盖依赖链、`x0` 写丢弃、
    负立即数、wraparound add、fallthrough PC、retired instruction count，以及 reject unit
    的拒绝透传；当前进一步覆盖逻辑运算、shift immediate / register 和 signed / unsigned
    set-less-than、`lui` / `auipc` 与 RV64 word ops。默认 runtime、JIT engine、
    host code emission 和 block cache 仍不启动。
  - 主线 `Wave 6` 完成 `translator reject taxonomy` 第一刀：`DbtTranslationUnit`
    新增 typed `DbtRejectKind`、reject PC 和 raw instruction；translator smoke 固定
    memory helper、control-flow、TLB flush、unsupported IR 和稳定 reject kind name。
    默认 runtime、JIT engine、host code emission 和 helper replay 仍不启动。
  - 主线 `Wave 6` 完成 `helper planning dry-run` matrix 扩展：`DbtBlockPlan` /
    `DbtTranslationUnit` 新增 typed `DbtHelperPlan`，host smoke 固定 memory load /
    store、CSR write、atomic 和 vector helper metadata 透传；默认 runtime、JIT engine、
    host code emission、helper replay 和 helper inline 仍不启动。
  - 主线 `Wave 6` 完成 `helper replay contract dry-run` 第一刀：新增非执行
    `dbt_helper_replay`，host smoke 固定 memory / CSR / atomic / vector helper 的
    replay kind 和 effect flags；默认 runtime、JIT engine、host code emission 和
    helper 执行仍不启动。
  - 主线 `Wave 6` 完成 `IR lowering contract dry-run` 第一刀：新增非执行
    `dbt_ir_lowering`，host smoke 固定 IR v0 到 backend-neutral lowered ops 的映射、
    rejected unit / unknown IR 整体拒绝和不暴露前缀 lowered ops；默认 runtime、
    JIT engine、host code emission 和 executable lowering 仍不启动。
  - 主线 `Wave 6` 完成 `JIT engine skeleton dry-run / runtime fallback bridge`
    第一刀：新增非执行 `dbt_jit_engine`，host smoke 固定 inline lowered-ready、
    metadata cache reuse、helper bridge required、reference fallback、profile hot-path
    dispatch 和 no-host-code flags；默认 runtime、host code emission、executable cache
    和 guest execution 仍不启动。
  - 主线 `Wave 6` 完成 `dispatch result serialization / debug-probe visibility bridge`
    第一刀：新增稳定 `DbtJitDryRunSummary` / `jit-dispatch:` 文本摘要，debug CLI
    支持显式 `jit_dispatch`，probe 支持 `--jit-dispatch`；默认 runtime、host code
    emission、executable cache 和 guest execution 仍不启动。
  - 主线 `Wave 6` 完成 `metadata-only block cache` 第一刀：新增非执行
    `dbt_block_cache`，只缓存 ok `DbtTranslationUnit` metadata；host smoke 固定
    exact-range lookup、hit / miss 计数、同 key replacement、rejected unit 不入 cache、
    invalidation check / examined entries / non-invalidating event 计数、empty cache 全局事件
    分类、overlapping guest store 局部删除和 `sfence.vma` / primary image load / debug reset /
    `satp` write / region 属性变化全局删除。默认 runtime、JIT engine、host code emission
    和 persistent / executable block cache 仍不启动。
  - 主线 `Wave 6` 证据链和原型边界阶段继续做代码边界收口：新增
    `src/exec/dbt_block_plan.{h,cpp}`，把 hot-path block preflight、typed boundary、
    pure inlineable dry-run IR metadata 和 future block-cache invalidation dry-run 从
    `interpreter_dbt_prototype` 中抽成共享 analyzer 入口；prototype 通过兼容 wrapper 继续
    消费该入口，新增 `tests/host/dbt_block_plan_smoke.cpp` 固定共享 API。当时仍不生成
    host code，也未实现 DBT translator / JIT engine / persistent block cache。
  - 主线 `Wave 6` 证据链和原型边界阶段补充代码级实现前置合同：`InterpreterDbtPrototypePlan`
    现在暴露 typed `InterpreterDbtBoundaryKind` 和 pure inlineable block 的 dry-run IR op；
    `sfence.vma` / TLB flush 不再只归为泛化 `control-flow`；新增 future block-cache
    invalidation dry-run 分类，覆盖 primary image load、debug reset、payload load、
    guest store、`satp` write、`sfence.vma` 和 region 属性变化等事件。默认执行路径仍不接入
    JIT / DBT runtime、不生成 host code、不创建 persistent block cache。
  - 主线 `Wave 6` 证据链和原型边界阶段继续收口：opt-in `translation-plan` dry-run、
    host-smoke-only functional fallback replay 和 first-boundary taxonomy 已落地。当前能报告
    top candidate 的 preflight 结果、从 block start 回到 functional reference 执行到
    first boundary，并用 `boundary_kind` 区分 `memory-load`、`memory-store`、
    `control-flow` 等第一拒绝点。默认执行路径仍不启用 JIT / DBT、不执行 prototype、
    不生成 host code、不创建 block cache。
  - 按新的推进口径，`Wave 6` 后续证据链和原型边界微任务不再为每个小补丁单开 plan；
    只在进入真正 JIT engine、host code、长期 block cache、runtime scheduler 或
    multicore / coherence 等整块任务时重新启用独立计划文档。

- `2026-04-29`
  - `Wave 3` 按当前真实实现收口：Linux checkpoint 线冻结到
    `timerfd-one-shot-readback-ok`，后续不再继续扩同类 Linux syscall breadth；
    `Wave 4` 随后激活并完成 AI accelerator 三段切片。
  - 同日为主线 `Wave 4` 打开 AI accelerator 三段切片计划：先做 `bounded dynamic
    shape / workload`，再做 profile / frontend 观察面，最后把 `Softmax + tiny
    static attention` 作为可降级 stretch；训练、`INT4`、MobileNet、Linux-facing
    NPU driver 等剩余远期目标后移到 AI accelerator 后续专项阶段。
  - 主线 `Wave 4` 的 AI accelerator 切片 A 已完成：`bounded dynamic shape`
    已从 `dynamic GEMM / FC-like` 扩到现有 op family 的正向或 fail-closed 合同，
    并新增 `dynamic_tiny_model` 动态小模型 workload。
  - 主线 `Wave 4` 的 AI accelerator 切片 B 已完成：profile / timing attribution
    已把 tile setup 归到 `stall_cycles`，host profile 与 guest debug counters
    稳定暴露 aggregate 字段；frontend 的 `guest_ai_accel_demo` workload card 和
    平台组 AI accelerator panel 已接入 `KMVAI`、queue、scratchpad、DMA bytes、
    device / dma / compute / stall cycles 与 utilization 等只读字段。
  - 主线 `Wave 4` 的 AI accelerator 切片 C stretch 已完成：新增静态 `fp32`
    row-wise `Softmax` 和 `tiny_attention_static` workload，固定验证
    `gemm -> softmax -> gemm` 的最小 attention-like 闭环；它不是完整 attention /
    Transformer runtime。
  - 主线 `Wave 4` 变更已提交为 `b11d10a`
    `feat(主线): 收口 Wave 4 AI accelerator 切片`。
  - 主线 `Wave 5 / cache / memory-system` 的 `Slice A / signal + contract` 已完成并
    归档：
    [../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-a-signal-contract-plan](../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-a-signal-contract-plan)。
    这一刀新增 pipeline-side `xv6 --backend pipeline` memory observation guardrail，
    继续保持 Linux 真实 `Image` runtime 为 opt-in，并固定后续最小 L1D 的
    RAM-only、write-through、no dirty write-back、bypass side-effect region 与 DMA
    不透明 coherence 合同。
  - 同日完成主线 `Wave 5` `Slice B / minimal executable L1D` 并归档：
    [../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-b-minimal-l1d-plan](../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-b-minimal-l1d-plan)。
    这一刀新增默认关闭、可显式启用、RAM-only、write-through 的最小 L1D 执行模型，
    只接入 data load/store；instruction fetch、page walk、atomic、MMIO、
    unmapped 和 side-effect region 继续 bypass L1D。
  - 同日完成主线 `Wave 5` `Slice C / L1D opt-in observation + guardrail` 并归档：
    [../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-c-l1d-observation-guardrail-plan](../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-c-l1d-observation-guardrail-plan)。
    这一刀新增顶层 `l1_data_cache` debug snapshot 只读 counters，并给
    `run_debug_cli_probe.py` 增加显式 `--l1d` guardrail；默认路径仍关闭 L1D，
    既有 `shadow_cache` 输出合同不变。
  - 同日完成主线 `Wave 5` `Slice D / L1D hardening` 并归档：
    [../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-d-l1d-hardening-plan](../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-d-l1d-hardening-plan)。
    这一刀固定跨 cache line store bypass 后失效重叠 line、store miss
    write-through + no-allocate、non-cacheable / side-effect / unmapped / refill
    fault 不污染 cache state，以及 atomic、page-walk、instruction fetch 继续
    bypass L1D；默认 `make test` / `make test-pipeline` 仍不打开 L1D。
  - 同日完成主线 `Wave 5` `Slice E / L1D frontend observation` 并归档：
    [../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-e-l1d-frontend-observation-plan](../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-e-l1d-frontend-observation-plan)。
    这一刀把已有顶层 `l1_data_cache` debug snapshot counters 接入 frontend 平台组
    只读观察面，缺失字段和默认关闭 snapshot 都保持稳定 fallback；本轮不扩
    debug ABI、cache 功能面或默认 L1D 开关。
  - 同日完成主线 `Wave 5` `Slice F / L1D lifecycle guardrail` 并归档：
    [../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-f-l1d-lifecycle-guardrail-plan](../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-f-l1d-lifecycle-guardrail-plan)。
    这一刀固定 opt-in L1D 在 primary load / debug reset 下继续清空 line state
    与 counters，debug reset 保留 `enabled=true`；payload load 覆盖已缓存 RAM
    line 后会失效对应 L1D line，避免后续 data load 读到旧 payload bytes。
  - 同日完成主线 `Wave 5 closeout / Wave 6 readiness` 并归档：
    [../plan/history_plan.md#mainline-wave5-closeout-wave6-readiness-plan](../plan/history_plan.md#mainline-wave5-closeout-wave6-readiness-plan)。
    这一刀把 `Wave 5` `Slice A ~ F` 固定为首轮 cache / memory-system 收口，并把
    主线 active wave 切到 `Wave 6`；`Wave 6` 第一刀选择 `JIT / DBT hot-path
    evidence`，不实现 JIT engine、block cache、host code emission、multicore 或
    coherence。
  - 同日主线 `Wave 6` 进入 JIT / DBT 证据链阶段：先固定 hot-path candidate 和
    translation contract，再补 `pc_costs` / `branch_targets`、host-smoke-only
    `interpreter_dbt_prototype` 与 preflight 整块拒绝 guardrail。此阶段只证明共享语义和
    commit boundary 可以被 DBT 形态复用，不实现 JIT engine、IR、block cache 或 host code。
  - 这次收口把 `xv6 / Linux` pipeline-side memory signal 明确降级为
    `Wave 5 / cache` 前置证据，而不是阻塞 `Wave 4` 的硬门槛；当前 `Wave 4`
    依赖的观测证据来自 pipeline vector CNN、functional `xv6`、functional
    `linux_proto` dummy-payload 以及现有 pipeline guest demos。
  - Linux fourth-stage baseline 已推进到
    `timerfd-one-shot-readback-ok`。
  - 这一轮先后新增验证了：
    `pidfd_open + ppoll + waitid(P_PIDFD)` 新式进程句柄合同，
    `rt_sigprocmask + signalfd4` 的 `SIGCHLD` 阻塞 / 消费 / child-exit readback，
    `renameat2(RENAME_EXCHANGE)` 的原子路径交换 / reopen / readback，
    `eventfd2 + epoll_create1 + epoll_ctl + epoll_pwait` 的 ready-queue / counter readback，
    `sendmsg + recvmsg + SCM_RIGHTS` 的 ancillary fd passing / parent readback，
    `copy_file_range` 的 kernel-side file copy / dst readback，
    `splice(file -> pipe -> file)` 的 zero-copy transfer / dst readback，
    `statx(relative dirfd)` 的 metadata ABI / size-mode readback，
    `inotify` 的 close-write fs-notify queue event 合同，
    以及 `timerfd` 的 one-shot timer queue expiration 合同。
  - 这说明 Linux checkpoint 线已经给出足够高信号的后续 userland contract；
    当前继续扩同类 marker 的边际收益低于切到 `Wave 4`。
- `2026-04-28`
  - Linux fourth-stage baseline 已推进到
    `openat2-self-beneath-readback-ok`。
  - 这一轮先后新增验证了：
    `pipe2` 匿名 IPC、`futex + MAP_SHARED|MAP_ANONYMOUS` 跨进程同步、
    `AF_UNIX socketpair` child-write / parent-readback / EOF，
    以及 `openat2(2)` dirfd-relative self-reopen / ELF header readback。
  - 其中 `socketpair` 和 `openat2` 说明 Linux checkpoint 线仍在持续产出新的高信号
    userland contract，而不是回到匿名 IPC 或旧 `open-fd / mmap` 的同类微分支。
  - 这也说明 Linux checkpoint 线当时仍在持续产出高信号边界，当时还不到切
    `Wave 4` 的时机。
- `2026-04-27`
  - repo-generated block-rootfs `/init + post-init` 路径与 fourth-stage cleanup
    runtime guardrail 冻结通过。
  - `C1 / P4-prep-2 memory observation / shadow cache` 第一刀完成，并接入
    `execution_profile`、debug JSON 和 probe 摘要。
- `2026-04-22`
  - `xv6` 在真实 `virtio-blk` board path 上稳定到 shell。
  - Linux-facing `flat image + payload + set_gpr + linux_sbi_shim + repo DTB`
    foundation 进入主线。
- `2026-04-12`
  - `P4-prep-1` 完成，`bus / memory_region` 合同收口为统一事实来源。

## 当前仍然有效的风险 / 限制

- 仓库默认位置仍不携带真实 Linux `Image`；因此 `timerfd` 冻结点的发布级 runtime
  断言仍需要开发者显式提供 `Image` 后重新运行 opt-in guardrail。默认门禁只能证明
  harness、构建、marker 和 dummy-payload observation 没有回退。
- pipeline-side `xv6` memory observation 已有稳定 guardrail，但它只是
  `shadow_cache` / memory profile 信号，不是 pipeline 完整 boot `xv6` 的支持声明。
- `Wave 5` `Slice B / C / D / E / F` 完成不代表完整 cache / DMA / multicore 已完成；当前
  只落地默认关闭、RAM-only、write-through、no dirty write-back 的最小 L1D 执行模型，
  显式 opt-in 的 L1D debug/probe 观察面、frontend 只读展示，以及若干 L1D 边界和
  lifecycle hardening 合同。
- `Wave 6` 当前完成证据链、原型边界和 `DBT translator + IR v0 dry-run`：probe 级 hot-path / translation candidate
  观察、translation contract、per-PC / branch-target 观察、host-smoke-only prototype、
  preflight guardrail、opt-in translation-plan dry-run、host-smoke-only fallback replay
  等价性、first-boundary taxonomy、typed boundary、dry-run IR metadata、future
  block-cache invalidation dry-run、共享 `DbtBlockPlan` analyzer 入口、非执行
  `dbt_ir` / `dbt_translator` v0 dry-run、translator reject taxonomy、helper planning
  dry-run / matrix 扩展、helper replay contract dry-run、IR lowering contract dry-run、
  JIT engine skeleton dry-run / runtime fallback bridge、runtime dispatch contract dry-run、
  executable-cache invalidation enforcement dry-run、reference fallback step bridge dry-run、
  helper execution bridge contract dry-run、
  runtime invalidation hook contract、
  reference fallback execution bridge、
  executable memory policy、
  `dbt_ir_eval`
  semantic differential dry-run 的更宽整数覆盖，
  以及 metadata-only `dbt_block_cache` dry-run / invalidation matrix hardening。这不是完整
  JIT / DBT engine；当前仍不生成宿主代码，不引入长期 block cache，不改变 guest 可见语义。
- 当前 `pc_costs.cycles` 只是 retire-side 观察成本，不是稳定性能模型或 benchmark
  结论。`interpreter_dbt_prototype` 仍只覆盖 pure straight-line inlineable block；
  memory、CSR、trap、atomic、vector、control-flow、system boundary 都还是 helper /
  fallback；当前已有 preflight 拒绝整块的 lifecycle guardrail、opt-in translation-plan
  观察、host-smoke-only functional fallback replay、first-boundary taxonomy、dry-run IR
  metadata、invalidation dry-run、共享 block analyzer、非执行 translator / IR v0、
  translator reject taxonomy、helper planning dry-run / matrix 扩展、helper replay contract dry-run、
  IR lowering contract dry-run、JIT engine skeleton dry-run / runtime fallback bridge、
  runtime dispatch contract dry-run、executable-cache invalidation enforcement dry-run、
  reference fallback step bridge dry-run、helper execution bridge contract dry-run、
  runtime invalidation hook contract、
  reference fallback execution bridge、
  executable memory policy、
  IR semantic differential dry-run 更宽整数覆盖和
  metadata-only block cache / invalidation matrix hardening，但尚未有 executable lowering、
  runtime helper execution、
  persistent block lifecycle、host-code dispatch 或 workload-level runtime execution harness。
- multicore / coherence 虽然属于 `Wave 6` 长期目标，但当前仍未启动；它必须等待
  atomic、memory-order、DMA / cache 交界和验证矩阵另行收口。
- `Softmax + tiny static attention` 已作为 `Wave 4` 后段 stretch 完成，但它只覆盖
  最小静态 `fp32` row-wise softmax 和极小 attention-like profile 闭环；不要把它
  写成完整 attention、动态 sequence length、KV-cache 或 Transformer runtime。
- `debug/frontend`、`pipeline` 和 guest runtime 都已形成可维护边界，但后续
  仍要避免真实 bug 修复把职责重新揉回大文件。

## 下一步

1. `Wave 6 JIT Execution Layer 实现计划` 已完成并归档：
   [history_plan.md#mainline-wave6-jit-execution-layer-plan](../plan/history_plan.md#mainline-wave6-jit-execution-layer-plan)。
   当前已有 host-smoke-only 的 opt-in executable JIT block，但默认仍不启用 JIT backend，
   不在默认执行路径生成或执行 host code，不改变 guest 可见语义。
2. 下一刀如继续推进，优先做 executable cache runtime hookup 或 helper execution opt-in
   设计；默认 backend、persistent / executable block cache、multicore、coherence 和新的
   memory consistency 模型仍不启动。
3. `pc_costs` / `branch_targets` 仍是 debug/profile 读侧合同，不是 guest ABI；后续
   如需调整排序或字段，必须先补 probe / host smoke 兼容门禁。
5. 继续把 pipeline-side `xv6` memory observation、functional `xv6`、Linux
   dummy/probe、pipeline `vector_cnn` 和现有 debug CLI 输出作为 `Wave 6`
   hot-path evidence 的前置 guardrail。
6. AI accelerator 后续若继续推进 `INT4 / training / MobileNet / Linux-facing NPU
   driver / real DMA overlap / multi outstanding queue`，应另开本方向专项 plan，并
   明确不占用主线 `Wave 6`。
7. Wave 4 AI accelerator 的完成记录统一见
   [../plan/history_plan.md#mainline-wave4-ai-accelerator-slices-plan](../plan/history_plan.md#mainline-wave4-ai-accelerator-slices-plan)。
8. 显式提供真实 Linux `Image` 时，补跑 `timerfd-one-shot-readback-ok` runtime
   guardrail；未提供 `Image` 时，不把该项写成默认已证明。
9. 继续守住 `xv6` shell、Linux probe、`kernel_alpha`、debug CLI、
   `make test` 和 `make test-pipeline` 这些稳定 guardrail。
10. 不继续向当前 Linux fourth-stage smoke 追加同类 syscall 微分支；如果真实 runtime
   暴露新 blocker，再按 blocker 驱动回补最窄 Linux guardrail。

## 验证基线

- `cd myCPU && make test`
- `cd myCPU && make test-pipeline`
- `cd frontend && node --test`

如果改动集中在 Linux / `xv6` workload harness、probe 或 runtime guardrail，
还应至少关注：

- `cd myCPU && make test-host-run_debug_cli_probe`
- `cd myCPU && make test-host-xv6_boot_smoke`
- `cd myCPU && make test-host-xv6_shell_smoke`
- `cd myCPU && make run-workload-xv6`
- `cd myCPU && python3 -m unittest tests.host.run_debug_cli_probe_test.RunDebugCliProbeTest.test_make_build_workload_linux_proto_block_mode_builds_post_init_smoke_elf`
- `cd myCPU && MYCPU_RUN_LINUX_PROTO_RUNTIME=1 MYCPU_LINUX_PROTO_RUNTIME_IMAGE=<Image> python3 -m unittest tests.host.run_debug_cli_probe_test.RunDebugCliProbeTest.test_linux_proto_block_mode_runtime_reaches_fourth_stage_when_requested`
