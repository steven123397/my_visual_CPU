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
- 为了支持多对话、多分支、多个 worktree 并行推进，当前已经为 4 条 workstream 规划了独立 branch / worktree 和独立 ownership。
- 当前不会把 `Linux` 或 `JIT / DBT` 当成本轮直接交付项，但所有新引入的抽象都必须考虑它们的后续复用路径。
- 在 docs 基线尚未独立落成提交之前，当前协调者工作树中的 design / status / plan 文档仍是权威执行口径；各 agent worktree 默认按 prompt 中给出的绝对路径读取这些文档，而不是在自己的 branch 上复制一套共享 docs 正文。
- `2026-04-22` 已按 `A -> B -> C -> D` 顺序把 4 条 workstream 的第一轮 foundation 整合进当前主工作树；本轮没有自动 commit，也没有自动清理源 worktree / branch。
- A 线的 `RV64A + CSR / privilege` contract 已成为主线事实来源：`InstructionSemantics` 通过共享 `AtomicRequest` 承接 `RV64A`，`misa.A`、`mhartid` 与 `wfi` 已落地，并通过 `make test-host-atomic_semantics_smoke test-atomic_basic test-atomic_ordering_smoke` 验证。
- B 线的 `virtio-mmio + virtqueue + virtio_device + virtio-blk` foundation 已进入主线，并通过 `make test-unit-virtio_mmio_contract test-unit-virtqueue_smoke test-host-virtio_blk_smoke` 验证；当前剩余的 B 类 blocker 已收敛到更窄的平台 follow-up：UART 与 `virtio` 仍需要独立的 PLIC source wiring，真实 `xv6` board profile 还没接到 `Machine` 的 `virtio` 路径。
- C 线的 external workload harness 已进入主线：`xv6-riscv` 外部源码树、board profile、profile make glue、`run-workload-xv6` / `smoke-workload-xv6` 与 `xv6_boot_smoke` 已可直接使用；其中 `xv6_boot_smoke` 已按 A/B 新主线刷新到新的 early-boot checkpoint：执行 4 个 cycle 后 `instret=4`、`pc=0x80000010`、仍处于 `M` mode，`mcause/mepc/mtval` 都保持为 0，UART 仍为空。
- D 线的 `execution_profile`、debug CLI profile 导出，以及面向 `memory_region` 的读侧观测合同也已进入主线；`test-host-execution_profile_smoke` 已补进默认 `make test` / `make test-pipeline` guardrail。
- A / D 的第一轮 post-integration hardening 也已补齐：普通 `store` 现在会正确失效 `LR/SC` reservation，delegated page/access fault 也会进入 `execution_profile` 的 `unmapped` fault observation；因此当前活跃 blocker 继续集中在 B/C 交界，而不是新发现的 architected correctness 缺口。
- 当前这条主线不再处于“等 handoff 收齐”的阶段，而是已经完成第一轮 foundation 整合；新的主阻塞点变成 B/C 交界：`xv6` board profile 仍记录 `simple_storage`，尚未切到真实 `virtio` contract。

## 关键历史节点

- `2026-04-22`
  - 已按 `A -> B -> C -> D` 顺序完成第一轮主工作树整合。
  - `xv6_boot_smoke` 已从旧的 `mhartid` illegal trap 口径刷新到 post-A 的 early-boot checkpoint。
  - `execution_profile_smoke` 已接入默认 `make test` / `make test-pipeline`。
  - 第一轮 post-integration correctness findings 已关闭：普通 `store` 会正确打破 `LR/SC` reservation，faulting memory access 也会被 profile 统计。
  - 本轮整合验证已覆盖 `make smoke-workload-xv6`、`make test` 和 `make test-pipeline`。
- `2026-04-21`
  - 正式决定从“默认延续线优先”切到“标准 OS bring-up 线为当前主线”。
  - 新增 `xv6 / Linux / JIT` 主线 design / status / wave 1 plan。
  - 确认按 4 个独立 worktree / 4 个独立对话并行推进。
  - 4 个 worktree 的第一轮 handoff 全部收齐：A 已落地首轮 `RV64A` foundation，B 已落地 `virtio` foundation，C 已落地 external workload harness，D 已落地 execution profile / observation foundation。

## 当前仍然有效的风险 / 限制

- `xv6-riscv` 预期会暴露大量 CSR、trap、timer、interrupt、storage / block、platform contract 细节缺口；当前仍无法精确预估这批缺口的规模。
- `xv6-riscv` bring-up 线虽然已经接入主线，但当前只稳定到了更靠前的 early-boot checkpoint；它仍是 gap finder 和整体验证牵引，不是“已可跑到 shell”的交付。
- 当前 docs 基线尚未以单独提交形式落到各 agent branch，因此跨对话协同仍依赖协调者维护的权威计划文档和绝对路径 prompt。
- “不做短寿命最小实现”会显著提高本轮对抽象边界的要求；如果控制不好，容易出现过度设计。当前必须持续用 `xv6`、未来 `Linux` 和未来 `JIT / DBT` 三个真实复用目标来约束抽象范围。
- 当前 `pipeline`、`V4` 和 `P4-prep-1` 已经具备可用结构边界，但这并不意味着可以直接跳到更重的 cache / DMA / multicore 或更激进 speculation；这些仍应在本轮主线站稳之后再决定。
- 当前最大的活跃 blocker 已经收敛到平台接线：`virtio` 当前测试用 IRQ source 不能与 UART 共用，真实 `xv6` profile 前必须先补一轮更窄的 `PLIC / UART / virtio` source 参数化和 `Machine` 级接线。
- 当前 C 线的 board profile 仍记录 `simple_storage`；这保证了 harness 已接入、镜像已 attach、probe 已可回归，但不等于真实 `xv6` 已经跑在 `virtio-mmio + virtio-blk` 合同上。
- A 已经补上第一轮 `mhartid / misa.A / wfi / RV64A` foundation，但这不代表 `xv6` 后续会用到的全部 CSR / timer contract 都已齐备；更后面的 `pmp*`、`menvcfg`、`stimecmp` 等缺口仍可能继续暴露。

## 下一步

1. 先做更窄的 B 类 follow-up：给 UART 与 `virtio` 拆开独立的 PLIC source wiring，并把 `Machine` / board profile 接到真实 `virtio-mmio + virtio-blk` contract。
2. 再推进 C 线的下一跳：把 `xv6` board profile 从 `simple_storage` 切到 `virtio`，并把 `xv6_boot_smoke` 从当前 4-cycle checkpoint 推到下一个稳定 early-boot 里程碑。
3. A 线后续改成 bug-driven hardening：随着 `xv6` 暴露下一批 CSR / privilege / timer 缺口，再补最窄 architected contract，不主动扩大无关 ISA 面。
4. D 线继续作为读侧 guardrail：优先用 `execution_profile`、debug CLI 和既有 workload smoke 锁住新引入的 `xv6 / virtio` 行为变化，而不是新增一次性日志。
5. 继续守住默认延续线：`kernel_alpha`、`interactive_os`、`V4`、`P4-prep-1`、debug/frontend 和 `make test` / `make test-pipeline` 不能因为主线切换而退化。

## 验证基线

- `cd myCPU && make test`
- `cd myCPU && make test-pipeline`
- `cd frontend && node --test`

本轮各 workstream 还应额外关注：

- Workstream A：`cd myCPU && make test-host-instruction_semantics_smoke`
- Workstream B：`cd myCPU && make test-unit-bus_device_guards`
- Workstream C：后续新增 `xv6` harness / boot smoke 后，把对应 target 接入计划文档
- Workstream D：`cd myCPU && make test-host-debug_cli_smoke`
