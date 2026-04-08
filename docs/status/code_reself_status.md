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

- `2026-04-08` 当前无活跃复查问题。
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
