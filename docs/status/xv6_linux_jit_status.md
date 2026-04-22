# xv6 / Linux / JIT 主线状态

## 文档定位

本文档用于跟踪当前已经正式激活的 `xv6 / Linux / JIT` 主线切换：

- 当前到底推进到哪一步
- 当前仍然有效的风险 / 限制是什么
- 下一轮 4 条 workstream 各自要先做什么

本文档不记录完整执行 checklist；执行细节统一放在 `plan` 文档里。

## 关联文档

- 相关设计：
  - [../design/xv6_linux_jit_mainline_design.md](../design/xv6_linux_jit_mainline_design.md)
  - [../design/future_expansion_roadmap_design.md](../design/future_expansion_roadmap_design.md)
  - [../design/phase4_preparation_design.md](../design/phase4_preparation_design.md)
  - [../design/vector_ml_workload_direction_design.md](../design/vector_ml_workload_direction_design.md)
- 当前计划：
  - [../plan/xv6_linux_jit_wave1_plan.md](../plan/xv6_linux_jit_wave1_plan.md)
- 已完成计划：
  - 当前暂无；完成后统一归档到 [../plan/history_plan.md](../plan/history_plan.md)

## 目标 / 主题

当前主题不是“是否要评估 `xv6-riscv`”，而是已经正式把它作为当前主线的近端牵引目标，同时把后续 `Linux` 与 `JIT / 动态二进制翻译` 作为结构决策的长线约束。当前波次的任务不是直接跑起 `Linux` 或写出 `JIT`，而是把这条路径所需的 durable foundation 在不破坏现有稳定基线的前提下分 workstream 落下来。

## 当前状态

- `2026-04-21` 已明确从 `future_expansion_roadmap_design.md` 的候选切换线里正式激活 `Path B`：当前主线改为 `RV64A + virtio 平台 + CSR / privilege 补全 + xv6-riscv bring-up`。
- 当前已经同时保留默认延续线 guardrail：`kernel_alpha`、`interactive_os`、`V4`、`P4-prep-1`、debug/frontend、`make test` / `make test-pipeline` 仍然是本轮主线的稳定性底座，而不是被放弃的旧分支。
- 本轮已经按低交叉依赖拆成 4 条 workstream：
  - A：`RV64A + CSR / privilege foundation`
  - B：`virtio / platform foundation`
  - C：`external guest workload harness + xv6 bring-up`
  - D：`observation / profile foundation + default-line guardrail`
- 本轮最初为了支持多对话、多分支、多个 worktree 并行推进，曾为 4 条 workstream 规划独立 branch / worktree 和独立 ownership。
- 当前不会把 `Linux` 或 `JIT / DBT` 当成本轮直接交付项，但所有新引入的抽象都必须考虑它们的后续复用路径。
- 首轮并行 worktree 阶段已经结束，原 4 个专项 worktree / branch 已清理；后续虽然回到 `main` 工作区推进，但 blocker 仍继续按 A / B / C / D ownership 分类和转派。
- `2026-04-22` 已按 `A -> B -> C -> D` 顺序把 4 条 workstream 的第一轮 foundation 整合进当前主工作树；随后又按清理请求收口了首轮专项 worktree / branch。
- A 线的 `RV64A + CSR / privilege` contract 已成为主线事实来源：`InstructionSemantics` 通过共享 `AtomicRequest` 承接 `RV64A`，`misa.A`、`mhartid` 与 `wfi` 已落地，并通过 `make test-host-atomic_semantics_smoke test-atomic_basic test-atomic_ordering_smoke` 验证。
- B 线的 `virtio-mmio + virtqueue + virtio_device + virtio-blk` foundation 已进入主线，并已完成首轮 post-integration 平台 follow-up：PLIC 现在按 `xv6` 约定把 `virtio` / UART 拆到独立 source（`virtio=1`、`UART=10`），`Machine` 已支持 `simple_storage / virtio-blk` 两条 block transport，并且 CLI、debug CLI `load.block_transport` 与 workload probe 都能显式选择真实 transport。
- C 线的 external workload harness 已进入主线：`xv6-riscv` 外部源码树、board profile、profile make glue、`run-workload-xv6` / `smoke-workload-xv6` 与 `xv6_boot_smoke` 已可直接使用；当前 `mycpu_virt` board profile 已切到 `virtio-blk`，`run-workload-xv6` 与 `xv6_boot_smoke` 都已走真实 `virtio-mmio + virtio-blk` board path，bring-up 现已稳定到 5000-cycle boot-banner checkpoint：`instret=5000`、`pc=0x800010dc`、已处于 `S` mode，并且 UART 已打印 `xv6 kernel is booting`；当前稳定停在 `kinit/freerange` 的 allocator warm-up `memset` 循环，而不再是早期 `M` mode trap。
- D 线的 `execution_profile`、debug CLI profile 导出，以及面向 `memory_region` 的读侧观测合同也已进入主线；`test-host-execution_profile_smoke` 已补进默认 `make test` / `make test-pipeline` guardrail。
- A / D 的第一轮 post-integration hardening 也已补齐：普通 `store` 现在会正确失效 `LR/SC` reservation，delegated page/access fault 也会进入 `execution_profile` 的 `unmapped` fault observation；当前新增 gap 也因此不再是已知 architected correctness 缺口，而是 `xv6` 在真实 `virtio` board path 上继续前进时会暴露出的下一处 bring-up blocker。
- 同日进一步的 bug-driven A / B follow-up 也已把 `xv6` 推过旧的 early-boot trap：A 已补齐 `pmpcfg0/pmpaddr0/menvcfg/stimecmp` 的最小合法 contract，B 已把 UART 扩到 `xv6 uartinit()` 需要的 `LCR/FCR/DLAB` 与 RX/TX interrupt identity；`run-workload-xv6` 现在也会直接打印 machine/supervisor trap 视图。
- 当前这条主线不再处于“等 handoff 收齐”或“补平台接线”的阶段，而是已经把真实 `virtio` board path 接通并越过了最早的 trap blocker；新的主阻塞点前移为：如何把 `xv6` 从当前 post-banner allocator warm-up checkpoint 再往后推进，以及下一处真正的 post-kinit gap 应该由哪条 ownership 去补。

## 关键历史节点

- `2026-04-22`
  - 已按 `A -> B -> C -> D` 顺序完成第一轮主工作树整合。
  - `xv6_boot_smoke` 已从旧的 `mhartid` illegal trap 口径刷新到 post-A 的 early-boot checkpoint。
  - `execution_profile_smoke` 已接入默认 `make test` / `make test-pipeline`。
  - 第一轮 post-integration correctness findings 已关闭：普通 `store` 会正确打破 `LR/SC` reservation，faulting memory access 也会被 profile 统计。
  - 已完成 B / C follow-up：PLIC source wiring 拆分、`Machine` block transport 选择、`mycpu_virt` board profile 切到 `virtio-blk`，`xv6_boot_smoke` / `run-workload-xv6` 开始消费真实 `virtio` board path。
  - 同日进一步的 A / B bug-driven follow-up 也已完成：`xv6` 已越过旧的 early-boot trap，当前 smoke/probe 稳定到 5000-cycle `S` mode boot-banner / allocator-warmup checkpoint。
  - 这一轮验证已覆盖 `make test-unit-mmio_contract_matrix`、`make test-host-debug_protocol_command_smoke`、`make test-host-virtio_blk_smoke`、`make test-host-xv6_boot_smoke`、`make run-workload-xv6`、`make test`、`make test-pipeline` 与 `cd frontend && node --test`。
- `2026-04-21`
  - 正式决定从“默认延续线优先”切到“标准 OS bring-up 线为当前主线”。
  - 新增 `xv6 / Linux / JIT` 主线 design / status / wave 1 plan。
  - 确认按 4 个独立 worktree / 4 个独立对话并行推进。
  - 4 个 worktree 的第一轮 handoff 全部收齐：A 已落地首轮 `RV64A` foundation，B 已落地 `virtio` foundation，C 已落地 external workload harness，D 已落地 execution profile / observation foundation。

## 当前仍然有效的风险 / 限制

- `xv6-riscv` 预期会暴露大量 CSR、trap、timer、interrupt、storage / block、platform contract 细节缺口；当前仍无法精确预估这批缺口的规模。
- `xv6-riscv` bring-up 线虽然已经接入主线，并且已越过最早的 trap blocker，但当前仍只稳定到 5000-cycle boot-banner / allocator-warmup checkpoint；它仍是 gap finder 和整体验证牵引，不是“已可跑到 shell”的交付。
- 当前 `Machine` 默认 block transport 仍保持 `simple_storage` 以守住既有 guest / debug 路径；真实 `virtio-blk` 路径现在需要由 workload profile、CLI 或 debug CLI 显式选择，这条兼容性策略当前是有意保留的。
- “不做短寿命最小实现”会显著提高本轮对抽象边界的要求；如果控制不好，容易出现过度设计。当前必须持续用 `xv6`、未来 `Linux` 和未来 `JIT / DBT` 三个真实复用目标来约束抽象范围。
- 当前 `pipeline`、`V4` 和 `P4-prep-1` 已经具备可用结构边界，但这并不意味着可以直接跳到更重的 cache / DMA / multicore 或更激进 speculation；这些仍应在本轮主线站稳之后再决定。
- 当前最大的活跃 blocker 已不再是 `virtio` 接线或最早的 trap，而是 `xv6` 在真实 `virtio` board path 上打印 boot banner、进入 `kinit/freerange` 之后，还要多久才能越过这条长清零路径并暴露下一处真实 post-kinit gap；这仍需要继续用更窄 smoke / probe 去定位。
- 真实 `virtio` board path 已经打通，并且 UART 已打印 boot banner，但当前还不能把它误读成“`xv6` 已接近 shell”；它目前仍停在更早的 allocator warm-up 阶段。
- A 已经补上第一轮 `mhartid / misa.A / wfi / RV64A` foundation，但这不代表 `xv6` 后续会用到的全部 CSR / timer contract 都已齐备；更后面的 `pmp*`、`menvcfg`、`stimecmp` 等缺口仍可能继续暴露。

## 下一步

1. 继续沿真实 `virtio` board path 推进 C 线：把 `xv6` 从当前 5000-cycle boot-banner / allocator-warmup checkpoint 推到下一个稳定 post-kinit 里程碑，并冻结新的 gap / blocker 归属。
2. A / B 后续都改成 bug-driven hardening：随着 `xv6` 暴露新的 CSR / privilege / timer / platform 缺口，再补最窄 contract，不主动扩大无关 ISA / device 面。
3. D 线继续作为读侧 guardrail：优先用 `execution_profile`、debug CLI 和既有 workload smoke 锁住新引入的 `xv6 / virtio` 行为变化，而不是新增一次性日志。
4. 继续守住默认延续线：`kernel_alpha`、`interactive_os`、`V4`、`P4-prep-1`、debug/frontend 和 `make test` / `make test-pipeline` 不能因为主线切换而退化。

## 验证基线

- `cd myCPU && make test`
- `cd myCPU && make test-pipeline`
- `cd frontend && node --test`

本轮各 workstream 还应额外关注：

- Workstream A：`cd myCPU && make test-host-instruction_semantics_smoke`
- Workstream B：`cd myCPU && make test-unit-mmio_contract_matrix`、`cd myCPU && make test-host-virtio_blk_smoke`
- Workstream C：`cd myCPU && make test-host-xv6_boot_smoke`、`cd myCPU && make run-workload-xv6`
- Workstream D：`cd myCPU && make test-host-debug_cli_smoke`
