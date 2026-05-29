# 代码复查状态

## 文档定位

本文档用于集中记录代码审查 / 复查任务中发现的问题、当前处理状态和下一步。

它不记录完整修复过程；具体执行步骤应进入对应 `plan` 文档，已完成事项统一归档到 [../plan/history_plan.md](../plan/history_plan.md)。

## 关联文档

- 相关状态：
  - [mainline_status.md](mainline_status.md)
- 已完成计划归档：
  - [../plan/history_plan.md#code-reself-remediation-plan](../plan/history_plan.md#code-reself-remediation-plan)

## 当前状态

- `2026-05-29` 已完成一次基于 `63f64bd docs(规划): 收口课程 OS 分线协调基线` 的全仓库只读代码审查。
  - 本轮覆盖 `ISA/reference truth`、`pipeline/JIT/DBT/cache runtime`、`platform/Linux distro/guest runtime/kernel_alpha`、`AI accelerator/NPU performance model`、`frontend/debug protocol/docs/test matrix` 五个方向；`ISA/reference truth` 因范围较大拆成 `decode/semantics/functional backend` 与 `arch/trap/mem/loader` 两个窄审查。
  - 审查口径除正确性、合同、fail-closed 和文档事实来源外，也覆盖代码本身的臃肿冗余、职责堆叠、低效路径和重复事实来源。
  - 该只读审查未修改生产代码，也未运行完整回归矩阵；所有 finding 均保留建议验证命令。完成态文档检查覆盖 `git diff --check`。
- `2026-05-29` 四条整改线已合入本地主线：
  - `fix(review): close linux frontend security findings`
  - `fix(core): close review correctness findings`
  - `fix(ai): harden profile and graph contracts`
  - `fix(code-reself): close dbt platform guest findings`
- `2026-05-29` 合并后统一验证已完成，整改计划已归档到
  [../plan/history_plan.md#code-reself-remediation-plan](../plan/history_plan.md#code-reself-remediation-plan)；
  当前 `必须修复` 与 `建议修改` active findings 均已关闭。

### 已关闭 findings

- `2026-05-29` 已关闭 M1 / M2 / M3 / M4 / M5：`pipeline` RVC
  fetch / commit 使用真实指令长度；FP RMM 覆盖算术和转换；Sv39 superpage
  fault 先检查对齐再写 A/D；reset / payload load 清 LR/SC reservation；
  `pipeline` / LSQ 基于翻译后 PA region 判定 forwarding 和 speculation。
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
- `2026-05-29` 已关闭 M10 / M11 / M12：AI profile manifest 数值字段改为
  strict `uint32` 解析；profile parse fault / compute fault 不复用旧 summary；
  graph package validator 在 parser 阶段拒绝 duplicate / missing memory plan。
- `2026-05-29` 已关闭 S1 / S2 / S18 / S19：DBT executable cache invalidation
  记录 physical span / address-space identity；helper store 对跨页或不完整 PA span
  保守清 LR/SC reservation；DBT retired count 明确为 IR expected contract；
  metadata / executable cache 增加 smoke cap。
- `2026-05-29` 已关闭 S3 / S13 / S14 / S15 / S17：`pipeline`
  记录 atomic memory observation；Sv39 leaf / non-leaf reserved PTE bits
  fail-closed；`mstatus.MPP=2` 写入规整并在 privilege decode fallback；
  debug bus 观察面区分 `source` / `kind` 并保留 guest-data slot；LSQ / FP
  metadata 改为消费 shared instruction descriptors。
- `2026-05-29` 已关闭 S5 / S6 / S7 / S8：PLIC / MMIO 平台契约文档已对齐
  virtio=1、AI=9、UART=10；README 将旧 `KMVPETDS` 改为历史 guardrail；
  `kernel_runtime` storage signature guardrail 已分层到 kernel_alpha / demo 语境；
  VirtQueue 在 status / used writeback 后提交 avail entry。
- `2026-05-29` 已关闭 S9 / S10 / S11 / S12：graph package reserved 字段
  fail-closed；guest C ABI 字段命名为 `runtime_shape_table_offset`；AI profile /
  timing schema 暴露版本；manifest `expected_output` 已成为 output 顺序 correctness gate。
- `2026-05-29` 已同步 `pipeline` guest gate：kernel-alpha pipeline guest
  demos 在当前 debug build 下约 8.4s，`PIPELINE_GUEST_TEST_TIMEOUT` 从 `8s`
  调整为 `12s`，避免正确性回归验证误报 timeout。

### 必须修复

当前无 active `必须修复` finding；本轮合并后统一验证和计划归档已完成。

### 建议修改

当前无 active `建议修改` finding；本轮合并后统一验证和计划归档已完成。

### 关键验证

`2026-05-29` 本轮合并后收尾已覆盖：

- `cd myCPU && make test-fast-smoke`
- `cd myCPU && make test-standard-regression`
- `cd myCPU && make test-pipeline`
- `cd myCPU && make test`
- `cd frontend && node --test`

`frontend` 默认测试中真实 Linux serial console e2e 仍保持显式 opt-in，未设置
`MYCPU_RUN_LINUX_PROTO_CONSOLE_E2E=1` 时按预期跳过。
`test-opt-in-external` 依赖外部 Linux Image / rootfs 和 Spike 等资产，未作为默认无资产门禁运行。

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

1. 当前无 code review remediation 活跃计划；后续只保留 `仅记录` 中尚未转入整改的长期观察项。
2. 若继续处理 `仅记录` 项，应先单独拆成新的设计 / 计划 / 验证边界，不回灌到本轮已归档整改计划。
3. 不要把已关闭 finding 的执行流水账继续堆在本文档。

## 记录规则

1. 问题按严重级别和影响面排序。
2. 每条问题至少写清影响范围、风险、建议动作和当前状态。
3. 如果问题进入修复，应补充对应 `plan` 或相关提交 / 分支说明。
4. 问题关闭后只保留简短结论，不在本文件堆积完整执行流水账。
