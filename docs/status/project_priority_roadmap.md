# 当前项目修正版优先级路线图

## 文档定位

本文档基于 `2026-04-03` 对当前 `main` 分支实现、测试入口和调试链路的重新审计，记录“当前已经能明确指出的问题”以及修正版优先级。

它不是执行计划，也不替代 [mainline_status.md](mainline_status.md)；它的作用是在下一轮建新 plan 之前，先把问题单列清楚，避免路线图继续停留在泛泛的方向描述。

## 关联文档

- 相关设计：
  - [design/regression_completion_criteria.md](../design/regression_completion_criteria.md)
  - [design/cpp_refactor_design.md](../design/cpp_refactor_design.md)
  - [design/phase3_ooo_execution_model_design.md](../design/phase3_ooo_execution_model_design.md)
  - [design/pipeline_speculation_contracts.md](../design/pipeline_speculation_contracts.md)
- 相关状态：
  - [status/mainline_status.md](mainline_status.md)
  - [status/kernel_alpha_status.md](kernel_alpha_status.md)
  - [status/code_self_review_status.md](code_self_review_status.md)
- 当前活跃计划：
  - 当前无活跃计划；最近完成项见 [plan/history_plan.md#p1-debug-frontend-boundary-refinement-plan](../plan/history_plan.md#p1-debug-frontend-boundary-refinement-plan)
- 已完成计划归档：
  - [plan/history_plan.md#p1-debug-frontend-boundary-refinement-plan](../plan/history_plan.md#p1-debug-frontend-boundary-refinement-plan)
  - [plan/history_plan.md#p1-reference-platform-contract-refinement-plan](../plan/history_plan.md#p1-reference-platform-contract-refinement-plan)
  - [plan/history_plan.md#p1-pipeline-backend-boundary-refinement-plan](../plan/history_plan.md#p1-pipeline-backend-boundary-refinement-plan)
  - [plan/history_plan.md#p1-guest-public-header-boundary-refinement-plan](../plan/history_plan.md#p1-guest-public-header-boundary-refinement-plan)
  - [plan/history_plan.md#p1-guest-smoke-orchestration-refinement-plan](../plan/history_plan.md#p1-guest-smoke-orchestration-refinement-plan)
  - [plan/history_plan.md#phase1-hardening-regressions-plan](../plan/history_plan.md#phase1-hardening-regressions-plan)
  - [plan/history_plan.md#phase3-ooo-execution-plan](../plan/history_plan.md#phase3-ooo-execution-plan)

## 审计结论

- 当前项目不缺“还能往哪里扩”，缺的是一批已经在代码里显形的具体问题。
- 这些问题主要集中在 4 类：
  - 失败路径没有回滚，状态可能半初始化泄漏。
  - 调试/测试工具链存在索引漂移和多处手工复制。
  - `guest` smoke 与 `pipeline/debug` 入口继续承担过多职责。
  - 部分关键路径仍缺少更贴近真实边界的持久回归。
- 因此，近期路线图不应继续写成“继续 hardening / 继续 refinement”这种泛表述，而应先把下面这些明确问题逐条压实。

## P0：先修的硬问题
`2026-04-04` 更新：本节 7 个 P0 问题已全部在 `main` 收口；以下条目保留为“已完成问题记录”以便后续追溯。当前若继续排优先级，应从 `P2` 开始。

本轮已完成的 P0 收口包括：

- `Bus::last_access` 已对 unmapped 访问补齐失败记录与回归，debug 快照不再保留旧值。
- `kernel_bringup` 已把 VM 建立后的失败路径收口为对称回滚，并补齐 setup/probe 两类 rollback-failure 保真测试。
- `user_program_smoke_prepare_standard()` 已收口 staged commit / rollback 合同。
- 前端 asm manifest 已重新对齐 canonical 测试集。
- `pipeline` 非法 `load/store funct3` 已稳定回到 illegal-instruction trap 合同。
- `xRET` 返回路径已清 `MPRV`，并有交叉回归守护。
- `instruction-address-misaligned` trap 已补成稳定回归，覆盖 `jal` / `jalr` / taken branch。

### 1. `Bus::last_access` 对 unmapped 访问不会更新，debug 观测面可能保留旧值

- 代码证据：
  - [myCPU/src/mem/bus.cpp](../../myCPU/src/mem/bus.cpp) 中，`try_load()` / `try_store()` 只有在命中某个 `Device` 时才会 `record_access()`；完全没命中时直接返回失败。
  - [myCPU/src/debug/debug_session.cpp](../../myCPU/src/debug/debug_session.cpp) 会把 `machine().bus().last_access()` 直接塞进快照。
- 影响：
  - 一旦出现 unmapped access fault，调试快照里的总线信息可能还是上一条访问，直接误导排查。
- 建议：
  - 给 unmapped load/store 补明确的失败记录与回归，不要继续把“无命中”混成“没有新访存”。

### 2. `kernel_bringup` 的 VM 建立失败路径没有回滚

- 代码证据：
  - [myCPU/guest/kernel/kernel_bringup.c](../../myCPU/guest/kernel/kernel_bringup.c) 的 `kernel_bringup_setup_vm()` 在 `vm_address_space_create()` 成功后，只要后续 `map/register/enable/validate` 任一步失败就直接返回。
  - `kernel_bringup_run_common()` 也没有补清理。
- 影响：
  - `vm_address_space` 可能停留在半初始化状态，后续 bring-up hardening 很难建立稳定合同。
- 建议：
  - 把 `kernel_bringup` 失败路径补成对称回滚，并新增“创建成功但后续失败”的 guest 单元回归。

### 3. `user_program_smoke_prepare_standard()` 的部分成功路径没有回滚，而且过早绑定 `smoke->program`

- 代码证据：
  - [myCPU/guest/kernel/user_program_smoke.c](../../myCPU/guest/kernel/user_program_smoke.c) 先 `user_program_create()`，再做 address space / runtime 准备。
  - `user_program_smoke_prepare_address_space()` 会先写 `smoke->program = program`，随后才继续做映射与 fault orchestration。
- 影响：
  - 中途失败时，`program` 和 `smoke` 可能留下半绑定状态；这已经不是纯结构债，而是状态污染风险。
- 建议：
  - 先把 prepare 路径改成 staged commit：本地临时状态准备完成后再对外发布，并补失败回滚测试。

### 4. 前端调试服务的 asm 测试目录已经落后于 canonical 测试集

- 代码证据：
  - [frontend/server/tests_manifest.mjs](../../frontend/server/tests_manifest.mjs) 的 `asmTests` 列表缺少 `mmio_access_faults`、`csr_illegal_matrix`、`sv39_mprv`、`sv39_pagewalk_contracts`、`sv39_exec_privilege`。
  - 这些测试已经在 [myCPU/Makefile](../../myCPU/Makefile) 的 `ASM_TESTS` 中。
- 影响：
  - `debug/frontend` 对当前 reference 主线不是完整视图；文档说它已正式接入，但测试入口实际上已经漂移。
- 建议：
  - 先把测试清单改成单一事实来源，再决定是否继续扩调试前端能力。

### 5. `pipeline` 对非法 `load/store funct3` 不是稳定进 trap，而是可能卡死

- 代码证据：
  - [myCPU/src/exec/memory_ops.cpp](../../myCPU/src/exec/memory_ops.cpp) 会先把非法内存编码归到 `Load/Store` 路径。
  - [myCPU/src/exec/pipeline_backend.cpp](../../myCPU/src/exec/pipeline_backend.cpp) 只有 `mem.kind == None` 时，才会把 trap / ready 结果直接发布到 `ROB`。
- 影响：
  - 这会让 `pipeline` 和 `functional` 在非法内存编码上的行为分叉；当前最坏结果不是“trap 错了”，而是直接卡在 `step_commit` 预算里。
- 建议：
  - 先把非法 `load/store funct3` 拉回共享 illegal-instruction 合同，并补 host-side 持久回归；这件事优先级高于继续扩更多 OoO 行为。

### 6. `xRET` 返回路径没有清 `MPRV`

- 代码证据：
  - [myCPU/src/trap.cpp](../../myCPU/src/trap.cpp) 的 `mret/sret` 返回路径会恢复 `MIE/SIE`、`MPIE/SPIE`、`MPP/SPP`，但没有同步清理 `MSTATUS_MPRV`。
  - 现有 asm 把 `MPRV` 语义和 `xRET` 语义分开测，没有交叉覆盖这条边界。
- 影响：
  - 当前文档把 `MPRV` 和 trap-return 都表述成已验证，但这条组合语义仍然可能偏离 RISC-V 合同。
- 建议：
  - 先补一条 `MPRV + mret/sret` 交叉回归，再决定是 trap 侧修正还是 CSR 侧收口。

### 7. 当前仍缺 `instruction-address-misaligned` trap

- 代码证据：
  - [myCPU/src/exec/control_flow_ops.cpp](../../myCPU/src/exec/control_flow_ops.cpp) 会直接安装跳转目标。
  - [myCPU/src/cpu.cpp](../../myCPU/src/cpu.cpp) 和 [myCPU/src/mem/address_space.cpp](../../myCPU/src/mem/address_space.cpp) 默认按 4 字节继续取指，没有单独的 misaligned control-transfer 异常路径。
- 影响：
  - 非对齐控制流当前可能被当成普通取指处理，而不是稳定进入 address-misaligned trap。
- 建议：
  - 这条应该按 reference correctness 缺口处理，不要拖到后面的微架构阶段。

## P1：下一层结构收口

`2026-04-04` 已完成本节新增一批收口：

- 原 `P1-1`：`pipeline_backend` 已按“构造+debug / cycle+commit-replay / execute / frontend”拆成四个编译单元，原文件不再继续混挂主调度、memory execute 和 decode/fetch。
- 原 `P1-2`：`user_program_smoke` 已把 `prepare / enter round / active memory` 收口为更窄的内部阶段 helper，`supervisor_demo_smoke` 已退回 bootstrap / user / session 组合层；当时没有顺手扩到 `P1-5`，该项已在同日后续一轮单独完成。
- 原 `P1-5`：`kernel_runtime`、`supervisor_runtime` 与 `user_program_smoke` 已补最小 helper / accessor，生产代码和相关单测不再直接依赖 public struct 的 interrupt counter、`address_space` 或 smoke scratch layout。
- 原 `P1-6`：`debug_protocol` 已拆成 `CLI loop / command codec / response codec` 三块，`debug_server` 已拆成 HTTP / WebSocket 入口、`DebugCliSession` 与 server runtime，terminal 跟踪不再和子进程管理、路由逻辑揉在同一文件里。
- 原 `P1-12`：`AddressSpace` 已把 page walk 期间的页表项读取失败与 A/D 位回写失败从 page fault 收口为对应 access fault，并由 host smoke 明确守住 `fetch/load/store cause + tval=原始虚拟地址` 合同。
- 原 `P1-13`：`PLIC` 的 claim / complete 已改为按 context 记账；错误 context 的 complete 不再释放 claim，现有 machine/supervisor asm 和 host smoke 均保持通过。
- 原 `P1-14`：ELF loader 已把 endianness、ident version、ELF version、type、machine 和 entry-range 纳入明确 reject 合同，相关 unit fixture 和 guest ELF 正向路径已同步对齐。

`2026-04-03` 已完成本节两批收口：

- 首批：
  - 原 `P1-3`：`supervisor_demo / interactive_os` 入口已分别复用 `kernel_runtime` 的 entry bring-up 和 identity-superpage bring-up helper。
  - 原 `P1-4`：`supervisor_demo_smoke` 的 storage signature + platform-tail 组合逻辑已下沉到 `kernel_runtime`。
  - 原 `P1-7`：Node debug server 已统一 CLI `{type:"error"}` 的 server 侧异常语义。
  - 原 `P1-10`：`Makefile` 的 asm / guest functional-vs-pipeline 测试 contract 已抽成共享宏。
- 第二批：
  - 原 `P1-8`：`DebugCliSession` 已补 timeout、exit/close teardown 和 pending 一致 reject，长会话不再静默悬挂。
  - 原 `P1-9`：predictor 统计口径已统一为“已解析分支”，前端命中率展示与后端合同一致。
  - 原 `P1-11`：`CLINT` timer pending 已收口为平台电平语义，`mtimecmp` 条件撤销后不会残留 stale pending。

这意味着路线图里的 `P1` 结构收口已经全部关闭；后续如果继续推进，应把重心转到 `P2` 的测试与验证补洞。

## P2：测试与验证补洞

### 1. `BinaryLoader` 缺少直接回归

- 代码证据：
  - [myCPU/src/loader/binary_loader.cpp](../../myCPU/src/loader/binary_loader.cpp) 有独立的打开、大小、地址范围和短读错误路径。
  - 当前测试里没有 `BinaryLoader` 的直接单测；只有 `hello.bin` 这类 flat binary smoke 间接覆盖。
- 建议：
  - 给 `BinaryLoader` 补最小单测，至少覆盖 bad path 和 range reject。

### 2. guest 侧最复杂的 smoke 路径仍缺更贴近新边界的直接单测

- 代码证据：
  - [myCPU/Makefile](../../myCPU/Makefile) 没有 `supervisor_demo_smoke` 的单测目标。
  - [myCPU/tests/unit/user_program_smoke.c](../../myCPU/tests/unit/user_program_smoke.c) 现在已覆盖 `prepare_standard()` 的 address-space/runtime rollback，但 `active-memory`、`interrupt round` 等阶段 helper 仍主要靠 guest demo 间接覆盖。
- 建议：
  - 下一轮应优先把 `supervisor_demo_smoke` 和 `user_program_smoke` 新收口出来的关键失败路径补成窄单测，再继续做 guest 结构拆分。

### 3. Node 侧调试服务缺少“真实 server + 真实 debug CLI”端到端回归

- 代码证据：
  - [frontend/tests/debug_server.test.mjs](../../frontend/tests/debug_server.test.mjs) 使用的是 fake `createSession()`。
  - 当前真实 `mycpu --debug-cli` 主要由 [myCPU/tests/host/debug_cli_smoke.cpp](../../myCPU/tests/host/debug_cli_smoke.cpp) 单独覆盖。
- 影响：
  - C++ CLI 和 Node server 各自过测，并不等于整条链路稳定。
- 建议：
  - 增加一条最小真实集成 smoke，哪怕只守 `load -> snapshot -> terminal-output -> quit`。

### 4. pipeline 验证过于集中在少数 mega-smoke

- 代码证据：
  - [myCPU/tests/host/backend_differential_smoke.cpp](../../myCPU/tests/host/backend_differential_smoke.cpp)
  - [myCPU/tests/host/pipeline_backend_smoke.cpp](../../myCPU/tests/host/pipeline_backend_smoke.cpp)
  - [myCPU/tests/host/pipeline_speculation_contracts_smoke.cpp](../../myCPU/tests/host/pipeline_speculation_contracts_smoke.cpp)
- 影响：
  - 覆盖面虽然在增长，但定位成本也在增长；后续再做 `Phase 3` bug-driven hardening 会越来越吃力。
- 建议：
  - 下一轮不要先扩新场景，而是先把现有 mega-smoke 按合同拆得更易定位。

### 5. 调试和交互链路的预算参数散落多处

- 代码证据：
  - [myCPU/src/debug/debug_session.cpp](../../myCPU/src/debug/debug_session.cpp)
  - [frontend/server/tests_manifest.mjs](../../frontend/server/tests_manifest.mjs)
  - [frontend/server/debug_server_runtime.mjs](../../frontend/server/debug_server_runtime.mjs)
  - [myCPU/tests/host/interactive_terminal_smoke.cpp](../../myCPU/tests/host/interactive_terminal_smoke.cpp)
  - [myCPU/Makefile](../../myCPU/Makefile)
- 影响：
  - `step_commit` budget、interactive boot steps、terminal settle budget、guest timeout 分散在多层，随着 `pipeline` 继续演进会越来越容易漂。
- 建议：
  - 当前 `myCPU/Makefile` 已先把 `pipeline` guest demo 的预算收口到 `8s / 12s`，消除了眼前的误报超时；但 `debug_session`、`tests_manifest`、`debug_server` 和 interactive smoke 侧预算来源仍然分散，下一轮仍应继续收成少量共享常量。

### 6. 当前 `Phase 3` 的真实下一个 blocker 已经不是抽象的“以后再做 memory speculation”

- 代码证据：
  - [myCPU/src/exec/load_store_queue.cpp](../../myCPU/src/exec/load_store_queue.cpp) 当前对任何 address/data 未 ready 的 older store 都会先把 younger load 标成 `BlockedByUnresolvedStore`。
  - [myCPU/src/exec/pipeline_backend.cpp](../../myCPU/src/exec/pipeline_backend.cpp) 会在 decode 阶段直接据此把 load 卡住。
- 影响：
  - 现在更真实的问题不是“要不要更激进地做 speculation”，而是 decode 级串行化已经成为当前 OoO 模型的明确性能和复杂度边界。
- 建议：
  - 下一轮如果继续碰 `Phase 3`，要把这个具体边界写成单独问题，而不是继续写成笼统的 “memory-order hardening”。

### 7. `Machine::load_elf()` / `load_binary()` 不是完整 reset 语义

- 代码证据：
  - [myCPU/src/platform/machine.h](../../myCPU/src/platform/machine.h) 和 [myCPU/src/platform/machine.cpp](../../myCPU/src/platform/machine.cpp) 会长期持有 `Ram`、`Clint`、`Plic`、`Uart`、`SimpleStorage`。
  - 当前 reload 路径主要重置的是 CPU / CSR / TLB，不是整个平台状态。
- 影响：
  - 新镜像加载和“重建一台干净机器”现在不是同一语义；这条 API 级状态边界仍然偏模糊。
- 建议：
  - 短期先补最小回归说明语义，长期再决定是否要把 reload 收成真正的平台 reset。

## P3：明确暂缓，不要抢跑

### 1. 不要现在就继续扩 `pipeline` 的更激进行为

- 在 `P2` 这轮验证补洞没有继续明显收口前，不建议继续推进更激进的 issue / replay / memory speculation / predictor 组合。

### 2. 不要把 `debug/frontend` 立刻扩成通用调试器

- 当前更真实的问题是长会话压力、真实端到端集成、预算常量与测试清单的一致性，不是功能按钮不够多。

### 3. 不要把 `interactive_os` 当作新的功能主线

- 它当前仍有独立 bring-up 路径和专属 monitor 复杂度，先把它和基础设施边界收紧，再谈扩命令面。

### 4. `SimpleStorage` 的更完整设备模型放到更后面

- [myCPU/include/platform_mmio.h](../../myCPU/include/platform_mmio.h) 和 [myCPU/src/devices/simple_storage.cpp](../../myCPU/src/devices/simple_storage.cpp) 仍然是单块、同步、无 completion interrupt、无宿主持久化的最小模型。
- 这些限制当前是已知边界，但不应抢在前面的 correctness / structure hardening 之前处理。

## 建议的下一轮拆分顺序

1. 下一轮优先回到 `P2` 的验证补洞：
   - `BinaryLoader` 直接单测
   - `supervisor_demo_smoke / user_program_smoke` 更窄单测
   - 真实 `debug server + debug CLI` 端到端 smoke
   - `pipeline` mega-smoke 拆分
   - `Machine::load_elf()` / `load_binary()` reset 语义说明与回归
2. 如果按多 worktree / 多 agent 并行推进，当前最稳的分法是：
   - 线 A：`P2-1 + P2-7`
   - 线 B：`P2-2`
   - 线 C：`P2-3 + P2-5`
   - 线 D：`P2-4`
   - 集成线：`P2-6` 由主分支在前几条结果明确后统一回写 `docs/status/*`、`docs/plan/history_plan.md` 和阶段判断
3. 在上面这些 `P2` 验证问题没有继续明显收口前，不建议再把路线图重心放回更远期的 `Phase 3` 扩展或平台功能面扩张。
