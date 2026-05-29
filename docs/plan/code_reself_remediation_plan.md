# 代码复查整改总计划

> **文档状态：** 执行中
>
> **面向 AI 代理的工作者：** 必需子技能：使用 `superpowers:subagent-driven-development`（推荐）或 `superpowers:executing-plans` 逐任务实现此计划。步骤使用复选框（`- [ ]`）语法来跟踪进度。

## 文档定位

本文档把 [../status/code_reself_status.md](../status/code_reself_status.md) 中 `2026-05-29` 全仓库只读代码审查产生的全部 `必须修复` 与 `建议修改` active findings 拆成可执行整改批次。

它只承接执行步骤和验收顺序；问题事实来源、严重级别和关闭状态仍以 [../status/code_reself_status.md](../status/code_reself_status.md) 为准。

## 关联文档

- 来源设计：
  - [../design/pipeline_speculation_contracts.md](../design/pipeline_speculation_contracts.md)
  - [../design/wave6_jit_dbt_readiness_design.md](../design/wave6_jit_dbt_readiness_design.md)
  - [../design/platform_mmio_contract.md](../design/platform_mmio_contract.md)
  - [../design/post_wave7_linux_distribution_platform_design.md](../design/post_wave7_linux_distribution_platform_design.md)
  - [../design/post_wave7_frontend_lab_product_design.md](../design/post_wave7_frontend_lab_product_design.md)
  - [../design/post_wave7_ai_user_tasks_npu_performance_design.md](../design/post_wave7_ai_user_tasks_npu_performance_design.md)
  - [../design/regression_completion_criteria.md](../design/regression_completion_criteria.md)
- 目标状态：
  - [../status/code_reself_status.md](../status/code_reself_status.md)
  - [../status/mainline_status.md](../status/mainline_status.md)
  - [../status/linux_distribution_platform_status.md](../status/linux_distribution_platform_status.md)
  - [../status/npu_tpu_accelerator_status.md](../status/npu_tpu_accelerator_status.md)

## 目标

按 `P0 -> P1 -> P2 -> P3` 顺序关闭全部 12 条 `必须修复` 和 19 条 `建议修改`，优先保证 guest-visible correctness、fail-closed、安全边界和单一事实来源，再处理结构性简化与测试矩阵分层。

## 架构

整改采用 `reference-first` 和小步红灯优先策略：每条 guest-visible 或 fail-closed finding 先补最窄回归，再修改共享语义、平台边界或 parser / validator，最后扩到对应 backend、frontend 或文档门禁。

跨模块问题按交付面拆包：`pipeline / ISA / MMU`、`loader / LR-SC / DBT`、`Linux / frontend / auth`、`AI manifest / profile / package`、`platform / guest / docs / test matrix` 分别收口，避免多个 agent 同时改同一事实来源。

## 技术栈

- C++17 simulator core、host smoke、unit tests、asm contract tests
- guest C runtime 与 freestanding tests
- Node `node --test` frontend / debug server tests
- Makefile regression targets
- Markdown docs under `docs/design`、`docs/plan`、`docs/status`

## Finding 覆盖表

### 必须修复

| 编号 | 计划任务 | 关闭条件 |
| --- | --- | --- |
| M1 RVC pipeline 按 32-bit 推进 | 任务 1 | pipeline fetch / commit / predictor fallthrough 使用 `Insn::size`，RVC differential 通过 |
| M2 RMM FP 算术被当成非法 | 任务 2 | FP 算术 / 转换统一支持 RMM，shared semantics 与 pipeline 用例通过 |
| M3 Sv39 misaligned superpage fault 前写 A/D | 任务 3 | superpage 对齐检查早于 A/D 回写，fault PTE 原值不变 |
| M4 image reload / payload load 未清 LR/SC | 任务 4 | reset、primary image load、payload load 均清 reservation |
| M5 pipeline / LSQ 用 VA 判断 RAM / MMIO | 任务 5 | pipeline memory classification 基于翻译后 PA / region，unknown 不 speculative forwarding |
| M6 标准发行版 rootfs fail-closed 可被绕过 | 任务 6 | repo 内 rootfs 的绝对、相对和 symlink 路径均被拒绝 |
| M7 Linux topic 绕过 manifest gating 暴露 live 控件 | 任务 7 | topic 可读与 runtime 可加载状态分离，缺 Image 时 live 控件禁用 |
| M8 Linux reset re-arm 重复持久化 action | 任务 8 | reset 后 post-load action 不增长，Node 不重复 append |
| M9 远端部署样例默认关闭认证 | 任务 9 | 远端模板 fail-closed，未认证 mutating API 不开放 |
| M10 AI manifest 数值字段非 strict uint32 | 任务 10 | `max_ticks` / `source_tag` 拒绝负号、溢出和 trailing junk |
| M11 AI profile 失败路径复用旧 summary | 任务 11 | parse fault / compute fault 都不会带旧 aggregate / op summary |
| M12 AI memory plan parser 未保证单一事实来源 | 任务 12 | parser 阶段拒绝 duplicate / missing memory plan |

### 建议修改

| 编号 | 计划任务 | 关闭条件 |
| --- | --- | --- |
| S1 DBT guest-store invalidation 只看 VA PC range | 任务 13 | cache entry 记录 physical span / address-space identity，不可靠时全局 invalidate |
| S2 DBT helper store LR/SC invalidation 未处理跨页 | 任务 14 | helper store 复用 commit-boundary reservation invalidation，跨页不完整时清 reservation |
| S3 pipeline profile 漏记 atomic observations | 任务 15 | pipeline atomic commit 复用 functional observation 逻辑 |
| S4 debug CLI 数字字段解析 fail-open | 任务 16 | 所有 debug protocol 数字字段 strict parse |
| S5 PLIC / MMIO 文档与 DTB / 常量不一致 | 任务 17 | [../design/platform_mmio_contract.md](../design/platform_mmio_contract.md) 与代码 / DTB source ID 一致 |
| S6 README 仍把旧 KMVPETDS 写成当前能力 | 任务 18 | README 把 KMVPETDS 降级为历史 guardrail，并说明课程 OS stage1 分线 |
| S7 kernel_runtime helper 硬编码 demo storage signature | 任务 19 | `'Stor'` guardrail 移入 kernel_alpha / demo 专用层或命名清晰 |
| S8 VirtQueue 过早消费 avail entry | 任务 20 | used / status 可写后才提交 avail index，失败路径有 IOERR 合同 |
| S9 graph package reserved 字段未 fail-closed | 任务 21 | tensor / op / memory-plan / dynamic-tensor reserved 非零均 reject |
| S10 guest C ABI `reserved0` 命名不清 | 任务 22 | 字段改为 `runtime_shape_table_offset` 并保持 ABI size |
| S11 AI profile schema 缺版本边界 | 任务 23 | profile / timing schema 有版本字段，CLI 输出 schema tag |
| S12 AI manifest `expected_output` 被忽略 | 任务 24 | 字段被移除或 runner 按输出顺序比对 |
| S13 Sv39 leaf PTE reserved 高位未 fail-closed | 任务 25 | leaf / non-leaf reserved mask 有 asm 回归 |
| S14 `mstatus.MPP=2` 可写入并由 `mret` 解码为 M | 任务 26 | reserved MPP 写入或 mret 路径被规整 / 拒绝 |
| S15 debug bus 观察面单槽易覆盖 | 任务 27 | 观察面区分 source / kind，guest data / MMIO commit 有独立槽 |
| S16 测试矩阵缺少 fast / standard / slow / external 分层 | 任务 28 | Makefile 暴露并文档化分层 target |
| S17 pipeline LSQ / FP metadata 复制 shared semantics 事实 | 任务 29 | shared memory shape / FP descriptor 被 pipeline 只读消费 |
| S18 DBT retired count 来自 IR 预期 | 任务 30 | host ABI 返回 executed / retired，或字段明确标为 IR-expected |
| S19 DBT metadata / executable cache 无界 vector + 线性查找 | 任务 31 | smoke cap 明确，或 keyed map + LRU / 上限落地 |

## 执行批次

1. `P0 guest-visible correctness`：任务 1、2、3、4、5、25、26。
2. `P0 fail-closed / security`：任务 6、7、8、9、10、11、12、16、21、24。
3. `P1 DBT / cache lifecycle`：任务 13、14、30、31。
4. `P1 observability / profile schema`：任务 15、23、27、29。
5. `P2 platform / guest / virtio / docs`：任务 17、18、19、20、22。
6. `P3 test matrix and final closure`：任务 28、32。

## 任务

### 任务 1：pipeline RVC fetch / commit 合同

**覆盖 finding：** M1

**文件：**
- 修改：`myCPU/src/exec/pipeline_backend_frontend.cpp`
- 修改：`myCPU/src/exec/pipeline_backend_cycle.cpp`
- 修改：`myCPU/src/exec/pipeline_types.h`
- 修改：`myCPU/src/exec/reorder_buffer.h`
- 修改：`myCPU/src/exec/reorder_buffer.cpp`
- 测试：`myCPU/tests/host/pipeline_backend_smoke.cpp`
- 测试：`myCPU/tests/host/backend_differential_smoke.cpp`
- 参考：`myCPU/tests/host/rvc_semantics_smoke.cpp`

- [ ] **步骤 1：补 pipeline RVC 红灯**
  - 在 `pipeline_backend_smoke` 增加压缩指令样本，至少覆盖 16-bit fallthrough、compressed branch / jal link PC 和跨 32-bit packet fetch。
  - 在 `backend_differential_smoke` 增加同一 RVC block 的 functional / pipeline PC 与 GPR 对齐断言。
- [ ] **步骤 2：运行红灯验证**
  - 运行：`cd myCPU && make test-host-rvc_semantics_smoke test-host-pipeline_backend_smoke test-host-backend_differential_smoke`
  - 预期：新增 pipeline RVC 用例失败，functional RVC smoke 继续通过。
- [ ] **步骤 3：实现 16-bit-first fetch**
  - 让 IF 阶段先取 16-bit，只有 decode 判定需要 32-bit 时再取完整 instruction word。
  - 把 `Insn::size` 写入 pipeline stage payload 和 ROB entry，不再用固定 `pc + 4` 表示 fallthrough。
- [ ] **步骤 4：修正 commit / predictor fallthrough**
  - commit redirect、link register、branch predictor update 和 fallthrough PC 使用 `rob_entry.insn_size`。
  - fetch fault / halfword fault 的 `tval` 与 functional path 对齐。
- [ ] **步骤 5：运行验证**
  - 运行：`cd myCPU && make test-host-rvc_semantics_smoke test-host-pipeline_backend_smoke test-host-backend_differential_smoke test-pipeline`
  - 预期：全部通过。
- [ ] **步骤 6：提交**
  - 运行：`git add myCPU/src/exec/pipeline_backend_frontend.cpp myCPU/src/exec/pipeline_backend_cycle.cpp myCPU/src/exec/pipeline_types.h myCPU/src/exec/reorder_buffer.h myCPU/src/exec/reorder_buffer.cpp myCPU/tests/host/pipeline_backend_smoke.cpp myCPU/tests/host/backend_differential_smoke.cpp && git commit -m "fix(pipeline): honor RVC instruction size"`

### 任务 2：FP RMM rounding mode 合同

**覆盖 finding：** M2

**文件：**
- 修改：`myCPU/src/exec/floating_ops.cpp`
- 修改：`myCPU/src/exec/floating_ops.h`
- 测试：`myCPU/tests/host/instruction_semantics_smoke_fp_compare_convert.cpp`
- 测试：`myCPU/tests/host/instruction_semantics_smoke_fp_fma_flags.cpp`
- 测试：`myCPU/tests/host/pipeline_backend_smoke_fp_arith_fma.cpp`
- 测试：`myCPU/tests/host/pipeline_backend_smoke_fp_convert_tail.cpp`

- [ ] **步骤 1：补 RMM 红灯**
  - 为 `fadd/fmul/fdiv/fsqrt/fmadd/fcvt.s.d` 添加 `rm=100` immediate 与 `frm=RMM` 动态 rounding 用例。
  - shared semantics 与 pipeline FP smoke 使用同一组输入，断言不会 illegal instruction，并校验结果 / fflags。
- [ ] **步骤 2：运行红灯验证**
  - 运行：`cd myCPU && make test-host-instruction_semantics_smoke test-host-pipeline_backend_smoke`
  - 预期：新增 RMM 算术或转换用例失败，既有 FP 比较 / convert 用例保持现状。
- [ ] **步骤 3：实现 RMM helper**
  - 在 `floating_ops.cpp` 增加统一 RMM rounding helper；不能依赖 host `fesetround()` 支持不存在的 RMM。
  - 所有 binary / unary / ternary arithmetic 和 floating convert path 通过同一 helper 分支处理 RMM。
- [ ] **步骤 4：统一 illegal 判断**
  - `resolve_rounding_mode()` 只负责解析合法 rounding 编码；合法 RMM 不再在 opcode helper 中转成 illegal。
- [ ] **步骤 5：运行验证**
  - 运行：`cd myCPU && make test-host-instruction_semantics_smoke test-host-pipeline_backend_smoke test-pipeline`
  - 预期：全部通过。
- [ ] **步骤 6：提交**
  - 运行：`git add myCPU/src/exec/floating_ops.cpp myCPU/src/exec/floating_ops.h myCPU/tests/host/instruction_semantics_smoke_fp_compare_convert.cpp myCPU/tests/host/instruction_semantics_smoke_fp_fma_flags.cpp myCPU/tests/host/pipeline_backend_smoke_fp_arith_fma.cpp myCPU/tests/host/pipeline_backend_smoke_fp_convert_tail.cpp && git commit -m "fix(fp): support RMM arithmetic rounding"`

### 任务 3：Sv39 superpage A/D side effect 顺序

**覆盖 finding：** M3

**文件：**
- 修改：`myCPU/src/mem/address_space.cpp`
- 修改：`myCPU/src/mem/address_space.h`
- 测试：`myCPU/tests/asm/sv39_pagewalk_contracts.S`

- [ ] **步骤 1：补 fault PTE 不变红灯**
  - 在 `sv39_pagewalk_contracts.S` 的 misaligned superpage case 中读取 fault 前 leaf PTE，触发 page fault 后再次读取同一 PTE。
  - 断言故障 leaf 的 A/D 位与原始 PTE 完全一致。
- [ ] **步骤 2：运行红灯验证**
  - 运行：`cd myCPU && make test-sv39_pagewalk_contracts test-pipeline-sv39_pagewalk_contracts`
  - 预期：新增 PTE 不变断言失败。
- [ ] **步骤 3：重排 page-walk 校验顺序**
  - 在 `AddressSpace::walk_page_table()` 中先检查 level 1/2 superpage PPN 对齐，再调用 `update_pte_access_bits()`。
  - 保持普通 leaf 的 A/D 更新和 fault cause / tval 不变。
- [ ] **步骤 4：运行验证**
  - 运行：`cd myCPU && make test-sv39_pagewalk_contracts test-pipeline-sv39_pagewalk_contracts test`
  - 预期：全部通过。
- [ ] **步骤 5：提交**
  - 运行：`git add myCPU/src/mem/address_space.cpp myCPU/src/mem/address_space.h myCPU/tests/asm/sv39_pagewalk_contracts.S && git commit -m "fix(sv39): check superpage alignment before A/D updates"`

### 任务 4：LR/SC reservation lifecycle

**覆盖 finding：** M4

**文件：**
- 修改：`myCPU/src/cpu.cpp`
- 修改：`myCPU/src/platform/machine.cpp`
- 测试：`myCPU/tests/unit/machine_loader_reset.cpp`
- 测试：`myCPU/tests/host/atomic_semantics_smoke.cpp`

- [ ] **步骤 1：补 reset / payload reservation 红灯**
  - 在 `machine_loader_reset.cpp` 构造 `lr` 后执行 `cpu_init()` 或 image reload，断言后续 `sc` 不会因旧 reservation 成功。
  - 在 payload load case 中模拟 host 写 RAM 后的 reservation 清理。
- [ ] **步骤 2：运行红灯验证**
  - 运行：`cd myCPU && make test-unit-machine_loader_reset test-host-atomic_semantics_smoke`
  - 预期：新增 lifecycle case 失败。
- [ ] **步骤 3：实现 reset 清理**
  - `cpu_init()` reset core / CSR / TLB / L1D 时调用 `trap().clear_reservation()`。
- [ ] **步骤 4：实现 payload load 清理**
  - `Machine::load_binary_payload()` 写 RAM 后清 reservation；如果保留按范围清理，跨范围或无法判定时保守清全部。
- [ ] **步骤 5：运行验证**
  - 运行：`cd myCPU && make test-unit-machine_loader_reset test-host-atomic_semantics_smoke test`
  - 预期：全部通过。
- [ ] **步骤 6：提交**
  - 运行：`git add myCPU/src/cpu.cpp myCPU/src/platform/machine.cpp myCPU/tests/unit/machine_loader_reset.cpp myCPU/tests/host/atomic_semantics_smoke.cpp && git commit -m "fix(loader): clear LR reservations on reload"`

### 任务 5：pipeline VA / PA region 分类

**覆盖 finding：** M5

**文件：**
- 修改：`myCPU/src/exec/pipeline_backend_execute.cpp`
- 修改：`myCPU/src/exec/load_store_queue.h`
- 修改：`myCPU/src/exec/load_store_queue.cpp`
- 修改：`myCPU/src/exec/pipeline_types.h`
- 测试：`myCPU/tests/host/pipeline_speculation_contracts_smoke.cpp`
- 测试：`myCPU/tests/host/backend_differential_smoke.cpp`

- [ ] **步骤 1：补 Sv39 VA->MMIO / VA->RAM alias 红灯**
  - 在 `pipeline_speculation_contracts_smoke` 增加分页开启后 VA 映射到 MMIO PA 的 load / store case，断言不得 speculative forwarding。
  - 增加 VA 看似 non-RAM 但 PA 是 RAM 的 alias case，断言 stall / forwarding 由 PA region 决定。
- [ ] **步骤 2：运行红灯验证**
  - 运行：`cd myCPU && make test-host-pipeline_speculation_contracts_smoke test-pipeline`
  - 预期：新增 alias case 失败。
- [ ] **步骤 3：携带翻译后 PA / region**
  - StageSlot 和 LSQ entry 保存 translation result、PA、region kind、fault state。
  - `needs_memory_issue_delay()` 与 forwarding 判断只消费 PA region。
- [ ] **步骤 4：收紧 unknown / cross-page / fault 合同**
  - unknown、cross-page 或翻译 fault 的 memory op 一律不可 forwarding，MMIO / side-effect path 按 non-speculative 处理。
- [ ] **步骤 5：运行验证**
  - 运行：`cd myCPU && make test-host-pipeline_speculation_contracts_smoke test-host-backend_differential_smoke test-pipeline`
  - 预期：全部通过。
- [ ] **步骤 6：提交**
  - 运行：`git add myCPU/src/exec/pipeline_backend_execute.cpp myCPU/src/exec/load_store_queue.h myCPU/src/exec/load_store_queue.cpp myCPU/src/exec/pipeline_types.h myCPU/tests/host/pipeline_speculation_contracts_smoke.cpp myCPU/tests/host/backend_differential_smoke.cpp && git commit -m "fix(pipeline): classify memory by translated physical region"`

### 任务 6：Linux distribution rootfs fail-closed

**覆盖 finding：** M6

**文件：**
- 修改：`myCPU/tests/host/run_debug_cli_probe_test.py`
- 修改：`myCPU/Makefile`
- 可能修改：`myCPU/src/main.cpp`

- [ ] **步骤 1：补相对路径与 symlink 负向测试**
  - 在 `run_debug_cli_probe_test.py` 增加 `MYCPU_LINUX_DISTRO_RUNTIME_ROOTFS=workloads/linux_proto/rootfs.ext4` 负例。
  - 增加指向 repo 内默认 rootfs 的 symlink 负例。
  - 覆盖 curated Alpine / Debian shell 入口和 distribution runtime 入口。
- [ ] **步骤 2：运行红灯验证**
  - 运行：`cd myCPU && make test-host-run_debug_cli_probe`
  - 预期：新增 rootfs fail-closed 用例失败。
- [ ] **步骤 3：实现 canonical path 拒绝**
  - 对 rootfs path 使用 `resolve(strict=True)` / `samefile` 判断 repo 内默认 rootfs。
  - 相对路径、绝对路径、symlink 都必须收敛到同一拒绝逻辑。
- [ ] **步骤 4：收紧 Makefile env gate**
  - distro runtime target 不只检查 env 非空，还要确保不是 repo 内默认 rootfs canonical path。
- [ ] **步骤 5：运行验证**
  - 运行：`cd myCPU && make test-host-run_debug_cli_probe`
  - 手工验证：`cd myCPU && MYCPU_LINUX_DISTRO_RUNTIME_ROOTFS=workloads/linux_proto/rootfs.ext4 make test-host-run_debug_cli_probe_linux_distribution_runtime`
  - 预期：手工验证 fail-closed，错误信息指出需要外部标准发行版 rootfs。
- [ ] **步骤 6：提交**
  - 运行：`git add myCPU/tests/host/run_debug_cli_probe_test.py myCPU/Makefile myCPU/src/main.cpp && git commit -m "fix(linux): reject repo rootfs aliases for distro runtime"`

### 任务 7：frontend Linux topic gating

**覆盖 finding：** M7

**文件：**
- 修改：`frontend/app/render.js`
- 修改：`frontend/server/tests_manifest.mjs`
- 测试：`frontend/tests/render.test.mjs`
- 测试：`frontend/tests/debug_server.test.mjs`
- 测试：`frontend/tests/ui_state.test.mjs`

- [ ] **步骤 1：补缺 Image UI 红灯**
  - 添加缺 `linux_proto_console` manifest 时的 render test，断言 Linux topic 可读但 `Sync session`、`Load current scenario`、`Open live shell` 均 disabled 或 hidden。
  - 添加已有 workload session 时切到 Linux topic 不会复用上一 session workload 的断言。
- [ ] **步骤 2：运行红灯验证**
  - 运行：`cd frontend && node --test`
  - 预期：新增 Linux topic gating 用例失败。
- [ ] **步骤 3：拆分 readable / loadable 状态**
  - `resolveDemoState()` 输出 `topicAvailable` 与 `runtimeAvailable` 两个状态。
  - 只有 `scenarioTest` 存在于 `/api/tests` 时，live action 才可用。
- [ ] **步骤 4：同步 manifest / UI 文案状态**
  - Linux topic 在缺 Image 时只展示配置诊断，不展示 `Topic ready` 或可运行暗示。
- [ ] **步骤 5：运行验证**
  - 运行：`cd frontend && node --test`
  - 预期：全部通过。
- [ ] **步骤 6：提交**
  - 运行：`git add frontend/app/render.js frontend/server/tests_manifest.mjs frontend/tests/render.test.mjs frontend/tests/debug_server.test.mjs frontend/tests/ui_state.test.mjs && git commit -m "fix(frontend): gate Linux live controls on runtime manifest"`

### 任务 8：debug reset re-arm 单一归属

**覆盖 finding：** M8

**文件：**
- 修改：`frontend/server/debug_server_runtime.mjs`
- 修改：`myCPU/src/debug/debug_session.cpp`
- 修改：`myCPU/src/debug/debug_session.h`
- 测试：`frontend/tests/debug_server_runtime.test.mjs`
- 测试：`myCPU/tests/host/debug_cli_smoke.cpp`

- [ ] **步骤 1：补 reset action 不增长红灯**
  - 在 Node runtime test 中连续 reset Linux session 两次，断言 payload / GPR post-load action 数量不增长。
  - 在 debug CLI smoke 中增加 reset replay 后 action 列表稳定性或等价 observability 断言。
- [ ] **步骤 2：运行红灯验证**
  - 运行：`cd frontend && node --test`
  - 运行：`cd myCPU && make test-host-debug_cli_smoke`
  - 预期：新增重复 append case 失败。
- [ ] **步骤 3：确定 C++ reset 为 replay 归属**
  - 保留 `DebugSession::reset()` 重放 `config_.post_load_actions`。
  - Node `rearmSessionStateAfterReset()` 不再调用 `loadPayload()` / `setGpr()` 追加同一 action。
- [ ] **步骤 4：Node 只等待 boot marker**
  - reset 后 Node 只读取 UART / 等待 boot marker / 更新 terminal state。
- [ ] **步骤 5：运行验证**
  - 运行：`cd frontend && node --test`
  - 运行：`cd myCPU && make test-host-debug_cli_smoke`
  - 预期：全部通过。
- [ ] **步骤 6：提交**
  - 运行：`git add frontend/server/debug_server_runtime.mjs frontend/tests/debug_server_runtime.test.mjs myCPU/src/debug/debug_session.cpp myCPU/src/debug/debug_session.h myCPU/tests/host/debug_cli_smoke.cpp && git commit -m "fix(debug): avoid duplicate reset rearm actions"`

### 任务 9：远端部署认证 fail-closed

**覆盖 finding：** M9

**文件：**
- 修改：`deploy/env/mycpu-frontend.env.example`
- 修改：`deploy/README.md`
- 修改：`deploy/operations.md`
- 修改：`frontend/server/security.mjs`
- 修改：`frontend/server/debug_server.mjs`
- 测试：`frontend/tests/security.test.mjs`
- 测试：`frontend/tests/debug_server.test.mjs`

- [ ] **步骤 1：补远端未认证红灯**
  - 添加使用 deploy env example 的 server config test，断言默认远端模式未设置 auth hash 时启动失败或 `/api/tests` 返回 `401`。
  - 添加 `MYCPU_PUBLIC_UNAUTH_OK=1` 显式 opt-in 后本地无认证路径仍可用于开发的断言。
- [ ] **步骤 2：运行红灯验证**
  - 运行：`cd frontend && node --test`
  - 预期：新增远端 fail-closed 用例失败。
- [ ] **步骤 3：调整 env example**
  - `deploy/env/mycpu-frontend.env.example` 默认启用 `MYCPU_AUTH_ENABLED=1`，hash 留空并在注释中说明必须生成。
  - 无认证公开部署必须显式设置 `MYCPU_PUBLIC_UNAUTH_OK=1`。
- [ ] **步骤 4：收紧 server security gate**
  - auth disabled 且非显式 public unauth opt-in 时，mutating API 与 `/ws` 不可开放。
  - 未登录请求 `/api/tests` 在远端模板语境下返回 `401`。
- [ ] **步骤 5：运行验证**
  - 运行：`cd frontend && node --test`
  - 预期：全部通过。
- [ ] **步骤 6：提交**
  - 运行：`git add deploy/env/mycpu-frontend.env.example deploy/README.md deploy/operations.md frontend/server/security.mjs frontend/server/debug_server.mjs frontend/tests/security.test.mjs frontend/tests/debug_server.test.mjs && git commit -m "fix(deploy): require auth for public frontend templates"`

### 任务 10：AI profile manifest strict uint32 parser

**覆盖 finding：** M10

**文件：**
- 修改：`myCPU/src/platform/machine.cpp`
- 修改：`myCPU/src/platform/machine.h`
- 测试：`myCPU/tests/host/ai_accelerator_profile_smoke.cpp`

- [ ] **步骤 1：补 malformed scalar 红灯**
  - 在 profile smoke 中添加 `source_tag=4294967296`、`max_ticks=1junk`、`max_ticks=-1`、空字符串和 whitespace 包裹 case。
  - 断言 manifest parse fail-closed，且错误信息包含字段名。
- [ ] **步骤 2：运行红灯验证**
  - 运行：`cd myCPU && make test-host-ai_accelerator_profile_smoke`
  - 预期：新增 scalar 负例失败。
- [ ] **步骤 3：实现 strict parser**
  - 增加 `parse_uint32()` 与 `parse_uint32_nonzero()` helper。
  - 检查全字符串消费、无负号、范围不超过 `UINT32_MAX`，`max_ticks` 非零。
- [ ] **步骤 4：替换 manifest scalar 解析**
  - `parse_ai_profile_manifest_file()` 中 `max_ticks` / `source_tag` 全部改用 strict helper。
- [ ] **步骤 5：运行验证**
  - 运行：`cd myCPU && make test-host-ai_accelerator_profile_smoke`
  - 预期：全部通过。
- [ ] **步骤 6：提交**
  - 运行：`git add myCPU/src/platform/machine.cpp myCPU/src/platform/machine.h myCPU/tests/host/ai_accelerator_profile_smoke.cpp && git commit -m "fix(ai): parse profile manifest scalars strictly"`

### 任务 11：AI profile failure summary 清理

**覆盖 finding：** M11

**文件：**
- 修改：`myCPU/src/platform/machine.cpp`
- 修改：`myCPU/src/devices/ai_accelerator.cpp`
- 修改：`myCPU/src/devices/ai_accelerator.h`
- 测试：`myCPU/tests/host/ai_accelerator_profile_smoke.cpp`
- 测试：`myCPU/tests/host/ai_accelerator_gemm_smoke.cpp`

- [ ] **步骤 1：补 parse fault / compute fault stale summary 红灯**
  - 先运行一次成功 profile，再运行 malformed manifest，断言 summary 没有上一轮 aggregate / op summaries。
  - 构造 accepted compute fault，断言 outcome 是 fault 且 tile / scratchpad / op summaries 被清空。
- [ ] **步骤 2：运行红灯验证**
  - 运行：`cd myCPU && make test-host-ai_accelerator_profile_smoke test-host-ai_accelerator_gemm_smoke`
  - 预期：新增 stale summary case 失败。
- [ ] **步骤 3：前移 profile clear**
  - `Machine::run_ai_profile_manifest()` 在 parse 之前或 parse guard scope 开始时清 AI profile summary。
  - manifest parse 异常不保留旧设备 summary。
- [ ] **步骤 4：accepted submission 开始时清 aggregate**
  - AI accelerator 接受新 submission 时清 `tile_count`、`scratchpad_peak_bytes`、`op_summaries`。
  - compute fault completion 只保留 fault outcome 与必要 error，不复用旧 op summary。
- [ ] **步骤 5：运行验证**
  - 运行：`cd myCPU && make test-host-ai_accelerator_profile_smoke test-host-ai_accelerator_gemm_smoke`
  - 预期：全部通过。
- [ ] **步骤 6：提交**
  - 运行：`git add myCPU/src/platform/machine.cpp myCPU/src/devices/ai_accelerator.cpp myCPU/src/devices/ai_accelerator.h myCPU/tests/host/ai_accelerator_profile_smoke.cpp myCPU/tests/host/ai_accelerator_gemm_smoke.cpp && git commit -m "fix(ai): clear profile summary on failure paths"`

### 任务 12：AI graph package memory plan validator

**覆盖 finding：** M12

**文件：**
- 修改：`myCPU/src/devices/ai_graph_package.cpp`
- 修改：`myCPU/src/devices/ai_graph_package.h`
- 测试：`myCPU/tests/unit/ai_graph_package.cpp`
- 测试：`myCPU/tests/host/ai_accelerator_profile_smoke.cpp`

- [ ] **步骤 1：补 duplicate / missing memory plan 红灯**
  - 在 `ai_graph_package` unit 中添加 duplicate memory-plan entry 负例。
  - 添加 op input / output tensor 缺失 memory plan 的负例。
  - 在 profile smoke 中覆盖非法 package 不进入 accepted submission。
- [ ] **步骤 2：运行红灯验证**
  - 运行：`cd myCPU && make test-unit-ai_graph_package test-host-ai_accelerator_profile_smoke`
  - 预期：新增 validator 用例失败。
- [ ] **步骤 3：建立 parser 阶段单一事实来源**
  - `validate_ai_graph_package()` 构造 `memory_plan_by_tensor`。
  - duplicate tensor id、缺失 input / output tensor plan、size / scratchpad 不一致均在 validator 阶段 reject。
- [ ] **步骤 4：删除 scheduler 侧重复事实依赖**
  - scheduler 仍可防御式检查，但不再作为 duplicate memory plan 的第一发现点。
- [ ] **步骤 5：运行验证**
  - 运行：`cd myCPU && make test-unit-ai_graph_package test-host-ai_accelerator_profile_smoke`
  - 预期：全部通过。
- [ ] **步骤 6：提交**
  - 运行：`git add myCPU/src/devices/ai_graph_package.cpp myCPU/src/devices/ai_graph_package.h myCPU/tests/unit/ai_graph_package.cpp myCPU/tests/host/ai_accelerator_profile_smoke.cpp && git commit -m "fix(ai): validate graph memory plans up front"`

### 任务 13：DBT executable invalidation physical span

**覆盖 finding：** S1

**文件：**
- 修改：`myCPU/src/exec/dbt_executable_cache.h`
- 修改：`myCPU/src/exec/dbt_executable_cache.cpp`
- 修改：`myCPU/src/exec/dbt_runtime_invalidation.h`
- 修改：`myCPU/src/exec/dbt_runtime_invalidation.cpp`
- 测试：`myCPU/tests/host/dbt_runtime_invalidation_smoke.cpp`
- 测试：`myCPU/tests/host/dbt_runtime_harness_smoke.cpp`

- [ ] **步骤 1：补 physical synonym 红灯**
  - 在 runtime invalidation smoke 中创建两个 VA range 映射同一 PA code page 的 metadata entry。
  - 对 synonym PA 写入后断言 executable cache entry 被 invalidated。
- [ ] **步骤 2：运行红灯验证**
  - 运行：`cd myCPU && make test-host-dbt_runtime_invalidation_smoke test-host-dbt_runtime_harness_smoke`
  - 预期：新增 synonym invalidation case 失败。
- [ ] **步骤 3：记录 executable physical span**
  - cache entry 保存 VA span、PA span、`satp` 或 address-space identity、region 属性。
  - 无法翻译完整 span 时标记 entry 需要全局 invalidate。
- [ ] **步骤 4：更新 invalidation 匹配**
  - guest store / payload load / primary image load 优先按 PA span 匹配。
  - `satp`、`sfence.vma`、region 属性变化继续保守 invalidation。
- [ ] **步骤 5：运行验证**
  - 运行：`cd myCPU && make test-host-dbt_runtime_invalidation_smoke test-host-dbt_runtime_harness_smoke`
  - 预期：全部通过。
- [ ] **步骤 6：提交**
  - 运行：`git add myCPU/src/exec/dbt_executable_cache.h myCPU/src/exec/dbt_executable_cache.cpp myCPU/src/exec/dbt_runtime_invalidation.h myCPU/src/exec/dbt_runtime_invalidation.cpp myCPU/tests/host/dbt_runtime_invalidation_smoke.cpp myCPU/tests/host/dbt_runtime_harness_smoke.cpp && git commit -m "fix(dbt): invalidate executable cache by physical span"`

### 任务 14：DBT helper store LR/SC reservation invalidation

**覆盖 finding：** S2

**文件：**
- 修改：`myCPU/src/exec/dbt_helper_execution_bridge.cpp`
- 修改：`myCPU/src/exec/dbt_helper_execution_bridge.h`
- 修改：`myCPU/src/isa/atomic_contract.h`
- 修改：`myCPU/src/isa/atomic_contract.cpp`
- 测试：`myCPU/tests/host/dbt_helper_execution_bridge_smoke.cpp`
- 测试：`myCPU/tests/host/dbt_runtime_harness_smoke.cpp`

- [ ] **步骤 1：补跨页 helper store 红灯**
  - 在 DBT helper smoke 中构造跨页 store，第二页翻译失败或 PA span 不完整。
  - 断言 LR reservation 被清理，后续 SC 不可错误成功。
- [ ] **步骤 2：运行红灯验证**
  - 运行：`cd myCPU && make test-host-dbt_helper_execution_bridge_smoke test-host-dbt_runtime_harness_smoke`
  - 预期：新增跨页 reservation case 失败。
- [ ] **步骤 3：复用 commit-boundary invalidation 合同**
  - 抽出 shared helper，让 functional store、pipeline commit store、DBT helper store 使用同一 reservation overlap 判断。
  - 跨页、PA span 不完整、translation unknown 时保守 `clear_reservation()`。
- [ ] **步骤 4：运行验证**
  - 运行：`cd myCPU && make test-host-dbt_helper_execution_bridge_smoke test-host-dbt_runtime_harness_smoke test-host-atomic_semantics_smoke`
  - 预期：全部通过。
- [ ] **步骤 5：提交**
  - 运行：`git add myCPU/src/exec/dbt_helper_execution_bridge.cpp myCPU/src/exec/dbt_helper_execution_bridge.h myCPU/src/isa/atomic_contract.h myCPU/src/isa/atomic_contract.cpp myCPU/tests/host/dbt_helper_execution_bridge_smoke.cpp myCPU/tests/host/dbt_runtime_harness_smoke.cpp && git commit -m "fix(dbt): clear reservations for uncertain helper stores"`

### 任务 15：pipeline atomic execution profile observation

**覆盖 finding：** S3

**文件：**
- 修改：`myCPU/src/exec/execution_profile.cpp`
- 修改：`myCPU/src/exec/execution_profile.h`
- 修改：`myCPU/src/exec/pipeline_commit_boundary.cpp`
- 测试：`myCPU/tests/host/execution_profile_smoke.cpp`

- [ ] **步骤 1：补 pipeline atomic observation 红灯**
  - 在 `execution_profile_smoke` 中运行 pipeline atomic workload，断言 atomic load / store / SC outcome 进入 memory observations。
- [ ] **步骤 2：运行红灯验证**
  - 运行：`cd myCPU && make test-host-execution_profile_smoke test-pipeline`
  - 预期：新增 pipeline atomic observation case 失败。
- [ ] **步骤 3：抽出 shared atomic observation helper**
  - functional atomic path 与 pipeline commit path 使用同一 observation builder。
  - observation 标记 atomic kind、PA region、fault / success outcome。
- [ ] **步骤 4：运行验证**
  - 运行：`cd myCPU && make test-host-execution_profile_smoke test-pipeline`
  - 预期：全部通过。
- [ ] **步骤 5：提交**
  - 运行：`git add myCPU/src/exec/execution_profile.cpp myCPU/src/exec/execution_profile.h myCPU/src/exec/pipeline_commit_boundary.cpp myCPU/tests/host/execution_profile_smoke.cpp && git commit -m "fix(profile): record pipeline atomic memory observations"`

### 任务 16：debug protocol strict numeric parsing

**覆盖 finding：** S4

**文件：**
- 修改：`myCPU/src/debug/debug_protocol_command.cpp`
- 修改：`myCPU/src/debug/debug_protocol_command.h`
- 测试：`myCPU/tests/host/debug_protocol_command_smoke.cpp`
- 测试：`myCPU/tests/host/debug_cli_smoke.cpp`

- [ ] **步骤 1：补非法数字红灯**
  - 添加 `0x10junk`、`123abc`、`-1`、空字符串、溢出值、带多余空白字段的 debug command 负例。
  - 断言命令被 reject，不能静默取 0 或前缀值。
- [ ] **步骤 2：运行红灯验证**
  - 运行：`cd myCPU && make test-host-debug_protocol_command_smoke test-host-debug_cli_smoke`
  - 预期：新增 strict parse 用例失败。
- [ ] **步骤 3：实现 strict parse helper**
  - `strtoull` 必须检查 `end pointer`、`errno`、base 和范围。
  - JSON string number 与 raw command number 使用同一 helper。
- [ ] **步骤 4：运行验证**
  - 运行：`cd myCPU && make test-host-debug_protocol_command_smoke test-host-debug_cli_smoke`
  - 预期：全部通过。
- [ ] **步骤 5：提交**
  - 运行：`git add myCPU/src/debug/debug_protocol_command.cpp myCPU/src/debug/debug_protocol_command.h myCPU/tests/host/debug_protocol_command_smoke.cpp myCPU/tests/host/debug_cli_smoke.cpp && git commit -m "fix(debug): parse numeric fields strictly"`

### 任务 17：PLIC / MMIO 平台契约文档校准

**覆盖 finding：** S5

**文件：**
- 修改：`docs/design/platform_mmio_contract.md`
- 修改：`docs/status/code_reself_status.md`
- 测试：`myCPU/tests/host/virtio_blk_smoke.cpp`
- 验证：`myCPU/tests/host/xv6_shell_smoke.cpp`

- [ ] **步骤 1：核对代码常量与 DTB**
  - 检查 `myCPU/src/platform/address_map.h`、PLIC source 常量、virtio / storage device wiring 和 DTB 生成路径。
- [ ] **步骤 2：更新文档**
  - 将文档中的 UART source 1 修正为当前代码 / DTB 合同：virtio=1、AI=9、UART=10。
  - 明确 SimpleStorage 与 Virtio transport 的选择关系和 guest-visible 边界。
- [ ] **步骤 3：运行验证**
  - 运行：`cd myCPU && make test-host-virtio_blk_smoke test-host-xv6_shell_smoke`
  - 预期：全部通过。
- [ ] **步骤 4：提交**
  - 运行：`git add docs/design/platform_mmio_contract.md docs/status/code_reself_status.md && git commit -m "docs(platform): align PLIC source contract"`

### 任务 18：README kernel_alpha 能力口径

**覆盖 finding：** S6

**文件：**
- 修改：`README.md`
- 修改：`docs/status/kernel_alpha_status.md`
- 修改：`docs/status/code_reself_status.md`

- [ ] **步骤 1：定位旧口径**
  - 运行：`rg -n "kernel_alpha = KMVPETDS|KMVPETDS" README.md docs/status docs/design myCPU/Makefile`
  - 预期：输出所有仍把旧 marker 写成当前能力的引用。
- [ ] **步骤 2：更新 README**
  - 把旧 `KMVPETDS` 表述改为 Phase 1 guardrail 历史基线。
  - 增加课程 OS stage1 接管线的当前定位，避免与 mainline kernel_alpha 能力混写。
- [ ] **步骤 3：同步专项状态**
  - [../status/kernel_alpha_status.md](../status/kernel_alpha_status.md) 只保留当前状态、历史 marker 和下一步，不堆 checklist。
- [ ] **步骤 4：运行验证**
  - 运行：`rg -n "kernel_alpha = KMVPETDS|KMVPETDS" README.md docs/status docs/design myCPU/Makefile`
  - 预期：只剩历史语境引用，且措辞明确为历史 guardrail。
- [ ] **步骤 5：提交**
  - 运行：`git add README.md docs/status/kernel_alpha_status.md docs/status/code_reself_status.md && git commit -m "docs(kernel): mark KMVPETDS as historical guardrail"`

### 任务 19：kernel_runtime storage signature guardrail 分层

**覆盖 finding：** S7

**文件：**
- 修改：`myCPU/guest/kernel/kernel_runtime.c`
- 修改：`myCPU/guest/include/kernel_runtime.h`
- 修改：`myCPU/guest/kernel_alpha/common.c`
- 修改：`myCPU/guest/kernel_alpha/storage_contract.c`
- 测试：`myCPU/tests/unit/kernel_runtime.c`
- 测试：`myCPU/tests/unit/kernel_alpha_common.c`
- 验证：`myCPU/guest/AGENTS.md`

- [ ] **步骤 1：补通用 runtime 不依赖 `'Stor'` 红灯**
  - 在 `test-unit-kernel_runtime` 中增加不使用 demo storage signature 的 runtime bring-up helper case。
  - 在 `test-unit-kernel_alpha_common` 中保留 kernel_alpha `'Stor'` guardrail case。
- [ ] **步骤 2：运行红灯验证**
  - 运行：`cd myCPU && make test-unit-kernel_runtime test-unit-kernel_alpha_common`
  - 预期：新增分层 case 失败或暴露命名不清。
- [ ] **步骤 3：移动或重命名 guardrail**
  - 把 `'Stor'` signature 校验移入 kernel_alpha / demo 专用 helper。
  - 如果保留在 runtime 层，函数名必须显式包含 `demo_storage_signature_guardrail`。
- [ ] **步骤 4：运行验证**
  - 运行：`cd myCPU && make test-unit-kernel_runtime test-unit-kernel_alpha_common test-guest-kernel_alpha_demo test-guest-supervisor_demo`
  - 预期：全部通过。
- [ ] **步骤 5：提交**
  - 运行：`git add myCPU/guest/kernel/kernel_runtime.c myCPU/guest/include/kernel_runtime.h myCPU/guest/kernel_alpha/common.c myCPU/guest/kernel_alpha/storage_contract.c myCPU/tests/unit/kernel_runtime.c myCPU/tests/unit/kernel_alpha_common.c && git commit -m "refactor(guest): isolate kernel alpha storage signature guardrail"`

### 任务 20：VirtQueue avail consumption 边界

**覆盖 finding：** S8

**文件：**
- 修改：`myCPU/src/devices/virtqueue.cpp`
- 修改：`myCPU/src/devices/virtqueue.h`
- 修改：`myCPU/src/devices/virtio_blk.cpp`
- 测试：`myCPU/tests/host/virtio_blk_smoke.cpp`
- 测试：`myCPU/tests/unit/virtqueue_smoke.cpp`

- [ ] **步骤 1：补 bad descriptor / DMA fault 红灯**
  - 添加 descriptor chain 无效、DMA read fault、used ring writeback fault case。
  - 断言失败路径写 `VIRTIO_BLK_S_IOERR` 与 used entry，或者不推进 avail index。
- [ ] **步骤 2：运行红灯验证**
  - 运行：`cd myCPU && make test-host-virtio_blk_smoke test-unit-virtqueue_smoke`
  - 预期：新增 fail-path case 失败。
- [ ] **步骤 3：延后 avail index commit**
  - 只有 status / used writeback 成功后推进 consumed avail index。
  - 对可恢复失败尽量写 IOERR；无法写 used 时保持 queue 可观察错误状态。
- [ ] **步骤 4：运行验证**
  - 运行：`cd myCPU && make test-host-virtio_blk_smoke test-unit-virtqueue_smoke`
  - 预期：全部通过。
- [ ] **步骤 5：提交**
  - 运行：`git add myCPU/src/devices/virtqueue.cpp myCPU/src/devices/virtqueue.h myCPU/src/devices/virtio_blk.cpp myCPU/tests/host/virtio_blk_smoke.cpp myCPU/tests/unit/virtqueue_smoke.cpp && git commit -m "fix(virtio): commit avail entries after status writeback"`

### 任务 21：AI graph package reserved 字段 fail-closed

**覆盖 finding：** S9

**文件：**
- 修改：`myCPU/src/devices/ai_graph_package.cpp`
- 修改：`myCPU/src/devices/ai_graph_package.h`
- 测试：`myCPU/tests/unit/ai_graph_package.cpp`

- [ ] **步骤 1：补 reserved 非零红灯**
  - 为 tensor、op、memory-plan、dynamic-tensor record 增加 reserved 字段非零负例。
  - 断言 parser / validator 返回 reject，错误信息包含 record kind。
- [ ] **步骤 2：运行红灯验证**
  - 运行：`cd myCPU && make test-unit-ai_graph_package`
  - 预期：新增 reserved 负例失败。
- [ ] **步骤 3：统一 reserved 检查**
  - 所有 record decoder 对 reserved 非零 fail-closed。
  - validator 不接受 decoder 遗漏的 reserved 非零状态。
- [ ] **步骤 4：运行验证**
  - 运行：`cd myCPU && make test-unit-ai_graph_package`
  - 预期：全部通过。
- [ ] **步骤 5：提交**
  - 运行：`git add myCPU/src/devices/ai_graph_package.cpp myCPU/src/devices/ai_graph_package.h myCPU/tests/unit/ai_graph_package.cpp && git commit -m "fix(ai): reject nonzero graph package reserved fields"`

### 任务 22：Guest C ABI runtime shape 字段命名

**覆盖 finding：** S10

**文件：**
- 修改：`myCPU/guest/include/ai_accel.h`
- 修改：`myCPU/guest/kernel/ai_accel.c`
- 修改：`myCPU/tests/unit/ai_accel_queue.c`
- 测试：`myCPU/tests/host/ai_accel_guest_smoke.cpp`

- [ ] **步骤 1：补 ABI size / field intent 断言**
  - 在 guest queue unit 中断言 descriptor ABI size 不变。
  - 增加 `runtime_shape_table_offset` 被写入并传到 host summary 的 case。
- [ ] **步骤 2：运行红灯验证**
  - 运行：`cd myCPU && make test-unit-ai_accel_queue test-host-ai_accel_guest_smoke`
  - 预期：新增命名 case 编译失败或断言失败。
- [ ] **步骤 3：字段重命名**
  - 将 guest C ABI 的 `reserved0` 改名为 `runtime_shape_table_offset`。
  - 保留 padding / ABI size assert，避免结构布局变化。
- [ ] **步骤 4：运行验证**
  - 运行：`cd myCPU && make test-unit-ai_accel_queue test-host-ai_accel_guest_smoke`
  - 预期：全部通过。
- [ ] **步骤 5：提交**
  - 运行：`git add myCPU/guest/include/ai_accel.h myCPU/guest/kernel/ai_accel.c myCPU/tests/unit/ai_accel_queue.c myCPU/tests/host/ai_accel_guest_smoke.cpp && git commit -m "refactor(ai): name runtime shape table offset in guest ABI"`

### 任务 23：AI profile schema version

**覆盖 finding：** S11

**文件：**
- 修改：`myCPU/src/devices/ai_accelerator.h`
- 修改：`myCPU/src/devices/ai_accelerator.cpp`
- 修改：`myCPU/src/platform/machine.cpp`
- 测试：`myCPU/tests/host/ai_accelerator_profile_smoke.cpp`
- 文档：`docs/design/post_wave7_ai_user_tasks_npu_performance_design.md`
- 状态：`docs/status/npu_tpu_accelerator_status.md`

- [ ] **步骤 1：补 schema tag 红灯**
  - profile smoke 断言 structured summary 含 `profile_schema_version` 与 `timing_schema_version`。
  - CLI 文本至少输出 `schema=ai_profile_v1`。
- [ ] **步骤 2：运行红灯验证**
  - 运行：`cd myCPU && make test-host-ai_accelerator_profile_smoke`
  - 预期：新增 schema version 断言失败。
- [ ] **步骤 3：实现版本字段**
  - `AiProfileSummary` 增加固定版本字段。
  - CLI / manifest runner 输出稳定 schema tag。
- [ ] **步骤 4：同步设计与状态**
  - 设计文档写清 v1 schema 边界和不兼容变更规则。
  - 状态文档只保留当前 schema 结果摘要。
- [ ] **步骤 5：运行验证**
  - 运行：`cd myCPU && make test-host-ai_accelerator_profile_smoke`
  - 预期：全部通过。
- [ ] **步骤 6：提交**
  - 运行：`git add myCPU/src/devices/ai_accelerator.h myCPU/src/devices/ai_accelerator.cpp myCPU/src/platform/machine.cpp myCPU/tests/host/ai_accelerator_profile_smoke.cpp docs/design/post_wave7_ai_user_tasks_npu_performance_design.md docs/status/npu_tpu_accelerator_status.md && git commit -m "feat(ai): version profile schema output"`

### 任务 24：AI manifest expected_output 合同

**覆盖 finding：** S12

**文件：**
- 修改：`myCPU/src/platform/machine.cpp`
- 修改：`myCPU/src/platform/machine.h`
- 测试：`myCPU/tests/host/ai_accelerator_profile_smoke.cpp`
- 文档：`docs/design/post_wave7_ai_user_tasks_npu_performance_design.md`

- [ ] **步骤 1：选择合同**
  - 若 profile runner 需要 correctness gate：保留 `expected_output` 并按 output 顺序比对。
  - 若 profile runner 只负责 profiling：移除 manifest schema 字段并拒绝配置中出现该字段。
- [ ] **步骤 2：补红灯**
  - 保留字段路线：添加 expected output mismatch 负例，断言 runner fail-closed。
  - 移除字段路线：添加 manifest 包含 `expected_output` 的负例，断言 parser reject。
- [ ] **步骤 3：运行红灯验证**
  - 运行：`cd myCPU && make test-host-ai_accelerator_profile_smoke`
  - 预期：新增 expected_output 合同用例失败。
- [ ] **步骤 4：实现选择的合同**
  - 只保留一套事实来源：parser schema、runner 行为和设计文档必须一致。
- [ ] **步骤 5：运行验证**
  - 运行：`cd myCPU && make test-host-ai_accelerator_profile_smoke`
  - 预期：全部通过。
- [ ] **步骤 6：提交**
  - 运行：`git add myCPU/src/platform/machine.cpp myCPU/src/platform/machine.h myCPU/tests/host/ai_accelerator_profile_smoke.cpp docs/design/post_wave7_ai_user_tasks_npu_performance_design.md && git commit -m "fix(ai): make expected output manifest contract explicit"`

### 任务 25：Sv39 reserved PTE bits fail-closed

**覆盖 finding：** S13

**文件：**
- 修改：`myCPU/src/mem/address_space.cpp`
- 修改：`myCPU/src/mem/address_space.h`
- 测试：`myCPU/tests/asm/sv39_pagewalk_contracts.S`

- [ ] **步骤 1：补 reserved-bit asm 红灯**
  - 添加 leaf PTE 高位 reserved 非零 case。
  - 添加 non-leaf PTE reserved bit 非零 case。
  - 断言 fault cause / tval 与 page fault 合同一致。
- [ ] **步骤 2：运行红灯验证**
  - 运行：`cd myCPU && make test-sv39_pagewalk_contracts test-pipeline-sv39_pagewalk_contracts`
  - 预期：新增 reserved-bit case 失败。
- [ ] **步骤 3：实现 reserved mask**
  - 当前 Sv39 模型定义 leaf / non-leaf reserved mask。
  - page-walk 在 A/D 回写和 leaf 使用前检查 reserved bits。
- [ ] **步骤 4：运行验证**
  - 运行：`cd myCPU && make test-sv39_pagewalk_contracts test-pipeline-sv39_pagewalk_contracts test`
  - 预期：全部通过。
- [ ] **步骤 5：提交**
  - 运行：`git add myCPU/src/mem/address_space.cpp myCPU/src/mem/address_space.h myCPU/tests/asm/sv39_pagewalk_contracts.S && git commit -m "fix(sv39): reject reserved PTE bits"`

### 任务 26：mstatus.MPP reserved value

**覆盖 finding：** S14

**文件：**
- 修改：`myCPU/src/arch/csr_file.cpp`
- 修改：`myCPU/src/arch/csr_file.h`
- 修改：`myCPU/src/trap.cpp`
- 测试：`myCPU/tests/asm/privilege_transitions.S`
- 测试：`myCPU/tests/asm/trap_state.S`

- [ ] **步骤 1：补 MPP=2 红灯**
  - 在 `privilege_transitions.S` 写入 `mstatus.MPP=2` 后执行 `mret`。
  - 断言不会被解码成 M-mode；选择规整为 U/S/M 合法值或触发 illegal / trap 的合同必须固定。
- [ ] **步骤 2：运行红灯验证**
  - 运行：`cd myCPU && make test-privilege_transitions test-trap_state`
  - 预期：新增 reserved MPP case 失败。
- [ ] **步骤 3：规整 CSR 写入或 mret 解码**
  - 推荐在 `mstatus` 写入路径规整 MPP 到合法值，避免状态里长期保存 reserved encoding。
  - `mret` 前保留 defensive check，reserved MPP 不得默认为 M-mode。
- [ ] **步骤 4：运行验证**
  - 运行：`cd myCPU && make test-privilege_transitions test-trap_state test`
  - 预期：全部通过。
- [ ] **步骤 5：提交**
  - 运行：`git add myCPU/src/arch/csr_file.cpp myCPU/src/arch/csr_file.h myCPU/src/trap.cpp myCPU/tests/asm/privilege_transitions.S myCPU/tests/asm/trap_state.S && git commit -m "fix(privilege): reject reserved mstatus MPP values"`

### 任务 27：debug bus observation source / kind

**覆盖 finding：** S15

**文件：**
- 修改：`myCPU/src/exec/execution_profile.h`
- 修改：`myCPU/src/exec/execution_profile.cpp`
- 修改：`myCPU/src/mem/bus.h`
- 修改：`myCPU/src/mem/bus.cpp`
- 修改：`myCPU/src/debug/debug_snapshot.h`
- 修改：`myCPU/src/debug/debug_protocol_response.cpp`
- 测试：`myCPU/tests/host/debug_cli_smoke.cpp`
- 测试：`myCPU/tests/host/execution_profile_smoke.cpp`

- [ ] **步骤 1：补单槽覆盖红灯**
  - 构造同一 step 内有 internal fetch / page-walk 与 guest data / MMIO commit 的 case。
  - 断言 debug snapshot 能区分 source / kind，guest-visible commit observation 不被内部访问覆盖。
- [ ] **步骤 2：运行红灯验证**
  - 运行：`cd myCPU && make test-host-debug_cli_smoke test-host-execution_profile_smoke`
  - 预期：新增 observation source case 失败。
- [ ] **步骤 3：增加 observation source / kind**
  - observation 记录 `source`：fetch、page-walk、guest-data、MMIO-commit、DMA 等。
  - guest data / MMIO commit 至少有独立槽或可过滤列表。
- [ ] **步骤 4：更新 debug CLI 输出**
  - debug snapshot JSON 保持向后兼容字段，同时增加 source / kind。
- [ ] **步骤 5：运行验证**
  - 运行：`cd myCPU && make test-host-debug_cli_smoke test-host-execution_profile_smoke`
  - 预期：全部通过。
- [ ] **步骤 6：提交**
  - 运行：`git add myCPU/src/exec/execution_profile.h myCPU/src/exec/execution_profile.cpp myCPU/src/mem/bus.h myCPU/src/mem/bus.cpp myCPU/src/debug/debug_snapshot.h myCPU/src/debug/debug_protocol_response.cpp myCPU/tests/host/debug_cli_smoke.cpp myCPU/tests/host/execution_profile_smoke.cpp && git commit -m "feat(debug): tag memory observations by source"`

### 任务 28：测试矩阵分层入口

**覆盖 finding：** S16

**文件：**
- 修改：`myCPU/Makefile`
- 修改：`README.md`
- 修改：`docs/design/regression_completion_criteria.md`
- 修改：`docs/status/mainline_status.md`
- 修改：`docs/index.md`

- [ ] **步骤 1：定义分层 target**
  - `test-fast-smoke`：host unit / host smoke 中无 guest 长链和无外部资产的快速门禁。
  - `test-standard-regression`：当前常用 `make test` + 关键 pipeline guardrail。
  - `test-slow-guest`：guest demos、pipeline guest demos、xv6 shell 类慢门禁。
  - `test-opt-in-external`：真实 Linux Image / distro rootfs / external asset targets。
- [ ] **步骤 2：补 dry-run 红灯**
  - 添加 `run_debug_cli_probe_test.py` 或 Makefile self-test，断言 `make -n` 能展开四个 target。
- [ ] **步骤 3：运行红灯验证**
  - 运行：`cd myCPU && make -n test-fast-smoke test-standard-regression test-slow-guest test-opt-in-external`
  - 预期：当前 target 不存在或展开不完整。
- [ ] **步骤 4：实现 Makefile aliases**
  - aliases 只编排现有 target，不引入新的生产代码行为。
  - external target 必须保留 env opt-in 和 fail-closed guard。
- [ ] **步骤 5：同步文档**
  - README 给出开发者选择建议。
  - regression completion criteria 记录分层语义。
  - mainline status 只记录当前门禁状态摘要。
- [ ] **步骤 6：运行验证**
  - 运行：`cd myCPU && make -n test-fast-smoke test-standard-regression test-slow-guest test-opt-in-external`
  - 运行：`cd myCPU && make test-fast-smoke`
  - 预期：dry-run 全部可展开，fast smoke 通过。
- [ ] **步骤 7：提交**
  - 运行：`git add myCPU/Makefile README.md docs/design/regression_completion_criteria.md docs/status/mainline_status.md docs/index.md && git commit -m "build(test): add layered regression targets"`

### 任务 29：shared memory shape / FP descriptor

**覆盖 finding：** S17

**文件：**
- 修改：`myCPU/src/isa/instruction_semantics.h`
- 修改：`myCPU/src/isa/instruction_semantics.cpp`
- 修改：`myCPU/src/isa/effects.h`
- 修改：`myCPU/src/exec/pipeline_backend_execute.cpp`
- 修改：`myCPU/src/exec/load_store_queue.cpp`
- 修改：`myCPU/src/exec/floating_ops.cpp`
- 测试：`myCPU/tests/host/pipeline_backend_smoke.cpp`
- 测试：`myCPU/tests/host/backend_differential_smoke.cpp`

- [ ] **步骤 1：补 metadata consistency 红灯**
  - 添加 pipeline 用例，覆盖 memory width / sign / atomic shape 与 FP source-dest descriptor。
  - 断言 pipeline 消费 shared descriptor，而不是复刻 opcode 分类。
- [ ] **步骤 2：运行红灯验证**
  - 运行：`cd myCPU && make test-host-pipeline_backend_smoke test-host-backend_differential_smoke test-pipeline`
  - 预期：新增 consistency case 暴露重复分类或失败。
- [ ] **步骤 3：抽出共享只读描述**
  - 在 shared semantics 层提供 memory shape descriptor 和 FP operand descriptor。
  - descriptor 不提交状态，只描述指令事实。
- [ ] **步骤 4：pipeline 改为消费 descriptor**
  - LSQ、execute、FP metadata 移除私有 opcode truth table。
- [ ] **步骤 5：运行验证**
  - 运行：`cd myCPU && make test-host-pipeline_backend_smoke test-host-backend_differential_smoke test-pipeline`
  - 预期：全部通过。
- [ ] **步骤 6：提交**
  - 运行：`git add myCPU/src/isa/instruction_semantics.h myCPU/src/isa/instruction_semantics.cpp myCPU/src/isa/effects.h myCPU/src/exec/pipeline_backend_execute.cpp myCPU/src/exec/load_store_queue.cpp myCPU/src/exec/floating_ops.cpp myCPU/tests/host/pipeline_backend_smoke.cpp myCPU/tests/host/backend_differential_smoke.cpp && git commit -m "refactor(pipeline): consume shared instruction metadata"`

### 任务 30：DBT retired count 语义

**覆盖 finding：** S18

**文件：**
- 修改：`myCPU/src/exec/dbt_host_emitter.h`
- 修改：`myCPU/src/exec/dbt_host_emitter.cpp`
- 修改：`myCPU/src/exec/dbt_runtime_harness.h`
- 修改：`myCPU/src/exec/dbt_runtime_harness.cpp`
- 测试：`myCPU/tests/host/dbt_runtime_harness_smoke.cpp`
- 测试：`myCPU/tests/host/dbt_host_emitter_smoke.cpp`

- [ ] **步骤 1：补 host executed count 红灯**
  - 在 runtime harness smoke 中执行 host block，断言返回 `host_executed_count` 或字段名明确为 `ir_expected_retired_count`。
- [ ] **步骤 2：运行红灯验证**
  - 运行：`cd myCPU && make test-host-dbt_runtime_harness_smoke test-host-dbt_host_emitter_smoke`
  - 预期：新增 retired count contract 失败。
- [ ] **步骤 3：选择实现路径**
  - 若 emitter 可返回实测数：host ABI 增加 executed / retired count 输出。
  - 若仍为 dry-run guardrail：字段重命名为 `ir_expected_retired_count`，所有 summary / stats 同步命名。
- [ ] **步骤 4：运行验证**
  - 运行：`cd myCPU && make test-host-dbt_runtime_harness_smoke test-host-dbt_host_emitter_smoke`
  - 预期：全部通过。
- [ ] **步骤 5：提交**
  - 运行：`git add myCPU/src/exec/dbt_host_emitter.h myCPU/src/exec/dbt_host_emitter.cpp myCPU/src/exec/dbt_runtime_harness.h myCPU/src/exec/dbt_runtime_harness.cpp myCPU/tests/host/dbt_runtime_harness_smoke.cpp myCPU/tests/host/dbt_host_emitter_smoke.cpp && git commit -m "fix(dbt): make retired count semantics explicit"`

### 任务 31：DBT cache bounded lookup

**覆盖 finding：** S19

**文件：**
- 修改：`myCPU/src/exec/dbt_block_cache.h`
- 修改：`myCPU/src/exec/dbt_block_cache.cpp`
- 修改：`myCPU/src/exec/dbt_executable_cache.h`
- 修改：`myCPU/src/exec/dbt_executable_cache.cpp`
- 修改：`myCPU/src/exec/dbt_jit_engine.cpp`
- 测试：`myCPU/tests/host/dbt_executable_cache_smoke.cpp`
- 测试：`myCPU/tests/host/dbt_jit_engine_smoke.cpp`
- 测试：`myCPU/tests/host/dbt_runtime_harness_smoke.cpp`

- [ ] **步骤 1：决定 cache 边界**
  - smoke-only 路线：设置明确 max entry / cap，超过上限 fail-closed 或 evict oldest。
  - runtime 演进路线：改用 keyed map + LRU / 上限。
- [ ] **步骤 2：补 cap / eviction 红灯**
  - 添加超过上限的 metadata cache 与 executable cache case。
  - 断言 lookup 不退化成无界增长，stats 暴露 eviction / cap hit。
- [ ] **步骤 3：运行红灯验证**
  - 运行：`cd myCPU && make test-host-dbt_executable_cache_smoke test-host-dbt_jit_engine_smoke test-host-dbt_runtime_harness_smoke`
  - 预期：新增 cap case 失败。
- [ ] **步骤 4：实现 bounded cache**
  - 使用 exact key：PC range、satp / address-space identity、region attributes。
  - 插入、lookup、invalidation 维护 O(1) 或受控 O(log n) 查询；smoke-only cap 也必须显式记录上限。
- [ ] **步骤 5：运行验证**
  - 运行：`cd myCPU && make test-host-dbt_executable_cache_smoke test-host-dbt_jit_engine_smoke test-host-dbt_runtime_harness_smoke`
  - 预期：全部通过。
- [ ] **步骤 6：提交**
  - 运行：`git add myCPU/src/exec/dbt_block_cache.h myCPU/src/exec/dbt_block_cache.cpp myCPU/src/exec/dbt_executable_cache.h myCPU/src/exec/dbt_executable_cache.cpp myCPU/src/exec/dbt_jit_engine.cpp myCPU/tests/host/dbt_executable_cache_smoke.cpp myCPU/tests/host/dbt_jit_engine_smoke.cpp myCPU/tests/host/dbt_runtime_harness_smoke.cpp && git commit -m "refactor(dbt): bound metadata and executable caches"`

### 任务 32：整改关闭与归档

**覆盖 finding：** 全部 M1-M12、S1-S19

**文件：**
- 修改：`docs/status/code_reself_status.md`
- 修改：`docs/status/mainline_status.md`
- 修改：`docs/status/linux_distribution_platform_status.md`
- 修改：`docs/status/npu_tpu_accelerator_status.md`
- 修改：`docs/plan/history_plan.md`
- 删除：`docs/plan/code_reself_remediation_plan.md`

- [ ] **步骤 1：运行分层验证**
  - 运行：`cd myCPU && make test-fast-smoke`
  - 运行：`cd myCPU && make test-standard-regression`
  - 运行：`cd myCPU && make test-pipeline`
  - 运行：`cd frontend && node --test`
  - 若外部资产存在，运行：`cd myCPU && make test-opt-in-external`
  - 预期：全部通过；外部资产缺失时 opt-in target 必须 fail-closed 或明确跳过。
- [ ] **步骤 2：运行全量基线**
  - 运行：`cd myCPU && make test`
  - 预期：全部通过。
- [ ] **步骤 3：回写状态**
  - [../status/code_reself_status.md](../status/code_reself_status.md) 将已关闭 findings 移出 active 列表或改成简短关闭结论。
  - [../status/mainline_status.md](../status/mainline_status.md) 只记录整改对主线门禁的当前状态摘要。
  - Linux / AI 专项状态只记录各自已完成结果和仍有效风险。
- [ ] **步骤 4：归档计划**
  - 在 `docs/plan/history_plan.md` 追加完成时间、完成内容、过程摘要和关键验证命令。
  - 删除 `docs/plan/code_reself_remediation_plan.md`。
- [ ] **步骤 5：文档检查**
  - 运行：`git diff --check`
  - 运行：`rg -n "当前无活跃计划|code_reself_remediation_plan" docs/status docs/index.md docs/plan`
  - 预期：没有过期活跃计划引用；归档后只保留 history 链接。
- [ ] **步骤 6：最终提交**
  - 运行：`git add docs/status/code_reself_status.md docs/status/mainline_status.md docs/status/linux_distribution_platform_status.md docs/status/npu_tpu_accelerator_status.md docs/plan/history_plan.md docs/index.md && git add -u docs/plan/code_reself_remediation_plan.md && git commit -m "docs(review): close code review remediation plan"`

## 并行执行建议

- 可并行：任务 1、2、3、4、5 分属不同 core 边界，但任务 1 / 5 都改 pipeline common types，不能同一批无协调合并。
- 可并行：任务 6、7、8、9 分属 probe、frontend render、reset runtime、deploy security，但任务 7 / 8 都改 frontend runtime tests，合并前需统一 Node test。
- 可并行：任务 10、11、12、21、23、24 都在 AI 方向；推荐分成 `manifest/profile` 与 `graph package/guest ABI` 两条 agent 线，最后由 AI owner 合并。
- 可并行：任务 13、14、30、31 都在 DBT；它们共享 runtime harness 与 executable cache tests，必须串行集成。
- 串行：任务 32 必须等所有 active findings 关闭后执行。

## 验证总线

- Core / ISA / MMU：`cd myCPU && make test test-pipeline`
- Pipeline smoke：`cd myCPU && make test-host-pipeline_backend_smoke test-host-backend_differential_smoke test-host-pipeline_speculation_contracts_smoke test-pipeline`
- DBT：`cd myCPU && make test-host-dbt_runtime_invalidation_smoke test-host-dbt_helper_execution_bridge_smoke test-host-dbt_runtime_harness_smoke test-host-dbt_executable_cache_smoke test-host-dbt_jit_engine_smoke`
- Linux / debug / workload：`cd myCPU && make test-host-run_debug_cli_probe test-host-debug_cli_smoke test-host-debug_protocol_command_smoke test-host-virtio_blk_smoke test-host-xv6_shell_smoke`
- AI：`cd myCPU && make test-unit-ai_graph_package test-unit-ai_accel_queue test-host-ai_accelerator_profile_smoke test-host-ai_accelerator_gemm_smoke test-host-ai_accel_guest_smoke`
- Guest runtime：`cd myCPU && make test-unit-kernel_runtime test-unit-kernel_alpha_common test-guest-kernel_alpha_demo test-guest-supervisor_demo`
- Frontend：`cd frontend && node --test`
- 文档：`git diff --check`

## 完成定义

- 12 条 `必须修复` 均有红灯、实现修复和对应验证记录。
- 19 条 `建议修改` 均已完成、降级为明确非目标并写入状态，或拆成新的已批准专项计划；不能静默遗留。
- [../status/code_reself_status.md](../status/code_reself_status.md) 不再保留已关闭 finding 的完整执行流水账，只保留关闭结论、关键验证和仍有效风险。
- 所有新增 / 修改正式文档已同步 `docs/index.md`。
- 完成后本计划按 `docs/AGENTS.md` 归档到 `docs/plan/history_plan.md` 并删除原计划文件。

## 完成态回写要求

- 全部 checklist 必须勾完。
- [../status/code_reself_status.md](../status/code_reself_status.md) 必须增加：
  - 完成结果摘要
  - 已关闭 finding 列表
  - 仍然有效的剩余风险
  - 执行过的关键验证命令
- [../status/mainline_status.md](../status/mainline_status.md) 只回写主线门禁状态，不复制完整 finding 列表。
- [../status/linux_distribution_platform_status.md](../status/linux_distribution_platform_status.md) 只回写 Linux / frontend / deploy 相关关闭结论。
- [../status/npu_tpu_accelerator_status.md](../status/npu_tpu_accelerator_status.md) 只回写 AI manifest / profile / graph package 相关关闭结论。
- 把“完成时间 + 完成内容 + 过程摘要 + 关键验证命令”追加到 [history_plan.md](history_plan.md)。
- 归档完成后删除本计划文件，不长期保留完成态 checklist。
