# 主线状态

## 文档定位

本文档只记录当前 `main` 分支的稳定快照、少量关键时间节点、当前仍有效的风险和下一步。

执行过程与已完成 checklist 统一归档到 [plan/history_plan.md](../plan/history_plan.md)；具体优先级判断见 [project_priority_roadmap.md](project_priority_roadmap.md)。

## 关联文档

- 相关设计：
  - [design/regression_completion_criteria.md](../design/regression_completion_criteria.md)
  - [design/debug_frontend_integration.md](../design/debug_frontend_integration.md)
  - [design/debug_frontend_ui_refresh_design.md](../design/debug_frontend_ui_refresh_design.md)
  - [design/phase3_ooo_execution_model_design.md](../design/phase3_ooo_execution_model_design.md)
  - [design/blocked_by_unresolved_store_boundary.md](../design/blocked_by_unresolved_store_boundary.md)
  - [design/phase3_issue_replay_speculation_assessment.md](../design/phase3_issue_replay_speculation_assessment.md)
  - [design/pipeline_speculation_contracts.md](../design/pipeline_speculation_contracts.md)
- 相关状态：
  - [project_priority_roadmap.md](project_priority_roadmap.md)
  - [kernel_alpha_status.md](kernel_alpha_status.md)
- 当前计划：
  - 当前无活跃计划。
- 已完成计划归档：
  - [plan/history_plan.md#spike-external-differential-validation-plan](../plan/history_plan.md#spike-external-differential-validation-plan)
  - [plan/history_plan.md#p2-validation-gap-backfill-round-1](../plan/history_plan.md#p2-validation-gap-backfill-round-1)
  - [plan/history_plan.md#p2-validation-gap-backfill-round-2](../plan/history_plan.md#p2-validation-gap-backfill-round-2)
  - [plan/history_plan.md#p1-debug-frontend-boundary-refinement-plan](../plan/history_plan.md#p1-debug-frontend-boundary-refinement-plan)
  - [plan/history_plan.md#p1-reference-platform-contract-refinement-plan](../plan/history_plan.md#p1-reference-platform-contract-refinement-plan)
  - [plan/history_plan.md#p1-pipeline-backend-boundary-refinement-plan](../plan/history_plan.md#p1-pipeline-backend-boundary-refinement-plan)
  - [plan/history_plan.md#p1-guest-public-header-boundary-refinement-plan](../plan/history_plan.md#p1-guest-public-header-boundary-refinement-plan)
  - [plan/history_plan.md#p1-guest-smoke-orchestration-refinement-plan](../plan/history_plan.md#p1-guest-smoke-orchestration-refinement-plan)

## 当前快照

- 当前仓库已经是一个已可运行的模拟器原型，不是纯设计稿。
- `phase1-stable`（`283aee6`）对应的 Phase 1 核心 bring-up 冻结基线已经形成。
- 默认 `functional` reference path、`make test` 主门禁，以及 `kernel_alpha` 正向与九条负向回归都已稳定接通。
- `pipeline core`、`make test-pipeline`、`debug_session/protocol`、本地 Node 调试服务和浏览器前端都已经正式接入主线，不再是待合入功能。
- `P1` 结构收口已经全部完成；`P2` 首轮验证补洞也已完成两轮收口，新增 loader 单测、guest smoke 窄单测、真实 debug e2e smoke、预算常量收口和 pipeline smoke 拆分都已进入现有门禁。
- `2026-04-06` 已完成 Spike 外部差分验证 V1 第一轮落地：`make test-host-spike_differential` 作为显式离线入口已经可用，当前真实接通的正向场景包括 `alu_mem_csr`、`control_flow`、`predictable_branch_loop`、`trap_return`、`illegal_trap` 与 `delegated_user_ecall_to_supervisor`；`make test` 与 `make test-pipeline` 继续不依赖 Spike。
- `2026-04-07` 已把 Spike 外部差分继续外推到第一批 device-free `Sv39/page fault` final-state subset：`make test-host-spike_differential` 当前已额外接通 `sv39_instruction_page_fault`、`sv39_load_page_fault`、`sv39_store_page_fault` 与 `sv39_reserved_non_leaf_fault`；对应场景不再依赖 `configure hook`，而是统一改写成显式 `initial_gprs / initial_csrs / initial_memory`。
- `2026-04-07` 同日也已把 Spike 外部差分的 returning trap handler checkpoint 正式接通：当前对带 `mret / sret` 的场景会在单次 Spike 运行里额外抓 first-trap checkpoint，并比较 `first_trap_summary`；`trap_return`、`delegated_user_ecall_to_supervisor` 以及新增的 returning `Sv39/page fault` 场景都已进入真实差分门禁。
- `2026-04-08` 当前工作区正在推进一轮不扩功能面的 `debug/frontend` UI refresh：范围限于浏览器壳层的 Hero / 控制带 / `terminal` / `inspector` 布局收口，以及 `terminal collapsed` 交互语义；当前边界仍明确为不修改 `debug_session/protocol`、`DebugSnapshot`、guest 合同或更重浏览器压力面。
- `2026-04-05` 又补上一条更窄的 platform hardening：`Machine::load_elf()/load_binary()` 在保留“非完整平台 reset”语义的前提下，image reload 不再把上一轮 guest 留下的 `SimpleStorage` sticky error 带进新镜像；对应 `machine_loader_reset` 已补上 binary/ELF 两侧回归。
- `2026-04-05` 又补上一条更窄的 guest runtime hardening：`kernel_runtime_complete_storage_signature_check()` 现在即使在 storage read 失败或签名不匹配时也会释放临时 PMM 页，避免把页泄漏藏在 `kernel_alpha` storage probe/signature 的失败路径里；对应 `kernel_runtime` 单测已补上坏签名与读失败两侧回归。
- `2026-04-05` 又补上一条更窄的 guest runtime rollback hardening：`kernel_runtime_run_identity_superpage_bringup()` 现在在复用同一个 `kernel_runtime_t` 时会先清空旧 `address_space`，因此即使随后在 PMM 早期检查、mapping failure 或 satp mismatch 上失败，也不会把上一轮 stale VM 指针泄露给后续路径；对应 `kernel_runtime` 单测已补上 runtime reuse + early failure 回归。
- `2026-04-05` 又继续补上一条更深一层的 guest runtime reuse teardown hardening：`kernel_runtime_run_identity_superpage_bringup()` 与 `kernel_runtime_run_common_bringup()` 在复用同一个 `kernel_runtime_t` 时，现在都会先走 `vm_address_space_destroy()` 正式 teardown 已拥有的旧 VM；若 teardown 失败则直接 fail-closed，不再只是把旧 `address_space` 指针藏起来。对应 `kernel_runtime` / `kernel_bringup` 窄门禁，以及 `make test`、`make test-pipeline` 已全部守住。
- `2026-04-05` 已为 `debug/frontend` 新增一组更窄的 Node/runtime 压力验证：持续 `run/pause`、运行中 session replacement、高吞吐 terminal 输入聚合，以及 `DebugCliSession` 请求超时后的 fail-closed 边界，避免迟到 CLI 响应错配后续请求。
- `2026-04-05` 也已继续把 `debug/frontend` 压力验证外推到更长会话和更像浏览器的操作节奏：新增 repeated `run/pause` 长会话恢复、`reset` 后 terminal reset / offset 重启语义，以及真实 `debug server + mycpu --debug-cli` 下 `guest_interactive_os_demo` 的 `run/pause + terminal-input` e2e。
- `2026-04-05` 已把 decode 级 `BlockedByUnresolvedStore` 串行化边界按专项设计落地为“仅 unknown-address 阻塞”：地址已知但 data 未 ready 的 older store 不再全局阻塞非重叠 younger load，重叠场景继续返回 `BlockedByOverlappingStore`，相关 `LSQ` / `pipeline` smoke 与 `make test-pipeline` 已守住。
- `2026-04-05` 已完成 decode 级收窄之后的 `Phase 3` 后续取舍评估：考虑到当前 backend 仍是 decode 级 load 前置分类、单 memory execute 通道与 coarse replay flush，继续主动扩大更激进的 `issue / replay / speculation` 当前收益不足；后续仅在出现真实 workload 证据或明确研究目标时重开。
- `2026-04-05` 也已补上一层更窄的 `pipeline stall attribution` 观测：当前 debug snapshot / CLI 已能直接暴露 `stall_reason`，区分 `blocked_by_unresolved_store`、`blocked_by_overlapping_store`、`memory_path_busy`、`non_ram_load_waiting_for_rob_head`、`serializing_system_wait_for_rob_head`、`source_operands_not_ready` 和 `decode_backpressure`，供后续是否重开 issue decoupling 判断使用。
- `2026-04-05` 同日也已把浏览器前端调试面板按工程调试视角重排为分层布局：常显区保留 `运行摘要 / 五级流水线 / OoO / 微架构 / 分支预测器 / 最近周期`，`架构状态` 与 `平台与 I/O` 改为折叠分组，并把 `stall_reason`、`lsq_load_state` 与最小 `ROB / LSQ` 观测轻量接入 UI。
- `2026-04-05` 随后又用真实 `debug server + pipeline` 对 `hello`、`guest_interactive_os_demo` 和 `guest_kernel_alpha_demo` 做了一轮短 smoke：`hello` 主要观察到 `source_operands_not_ready`，`interactive_os` 与 `kernel_alpha_demo` 则主要是 `memory_path_busy`；全程没有形成稳定的 `decode_backpressure`、`BlockedByUnresolvedStore`、`BlockedByOverlappingStore` 或 replay hotspot，且 `ROB / LSQ` 深度仍然很浅，因此当前仍不值得为 issue decoupling 或更激进 speculation 重开专项。

## 近期时间线（按时间倒序）

- `2026-04-07`
  - Spike 外部差分验证继续补上第一批 device-free `Sv39/page fault` final-state subset：`sv39_instruction_page_fault`、`sv39_load_page_fault`、`sv39_store_page_fault` 和 `sv39_reserved_non_leaf_fault` 已接入 `make test-host-spike_differential`。
  - 同时也把这组 `Sv39` 场景从 backend differential 里的 `configure hook` 形态收口为显式 `initial_gprs / initial_csrs / initial_memory`，避免为了 Spike adapter 去扩大通用 `configure hook` 支持面。
  - Spike 外部差分同日也已补上 returning trap handler 的 first-trap checkpoint：当前通过单个 Spike 进程的多阶段 snapshot 抓取首个 trap 入口与最终态，并用“有效初始 CSR 值”过滤 non-`M-mode` bootstrap 预写 `mepc` 带来的 machine-trap 伪阳性。
- `2026-04-05`
  - `kernel_runtime_run_identity_superpage_bringup()` 补上一条更窄的 runtime reuse rollback 合同：函数入口先清空旧 `address_space`，避免复用同一个 `kernel_runtime_t` 时在 PMM 早退、mapping failure 或 satp mismatch 后仍暴露 stale VM 指针；对应 `kernel_runtime` 单测已补上 runtime reuse + early failure 回归。
  - `kernel_runtime_run_identity_superpage_bringup()` 与 `kernel_runtime_run_common_bringup()` 随后又继续补上一层更深的 runtime reuse teardown 合同：复用同一个 `kernel_runtime_t` 时，现在都会先走 `vm_address_space_destroy()` 正式 teardown 已拥有的旧 VM；若 teardown 失败则直接 fail-closed，不再只是把旧 `address_space` 指针藏起来。对应 `kernel_runtime` / `kernel_bringup` 窄门禁，以及 `make test`、`make test-pipeline` 已全部通过。
  - `debug/frontend` 新增一组更窄的 runtime 级压力验证：持续 `run/pause` 广播、运行中 session replacement generation guard，以及更高吞吐 terminal 输入聚合。
  - `debug/frontend` 同日也继续外推到更长会话和更像浏览器的节奏：repeated `run/pause` 长会话恢复、`reset` 后 terminal reset / offset 重启语义，以及真实 `debug server + mycpu --debug-cli` 下 `guest_interactive_os_demo` 的 `run/pause + terminal-input` e2e。
  - `DebugCliSession` 补上 timeout fail-closed 行为：一旦 CLI 请求超时，当前 session 直接失效并 teardown，避免没有 request id 的 JSON line 响应在迟到时错配后续请求。
  - decode 级 `BlockedByUnresolvedStore` 边界已按专项设计收窄为“仅 older store 地址未知才阻塞”；地址已知但 data 未 ready 的 older store 不再全局阻塞非重叠 younger load，重叠场景继续暴露 `BlockedByOverlappingStore`，相关 `LSQ` / `pipeline` smoke 与 `make test-pipeline` 已通过。
  - 同日也已完成 decode 级收窄之后的 `Phase 3` 后续取舍评估：在 decode 级 load 前置分类、单 memory execute 通道和 coarse replay flush 仍然成立的前提下，当前不主动继续扩大更激进的 `issue / replay / speculation`。
  - 同日也已补上更窄的 `pipeline stall attribution`：debug snapshot / CLI 新增 `stall_reason`，可直接区分 decode 级 `LSQ block`、`memory_path_busy`、`non_ram_load_waiting_for_rob_head`、`serializing_system_wait_for_rob_head`、`source_operands_not_ready` 与 `decode_backpressure`。
  - 浏览器前端调试面板也同日按工程调试视角重排为分层布局：`OoO / 微架构` 面板进入常显区，`架构状态` 与 `平台与 I/O` 改为折叠分组，timeline 和五级流水线头部可直接显示 `stall_reason` / `lsq_load_state`。
  - 用真实 `debug server + pipeline` 对 `hello`、`guest_interactive_os_demo` 和 `guest_kernel_alpha_demo` 做短 smoke 后，也没有观察到值得为 issue decoupling 单开专项的热点；当前主导 stall 仍主要是 `memory_path_busy` 和 `source_operands_not_ready`，而不是 decode 级 load/store 串行化或 replay。
- `2026-04-04`
  - 完成 `P2` 首轮验证补洞两轮收口：`BinaryLoader` 直接单测、`Machine::load_elf()/load_binary()` 最小 reload/reset 回归、`supervisor_demo_smoke` 与 `user_program_smoke` 更窄直测、真实 `debug server + mycpu --debug-cli` 端到端 smoke、Node/C++ 两侧调试预算常量收口，以及 `pipeline` mega-smoke 拆分。
  - 同日也完成最后一批 `P1` 结构收口：`pipeline_backend` 拆分、`debug/frontend` 协议与运行时边界收口、guest public header 与 smoke orchestration 收口，以及 reference / platform 合同补洞。
- `2026-04-03`
  - `interactive_os / monitor / vm_debug`、browser terminal 壳和 Node debug server 的最小交互闭环已经稳定，相关 smoke 和 Node 测试都已接入主门禁。
  - 文档体系完成一轮归并，已完成计划统一回写到 `history_plan`，不再长期保留活跃 checklist。
- `2026-04-02`
  - `Phase 3-B/C` 首轮最小真实 `OoO execute` 基线形成：`rename + ROB + LSQ`、统一 rollback、coarse replay 和 `RAM-only` forwarding 已进入当前实现。
- `2026-03-27`
  - `Phase 3-A` 第一轮分支预测增强落地，`pipeline` 获得最小 predictor 与相应 smoke / differential 门禁。
- `2026-03-26`
  - Phase 1 hardening 第一轮矩阵化回归落地：illegal encoding、MMIO、ELF、CSR / privilege、Sv39 / `MPRV` 等关键合同已进入 asm / host / guest 门禁。
- `2026-03-25`
  - 完成一批 simulator-side correctness 修复，包括非法整数编码、`DIV/REM` 边界、ELF pure-BSS `PT_LOAD`，以及 bus / device 第一轮边界防御。

## 当前仍然有效的风险 / 限制

- `debug/frontend` 当前已经可用，并且 Node/runtime 级持续 `run`、session replacement、高吞吐 terminal 输入聚合、repeated `run/pause` 长会话、`reset` 后 terminal reset / offset 重启，以及真实 `interactive_os` `run/pause + terminal-input` e2e 都已接入；对当前单用户、本地教学/调试使用，这组门禁已经足够。
- 当前工作区中的 `debug/frontend` UI refresh 仍是浏览器壳层收口，不扩成新的协议或压力专项；其中 `terminal collapsed` 视图必须继续如实反映连接、待输入和不可交互状态，不能把“尚未加载会话”或“仍在发送输入”伪装成“可直接继续交互”。
- Spike 外部差分验证当前仍是独立离线能力，不进入默认主门禁；当前仍以 final state 为主体，并对 `instret` 与 non-`M-mode` bootstrap 带来的少量 `mstatus/mepc` 噪音做了受控收窄。对执行 `mret / sret` 的 returning trap handler，当前已额外比较 first-trap checkpoint summary，但还不扩成更大的中间态 trace。
- Spike 外部差分当前已经覆盖第一批 device-free `Sv39/page fault` final-state subset 和 returning trap handler 的首个 checkpoint summary，但仍不覆盖 `configure hook`、`PlatformFixture::UartPlic`、设备 side effect、更广 `Sv39` 语义面、更复杂的多 checkpoint / nested trap 变体和逐提交 trace；如果未来要把它升级成更强 oracle，下一刀应优先看更广 `Sv39` / device-free privilege 或确有收益的多 checkpoint 变体，而不是直接做更大的统一框架。
- Node 侧 `debug_budget.mjs` 与 C++ 侧 `debug_budget.h` 已分别收口，但它们仍是分语言维护，不是跨语言单一事实来源。
- guest runtime 的 `vm*`、`trap*`、`kernel_bringup`、`kernel_runtime` 等边界已经比早期清晰得多，但后续仍要防止重新膨胀回大文件或重新暴露临时内部布局。
- `Machine::load_elf()/load_binary()` 当前语义已经明确为“替换 RAM 并 reset CPU/backend”，但这不是完整平台 reset；设备状态是否也要复位，仍是后续独立设计问题。
- `Phase 3-B/C` 当前仍是单发射、顺序退休、最小完成窗口的克制形态；decode 级 `BlockedByUnresolvedStore` 的第一轮边界收窄之后，主线也已完成后续判断：当前不主动继续扩更激进的 `issue / replay / memory disambiguation`。
- 当前这条 decode 级边界已经明确：`BlockedByUnresolvedStore` 只保留给 older store 地址未知场景；对地址已知但 data 未 ready 的 older store，仅在与年轻 load 明确重叠时继续阻塞。
- 当前如果未来重开 `Phase 3` 这条线，首个最小切片应优先评估 issue decoupling，而不是直接放宽 unknown-address speculation 或扩大 replay 触发面。

## 下一步

1. 当前不主动重开更激进的 `Phase 3` issue / replay / speculation 扩展；后续仅在出现真实 stall hotspot 证据或明确研究目标时再单开专项。
2. Spike 外部差分验证当前转入 bug-driven 扩展阶段：保留离线 oracle 入口，后续只在出现真实 correctness 缺口时，按最小切片继续补更广 `Sv39 / page fault`、设备无关 privilege / CSR，或必要时的更复杂多 checkpoint 变体。
3. 继续以 bug-driven hardening 的方式维护 guest runtime、`kernel_alpha` 十条基线和 reference correctness 矩阵，不做无关大重构。
4. 继续把 `pipeline` 与 `debug/frontend` 限定在当前已接入、可验证的范围内；当前这轮 `debug/frontend` UI refresh 也只收口浏览器壳层布局和真实状态表达，不扩大协议或浏览器端压力面，后续仍按真实 bug 或明确新需求补最小回归。

## 验证基线

- `cd myCPU && make test`
- `cd myCPU && make test-pipeline`
- `cd frontend && node --test`

如果改动集中在本轮已补洞的入口，至少额外关注：

- `cd myCPU && make test-unit-binary_loader`
- `cd myCPU && make test-unit-machine_loader_reset`
- `cd myCPU && make test-unit-supervisor_demo_smoke`
- `cd myCPU && make test-unit-user_program_smoke`
- `cd myCPU && make test-host-debug_cli_smoke`
- `cd myCPU && make test-host-interactive_terminal_smoke`
