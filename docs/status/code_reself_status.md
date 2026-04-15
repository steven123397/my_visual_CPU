# 代码复查状态

## 文档定位

本文档用于集中记录代码审查 / 复查任务中发现的问题、当前处理状态和下一步。

它不记录完整修复过程；具体执行步骤应进入对应 `plan` 文档，已完成事项统一归档到 [plan/history_plan.md](../plan/history_plan.md)。

## 关联文档

- 相关状态：
  - [mainline_status.md](mainline_status.md)
  - [project_priority_roadmap.md](project_priority_roadmap.md)
- 当前计划：
  - 当前无活跃计划。
- 已完成计划归档：
  - [../plan/history_plan.md#spike-external-differential-validation-plan](../plan/history_plan.md#spike-external-differential-validation-plan)

## 当前状态

- `2026-04-15` 当前这轮全量对抗性审查新增的 3 条活跃问题已关闭：
  - [../../frontend/server/debug_server_runtime.mjs](../../frontend/server/debug_server_runtime.mjs)
    `load()` 现在改成 staged swap：新会话先在局部完成 `load + snapshot/boot + terminal` 初始化，只有完全 ready 后才替换 `currentSession`；初始化失败会显式 close 新会话，旧会话继续保持可用。对应 [../../frontend/tests/debug_server_runtime.test.mjs](../../frontend/tests/debug_server_runtime.test.mjs) 已补 replacement `load` / `snapshot` 失败回滚回归。
  - [../../myCPU/guest/kernel/user_program.c](../../myCPU/guest/kernel/user_program.c)
    `user_program_rebind_region_fault_object()` 现在会在新 fault object 绑定失败时恢复旧 region binding，不再把 helper 留在“旧绑定已拆、新绑定未成”的半状态；对应 [../../myCPU/tests/unit/user_program.c](../../myCPU/tests/unit/user_program.c) 已补 rollback 回归。
  - [../../myCPU/guest/kernel/vm_debug.c](../../myCPU/guest/kernel/vm_debug.c) 和 [../../myCPU/guest/kernel/monitor_commands.c](../../myCPU/guest/kernel/monitor_commands.c)
    `vm_debug_walk()` 现在会拒绝 non-leaf PTE 上保留的 `U/A/D` 位以及 misaligned superpage，`pagewalk` / `pte dump` 不再把 host 会 page fault 的页表状态误讲成合法映射；对应 [../../myCPU/tests/unit/monitor_commands.c](../../myCPU/tests/unit/monitor_commands.c) 已补 monitor 回归。
- `2026-04-15` 当前这轮审查记录的 2 条结构性维护问题已完成本轮收口：
  - [../../frontend/app/components/panels.js](../../frontend/app/components/panels.js)
    前端面板现已按 `workload / vector / predictor-ooo / platform-arch` 拆到 [../../frontend/app/components/panels/](../../frontend/app/components/panels/) 子模块，`panels.js` 只保留聚合导出；对应 [../../frontend/tests/panels.test.mjs](../../frontend/tests/panels.test.mjs) 与 [../../frontend/tests/render.test.mjs](../../frontend/tests/render.test.mjs) 继续守住现有展示合同。
  - [../../myCPU/guest/kernel/user_program_smoke.c](../../myCPU/guest/kernel/user_program_smoke.c)
    `user_program_smoke` 已按 `prepare`、`active-memory`、`lifecycle`、`prepare-runtime`、`round/interrupt` 五组 helper include 收口到更窄模块，主文件不再继续承载整段长链路细节；对应 [../../myCPU/tests/unit/user_program_smoke.c](../../myCPU/tests/unit/user_program_smoke.c) 与 [../../myCPU/tests/unit/supervisor_demo_smoke.c](../../myCPU/tests/unit/supervisor_demo_smoke.c) 已继续通过。
- `2026-04-15` 第二轮 guest/runtime 与 VM 边界深挖新增 3 条活跃问题已关闭：
  - [../../myCPU/guest/kernel/vm_process.c](../../myCPU/guest/kernel/vm_process.c)
    `vm_process_remove_user_region()` 现在会在 unregister 失败时恢复旧 object binding，避免 region 仍挂在 process 上却已经把 object detach 掉的 split-brain；对应 [../../myCPU/tests/unit/vm_process.c](../../myCPU/tests/unit/vm_process.c) 已补 rollback 回归。
  - [../../myCPU/guest/kernel/vm_address_space.c](../../myCPU/guest/kernel/vm_address_space.c)
    kernel fault range 注册继续显式拒绝重叠，同时单测现在锁住“fault range 对 fault range overlap 直接失败”的合同，避免未来静默回退到注册顺序决定命中的语义；对应 [../../myCPU/tests/unit/vm_address_space.c](../../myCPU/tests/unit/vm_address_space.c) 已补重叠回归。
  - [../../myCPU/guest/kernel/vm_address_space.c](../../myCPU/guest/kernel/vm_address_space.c)
    `vm_address_space_destroy()` 在递归释放失败时现在会 fail-closed 地把 address space 重新置回 clean state，不再把半 torn-down 对象暴露给上层继续使用；对应 [../../myCPU/tests/unit/vm_address_space.c](../../myCPU/tests/unit/vm_address_space.c) 已补失败路径回归。
- `2026-04-15` 继续向 guest/runtime 与 VM 边界深挖，再新增 3 条活跃问题已关闭：
  - [../../myCPU/guest/kernel/trap.c](../../myCPU/guest/kernel/trap.c) 和 [../../myCPU/guest/kernel/trap_dispatch.c](../../myCPU/guest/kernel/trap_dispatch.c)
    `trap_user_runtime_deactivate()` 现在会显式 disarm timer / external signal，dispatch 侧也要求 active runtime、process、address space 与 trap context 全部一致后才允许投递，避免迟到中断写回旧 page 指针；对应 [../../myCPU/tests/unit/trap_runtime.c](../../myCPU/tests/unit/trap_runtime.c) 与 [../../myCPU/tests/unit/trap_dispatch.c](../../myCPU/tests/unit/trap_dispatch.c) 已补回归。
  - [../../myCPU/guest/kernel/vm_address_space.c](../../myCPU/guest/kernel/vm_address_space.c)
    `vm_address_space_register_fault_range()` 现在会拒绝普通 user VA，但继续允许 kernel window 与已知 platform MMIO 范围，不再把 alias 之类 user 区误注册成 kernel fault range，同时也不误伤 `kernel_alpha` 这类低地址 MMIO bring-up；对应 [../../myCPU/tests/unit/vm_address_space.c](../../myCPU/tests/unit/vm_address_space.c) 已补 user-window reject 与 MMIO allow 回归。
  - [../../myCPU/guest/kernel/vm_process.c](../../myCPU/guest/kernel/vm_process.c)
    `vm_process_reset()` 现在会显式拒绝 active process，避免制造“process 已 reset、旧页表仍 active”的 split-brain；对应 [../../myCPU/tests/unit/vm_process.c](../../myCPU/tests/unit/vm_process.c) 已补 active-reset reject 回归。
- `2026-04-15` 继续做 `storage / kernel_alpha contract / bring-up` 路径审查，再新增 3 条活跃问题已关闭：
  - [../../myCPU/guest/kernel/kernel_runtime.c](../../myCPU/guest/kernel/kernel_runtime.c)
    `kernel_runtime_run_entry_bringup()` 现在会先 teardown runtime 持有的旧 address space，再做 `memory / runtime_context / trap_context` reset，不再把“旧页表仍被 runtime 持有或 satp 仍在用”的脏状态带进 reused runtime；对应 [../../myCPU/tests/unit/kernel_runtime.c](../../myCPU/tests/unit/kernel_runtime.c) 已补 reused-runtime teardown 回归。
  - [../../myCPU/guest/kernel/kernel_bringup.c](../../myCPU/guest/kernel/kernel_bringup.c)
    `kernel_bringup_run_common()` 现在把 trap-context activation 后移到 VM ready 之后，并在失败路径统一清掉 mutated trap context，不再把半初始化 bring-up 留成 live trap state；对应 [../../myCPU/tests/unit/kernel_bringup.c](../../myCPU/tests/unit/kernel_bringup.c) 已补 pre-VM mutation cleanup 回归。
  - [../../myCPU/guest/kernel/kernel_bringup.c](../../myCPU/guest/kernel/kernel_bringup.c)
    `kernel_bringup_probe_pmm_page()` 现在无论 marker mismatch 还是 free 失败都会走 staged cleanup，不再在 bring-up 自检失败时额外泄漏 probe page。
- `2026-04-15` 对 `1bbce86..HEAD` 那 3 条前端 / 调试活跃问题已关闭：
  - [../../frontend/app/state.js](../../frontend/app/state.js) 、 [../../frontend/app/render.js](../../frontend/app/render.js) 、 [../../frontend/app/components/terminal.js](../../frontend/app/components/terminal.js) 和 [../../frontend/app/components/panels.js](../../frontend/app/components/panels.js)
    前端现在新增独立 `loadedSession`，`terminal` 标题、`workload` 面板与 vector backend 展示都优先绑定已加载 session，不再把 pending selector 伪装成当前会话；对应 [../../frontend/tests/render.test.mjs](../../frontend/tests/render.test.mjs) 与 [../../frontend/tests/terminal_render.test.mjs](../../frontend/tests/terminal_render.test.mjs) 已补回归。
  - [../../frontend/app/components/panels.js](../../frontend/app/components/panels.js)
    `decodeSignedLanes()` 现在保留字符串级整数表示，不再把 64-bit lane 无条件降到 `Number`；对应 [../../frontend/tests/render.test.mjs](../../frontend/tests/render.test.mjs) 已补 `SEW=8B` 精度回归。
  - [../../myCPU/src/debug/debug_session.cpp](../../myCPU/src/debug/debug_session.cpp) 和 [../../myCPU/tests/host/debug_cli_smoke.cpp](../../myCPU/tests/host/debug_cli_smoke.cpp)
    `record_step_events()` 现在直接复用 `stall_reason` 生成 `pipeline stalled: <reason>` 事件文案，避免和快照 / 前端形成双轨解释；对应 `debug_cli_smoke` 已补事件流回归。
- `2026-04-11` 对最新提交 `1bbce86`（`feat(向量流水线): 收窄 V4 依赖链阻塞边界`）完成一轮 adversarial review；当日新增的 2 条向量访存活跃问题现已关闭：
  - [../../myCPU/src/exec/vector_ops.cpp](../../myCPU/src/exec/vector_ops.cpp)
    `VectorRequest::Load` 现在会先对整段 span 做无副作用预校验；如果映射落到 live `MMIO` 或非 RAM 区间，会直接以 `access fault` fail-closed，不再先消费 `UART_REG_RBR` 之类读即取走状态。对应 [../../myCPU/tests/host/vector_vlite_smoke.cpp](../../myCPU/tests/host/vector_vlite_smoke.cpp) 已补上 UART 输入不被提前消费的 host 回归。
  - [../../myCPU/src/exec/vector_ops.cpp](../../myCPU/src/exec/vector_ops.cpp)
    `VectorRequest::Store` 现在同样先做整段预校验，再逐字节提交；跨出 RAM 的向量 `store` 不再留下部分写入，而 live `MMIO` 也会在首字节直接 fail-closed，不再先输出 UART 字符或改写 `IER`。对应 [../../myCPU/tests/host/vector_vlite_smoke.cpp](../../myCPU/tests/host/vector_vlite_smoke.cpp) 已补上 RAM tail partial-write 与 UART side-effect 两条 host 回归。
- `2026-04-08` 对当前 `debug/frontend` UI 刷新工作区的一轮复查问题已关闭。
- 关闭结论：
  - [../../frontend/app/components/terminal.js#L20](../../frontend/app/components/terminal.js#L20)
    `renderTerminal()` 现在会继续以 `terminal.connected` / `terminal.pendingInput` 作为更高优先级状态来源；`terminal collapsed` 视图下，未加载会话与输入仍在排队都不再被伪装成“展开后即可继续交互”。
  - [../../frontend/tests/terminal_render.test.mjs#L71](../../frontend/tests/terminal_render.test.mjs#L71)
    新增了 collapsed + disconnected、collapsed + pending-input 两条前端回归，锁住上述提示语义。
  - [../../AGENTS.md](../../AGENTS.md) 、 [mainline_status.md](mainline_status.md) 和 [project_priority_roadmap.md](project_priority_roadmap.md)
    `AGENTS`、`mainline_status` 与 `project_priority_roadmap` 已同步这一轮 `debug/frontend` UI refresh 的当前边界：只收口浏览器壳层布局与状态表达，不扩大协议或浏览器压力面。
  - [../../frontend/tests/render.test.mjs#L645](../../frontend/tests/render.test.mjs#L645)
    末尾多余空行已清掉；`git diff --check` 当前为干净状态。
- `2026-04-07` 对 guest/runtime 主线边界做的一轮并行普查里的最后一条活跃问题已关闭：
  - [../../myCPU/guest/kernel/supervisor_demo_smoke.c#L375](../../myCPU/guest/kernel/supervisor_demo_smoke.c#L375) 和 [../../myCPU/guest/kernel/supervisor_demo_smoke.c#L386](../../myCPU/guest/kernel/supervisor_demo_smoke.c#L386)
    `supervisor_demo_smoke_probe_storage_page()` 与 `supervisor_demo_smoke_alloc_pages()` 都已改成显式 staged cleanup；`supervisor_demo_smoke` 单测也补上了 storage-page probe 失败、部分分配失败和尾部统计失败后的对称释放回归。
- `2026-04-07` 同一轮 guest/runtime 复查里的以下问题已关闭：
  - [../../myCPU/guest/kernel/user_program.c#L120](../../myCPU/guest/kernel/user_program.c#L120)
    `user_program_create()` 配置失败后现在会 best-effort 做 cleanup / replan，但 `create()` 本身稳定返回 `false`；对应 `user_program` 单测已补 cleanup/replan 失败场景。
  - [../../myCPU/guest/kernel/trap.c#L217](../../myCPU/guest/kernel/trap.c#L217) 、 [../../myCPU/guest/kernel/trap.c#L241](../../myCPU/guest/kernel/trap.c#L241) 、 [../../myCPU/guest/kernel/trap.c#L251](../../myCPU/guest/kernel/trap.c#L251) 和 [../../myCPU/guest/kernel/vm_process.c#L446](../../myCPU/guest/kernel/vm_process.c#L446)
    `trap_user_runtime_prepare()` / `trap_user_runtime_prepare_standard()` 已改成事务性 rollback；rollback 载体也从整块 `trap_context` 全量快照收窄到 policy/handler 级快照，并补了 policy-install failure 的 host 单测，避免再次把 guest `supervisor_demo` 的 boot stack 顶穿。
  - [../../myCPU/guest/kernel_alpha/interrupt_contract.c#L43](../../myCPU/guest/kernel_alpha/interrupt_contract.c#L43)
    `kernel_alpha_validate_plic_not_ready_contract()` 现在会先拒绝 `timeout_delta == 0`，对应 invalid-input 单测已补齐。
  - [../../myCPU/guest/kernel/user_task.c#L43](../../myCPU/guest/kernel/user_task.c#L43)
    `user_task_destroy()` 在 address-space destroy 半失败后会 fail-closed 回到 create-ready 状态，不再残留半销毁对象；对应半失败状态断言已补。
  - [../../myCPU/guest/kernel/user_program_smoke.c#L1250](../../myCPU/guest/kernel/user_program_smoke.c#L1250) 、 [../../myCPU/guest/kernel/user_program_smoke.c#L1310](../../myCPU/guest/kernel/user_program_smoke.c#L1310) 和 [../../myCPU/guest/kernel/user_program_smoke.c#L1354](../../myCPU/guest/kernel/user_program_smoke.c#L1354)
    `user_program_smoke` 的 activate / enter helper 失败后已统一走 best-effort deactivate 回滚，`enter` / arm-signal 失败回归已补上。
- `2026-04-06` 对当前 `spike` 外部差分验证工作区完成的实现级复查已关闭。
- 关闭结论：
  - `make test-host-spike_differential` 显式入口已经补齐，本地验证可直接跑通，不再与 `tests/host/spike_differential/` 目录名冲突。
  - `spike_differential_smoke` 已经串起真实正向的 `myCPU vs Spike` final-state differential；当前 `alu_mem_csr`、`control_flow`、`predictable_branch_loop`、`trap_return`、`illegal_trap` 与 `delegated_user_ecall_to_supervisor` 6 条场景都能在真实 Spike 环境下匹配通过。
  - Spike adapter 当前已经支持最小初始 GPR / CSR / memory、非 M-mode 起始态、`trap_program` 与 final privilege 明确读取；输出解析也已改成精确字段计数、未知行 fail-closed 的严格策略。
  - 当前仍保留的设计边界，已转入实现已知限制而非活跃缺陷：V1 仍是 final-state differential，不覆盖 `configure hook`、平台 fixture、设备 side effect 与 `Sv39 / page fault` 子集；对执行 `mret/sret` 的 returning trap handler，当前也还不比较“第一现场” trap summary，而只比较最终可恢复状态。
- `2026-04-05` 对 `8403a563c3578291990220f56010488d37e18dd4`（`fix(phase3): 收窄 decode 级 blocked-by-unresolved-store 边界`）完成一轮提交级复查。
- 代码路径本身未发现新的 `LSQ` / `pipeline` correctness 回归；相关 `host smoke` 与 `make test-pipeline` 已通过。
- 本轮复查暴露的两条文档同步问题已关闭：
  - [docs/design/blocked_by_unresolved_store_boundary.md](../design/blocked_by_unresolved_store_boundary.md) 已改为指向 [plan/history_plan.md#phase3-blocked-by-unresolved-store-boundary-plan](../plan/history_plan.md#phase3-blocked-by-unresolved-store-boundary-plan)，不再保留已删除的活跃 plan 死链。
  - [AGENTS.md](../../AGENTS.md) 已同步最新 `Phase 3` 口径：decode 级 `BlockedByUnresolvedStore` 边界专项已完成，当前下一步改为评估是否继续扩 issue / replay / speculation。

## 记录规则

1. 问题按严重级别和影响面排序。
2. 每条问题至少写清影响范围、风险、建议动作和当前状态。
3. 如果问题进入修复，应补充对应 `plan` 或相关提交 / 分支说明。
4. 问题关闭后只保留简短结论，不在本文件堆积完整执行流水账。
