# 代码复查状态

## 文档定位

本文档用于集中记录代码审查 / 复查任务中发现的问题、当前处理状态和下一步。

它不记录完整修复过程；具体执行步骤应进入对应 `plan` 文档，已完成事项统一归档到 [../plan/history_plan.md](../plan/history_plan.md)。

## 关联文档

- 相关状态：
  - [mainline_status.md](mainline_status.md)
- 当前计划：
  - [../plan/code_reself_remediation_plan.md](../plan/code_reself_remediation_plan.md)

## 当前状态

- `2026-05-29` 已完成一次基于 `63f64bd docs(规划): 收口课程 OS 分线协调基线` 的全仓库只读代码审查。
  - 本轮覆盖 `ISA/reference truth`、`pipeline/JIT/DBT/cache runtime`、`platform/Linux distro/guest runtime/kernel_alpha`、`AI accelerator/NPU performance model`、`frontend/debug protocol/docs/test matrix` 五个方向；`ISA/reference truth` 因范围较大拆成 `decode/semantics/functional backend` 与 `arch/trap/mem/loader` 两个窄审查。
  - 审查口径除正确性、合同、fail-closed 和文档事实来源外，也覆盖代码本身的臃肿冗余、职责堆叠、低效路径和重复事实来源。
  - 该只读审查未修改生产代码，也未运行完整回归矩阵；所有 finding 均保留建议验证命令。完成态文档检查覆盖 `git diff --check`。
- 当前存在以下 active findings；本轮已关闭项只保留简短结论。

### 已关闭 findings

- `2026-05-29` 已关闭 M6：Linux distribution rootfs gate 现在会拒绝 repo 内
  `linux_proto/rootfs.ext4` 的相对路径、绝对路径和 symlink alias；curated Alpine /
  Debian shell 入口也复用同一 external-rootfs gate。
- `2026-05-29` 已关闭 M7：frontend Linux topic 已拆分 topic readable 与 runtime
  loadable；缺 `linux_proto_console` manifest 时仍可阅读专题，但 `Sync session`、
  `Load current scenario` 和 `Open live shell` 不再可用，也不会复用上一 session 的 workload 文案。
- `2026-05-29` 已关闭 M8：reset re-arm 单一归属回到 C++ `DebugSession::reset()`；
  Node reset 后只等待 boot marker，不再重复 append payload / GPR post-load action。
- `2026-05-29` 已关闭 M9：远端 env 模板默认启用 auth 且 blank hash fail-fast；
  production-like auth disabled 必须显式设置 `MYCPU_PUBLIC_UNAUTH_OK=1`。
- `2026-05-29` 已关闭 S4：debug protocol 数字字段统一 strict parse，拒绝 trailing junk、
  负号、空字符串、溢出和多余空白。
- `2026-05-29` 已关闭 S16：`myCPU/Makefile` 已新增并文档化
  `test-fast-smoke`、`test-standard-regression`、`test-slow-guest` 和 `test-opt-in-external`
  分层入口。
- `2026-05-29` 已关闭 core correctness 任务 1/2/3/4/5：`pipeline` RVC
  fetch / commit 使用真实指令长度；FP RMM 覆盖算术和转换；Sv39 superpage
  fault 先检查对齐再写 A/D；reset / payload load 清 LR/SC reservation；
  `pipeline` / LSQ 基于翻译后 PA region 判定 forwarding 和 speculation。
- `2026-05-29` 已关闭 core correctness 任务 15/25/26/27/29：`pipeline`
  记录 atomic memory observation；Sv39 leaf / non-leaf reserved PTE bits
  fail-closed；`mstatus.MPP=2` 写入规整并在 privilege decode fallback；
  debug bus 观察面区分 `source` / `kind` 并保留 guest-data slot；LSQ / FP
  metadata 改为消费 shared instruction descriptors。
- `2026-05-29` 已同步 `pipeline` guest gate：kernel-alpha pipeline guest
  demos 在当前 debug build 下约 8.4s，`PIPELINE_GUEST_TEST_TIMEOUT` 从 `8s`
  调整为 `12s`，避免正确性回归验证误报 timeout。

### 必须修复

1. AI profile manifest 数值字段不是严格 `uint32` 解析。
    - 影响范围：host-only importer / manifest fail-closed。
    - 证据：`myCPU/src/platform/machine.cpp` 的 `parse_ai_profile_manifest_file()` 对 `max_ticks` / `source_tag` 使用 `std::stoul(value, nullptr, 0)` 后直接 cast 到 `uint32_t`，没有检查全字符串消费、负号和范围溢出。
    - 风险：`source_tag=4294967296` 会截断，`max_ticks=1junk` 会被当成 `1`，manifest 路径比 task-spec importer 更 fail-open。
    - 建议动作：新增 strict `parse_uint32` / `parse_uint32_nonzero`，检查 `pos == value.size()`、无符号、范围和空白。
    - 建议验证：`cd myCPU && make test-host-ai_accelerator_profile_smoke`，补 negative / oversized / trailing-junk manifest scalar 负例。
2. AI profile 失败路径可能复用旧 summary。
    - 影响范围：profile / observability schema。
    - 证据：`Machine::run_ai_profile_manifest()` 在 manifest parse 成功后才 reset AI 设备和 RAM，解析异常会保留上一轮 `profile_summary()`；`myCPU/src/devices/ai_accelerator.cpp` 的 accepted compute fault 路径只更新 completion outcome，`tile_count` / `scratchpad_peak_bytes` / `op_summaries` 只在 scheduler 成功后覆盖。
    - 风险：success 后再触发 malformed manifest 或 accepted compute fault，profile 可能显示新 fault + 旧 aggregate / op summary，破坏 fail-closed 和观察面真实性。
    - 建议动作：把 AI reset / profile clear 前移或用 guard 覆盖所有失败出口；accepted submission 开始时清空 aggregate / op summaries，compute fault completion 保持 outcome 但不复用旧 op summary。
    - 建议验证：`cd myCPU && make test-host-ai_accelerator_profile_smoke test-host-ai_accelerator_gemm_smoke`。
3. AI graph package memory plan 没有在 parser 阶段保证单一事实来源。
    - 影响范围：device descriptor / future Linux-facing driver route。
    - 证据：`myCPU/src/devices/ai_graph_package.cpp` 的 `validate_ai_graph_package()` 只逐 entry 校验大小和 scratchpad budget，不拒绝重复或缺失 memory plan；设备随后按 `package.memory_plan` 构造 DMA load/store，scheduler 到 compute 前才发现 duplicate。
    - 风险：非法 memory plan 可进入 accepted submission，甚至先触发 DMA，再在 scheduler 才 fault；descriptor validation、DMA plan 和 scheduler 形成多套事实来源。
    - 建议动作：在 `validate_ai_graph_package()` 建立 `memory_plan_by_tensor`，拒绝重复 entry，并要求所有 op input / output tensor 都有唯一 memory plan。
    - 建议验证：`cd myCPU && make test-unit-ai_graph_package test-host-ai_accelerator_profile_smoke`。

### 建议修改

1. DBT guest-store invalidation 只比对虚拟 PC range，物理代码页 synonym 自修改代码可能留下 stale executable contract。建议在 cache entry 记录 physical span / satp / 属性，无法可靠翻译时全局 invalidate；验证：`cd myCPU && make test-host-dbt_runtime_invalidation_smoke test-host-dbt_runtime_harness_smoke`。
2. DBT helper store 的 LR/SC reservation invalidation 未处理跨页 store。建议复用 commit-boundary 逻辑，跨页或 PA span 不完整时 clear reservation；验证：`cd myCPU && make test-host-dbt_helper_execution_bridge_smoke test-host-dbt_runtime_harness_smoke`。
3. PLIC / MMIO 平台契约文档与当前 DTB / 常量不一致。`docs/design/platform_mmio_contract.md` 仍写 UART source 1，代码 / DTB 已是 virtio=1、AI=9、UART=10。建议更新文档并明确 SimpleStorage / Virtio transport 选择关系；验证：`cd myCPU && make test-host-virtio_blk_smoke test-host-xv6_shell_smoke`。
4. `README.md` 仍把旧 `KMVPETDS` 写成当前 `kernel_alpha` 能力。建议改为历史 Phase 1 guardrail，并单独说明课程 OS stage1 接管线；验证：`rg -n "kernel_alpha = KMVPETDS|KMVPETDS" README.md docs/status docs/design myCPU/Makefile`。
5. 通用 `kernel_runtime` helper 仍硬编码 demo storage signature。建议把 `'Stor'` signature guardrail 移到 kernel_alpha / demo 专用 helper，或重命名为明确 guardrail contract；验证：`cd myCPU && make test-unit-kernel_runtime test-unit-kernel_alpha_common test-guest-kernel_alpha_demo test-guest-supervisor_demo`。
6. VirtQueue 在确认 used / status 写回前就消费 avail entry。建议延后提交 avail index，或失败路径尽量写 `VIRTIO_BLK_S_IOERR` 与 used entry；验证：`cd myCPU && make test-host-virtio_blk_smoke` 并补 bad descriptor / DMA fault host test。
7. Graph package record reserved 字段未 fail-closed。建议对 tensor / op / memory-plan / dynamic-tensor record 的 reserved 字段统一非零 reject；验证：`cd myCPU && make test-unit-ai_graph_package`。
8. Guest C ABI 仍把 runtime shape offset 命名为 `reserved0`。建议改名为 `runtime_shape_table_offset`，保留 ABI size assert，并补 guest queue unit；验证：`cd myCPU && make test-unit-ai_accel_queue test-host-ai_accel_guest_smoke`。
9. AI profile 文本 / 结构缺少版本边界且字段明显膨胀。建议加 `profile_schema_version` / `timing_schema_version`，CLI 至少输出 `schema=ai_profile_v1`；验证：`cd myCPU && make test-host-ai_accelerator_profile_smoke`。
10. `expected_output` 是 AI manifest 字段但 runner 忽略它。建议移除该 schema 字段或在 runner 中按 output 顺序比对；验证：`cd myCPU && make test-host-ai_accelerator_profile_smoke`。
11. DBT guardrail 的 retired count 来自 IR 预期，不是 host executable 实测。建议 host ABI 返回 executed / retired count，或把字段明确标成 IR-expected；验证：`cd myCPU && make test-host-dbt_runtime_harness_smoke test-host-dbt_host_emitter_smoke`。
12. DBT metadata / executable cache 是无界 vector + 线性查找。若只服务 smoke，建议明确 max entry / cap；若准备演进 runtime，改用 keyed map + LRU / 上限；验证：`cd myCPU && make test-host-dbt_executable_cache_smoke test-host-dbt_jit_engine_smoke test-host-dbt_runtime_harness_smoke`。

### 仅记录

1. 外部 ext4 rootfs 临时复制和 `virtio-blk` backend 当前整文件读入内存。真实 Alpine / Debian rootfs 较大时可能 OOM 或显著变慢；后续可改 streaming copy / reflink fallback，并评估分页或文件后端。
2. guest runtime timer / interrupt wait 仍是忙等。当前 guardrail 可接受，但会消耗模拟步数；后续若 guest runtime 继续通用化，可引入统一 wait helper，并在模拟器支持后使用 `wfi`。
3. `dynamic_tiny_model` 和 `tiny_attention_static` 的 graph / memory-plan 定义在 `pack_graph.py` 与 `task_spec_lowering.py` 重复维护。后续可让 baseline workload 复用 shared builder。
4. debug `jit_dispatch` 的 `cache_state` 是单次 dry-run cache，不是 session cache。建议后续通过字段命名或文档标注为 per-request dry-run cache。
5. 标量跨页 store 当前允许先写前半段再 fault。需要明确这是刻意支持的 partial side effect，还是应改成 faulting store 无 partial RAM side effect，并补最小 asm 回归。
- `2026-04-22` 已完成一轮针对主分支顶部向量 workload 合并的复查，当前未发现新的 active finding：
  - `fix(向量): 显式物化 ReLU zero vector` 这轮改动已把 `vector_demo` / `vector_cnn_demo` 的 ReLU 路径从“隐式依赖 reset-zero `v0`”收口成“显式加载 zero vector”，并同步更新了对应 host smoke。
  - 复查已确认 guest demo 与 host smoke 的 functional / pipeline 两条入口都保持通过；当前没有发现新的行为回归、contract 破坏或遗漏的 workload 入口。
  - 本轮确认验证已覆盖：
    - `cd myCPU && make test-host-vector_operator_smoke test-host-vector_cnn_smoke`
    - `cd myCPU && make test-host-vector_backend_smoke test-host-vector_vlite_smoke`
    - `cd myCPU && make test-guest-vector_demo test-pipeline-guest-vector_demo test-guest-vector_cnn_demo test-pipeline-guest-vector_cnn_demo`
- `2026-04-22` 已关闭 `xv6 / Linux / JIT` 第一轮整合后的 2 条集中复查 findings：
  - 普通 `store` 现在会在 functional / commit-boundary 路径上正确打破 `LR/SC` reservation，`lr -> overlapping sw -> sc` 已补 host 回归。
  - `execution_profile` 现在会把 translation-fault memory access 记成 `unmapped` fault observation，并由 `execution_profile_smoke` 守住 `total_memory_observations` / `memory_regions[].faults`。
- 本轮关闭验证已覆盖：
  - `cd myCPU && make test-host-atomic_semantics_smoke`
  - `cd myCPU && make test-host-execution_profile_smoke`
  - `cd myCPU && make test-host-debug_cli_smoke`
  - `cd myCPU && make test`
  - `cd myCPU && make test-pipeline`

## 下一步

1. 第一批整改继续处理 `必须修复` 中剩余的 AI manifest / profile / memory-plan fail-closed 问题。
2. 跨多个子系统的结构问题不要直接派零散修复 agent；优先拆成小计划，例如 `shared instruction metadata`、`AI profile schema v1`、`test matrix layering`、`DBT invalidation physical span`。
3. 每条整改都应先补最窄红灯，再改代码，并按影响面选择 `make test`、`make test-pipeline`、`frontend node --test` 或外部资产 opt-in target。

## 记录规则

1. 问题按严重级别和影响面排序。
2. 每条问题至少写清影响范围、风险、建议动作和当前状态。
3. 如果问题进入修复，应补充对应 `plan` 或相关提交 / 分支说明。
4. 问题关闭后只保留简短结论，不在本文件堆积完整执行流水账。
