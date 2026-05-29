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
  - 本轮未修改生产代码，也未运行完整回归矩阵；所有 finding 均保留建议验证命令。完成态文档检查覆盖 `git diff --check`。
- 当前存在以下 active findings。

### 必须修复

1. `pipeline` 对 RVC 压缩指令仍按 32-bit 指令推进。
   - 影响范围：guest-visible behavior / pipeline reference divergence。
   - 证据：`myCPU/src/exec/pipeline_backend_frontend.cpp` 的 `PipelineBackend::step_if` 仍直接走 32-bit fetch / `pc + 4` fallthrough；`myCPU/src/exec/pipeline_backend_cycle.cpp` 的 commit 路径也按 `rob_head->pc + 4` 推进，而 functional path 和 `rvc_semantics_smoke` 已支持 `insn.size == 2`。
   - 风险：压缩指令 workload 在 `pipeline` backend 下会跳过半字或取错包，functional / pipeline 对同一指令产生不同 PC、link register 和 fetch fault 行为。
   - 建议动作：让 pipeline fetch 先取 16-bit、必要时再取 32-bit，把 `insn.size` 带进 stage / ROB / commit / predictor fallthrough，并补 pipeline RVC 或 backend differential 覆盖。
   - 建议验证：`cd myCPU && make test-host-rvc_semantics_smoke test-host-pipeline_backend_smoke test-host-backend_differential_smoke test-pipeline`。
2. RMM rounding mode 在 FP 算术路径被当成非法指令。
   - 影响范围：guest-visible F/D semantics。
   - 证据：`myCPU/src/exec/floating_ops.cpp` 中 `resolve_rounding_mode()` 接受 `FCSR_FRM_RMM == 4`，但 binary / unary / ternary FP helpers 随后对 `resolved_rm == FCSR_FRM_RMM` 直接失败，最终转成 illegal instruction。
   - 风险：合法 `rm=100` 或 `frm=RMM` 的 `fadd/fmul/fdiv/fsqrt/fmadd/fcvt.s.d` 等路径会 trap，FCSR rounding mode 在不同 FP opcode 类之间不一致。
   - 建议动作：为 FP 算术 / 转换统一实现 RMM，或引入明确的 softfloat / rounding helper，并补 shared semantics 与 pipeline 的 RMM 算术用例。
   - 建议验证：`cd myCPU && make test-host-instruction_semantics_smoke test-host-pipeline_backend_smoke`。
3. Sv39 misaligned superpage fault 前会先写 A/D 位。
   - 影响范围：guest-visible Sv39 page-walk side effect。
   - 证据：`myCPU/src/mem/address_space.cpp` 的 `AddressSpace::walk_page_table()` 在 leaf PTE 上先执行 `update_pte_access_bits()`，之后才检查 level 1/2 superpage PPN 对齐；`myCPU/tests/asm/sv39_pagewalk_contracts.S` 只断言 fault cause / tval，没有断言故障 PTE 未被改写。
   - 风险：应当 page fault 的非法 leaf 会留下 A/D 位副作用，page-fault handler 或后续替换策略可能看到错误访问痕迹。
   - 建议动作：把 superpage 对齐检查移到 A/D 回写之前，并补 fault 后 PTE 原值不变的 asm 回归。
   - 建议验证：`cd myCPU && make test-sv39_pagewalk_contracts test-pipeline-sv39_pagewalk_contracts`。
4. image reload / payload load 未清 LR/SC reservation。
   - 影响范围：guest-visible atomic semantics / loader lifecycle。
   - 证据：`myCPU/src/cpu.cpp` 的 `cpu_init()` reset core / CSR / TLB / L1D，但不调用 `trap().clear_reservation()`；`myCPU/src/platform/machine.cpp` 的 `load_binary_payload()` 写 RAM 后只 invalidates L1D。
   - 风险：旧 guest 的 LR reservation 可能穿过 primary image reload 或 host payload 写入，导致后续 SC 错误成功。
   - 建议动作：在 `cpu_init()` 清 reservation；payload load 后保守清 reservation，或按写入物理范围 invalidate。
   - 建议验证：`cd myCPU && make test-unit-machine_loader_reset test-host-atomic_semantics_smoke`。
5. `pipeline` / LSQ 在分页后仍用虚拟地址判断 RAM / MMIO。
   - 影响范围：pipeline correctness / memory side-effect boundary。
   - 证据：`myCPU/src/exec/pipeline_backend_execute.cpp` 的 `is_ram_access()` / `needs_memory_issue_delay()` 和 `myCPU/src/exec/load_store_queue.cpp` 的 `is_ram_range()` 直接把 `effects.mem.addr` / LSQ addr 传给 `bus.describe_region()`；真实访问路径在 `AddressSpace::access_result()` 翻译后才访问 PA。
   - 风险：Sv39 / MPRV 下 VA 可映射到不同 PA；VA 看似 RAM 但 PA 是 MMIO 时可能提前执行 MMIO load，VA 看似非 RAM 但 PA 是 RAM 时可能错误 stall 或禁用 forwarding。
   - 建议动作：LSQ / StageSlot 携带翻译后的 PA 和 region；unknown / cross-page / fault 一律不可 forwarding 且按 non-speculative 处理。
   - 建议验证：`cd myCPU && make test-host-pipeline_speculation_contracts_smoke test-pipeline`，并新增 Sv39 VA->MMIO、VA->RAM alias smoke。
6. 标准发行版 rootfs fail-closed 可被相对路径或 symlink 绕过。
   - 影响范围：opt-in external rootfs gate。
   - 证据：`myCPU/tests/host/run_debug_cli_probe_test.py` 的 Linux distro runtime 测试只用 `disk_path == DEFAULT_LINUX_DISTRO_RUNTIME_ROOTFS` 比较；默认路径是绝对 `Path`，`MYCPU_LINUX_DISTRO_RUNTIME_ROOTFS=workloads/linux_proto/rootfs.ext4` 或指向它的 symlink 不会相等。`myCPU/Makefile` 对 distro runtime targets 主要检查 env 非空。
   - 风险：repo 内 `linux_proto/rootfs.ext4` 可能被误当作标准发行版外部 rootfs 证据，破坏标准发行版主线的 fail-closed 约定。
   - 建议动作：对 rootfs 路径做 `resolve(strict=True)` / `samefile` 级别拒绝，并覆盖 curated Alpine / Debian 各入口；补相对路径和 symlink 负向测试。
   - 建议验证：`cd myCPU && make test-host-run_debug_cli_probe`；再用相对 repo rootfs 路径运行 distro runtime target，预期应 fail-closed。
7. Linux topic 绕过 manifest gating 暴露 live 控件。
   - 影响范围：frontend runtime behavior / support claim。
   - 证据：`frontend/server/tests_manifest.mjs` 在缺 Image 时不会暴露 `linux_proto_console`，`frontend/tests/debug_server.test.mjs` 也固定这一点；但 `frontend/app/render.js` 的 `resolveDemoState()` 把任意 `scenarioKey` 当作 `available`，后续会渲染 `Topic ready`、`Sync session`、`Load current scenario`、`Open live shell`。
   - 风险：缺 runtime Image 时，catalog 仍给出可运行暗示；`Load current scenario` 可能加载上一个真实 workload，造成 Linux topic 与实际 session 不一致。
   - 建议动作：拆开“topic 可读”和“runtime 可加载”；只有 `scenarioTest` 存在于 `/api/tests` 时才启用 Sync / Load / Open live。
   - 建议验证：`cd frontend && node --test`。
8. Linux reset re-arm 会重复持久化 payload / GPR post-load action。
   - 影响范围：debug protocol / server boundary。
   - 证据：`myCPU/src/debug/debug_session.cpp` 的 `reset()` 通过 `recreate_machine()` 已重放 `config_.post_load_actions`；`frontend/server/debug_server_runtime.mjs` 的 `rearmSessionStateAfterReset()` 在 reset 后又调用 `loadPayload()` / `setGpr()`，这些 C++ 方法会继续 append 到同一个 action 列表。
   - 风险：同一 session 每次 reset 都增加一组 payload / GPR action；Linux Image / DTB 会被重复加载，reset 越跑越慢，最终可能表现为 timeout 或卡顿。
   - 建议动作：明确 re-arm 单一归属；优先让 C++ reset 负责重放 payload / GPR，Node 只等待 boot marker 和读取 UART。
   - 建议验证：`cd frontend && node --test`；`cd myCPU && make test-host-debug_cli_smoke`。
9. 远端部署样例默认关闭认证但公开代理 mutating API。
   - 影响范围：debug protocol / server security boundary。
   - 证据：`deploy/env/mycpu-frontend.env.example` 默认 `MYCPU_AUTH_ENABLED=0`；`deploy/nginx/mycpu.conf` 公开代理 `/api/` 和 `/ws`；`frontend/server/debug_server.mjs` 在 auth disabled 时仍允许 Load / Run / Reset / Terminate / terminal input 等 mutating API。
   - 风险：照 runbook 复制 env 后，公网入口可直接控制单 session debug server。
   - 建议动作：远端模板 fail-closed：要求显式生成 hash 并启用 auth，或要求设置明确的 `MYCPU_PUBLIC_UNAUTH_OK=1` 才允许无认证部署。
   - 建议验证：`cd frontend && node --test`；远端未登录请求 `/api/tests` 应返回 `401`。
10. AI profile manifest 数值字段不是严格 `uint32` 解析。
    - 影响范围：host-only importer / manifest fail-closed。
    - 证据：`myCPU/src/platform/machine.cpp` 的 `parse_ai_profile_manifest_file()` 对 `max_ticks` / `source_tag` 使用 `std::stoul(value, nullptr, 0)` 后直接 cast 到 `uint32_t`，没有检查全字符串消费、负号和范围溢出。
    - 风险：`source_tag=4294967296` 会截断，`max_ticks=1junk` 会被当成 `1`，manifest 路径比 task-spec importer 更 fail-open。
    - 建议动作：新增 strict `parse_uint32` / `parse_uint32_nonzero`，检查 `pos == value.size()`、无符号、范围和空白。
    - 建议验证：`cd myCPU && make test-host-ai_accelerator_profile_smoke`，补 negative / oversized / trailing-junk manifest scalar 负例。
11. AI profile 失败路径可能复用旧 summary。
    - 影响范围：profile / observability schema。
    - 证据：`Machine::run_ai_profile_manifest()` 在 manifest parse 成功后才 reset AI 设备和 RAM，解析异常会保留上一轮 `profile_summary()`；`myCPU/src/devices/ai_accelerator.cpp` 的 accepted compute fault 路径只更新 completion outcome，`tile_count` / `scratchpad_peak_bytes` / `op_summaries` 只在 scheduler 成功后覆盖。
    - 风险：success 后再触发 malformed manifest 或 accepted compute fault，profile 可能显示新 fault + 旧 aggregate / op summary，破坏 fail-closed 和观察面真实性。
    - 建议动作：把 AI reset / profile clear 前移或用 guard 覆盖所有失败出口；accepted submission 开始时清空 aggregate / op summaries，compute fault completion 保持 outcome 但不复用旧 op summary。
    - 建议验证：`cd myCPU && make test-host-ai_accelerator_profile_smoke test-host-ai_accelerator_gemm_smoke`。
12. AI graph package memory plan 没有在 parser 阶段保证单一事实来源。
    - 影响范围：device descriptor / future Linux-facing driver route。
    - 证据：`myCPU/src/devices/ai_graph_package.cpp` 的 `validate_ai_graph_package()` 只逐 entry 校验大小和 scratchpad budget，不拒绝重复或缺失 memory plan；设备随后按 `package.memory_plan` 构造 DMA load/store，scheduler 到 compute 前才发现 duplicate。
    - 风险：非法 memory plan 可进入 accepted submission，甚至先触发 DMA，再在 scheduler 才 fault；descriptor validation、DMA plan 和 scheduler 形成多套事实来源。
    - 建议动作：在 `validate_ai_graph_package()` 建立 `memory_plan_by_tensor`，拒绝重复 entry，并要求所有 op input / output tensor 都有唯一 memory plan。
    - 建议验证：`cd myCPU && make test-unit-ai_graph_package test-host-ai_accelerator_profile_smoke`。

### 建议修改

1. DBT guest-store invalidation 只比对虚拟 PC range，物理代码页 synonym 自修改代码可能留下 stale executable contract。建议在 cache entry 记录 physical span / satp / 属性，无法可靠翻译时全局 invalidate；验证：`cd myCPU && make test-host-dbt_runtime_invalidation_smoke test-host-dbt_runtime_harness_smoke`。
2. DBT helper store 的 LR/SC reservation invalidation 未处理跨页 store。建议复用 commit-boundary 逻辑，跨页或 PA span 不完整时 clear reservation；验证：`cd myCPU && make test-host-dbt_helper_execution_bridge_smoke test-host-dbt_runtime_harness_smoke`。
3. `pipeline` profile 漏记 atomic memory observations。建议 pipeline 复用 functional atomic observation 逻辑；验证：`cd myCPU && make test-host-execution_profile_smoke test-pipeline`。
4. debug CLI 数字字段解析对非法字符串静默取 0 / 前缀值。建议严格检查 `strtoull` end pointer / errno；验证：`cd myCPU && make test-host-debug_protocol_command_smoke test-host-debug_cli_smoke`。
5. PLIC / MMIO 平台契约文档与当前 DTB / 常量不一致。`docs/design/platform_mmio_contract.md` 仍写 UART source 1，代码 / DTB 已是 virtio=1、AI=9、UART=10。建议更新文档并明确 SimpleStorage / Virtio transport 选择关系；验证：`cd myCPU && make test-host-virtio_blk_smoke test-host-xv6_shell_smoke`。
6. `README.md` 仍把旧 `KMVPETDS` 写成当前 `kernel_alpha` 能力。建议改为历史 Phase 1 guardrail，并单独说明课程 OS stage1 接管线；验证：`rg -n "kernel_alpha = KMVPETDS|KMVPETDS" README.md docs/status docs/design myCPU/Makefile`。
7. 通用 `kernel_runtime` helper 仍硬编码 demo storage signature。建议把 `'Stor'` signature guardrail 移到 kernel_alpha / demo 专用 helper，或重命名为明确 guardrail contract；验证：`cd myCPU && make test-unit-kernel_runtime test-unit-kernel_alpha_common test-guest-kernel_alpha_demo test-guest-supervisor_demo`。
8. VirtQueue 在确认 used / status 写回前就消费 avail entry。建议延后提交 avail index，或失败路径尽量写 `VIRTIO_BLK_S_IOERR` 与 used entry；验证：`cd myCPU && make test-host-virtio_blk_smoke` 并补 bad descriptor / DMA fault host test。
9. Graph package record reserved 字段未 fail-closed。建议对 tensor / op / memory-plan / dynamic-tensor record 的 reserved 字段统一非零 reject；验证：`cd myCPU && make test-unit-ai_graph_package`。
10. Guest C ABI 仍把 runtime shape offset 命名为 `reserved0`。建议改名为 `runtime_shape_table_offset`，保留 ABI size assert，并补 guest queue unit；验证：`cd myCPU && make test-unit-ai_accel_queue test-host-ai_accel_guest_smoke`。
11. AI profile 文本 / 结构缺少版本边界且字段明显膨胀。建议加 `profile_schema_version` / `timing_schema_version`，CLI 至少输出 `schema=ai_profile_v1`；验证：`cd myCPU && make test-host-ai_accelerator_profile_smoke`。
12. `expected_output` 是 AI manifest 字段但 runner 忽略它。建议移除该 schema 字段或在 runner 中按 output 顺序比对；验证：`cd myCPU && make test-host-ai_accelerator_profile_smoke`。
13. Sv39 leaf PTE reserved 高位未 fail-closed。建议为当前 Sv39 模型增加 leaf / non-leaf reserved mask，并补 leaf reserved-bit asm 回归；验证：`cd myCPU && make test-sv39_pagewalk_contracts test-pipeline-sv39_pagewalk_contracts`。
14. `mstatus.MPP=2` 可被写入并在 `mret` 时解码为 M-mode。建议写 `mstatus` 时规整 MPP 到合法值，或在 `mret` 前明确处理 reserved MPP；验证：`cd myCPU && make test-privilege_transitions test-trap_state`。
15. debug bus 观察面是单槽，容易被内部 fetch / page-walk 覆盖。建议增加 access source / kind，或为 guest data / MMIO commit 维护独立观察槽；验证：`cd myCPU && make test-host-debug_cli_smoke test-host-execution_profile_smoke`。
16. 测试矩阵缺少一等 fast / standard / slow / external 分层入口。建议新增并文档化 `test-fast-smoke`、`test-standard-regression`、`test-slow-guest`、`test-opt-in-external` 等别名；验证：`cd myCPU && make -n test-fast-smoke test-standard-regression test-slow-guest`。
17. `pipeline` LSQ 和 FP metadata 仍复制 shared semantics 指令事实。建议抽出共享 memory shape / FP source-dest descriptor，pipeline 只消费只读描述；验证：`cd myCPU && make test-host-pipeline_backend_smoke test-host-backend_differential_smoke test-pipeline`。
18. DBT guardrail 的 retired count 来自 IR 预期，不是 host executable 实测。建议 host ABI 返回 executed / retired count，或把字段明确标成 IR-expected；验证：`cd myCPU && make test-host-dbt_runtime_harness_smoke test-host-dbt_host_emitter_smoke`。
19. DBT metadata / executable cache 是无界 vector + 线性查找。若只服务 smoke，建议明确 max entry / cap；若准备演进 runtime，改用 keyed map + LRU / 上限；验证：`cd myCPU && make test-host-dbt_executable_cache_smoke test-host-dbt_jit_engine_smoke test-host-dbt_runtime_harness_smoke`。

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

1. 第一批整改优先处理 `必须修复` 中会直接造成 guest-visible divergence、fail-closed 破坏或安全边界问题的条目：RVC pipeline、RMM FP、Sv39 A/D side effect、LR/SC reservation lifecycle、pipeline VA/PA region 分类、Linux rootfs fail-closed、frontend gating/reset/auth，以及 AI manifest / profile / memory-plan fail-closed。
2. 跨多个子系统的结构问题不要直接派零散修复 agent；优先拆成小计划，例如 `shared instruction metadata`、`AI profile schema v1`、`test matrix layering`、`DBT invalidation physical span`。
3. 每条整改都应先补最窄红灯，再改代码，并按影响面选择 `make test`、`make test-pipeline`、`frontend node --test` 或外部资产 opt-in target。

## 记录规则

1. 问题按严重级别和影响面排序。
2. 每条问题至少写清影响范围、风险、建议动作和当前状态。
3. 如果问题进入修复，应补充对应 `plan` 或相关提交 / 分支说明。
4. 问题关闭后只保留简短结论，不在本文件堆积完整执行流水账。
