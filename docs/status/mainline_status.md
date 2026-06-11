# 主线状态

## 文档定位

本文档是仓库唯一的主线实时状态文档，用于记录：

- 当前稳定快照
- 当前优先级
- active line 与近端 blocker
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
  - [../design/simulator_evolution_observability_schema_design.md](../design/simulator_evolution_observability_schema_design.md)
  - [../design/wave5_cache_memory_system_design.md](../design/wave5_cache_memory_system_design.md)
  - [../design/wave6_jit_dbt_readiness_design.md](../design/wave6_jit_dbt_readiness_design.md)
  - [../design/wave7_productization_and_showcase_design.md](../design/wave7_productization_and_showcase_design.md)
  - [../design/wave7_remote_cloud_dev_environment_design.md](../design/wave7_remote_cloud_dev_environment_design.md)
  - [../design/ai_accelerator_linux_facing_contract_design.md](../design/ai_accelerator_linux_facing_contract_design.md)
- 相关状态：
  - [kernel_alpha_status.md](kernel_alpha_status.md)
  - [simulator_evolution_status.md](simulator_evolution_status.md)
  - [npu_tpu_accelerator_status.md](npu_tpu_accelerator_status.md)
  - [code_reself_status.md](code_reself_status.md)
- 当前活跃计划：
  - [../plan/project_evolution_priority_p1_plan.md](../plan/project_evolution_priority_p1_plan.md)
  - [../plan/project_evolution_priority_p2_plan.md](../plan/project_evolution_priority_p2_plan.md)
  - [../plan/project_evolution_priority_p3_plan.md](../plan/project_evolution_priority_p3_plan.md)
  - [../plan/wave7_remote_cloud_dev_environment_plan.md](../plan/wave7_remote_cloud_dev_environment_plan.md)
  - [../plan/post_wave7_linux_distribution_platform_longterm_plan.md](../plan/post_wave7_linux_distribution_platform_longterm_plan.md)
- 已完成计划归档：
  - [../plan/history_plan.md](../plan/history_plan.md)
  - [../plan/history_plan.md#project-evolution-priority-p0-plan](../plan/history_plan.md#project-evolution-priority-p0-plan)
  - [../plan/history_plan.md#simulator-evolution-slice2-debug-probe-event-summary-plan](../plan/history_plan.md#simulator-evolution-slice2-debug-probe-event-summary-plan)
  - [../plan/history_plan.md#simulator-evolution-slice1-observability-schema-plan](../plan/history_plan.md#simulator-evolution-slice1-observability-schema-plan)
  - [../plan/history_plan.md#course-os-kernel-alpha-quality-review-safe-fixes-closeout](../plan/history_plan.md#course-os-kernel-alpha-quality-review-safe-fixes-closeout)
  - [../plan/history_plan.md#course-os-kernel-alpha-review-remediation-and-linux-compat-convergence-plan](../plan/history_plan.md#course-os-kernel-alpha-review-remediation-and-linux-compat-convergence-plan)
  - [../plan/history_plan.md#course-os-kernel-alpha-quality-review-plan](../plan/history_plan.md#course-os-kernel-alpha-quality-review-plan)
  - [../plan/history_plan.md#code-reself-remediation-plan](../plan/history_plan.md#code-reself-remediation-plan)
  - [../plan/history_plan.md#course-os-kernel-alpha-stage1-plan](../plan/history_plan.md#course-os-kernel-alpha-stage1-plan)
  - [../plan/history_plan.md#post-wave7-linux-distribution-platform-plan](../plan/history_plan.md#post-wave7-linux-distribution-platform-plan)
  - [../plan/history_plan.md#pipeline-lsq-blocked-load-observation-plan](../plan/history_plan.md#pipeline-lsq-blocked-load-observation-plan)
  - [../plan/history_plan.md#mainline-wave7-linux-console-loading-experience-plan](../plan/history_plan.md#mainline-wave7-linux-console-loading-experience-plan)
  - [../plan/history_plan.md#mainline-wave7-linux-console-health-check-plan](../plan/history_plan.md#mainline-wave7-linux-console-health-check-plan)
  - [../plan/history_plan.md#mainline-wave7-linux-console-load-contract-plan](../plan/history_plan.md#mainline-wave7-linux-console-load-contract-plan)
  - [../plan/history_plan.md#mainline-wave7-linux-console-config-ux-plan](../plan/history_plan.md#mainline-wave7-linux-console-config-ux-plan)
  - [../plan/history_plan.md#mainline-wave7-linux-console-reset-rearm-plan](../plan/history_plan.md#mainline-wave7-linux-console-reset-rearm-plan)
  - [../plan/history_plan.md#mainline-wave7-linux-console-real-image-e2e-plan](../plan/history_plan.md#mainline-wave7-linux-console-real-image-e2e-plan)
  - [../plan/history_plan.md#mainline-wave7-linux-console-terminate-plan](../plan/history_plan.md#mainline-wave7-linux-console-terminate-plan)
  - [../plan/history_plan.md#mainline-wave7-linux-console-hardening-plan](../plan/history_plan.md#mainline-wave7-linux-console-hardening-plan)
  - [../plan/history_plan.md#mainline-wave7-linux-interactive-frontend-console-plan](../plan/history_plan.md#mainline-wave7-linux-interactive-frontend-console-plan)
  - [../plan/history_plan.md#mainline-wave7-product-docs-v1-plan](../plan/history_plan.md#mainline-wave7-product-docs-v1-plan)
  - [../plan/history_plan.md#mainline-wave7-console-demo-workspace-v1-plan](../plan/history_plan.md#mainline-wave7-console-demo-workspace-v1-plan)
  - [../plan/history_plan.md#mainline-wave7-product-website-shell-plan](../plan/history_plan.md#mainline-wave7-product-website-shell-plan)
  - [../plan/history_plan.md#mainline-wave6-executable-cache-runtime-hookup-plan](../plan/history_plan.md#mainline-wave6-executable-cache-runtime-hookup-plan)
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

`Wave 6 / JIT / DBT` 已完成证据链和原型边界阶段的首轮收口：
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
`jit_dispatch` opt-in JSON 事件、统一 `observation_event` wrapper，以及
`run_debug_cli_probe.py --jit-dispatch` 显式 probe 输出；该观察面仍只读，不驱动 runtime。
`simulator-evolution` observability wrapper 当前还已覆盖 debug-probe summary、
ExecutionProfile core snapshot、memory observation、`shadow_cache` 读侧摘要和
AI profile bridge；这些 wrapper 只包装既有 snapshot / profile / AI profile
文本事实，不改变 guest 可见行为。
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
当前已推进到 host-smoke-only opt-in 单步执行，可复用 `FunctionalBackend::step()` 覆盖
JIT miss / reject / helper-required / differential mismatch 的 reference fallback；它不生成
host code，不接默认 backend。
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
`executable cache runtime hookup` 已完成第一刀：
`DbtExecutableCacheRuntime` 只在 host smoke 显式 opt-in 时持有已生成并通过 differential
guardrail 的 `DbtHostExecutable`，lookup 命中可复用 resident host executable；guest store /
primary image load / debug reset / `satp` / `sfence.vma` / region 事件继续复用现有
invalidation 合同释放并删除 resident executable。默认 backend、persistent cache 和 helper
runtime execution 仍不启动。
`helper execution opt-in` 已完成第一刀：
`dbt_helper_execution_bridge` 仍保留 CSR / atomic / vector request-only metadata，但 scalar
memory load / store 可以在 host smoke 显式调用时经由 `AddressSpace -> Bus` 真实执行；
host smoke 固定 load GPR commit、store memory commit、trap/fault fallback 和 commit
boundary。默认 backend 和完整 helper runtime execution 仍不启动。
`JIT dispatch harness v1` 已完成第一刀：
`dbt_runtime_harness` opt-in cache path 显式记录 cache lookup、miss emit、hit execute；
invalidation 后必须 miss 并重新 emit，不允许 stale dispatch。runtime harness summary /
stats 只读暴露 hit / miss、emit、exec、fallback、invalidate 和 differential mismatch 计数。
默认 backend 接入评估结论仍是“不新增 `--backend jit`，不替换 functional 或 pipeline”。
`IR semantic coverage` 已扩到更宽的 pure integer 子集：逻辑运算、shift immediate /
register、signed / unsigned set-less-than、`lui` / `auipc` 和 RV64 word ops 都有
reference differential smoke。
`metadata-only block cache` 已完成第一刀：`dbt_block_cache` 只缓存成功翻译的
`DbtTranslationUnit` metadata，固定 exact-range lookup、hit / miss 计数、rejected unit
不入 cache，以及复用 invalidation dry-run 合同删除 metadata 条目；当前也固定了
invalidation check / examined entries / non-invalidating event 计数和空 cache 全局事件分类。

这些结果只说明“哪些 guest PC 区间值得观察、哪些能进入 IR v0 dry-run、为什么剩余部分
仍不能翻译、回退是否等价、第一拒绝边界是什么”。
当前仍不实现默认 JIT backend、persistent cache、helper runtime execution、workload-level
scheduler、multicore、coherence 或新的 memory consistency 模型，也不改变 guest 可见语义。

`Wave 7 / 产品化展示与在线调试平台收口` 已进入历史收口态。产品官网壳层、首页
和控制台 demo workspace v1 已完成：本地前端服务具备 `/`、`/console`、`/docs`
三入口，首页采用面向用户的产品叙事展示已完成能力，`/console` 顶层已按
`OS Bring-up`、`Machine Inspector`、`AI Accelerator`、`Runtime Labs` 组织体验入口，
`/docs` 已推进到产品文档 v1，按用户体验路线说明 Overview、Try the Console、
Demo Routes、Architecture、OS Bring-up、AI Accelerator、Runtime Labs、Verification、
Roadmap 和 Design References。`Linux Serial Console` 也已作为 gated runtime demo
接入 `/console`：配置 `MYCPU_LINUX_PROTO_CONSOLE_IMAGE=/path/to/Image` 后，manifest
会暴露 `linux_proto_console`，卡片从 `Not configured` / `Runtime Image required`
切换为 `Ready`；未配置前不会创建 session。`/api/tests` 同步返回只读
`diagnostics.linuxConsole`，用于在卡片上说明 env 未设置、路径不存在、路径不是文件、
不可读或 ready 等状态。配置后可通过现有 debug CLI / browser terminal 启动 flat
SBI shim、Linux `Image`、DTB 和 `virtio-blk` rootfs，并等待 `mycpu-linux# ` 提示符。
真实 Linux console route 当前固定使用 existing runtime guardrail 已证明的
`functional` backend 合同；boot marker wait 使用 Linux 专用 debug CLI request timeout，
不再复用普通 1500 ms 单请求预算。点击 Linux route 的 `Load` 后，`/console`
会在 workspace 和 terminal 中显示 Linux boot progress、已等待秒数、当前 backend
和等待中的 `mycpu-linux# ` prompt；如果 boot wait 超时，notice 会转成面向用户的
Image / prompt / backend 指引，而不是直接暴露 debug CLI 内部文本。普通 demo 的
loading 状态不显示 Linux 专属文案。前端输入的换行命令也改为等待当前 offset 之后的新
UART prompt，避免旧 prompt 让 command settling 提前返回。
本轮 hardening 已让 terminal title / pending hint 随 loaded workload 切到
`Linux serial terminal` / `Linux serial console`，并用 runtime characterization 固定
`help\r` 会等待 `commands: help uptime exit` 与新的 `mycpu-linux# ` 提示符后再返回。
本轮 terminate 收口新增显式 `Terminate` 主操作和 `POST /api/session/terminate`，
可停止 run loop、关闭当前 debug CLI session、清空 snapshot / terminal tracking，
并向浏览器广播空 terminal reset。
真实 Linux serial console e2e 现在也有显式 opt-in guardrail：默认 `frontend`
Node 测试只记录跳过条件；设置 `MYCPU_RUN_LINUX_PROTO_CONSOLE_E2E=1` 和
`MYCPU_LINUX_PROTO_CONSOLE_IMAGE=/path/to/Image` 后，会通过真实 debug server +
debug CLI 加载 `linux_proto_console`，输入 `help`，校验 `commands: help uptime exit`
和新的 `mycpu-linux# ` prompt，然后 terminate。
Linux console reset re-arm 也已完成：`/api/session/reset` 对带 payload / GPR seed /
boot marker 的 entry 会在 reset 后重新加载 Linux `Image` payload、DTB，重设
`a0/a1/a2`，并再次等待 `mycpu-linux# ` prompt；普通 demo reset 仍保持原有裸 reset
语义。
在此基础上，本地前端已经正式进入 `Post-Wave 7` 的 `Lab workbench` 重构：旧
`Wave 7` 产品首页 / demo workspace 设计现已降级为历史语境，当前现行入口改为
[../design/post_wave7_frontend_lab_product_design.md](../design/post_wave7_frontend_lab_product_design.md)。
`Post-Wave 7` 前端 `Lab workbench` 第一轮重构现已完成并归档到
[../plan/history_plan.md#post-wave7-frontend-lab-product-plan](../plan/history_plan.md#post-wave7-frontend-lab-product-plan)：
`/console` 当前已经以 `Lab Navigator + Session Bar + Primary Stage + Inspector Stack +
Evidence Drawer` 组织场景；解释层固定成 `Session contract -> Primary stage ->
Observation trail -> Evidence / boundary`；`Linux Distro Labs` 也已从单一卡片扩成
`Linux Serial Console + Alpine Distro Evidence + Capability & ISA Matrix` 的专题 family。
当前 AI Labs 仍不扩写成任意模型上传、完整 AI runtime 或 Linux-facing NPU driver；
Linux 更深一层的发行版专题页仍可作为后续单独切片继续推进。
这套 `Lab workbench` 当前又完成了一轮补完并归档到
[../plan/history_plan.md#post-wave7-frontend-lab-completion-plan](../plan/history_plan.md#post-wave7-frontend-lab-completion-plan)：
本机真实 Linux `Image` 已接到当前 worktree 的 debug server，`/api/tests` 现会把
`linux_proto_console` 暴露为 `ready`；工作台也新增了 `Scenario controls`，把专题卡、
`测试 / 后端 / Load` 与 live session 的职责边界收成一条主路径。Linux 专题卡现在会记住
它绑定的 runnable route 和 backend，`Open live shell` / `Load current scenario`
可直接回到真实串口 shell；`Runtime Labs` 也不再只剩占位卡，而是通过
`POST /api/session/jit-dispatch` 把当前 loaded session 的 JIT / DBT dry-run summary
接进专题工作台。AI Labs 这轮仍只保持 guest demo 与 parameterized tiny model 的
专题组织边界，没有提前扩写成完整 AI runtime。
当前 active line 已经拆成两部分：远端服务器上的 `Wave 7` 部署 / 运维升级继续在远端
checkout 推进；本地工作区正式打开两条 `Post-Wave 7` 新主线，并分别补齐独立文档入口：
- 标准 Linux 发行版平台：
  [../design/post_wave7_linux_distribution_platform_design.md](../design/post_wave7_linux_distribution_platform_design.md) /
  [linux_distribution_platform_status.md](linux_distribution_platform_status.md) /
  [../plan/history_plan.md#post-wave7-linux-distribution-platform-plan](../plan/history_plan.md#post-wave7-linux-distribution-platform-plan)
- 用户自定义 AI 任务与更接近商用 NPU 的性能模型：
  [../design/post_wave7_ai_user_tasks_npu_performance_design.md](../design/post_wave7_ai_user_tasks_npu_performance_design.md) /
  [npu_tpu_accelerator_status.md](npu_tpu_accelerator_status.md) /
  [../plan/history_plan.md#post-wave7-ai-user-tasks-npu-performance-plan](../plan/history_plan.md#post-wave7-ai-user-tasks-npu-performance-plan)

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
- Linux interactive frontend console 第一刀已完成：仓库默认仍不携带真实 Linux
  `Image`，但配置 `MYCPU_LINUX_PROTO_CONSOLE_IMAGE=/path/to/Image` 后，`/console`
  可选择 gated `linux_proto_console` 路线，经现有 Load / Run / Pause / Reset 和
  Terminate / browser terminal 进入 `mycpu-linux# ` 串口提示符。该提示符来自
  `linux_postinit_cleanup_smoke` 的最小用户态 shell，支持 `help`、`uptime`、
  `exit` 和未知命令回显；前端 terminal 已能按 loaded workload 显示 Linux serial
  title / hint，并通过 runtime characterization 覆盖 `help\r` 的 prompt settling。
  2026-05-02 本机真实 Image 验证已通过：
  `MYCPU_LINUX_PROTO_CONSOLE_IMAGE=/tmp/mycpu-linux-build-riscv64-linux-gnu/arch/riscv/boot/Image`
  启动 frontend 后，`POST /api/session/load` 以 `backend=functional` 到达
  `mycpu-linux# `；随后 `POST /api/session/terminal-input` 输入 `help\r`
  返回 `commands: help uptime exit` 和新的 `mycpu-linux# ` prompt。
  Linux card 未配置时显示 `Not configured` 和 `Runtime Image required`，并根据
  `diagnostics.linuxConsole` 说明 env 未设置、路径不存在、路径不是文件或不可读；
  未 ready 时不会创建 session，manifest 暴露后显示 `Ready`。显式 Terminate 会结束
  当前 debug session 并清空浏览器 session / terminal 状态。Load 期间 UI 会明确显示
  `Linux boot in progress`、backend、等待中的 `mycpu-linux# ` prompt 和已等待秒数；
  Linux boot timeout notice 会提示检查 `MYCPU_LINUX_PROTO_CONSOLE_IMAGE`，普通 demo
  不显示 Linux 专属 loading 文案。
  这不是浏览器内 Linux，也不开放任意用户镜像。
- `P4-prep-1` 和 `C1 / P4-prep-2 memory observation / shadow cache` 已完成。
  当前稳定的观测 guardrail 主要是：
  - pipeline vector CNN 的 `shadow_cache` RAM baseline
  - functional `xv6` 的稳定 profile / `shadow_cache` baseline
  - functional `linux_proto` dummy-payload observation baseline
- `debug/frontend`、`kernel_alpha` 十条基线、`make test` / `make test-pipeline`
  和现有 workload smoke 都已进入维护态。
- `PROJECT_EVOLUTION` P0 已完成并归档：`kernel_alpha` Stage 2 正向证据面和旧负向
  guardrail 已重跑守住；observability schema 已扩到 producer wrapper 和首个 frontend
  Evidence Drawer consumer；`/api/session/load` 已支持受控本地 `elfPath` / `elfBase64`；
  debug CLI / debug server 已补 `set_memory`、`set_csr` 和单地址 `break_at`。
- `PROJECT_EVOLUTION` P1 当前已完成任务 1：AI accelerator Linux-facing contract
  第一刀定调为 `host-facade`。`./mycpu --ai-linux-contract` 与 frontend
  `linuxFacingContract` 只读摘要现在固定 DT node、MMIO / PLIC、descriptor /
  completion queue、DMA buffer、devfs / ioctl 预留面和 profile schema 边界；真实 Linux
  driver、`/dev/mycpu-ai0`、ioctl 和 Linux integration smoke 仍未启动。
- `PROJECT_EVOLUTION` P1 当前已完成任务 2：JIT / DBT dry-run 去留决断结论是
  归档为 `method-demo / opt-in research asset`，不新增 `--backend jit`，不替换
  `functional` 或 `pipeline`，也不继续扩张新的 dry-run 接口面。现有 translator、IR eval、
  lowering、host emitter、executable cache、runtime harness、scalar memory helper、
  reference fallback 和 invalidation guardrail 继续作为 host smoke 资产保留；若未来重启
  backend-candidate 路线，必须另开窄计划并先补 guest 范围、差分门禁、fallback coverage、
  可重复性能证据和 workload-level scheduler 边界。
- `PROJECT_EVOLUTION` P1 当前已完成任务 3：测试矩阵分层执行纪律已收敛到
  `default / slow guest / opt-in external` 三类计划声明口径，并新增
  `make test-verification-layers` 轻量元门禁，用于确认 `test-fast-smoke`、
  `test-standard-regression`、`test-slow-guest` 和 `test-opt-in-external` 四个入口仍可被
  `make -n` 解析。完整层级语义继续以
  [regression_completion_criteria.md](../design/regression_completion_criteria.md) 为准，
  不在 status 复制测试矩阵。
- `PROJECT_EVOLUTION` P1 当前已完成任务 6：前端 AI 自定义 workload 第一刀收敛为
  `POST /api/ai/custom-graph` 的 bounded task-spec facade。浏览器只提交
  `ai_custom_graph_v1` JSON、白名单 op sequence、`int8/int32` dtype、受控 shape 和 input
  preset；服务端复用 `pack_graph.py --task-spec` 生成 graph package / runtime shape table /
  manifest，再调用 `mycpu --ai-profile-manifest` 返回输出、expected、profile counters、
  aggregate 和 per-op summary。该入口仍不接受任意 graph package、任意模型上传、
  ONNX / PyTorch runtime 或 Linux-facing NPU driver。
- `2026-05-29` 全仓库 code review remediation 已完成并归档；12 条
  `必须修复` 与 19 条 `建议修改` active findings 已由四条整改线关闭，合并后守住
  `test-fast-smoke`、`test-standard-regression`、`test-pipeline`、`make test`
  与 `frontend` Node 测试。剩余长期观察项继续只在
  [code_reself_status.md](code_reself_status.md) 维护，不作为当前主线 blocker。
- `pipeline / LSQ` 观察合同在 2026-05-02 补上一条窄门禁：
  `LoadStoreQueue::oldest_load_status()` 会报告队列中最老的 blocked / replay load，
  `PipelineBackend::debug_snapshot()` 在没有当周期 decode 观察记录时，也会从 LSQ
  队列暴露 blocked load 的 state、load sequence id 和 store sequence id。这只增强
  debug snapshot / frontend 可观察性，不改变 replay、forwarding、commit boundary 或 guest
  可见语义。
- 当前 active line 不再是单一 `Wave 7` 收尾：本地工作区以 `Post-Wave 7`
  标准 Linux 发行版平台和用户 AI 任务 / NPU 性能模型为主；远端部署升级仍沿
  `Wave 7` 远端云服务器计划推进。它不是继续深挖 Linux checkpoint 的 `Wave 3`，
  也不是默认 JIT backend 或完整 multicore / coherence 专项。
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
- 主线 `Wave 7` 的 AI 参数化小模型体验在 2026-05-02 继续扩到白名单模板矩阵：
  `/api/ai/tiny-model/templates` 当前暴露 `dynamic_tiny_model`、
  `dynamic_gemm`、`dynamic_cnn` 和 `tiny_attention_static` 四条服务器端模板；浏览器只能选择
  模板声明过的 batch、runtime shape 和输入 preset，服务端会重新生成 graph package、
  runtime shape table、输入和 expected output，再调用 `mycpu --ai-profile-manifest`。
  `/console` 已支持模板切换、模板专属参数控件，以及 `fp32 / int32` 输出、runtime shape、
  aggregate counters 和 per-op summary 的统一展示；同一面板现在还会按当前模板明确解释
  `Expected marker`、`What this proves` 和 `Current boundary`，把 demo 证据、证明点和
  Wave 7 边界放在同一个观察面。它仍然不是任意 graph package 上传、
  任意模型 authoring、ONNX / PyTorch runtime 或 Linux-facing NPU driver。
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
  字段；`jit_dispatch` JSON 另带统一 `observation_event` wrapper，仍不接 runtime backend。
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
  `src/exec/dbt_reference_fallback_execution.{h,cpp}`，把 fallback plan 转成 reference
  execution request，并用 host smoke 固定 JIT miss、helper bridge、trap/fault placeholder、
  differential mismatch 分类和 opt-in `FunctionalBackend::step()` 单步执行边界。
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
- 主线 `Wave 6` 已完成 `executable cache runtime hookup` 第一刀：新增
  `DbtExecutableCacheRuntime` opt-in API，把已生成的 `DbtHostExecutable` 接入现有
  executable-cache invalidation 合同；host smoke 固定 resident host executable lookup /
  reuse、guest store / global invalidation 释放与删除，以及 runtime harness cache
  miss 生成 / cache hit 复用的 differential guardrail。默认 backend、persistent cache
  和 helper runtime execution 仍不启动。
- 主线 `Wave 6` 已完成 `runtime execution sequence` 第一刀：helper execution opt-in
  只执行 scalar memory load / store；runtime fallback execution bridge 可 opt-in 调用
  functional reference 单步；dispatch harness v1 固定 cache lookup、emit-on-miss、
  execute-on-hit、invalidate 后禁止 stale dispatch；executable JIT host smoke 矩阵扩到
  `lui` / `auipc`、word ops、逻辑、shift 和 compare；runtime harness stats 暴露
  hit / miss、emit、exec、fallback、invalidate 和 differential mismatch。默认 backend
  评估结论仍是不接入。
- 主线 `Wave 6` 已完成 `closeout runtime guardrail` 第一刀：`dbt_runtime_harness`
  新增 host-smoke-only opt-in runtime loop v1，可以连续串起 executable cache
  miss / hit、scalar memory helper execution、reference fallback execution 和 guest-store
  invalidation；helper store 会触发 runtime invalidation hook 并阻止 stale resident
  executable dispatch。helper store fault/no-commit 与 trap/fault fallback placeholder 也已
  由 host smoke 固定。该 loop 仍不是默认 backend，也不是 workload-level scheduler。
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
   executable cache runtime hookup，
   helper execution opt-in、runtime fallback opt-in execution、JIT dispatch harness v1、
   runtime loop v1 closeout guardrail、runtime harness summary / stats，
   以及 metadata-only block cache / invalidation matrix hardening
   第一刀已作为补充合同落地。
   当前执行计划已完成并归档：
   [Wave 6 closeout runtime guardrail 计划](../plan/history_plan.md#mainline-wave6-closeout-runtime-guardrail-plan)、
   [Wave 6 runtime execution sequence 计划](../plan/history_plan.md#mainline-wave6-runtime-execution-sequence-plan)、
   [Wave 6 executable cache runtime hookup 计划](../plan/history_plan.md#mainline-wave6-executable-cache-runtime-hookup-plan)、
   [Wave 6 JIT Execution Layer 实现计划](../plan/history_plan.md#mainline-wave6-jit-execution-layer-plan)。
   当前 closeout 判定是 opt-in executable path 与 runtime guardrail 收口；`PROJECT_EVOLUTION`
   P1 去留决断进一步把这条线归档为 `method-demo / opt-in research asset`，不新增
   `--backend jit`，不替换 functional 或 pipeline，也不继续扩张新的 dry-run 接口面。
   persistent cache、workload-level scheduler、CSR / atomic / vector helper runtime、
   multicore、coherence 或新的 memory consistency 模型仍不启动；后续如果重新打开
   backend-candidate 路线，必须另开计划并补齐 guest 范围、差分门禁、fallback coverage
   和可重复性能证据。
2. `Wave 7` 已完成产品官网壳层 / 首页第一刀、控制台 demo workspace v1、产品文档 v1、
   Linux interactive frontend console 第一刀、Linux console hardening 第一刀、
   Linux console terminate 收口、真实 Image opt-in e2e guardrail、reset re-arm、
   配置状态 UX、配置诊断 health check 和 AI 参数化小模型体验：
   [Wave 7 产品化展示与在线控制台设计](../design/wave7_productization_and_showcase_design.md)
   已固定真正产品官网、Apple-style 首页滚动叙事、demo-first 控制台、产品文档和部署边界；
   对外首页改为面向用户展示“能做什么”，不再把技术评审、招聘面试官、边界或未完成项作为主叙事。
   第一刀已落地 `/`、`/console`、`/docs` 三入口：`/` 是产品首页，`/console`
   现在先展示 demo workspace，再复用现有浏览器控制台会话控制；`/docs` 已从 curated
   壳层推进到可阅读的产品文档入口，覆盖 Overview、Try the Console、Demo Routes、
   Architecture、OS Bring-up、AI Accelerator、Runtime Labs、Verification、Roadmap 和
   Design References，并可通过本地 `/source/docs/...` 链接回 design / status 证据文档。
   完成计划已归档到 [Wave 7 产品官网壳层与首页第一刀计划](../plan/history_plan.md#mainline-wave7-product-website-shell-plan)
   、[Wave 7 控制台 demo workspace v1 计划](../plan/history_plan.md#mainline-wave7-console-demo-workspace-v1-plan)
   、[Wave 7 产品文档 v1 计划](../plan/history_plan.md#mainline-wave7-product-docs-v1-plan)
   和 [Wave 7 Linux interactive frontend console 计划](../plan/history_plan.md#mainline-wave7-linux-interactive-frontend-console-plan)。
   Linux console hardening 已归档到
   [Wave 7 Linux console hardening 计划](../plan/history_plan.md#mainline-wave7-linux-console-hardening-plan)。
   Linux console terminate 收口已归档到
   [Wave 7 Linux console terminate 计划](../plan/history_plan.md#mainline-wave7-linux-console-terminate-plan)。
   真实 Image opt-in e2e guardrail 已归档到
   [Wave 7 Linux console real Image e2e 计划](../plan/history_plan.md#mainline-wave7-linux-console-real-image-e2e-plan)。
   Reset re-arm 已归档到
   [Wave 7 Linux console reset re-arm 计划](../plan/history_plan.md#mainline-wave7-linux-console-reset-rearm-plan)。
   配置状态 UX 已归档到
   [Wave 7 Linux console config UX 计划](../plan/history_plan.md#mainline-wave7-linux-console-config-ux-plan)。
   配置诊断 health check 已归档到
   [Wave 7 Linux console health check 计划](../plan/history_plan.md#mainline-wave7-linux-console-health-check-plan)。
   AI 参数化小模型体验已归档到
   [Wave 7 AI 参数化小模型体验计划](../plan/history_plan.md#mainline-wave7-ai-parameterized-tiny-model-plan)。
   `/console` 当前可以直接运行服务器端白名单 `dynamic_tiny_model` host profile：
   浏览器只提交 batch 与 input preset，server 重新生成 graph package、runtime shape
   table、输入和 expected output，再调用现有 `mycpu --ai-profile-manifest`，返回输出、
   `runtime_shapes`、profile counters 与 op summary。该入口仍不接受任意 graph package、
   任意模型上传或 Linux-facing NPU driver。
   在此之后，当前前端已完成 `Post-Wave 7` 的 `Lab workbench` 第一轮重构并归档到
   [../plan/history_plan.md#post-wave7-frontend-lab-product-plan](../plan/history_plan.md#post-wave7-frontend-lab-product-plan)。
   现行设计入口为 [Post-Wave 7 前端 Lab 产品设计](../design/post_wave7_frontend_lab_product_design.md)；
   `/console` 当前已固定 `Session contract -> Primary stage -> Observation trail ->
   Evidence / boundary` 的解释层，并把 Linux family 扩成更明确的专题导航；AI Labs
   不扩写成任意模型上传或完整 AI runtime。
   本轮仍不做公网部署、底层 session API 重构、标准发行版镜像支持或任意用户镜像上传。
3. AI accelerator 的 `INT4 / training / MobileNet / Linux-facing NPU driver /
   real DMA overlap / multi outstanding queue` 等后续专项不得改写主线 `Wave 6`
   定位。
4. Linux `timerfd-one-shot-readback-ok` 作为 `Wave 3` 冻结边界进入守成；除非真实
   runtime 重新暴露新 blocker，不再继续向当前 fourth-stage smoke 追加同类
   `open-fd / mmap / pipe / futex / socketpair / openat2 / pidfd / signalfd /
   renameat2 / eventfd / epoll / sendmsg / recvmsg / SCM_RIGHTS / copy_file_range /
   splice / statx / inotify / timerfd` 微分支。
5. 把 `xv6` shell、Linux probe、`kernel_alpha`、`pipeline`、debug CLI 和
   现有回归矩阵守成稳定 guardrail。
6. 继续积累 `shadow_cache / observation / representative workload` 证据；后续
   cache / DMA / multicore / coherence 或 JIT engine 都必须另开设计/计划并补足验证。

## 关键历史节点

- `2026-06-11`
  - `PROJECT_EVOLUTION` P0 完成并归档到
    [../plan/history_plan.md#project-evolution-priority-p0-plan](../plan/history_plan.md#project-evolution-priority-p0-plan)。
    本轮守住 `kernel_alpha` Stage 2 / 旧负向 guardrail，完成 frontend Evidence Drawer
    `observation_event` read-side consumer，补齐本地受控 ELF load，并给 debug protocol /
    debug server 增加 `set_memory`、`set_csr` 与单地址 `break_at` 最小闭环。
- `2026-05-02`
  - 主线 `Wave 7` 完成 AI 参数化小模型体验：
    [../plan/history_plan.md#mainline-wave7-ai-parameterized-tiny-model-plan](../plan/history_plan.md#mainline-wave7-ai-parameterized-tiny-model-plan)。
    `/api/ai/tiny-model/templates` 暴露服务器端白名单模板；`/api/ai/tiny-model/run`
    只接受 `dynamic_tiny_model`、batch `1 / 2` 和固定 input preset，服务器端重新生成
    graph package / runtime shape table / 输入 / expected output 后调用现有
    `mycpu --ai-profile-manifest`。`/console` 展示参数控件、输出、runtime shape、
    profile counters 和 op summary；自定义 graph package 字段会 fail-closed。
  - 主线 `Wave 7` 完成 Linux console 配置诊断 health check：
    [../plan/history_plan.md#mainline-wave7-linux-console-health-check-plan](../plan/history_plan.md#mainline-wave7-linux-console-health-check-plan)。
    `/api/tests` 返回只读 `diagnostics.linuxConsole`，覆盖 env 未设置、路径不存在、
    路径不是普通文件、路径不可读和 ready；`/console` 卡片展示这些诊断，未 ready 时
    仍不暴露可点击 `linux_proto_console` session。
  - 主线 `Wave 7` 完成 Linux console 配置状态 UX：
    [../plan/history_plan.md#mainline-wave7-linux-console-config-ux-plan](../plan/history_plan.md#mainline-wave7-linux-console-config-ux-plan)。
    `/console` 的 Linux Serial Console 卡片在未配置 runtime Image 时显示
    `Not configured` / `Runtime Image required`，配置后显示 `Ready`，并保持未 ready
    时不创建 session 的 gating 语义。
  - 主线 `Wave 7` 完成 Linux console reset re-arm：
    [../plan/history_plan.md#mainline-wave7-linux-console-reset-rearm-plan](../plan/history_plan.md#mainline-wave7-linux-console-reset-rearm-plan)。
    `/api/session/reset` 对 `linux_proto_console` 会在 reset 后重新执行 Linux `Image`
    payload / DTB load、`a0/a1/a2` seed 和 `mycpu-linux# ` boot marker wait；普通 demo
    reset 继续保持原有 session reset 语义。
  - 主线 `Wave 7` 完成 Linux console 真实 Image opt-in e2e guardrail：
    [../plan/history_plan.md#mainline-wave7-linux-console-real-image-e2e-plan](../plan/history_plan.md#mainline-wave7-linux-console-real-image-e2e-plan)。
    默认 `cd frontend && node --test` 只记录跳过条件；设置
    `MYCPU_RUN_LINUX_PROTO_CONSOLE_E2E=1` 和
    `MYCPU_LINUX_PROTO_CONSOLE_IMAGE=/path/to/Image` 后，frontend e2e 会经真实 debug
    server + debug CLI 加载 `linux_proto_console`，输入 `help`，校验命令列表和
    `mycpu-linux# ` prompt，并执行 terminate 收口。
  - 主线 `Wave 7` 完成 Linux console terminate 收口：
    [../plan/history_plan.md#mainline-wave7-linux-console-terminate-plan](../plan/history_plan.md#mainline-wave7-linux-console-terminate-plan)。
    `/console` 新增显式 `Terminate` 主操作；`POST /api/session/terminate` 会停止 run loop、
    关闭当前 debug CLI session、清空 runtime snapshot / terminal tracking，并广播空
    terminal reset，前端同步清理 loaded session、snapshot history 和 terminal state。
  - 主线 `Wave 7` 完成 Linux console hardening 第一刀：
    [../plan/history_plan.md#mainline-wave7-linux-console-hardening-plan](../plan/history_plan.md#mainline-wave7-linux-console-hardening-plan)。
    `/console` 的 terminal title / pending hint 现在随 loaded workload 区分
    `interactive_os terminal` 和 `Linux serial terminal`；runtime characterization
    固定 `linux_proto_console` 下 `help\r` 会等待 `commands: help uptime exit` 与新的
    `mycpu-linux# ` prompt 后返回并只广播一次 terminal response。

- `2026-05-01`
  - 主线 `Wave 7` 完成 Linux interactive frontend console 第一刀：
    [../plan/history_plan.md#mainline-wave7-linux-interactive-frontend-console-plan](../plan/history_plan.md#mainline-wave7-linux-interactive-frontend-console-plan)。
    `/console` 新增 gated `Linux Serial Console` 路线；配置
    `MYCPU_LINUX_PROTO_CONSOLE_IMAGE=/path/to/Image` 后可经 flat SBI shim、Linux
    `Image`、DTB、`virtio-blk` rootfs 和 browser terminal 进入 `mycpu-linux# `
    提示符。该入口仍是受控串口 console，不声明浏览器内 Linux、图形桌面、网络或任意
    用户镜像支持。
  - 主线 `Wave 7` 完成产品文档 v1：
    [../plan/history_plan.md#mainline-wave7-product-docs-v1-plan](../plan/history_plan.md#mainline-wave7-product-docs-v1-plan)。
    `/docs` 现在是可阅读的产品文档入口，按 Overview、Try the Console、Demo Routes、
    Architecture、OS Bring-up、AI Accelerator、Runtime Labs、Verification、Roadmap 和
    Design References 组织内容；主叙事面向用户说明“能做什么、怎么体验”，工程边界继续通过
    design / status 证据链承接。验证覆盖 `cd frontend && node --test` 和 `git diff --check`。

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
    支持显式 `jit_dispatch` 和统一 observation event wrapper，probe 支持
    `--jit-dispatch`；默认 runtime、host code
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
- 当前 Linux runtime 没有正式交互入口：没有默认 BusyBox shell rootfs、没有前端
  Linux workload manifest、没有长期 session 资源限制，也没有前端层 Linux 命令输入
  smoke。已有 `uart_input` / terminal pump 可以复用，但必须先通过本地 CLI /
  debug server 链路证明 prompt、输入、输出裁剪、reset / terminate 和最小命令回显。
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
  host code emission v0、
  opt-in runtime harness + differential guardrail、
  executable cache runtime hookup、
  helper execution opt-in、
  runtime fallback opt-in execution、
  JIT dispatch harness v1、
  runtime loop v1 closeout guardrail、
  runtime harness summary / stats、
  `dbt_ir_eval`
  semantic differential dry-run 的更宽整数覆盖，
  以及 metadata-only `dbt_block_cache` dry-run / invalidation matrix hardening。这不是完整
  JIT / DBT engine；当前只有 host-smoke-only 的 opt-in host code emission / execution /
  runtime executable cache、scalar memory helper execution 和 reference fallback execution，
  不引入默认 JIT backend、persistent cache、workload-level scheduler 或新的 guest 可见语义。
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
  host code emission v0、
  opt-in runtime harness + differential guardrail、
  executable cache runtime hookup、
  helper execution opt-in、
  runtime fallback opt-in execution、
  JIT dispatch harness v1、
  runtime loop v1 closeout guardrail、
  IR semantic differential dry-run 更宽整数覆盖和
  metadata-only block cache / invalidation matrix hardening。当前已有 host-smoke-only
  executable lowering / host code emission / runtime harness / executable cache hookup 和 opt-in
  runtime loop，但尚未有 CSR / atomic / vector helper runtime、persistent block lifecycle、
  默认 host-code dispatch 或 workload-level runtime scheduler。
- multicore / coherence 虽然属于 `Wave 6` 长期目标，但当前仍未启动；它必须等待
  atomic、memory-order、DMA / cache 交界和验证矩阵另行收口。
- `Softmax + tiny static attention` 已作为 `Wave 4` 后段 stretch 完成，但它只覆盖
  最小静态 `fp32` row-wise softmax 和极小 attention-like profile 闭环；不要把它
  写成完整 attention、动态 sequence length、KV-cache 或 Transformer runtime。
- AI accelerator 在 `Wave 7` 的展示形态应保持为白名单 demo + 参数化小模型：
  当前已落地 `dynamic_tiny_model` 的受控 batch / preset 参数体验，并观察输出与 profile，
  但仍不开放任意模型上传、任意 graph package、通用 AI compiler 或 Linux-facing NPU driver。
  `Wave 7` 之后的新主线可以重新打开“用户自己的 AI 任务”和更接近商用 NPU 的性能模型。
- `Wave 7` 产品官网壳层、首页第一刀、控制台 demo workspace v1、产品文档 v1、
  Linux interactive frontend console 第一刀、Linux console terminate 收口、真实 Image
  opt-in e2e guardrail、reset re-arm 和 AI 参数化小模型体验已完成；但部署、安全、
  session 配额、标准发行版镜像支持、任意用户镜像上传和更深的 demo-specific
  控制台页面仍未实现。
- 用户已把 `Wave 7` 的部署目标明确为“另一台云服务器上的完整开发/验证环境”，而不是
  “本机 dev server 的简单公网暴露”。远端服务器已经以 `a8869a8` 作为正式部署基线完成环境
  搭建；当前 active 目标转为从该基线拉取最新 `main` 后的升级验证。升级时需要保留
  `/srv/apps/my_visual_CPU/runtime-assets/`、`logs/`、`tmp/` 和 host-local
  `deploy/env/mycpu-frontend.env`，重新构建 `myCPU`，重建 repo-generated
  `linux_proto` rootfs / DTB，检查 auth hash，并在重启 systemd / nginx 后运行
  `deploy/scripts/remote_smoke.sh`。对应设计与计划分别见
  [../design/wave7_remote_cloud_dev_environment_design.md](../design/wave7_remote_cloud_dev_environment_design.md)
  和 [../plan/wave7_remote_cloud_dev_environment_plan.md](../plan/wave7_remote_cloud_dev_environment_plan.md)。
- `debug/frontend`、`pipeline` 和 guest runtime 都已形成可维护边界，但后续
  仍要避免真实 bug 修复把职责重新揉回大文件。

## 下一步

1. `Wave 6 closeout runtime guardrail 计划` 已完成并归档：
   [history_plan.md#mainline-wave6-closeout-runtime-guardrail-plan](../plan/history_plan.md#mainline-wave6-closeout-runtime-guardrail-plan)。
   当前已有 host-smoke-only opt-in runtime loop v1，可连续覆盖 executable cache miss / hit、
   scalar memory helper execution、reference fallback execution、guest-store invalidation 和
   stale dispatch prevention；默认 backend 接入评估结论仍是暂不新增 `--backend jit`，
   不替换 `functional` 或 `pipeline`。
2. `Wave 6 runtime execution sequence 计划` 已完成并归档：
   [history_plan.md#mainline-wave6-runtime-execution-sequence-plan](../plan/history_plan.md#mainline-wave6-runtime-execution-sequence-plan)。
   当前已有 scalar memory helper opt-in execution、reference fallback opt-in execution、
   JIT dispatch harness v1、expanded executable integer matrix 和 runtime summary / stats；
   默认 backend 接入评估结论仍是暂不新增 `--backend jit`，不替换 `functional` 或 `pipeline`。
3. `Wave 6 executable cache runtime hookup 计划` 已完成并归档：
   [history_plan.md#mainline-wave6-executable-cache-runtime-hookup-plan](../plan/history_plan.md#mainline-wave6-executable-cache-runtime-hookup-plan)。
   当前已有 host-smoke-only 的 opt-in runtime executable cache，可以持有、复用并按
   invalidation 合同释放 resident host executable；默认仍不启用 JIT backend，不做
   persistent cache，不改变 guest 可见语义。
4. `Wave 6 JIT Execution Layer 实现计划` 已完成并归档：
   [history_plan.md#mainline-wave6-jit-execution-layer-plan](../plan/history_plan.md#mainline-wave6-jit-execution-layer-plan)。
   当前已有 host-smoke-only 的 opt-in executable JIT block，但默认仍不启用 JIT backend，
   不在默认执行路径生成或执行 host code，不改变 guest 可见语义。
5. `Wave 7 产品官网壳层与首页第一刀计划` 已完成并归档：
   [history_plan.md#mainline-wave7-product-website-shell-plan](../plan/history_plan.md#mainline-wave7-product-website-shell-plan)。
   当前本地前端服务已有 `/` 产品首页、`/console` 控制台入口和 `/docs`
   产品文档壳层；首页已覆盖 hero、控制台浮现、系统运行、机器观察、AI workload、
   JIT / DBT / L1D 未来原型和 guardrail 展示。
6. `Wave 7 控制台 demo workspace v1 计划` 已完成并归档：
   [history_plan.md#mainline-wave7-console-demo-workspace-v1-plan](../plan/history_plan.md#mainline-wave7-console-demo-workspace-v1-plan)。
   `/console` 顶层现在按 `OS Bring-up`、`Machine Inspector`、`AI Accelerator`、
   `Runtime Labs` 组织体验入口；可运行 demo 卡片可同步 workload / backend 选择并复用
   现有 `Load / Run / Pause / Reset / Terminate` 控制，future-only 路线只显示 `Coming soon`。
7. `Wave 7 产品文档 v1 计划` 已完成并归档：
   [history_plan.md#mainline-wave7-product-docs-v1-plan](../plan/history_plan.md#mainline-wave7-product-docs-v1-plan)。
   `/docs` 现在按用户体验路线说明 Overview、Try the Console、Demo Routes、
   Architecture、OS Bring-up、AI Accelerator、Runtime Labs、Verification、Roadmap 和
   Design References，并链接回设计 / 状态证据文档。
8. `Wave 7 Linux interactive frontend console 计划` 已完成并归档：
   [history_plan.md#mainline-wave7-linux-interactive-frontend-console-plan](../plan/history_plan.md#mainline-wave7-linux-interactive-frontend-console-plan)。
   当前 `/console` 已有 gated `Linux Serial Console` 路线；配置
   `MYCPU_LINUX_PROTO_CONSOLE_IMAGE=/path/to/Image` 后，可经 flat SBI shim、DTB、
   `virtio-blk` rootfs 和 browser terminal 进入 `mycpu-linux# ` 提示符。
9. `Wave 7 Linux console hardening 计划` 已完成并归档：
   [history_plan.md#mainline-wave7-linux-console-hardening-plan](../plan/history_plan.md#mainline-wave7-linux-console-hardening-plan)。
   当前 terminal title / pending hint 已随 loaded workload 区分 Linux serial console；
   `help\r` 的 prompt settling 已进入 frontend runtime characterization。
10. `Wave 7 Linux console terminate 计划` 已完成并归档：
   [history_plan.md#mainline-wave7-linux-console-terminate-plan](../plan/history_plan.md#mainline-wave7-linux-console-terminate-plan)。
   当前 `/console` 已有显式 `Terminate` 主操作，server runtime 能关闭当前 debug session、
   清空 terminal tracking 并广播 terminal reset。下一步如继续做 Linux 展示，应优先补真实
   Image 下最小命令 runtime smoke，而不是声明浏览器内 Linux、图形桌面、网络或任意用户镜像支持。
11. `Wave 7 Linux console real Image e2e 计划` 已完成并归档：
   [history_plan.md#mainline-wave7-linux-console-real-image-e2e-plan](../plan/history_plan.md#mainline-wave7-linux-console-real-image-e2e-plan)。
   当前真实 Linux serial console e2e 是显式 opt-in：默认测试记录跳过条件；提供真实
   `Image` 并设置 `MYCPU_RUN_LINUX_PROTO_CONSOLE_E2E=1` 后，frontend e2e 可验证
   Load -> `help` -> prompt settling -> Terminate 的完整链路。
12. `Wave 7 Linux console reset re-arm 计划` 已完成并归档：
   [history_plan.md#mainline-wave7-linux-console-reset-rearm-plan](../plan/history_plan.md#mainline-wave7-linux-console-reset-rearm-plan)。
   当前 `linux_proto_console` Reset 会重新应用 Linux payload / DTB、`a0/a1/a2` seed
   和 boot prompt wait，避免 Reset 后落回未装载 payload 的裸 machine 状态。
13. `Wave 7 AI 参数化小模型体验计划` 已完成并归档：
   [history_plan.md#mainline-wave7-ai-parameterized-tiny-model-plan](../plan/history_plan.md#mainline-wave7-ai-parameterized-tiny-model-plan)。
   当前 AI accelerator 展示切片已经有白名单 `dynamic_tiny_model` 参数体验；前端只提交
   batch / input preset，服务器端重新生成 / 校验 graph package、runtime shape table、
   dtype、shape 和 expected output，并固定输出、profile 和 fail-closed smoke。后续若要
   扩 op 组合或更多模板，仍必须保持 server-side whitelist，不开放任意 graph package。
14. `Wave 7` 当前活跃计划已切到远端云服务器开发/验证环境：
   [wave7_remote_cloud_dev_environment_plan.md](../plan/wave7_remote_cloud_dev_environment_plan.md)。
   远端服务器已经以 `a8869a8` 作为正式部署基线完成环境搭建；当前近端任务是按
   `deploy/operations.md` 从该基线升级到最新 `main`，并验证 auth/controller、
   `Post-Wave 7` 控制台、AI `Demo V1`、Linux console readiness 和 Spike smoke。
15. Wave 7 之后的新主线定为两条：一是像 QEMU 那样跑通标准 Debian / Alpine /
   RISC-V 发行版镜像；二是 AI accelerator 支持用户自己的 AI 任务，并逐步逼近商用
   NPU 的 tile scheduler、DMA / compute overlap、multi outstanding queue、Linux-facing
   driver 和性能模型。这两条都需要另开 design / plan，不并入 Wave 7 收尾。
16. 当前这两条 `Post-Wave 7` 新主线已经在本地工作区正式建档：
   Linux 方向进入
   [linux_distribution_platform_status.md](linux_distribution_platform_status.md) /
   [../design/post_wave7_linux_distribution_platform_design.md](../design/post_wave7_linux_distribution_platform_design.md) /
   [../plan/post_wave7_linux_distribution_platform_longterm_plan.md](../plan/post_wave7_linux_distribution_platform_longterm_plan.md)；
   AI 方向进入
   [npu_tpu_accelerator_status.md](npu_tpu_accelerator_status.md) /
   [../design/post_wave7_ai_user_tasks_npu_performance_design.md](../design/post_wave7_ai_user_tasks_npu_performance_design.md) /
   [../plan/history_plan.md#post-wave7-ai-user-tasks-npu-performance-plan](../plan/history_plan.md#post-wave7-ai-user-tasks-npu-performance-plan)。
   Linux 当前已跑通 `external Alpine ext4 + static /init` 和
   `external Alpine ext4 + dynamic /bin/sh` 两条 opt-in runtime smoke；动态 `/bin/sh`
   路线已能在同一 shell 会话内执行 `cat /etc/os-release`、`ls -l /bin/sh` 和
   `/tmp` 写读回显，每条命令都观察到期望输出后回到 `~ # ` prompt。当前还新增了
   `test-host-run_debug_cli_probe_linux_distribution_filesystem`，在同一动态 BusyBox shell
   会话内验证 `/tmp` 目录创建、文件写入、追加、读回、长度检查、删除、目录移除和后续
   shell 存活。当前长线阶段 1 还新增
   `test-host-run_debug_cli_probe_linux_distribution_tty_login`，在真实外部 Alpine 动态
   `/bin/sh` 下验证 TTY 工具盘点、`tty` / `stty` / `setsid` 往返，以及 BusyBox
   `getty -n -l /bin/sh -L 115200 ttyS0 vt100` 到等价 serial TTY prompt 后的输入输出往返。
   当前长线阶段 2 还新增
   `test-host-run_debug_cli_probe_linux_distribution_process_control`，在同一外部 Alpine
   动态 shell 下验证 `sleep 1`、后台子进程与 `wait` 返回码、`trap` / `kill -TERM $$`
   和基础 shell 控制流。当前长线阶段 3 还新增
   `test-host-run_debug_cli_probe_linux_distribution_filesystem_persistence`，复制外部 Alpine
   rootfs 到 `/tmp` 临时 ext4 副本后验证同会话目录创建、文件写入、`sync`、rename
   overwrite、目录遍历和 64 KiB 文件写读。当前长线阶段 4 也已完成第一刀：新增 curated
   distro matrix 入口 `MYCPU_LINUX_DISTRO_RUNTIME_DISTRO=alpine|debian`，以及
   `test-host-run_debug_cli_probe_linux_distribution_curated_alpine_shell`、
   `test-host-run_debug_cli_probe_linux_distribution_curated_debian_shell`、
   `test-host-run_debug_cli_probe_linux_distribution_curated_matrix` 三条 target。当前 Alpine
   curated shell 继续走外部动态 `/bin/sh -> ~ # ` 基线；Debian 13 (`trixie`) riscv64 已通过外部 ext4 +
   `init=/mycpu-debian-init` curated wrapper route 跑通
   `mycpu-debian# -> cat /etc/os-release -> ID=debian`。这条 Debian 路线当前只声明
   `shell` profile，不扩大解释成原生 Debian `/bin/sh` / getty / login 完整支持。
   这仍只是最小 shell command / filesystem / 等价 serial TTY prompt / process-control
   / 同会话 ext4 persistence smoke contract，不等同于完整发行版矩阵、init 管理的 getty、
   密码 login、完整 signal 子系统或跨 reboot 持久性。AI 当前第一刀采用
   host-side 受限 `ai_task_spec_v1` importer，并已覆盖 `bounded_dynamic_gemm_v1`
   与 `bounded_dynamic_cnn_v1` 到统一 graph package / runtime shape / manifest 路径；
   尚未开放任意模型上传、完整 ONNX / PyTorch runtime 或 wall-clock 性能结论。
17. 默认 JIT backend 仍只做后续 readiness 评估；默认 backend、persistent cache、
   workload-level scheduler、CSR / atomic / vector helper runtime、multicore、coherence 和
   新的 memory consistency 模型仍不启动。
18. `pc_costs` / `branch_targets` 仍是 debug/profile 读侧合同，不是 guest ABI；后续
   如需调整排序或字段，必须先补 probe / host smoke 兼容门禁。
19. 继续把 pipeline-side `xv6` memory observation、functional `xv6`、Linux
   dummy/probe、pipeline `vector_cnn` 和现有 debug CLI 输出作为 `Wave 6`
   hot-path evidence 的前置 guardrail。
20. AI accelerator 后续若继续推进 `INT4 / training / MobileNet / Linux-facing NPU
   driver / real DMA overlap / multi outstanding queue`，应另开本方向专项 plan；其中
   用户自定义 AI 任务和商用 NPU-like 性能模型已经提升为 Post-Wave 7 新主线，但不改写
   当前 `Wave 6 / Wave 7` 收口边界。
21. Wave 4 AI accelerator 的完成记录统一见
   [../plan/history_plan.md#mainline-wave4-ai-accelerator-slices-plan](../plan/history_plan.md#mainline-wave4-ai-accelerator-slices-plan)。
22. 显式提供真实 Linux `Image` 时，补跑 `timerfd-one-shot-readback-ok` runtime
   guardrail；未提供 `Image` 时，不把该项写成默认已证明。
23. 继续守住 `xv6` shell、Linux probe、`kernel_alpha`、debug CLI、
   `make test` 和 `make test-pipeline` 这些稳定 guardrail。
24. 不继续向当前 Linux fourth-stage smoke 追加同类 syscall 微分支；如果真实 runtime
   暴露新 blocker，再按 blocker 驱动回补最窄 Linux guardrail。

## 验证基线

- `cd myCPU && make test-fast-smoke`
- `cd myCPU && make test-standard-regression`
- `cd myCPU && make test-slow-guest`
- `cd myCPU && make test-opt-in-external`
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
