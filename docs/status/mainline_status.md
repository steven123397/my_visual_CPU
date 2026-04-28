# 主线状态

## 文档定位

本文档是仓库唯一的主线实时状态文档，用于记录：

- 当前稳定快照
- 当前优先级
- active wave 与近端 blocker
- 当前仍然有效的风险 / 限制
- 下一步工作

执行过程、阶段性 checklist 和已完成专项统一归档到
[../plan/history_plan.md](../plan/history_plan.md)。

## 关联文档

- 相关设计：
  - [../design/regression_completion_criteria.md](../design/regression_completion_criteria.md)
  - [../design/phase3_ooo_execution_model_design.md](../design/phase3_ooo_execution_model_design.md)
  - [../design/pipeline_speculation_contracts.md](../design/pipeline_speculation_contracts.md)
  - [../design/phase4_preparation_design.md](../design/phase4_preparation_design.md)
  - [../design/future_expansion_roadmap_design.md](../design/future_expansion_roadmap_design.md)
  - [../design/xv6_linux_jit_mainline_design.md](../design/xv6_linux_jit_mainline_design.md)
- 相关状态：
  - [kernel_alpha_status.md](kernel_alpha_status.md)
  - [npu_tpu_accelerator_status.md](npu_tpu_accelerator_status.md)
  - [code_reself_status.md](code_reself_status.md)
- 当前计划：
  - 当前无活跃计划。
- 已完成计划归档：
  - [../plan/history_plan.md#mainline-roadmap-rewrite-and-linux-checkpoint-closure-plan](../plan/history_plan.md#mainline-roadmap-rewrite-and-linux-checkpoint-closure-plan)
  - [../plan/history_plan.md#phase4-prep2-memory-observation-shadow-cache-plan](../plan/history_plan.md#phase4-prep2-memory-observation-shadow-cache-plan)
  - [../plan/history_plan.md#xv6-linux-jit-wave1-plan](../plan/history_plan.md#xv6-linux-jit-wave1-plan)

## 目标 / 主题

当前主线仍围绕 `reference-first`、真实 workload bring-up 和可观察性收口展开。
近端工程目标不是抢跑更重 `Phase 4`，而是继续沿现有 `xv6 / Linux`
guardrail 把 Linux block-rootfs 的真实 userland checkpoint 推向下一处自然边界。

## 当前状态

- 当前仓库已经是一个已可运行的模拟器原型，不是纯设计稿。
- `phase1-stable`（`283aee6`）仍是 Phase 1 冻结参考点。
- `xv6-riscv` 已在真实 `virtio-blk` board path 上稳定到 shell，当前主要承担
  workload guardrail 角色，不再是近端 blocker。
- Linux `linux_proto` 的 repo-generated block-rootfs fourth-stage runtime guardrail
  已稳定推进到：
  - `mycpu linux userland: stage=unlinkat-open-fd-mmap-shared-fork-parent-readback-ok`
- 当前已知的下一处自然边界是 `pipe2 + clone3()/wait4()` 匿名 IPC；探索结果表明，
  该 slice 仍会在上面这个 checkpoint 之后命中：
  - `mycpu linux userland: fourth-stage pipe2 smoke failed`
- `P4-prep-1` 和 `C1 / P4-prep-2 memory observation / shadow cache` 已完成。
  当前稳定的观测 guardrail 主要是：
  - pipeline vector CNN 的 `shadow_cache` RAM baseline
  - functional `xv6` 的稳定 profile / `shadow_cache` baseline
  - functional `linux_proto` dummy-payload observation baseline
- `debug/frontend`、`kernel_alpha` 十条基线、`make test` / `make test-pipeline`
  和现有 workload smoke 都已进入维护态。
- 当前 active wave 仍是 Wave 3，不是 Wave 4。

## 当前优先级

1. 继续沿 Linux 当前 baseline 往后收窄新的 userland checkpoint 或 blocker。
   当前首选入口就是 `shared-fork-parent-readback-ok` 之后的 `pipe2` IPC 边界。
2. 把 `xv6` shell、Linux probe、`kernel_alpha`、`pipeline`、debug CLI 和
   现有回归矩阵守成稳定 guardrail。
3. 继续积累 `shadow_cache / observation / representative workload` 证据，
   但只做观测，不提前切到真实 `cache / DMA / multicore / coherence`。
4. 更后续 wave 上的 `NPU / TPU-like`、向量 workload 和更重 `Phase 4`
   当前都不是近端 blocker，继续保持 maintenance / bug-driven 节奏。

## 关键历史节点

- `2026-04-28`
  - Linux fourth-stage baseline 已推进到
    `unlinkat-open-fd-mmap-shared-fork-parent-readback-ok`。
  - 这一轮验证了匿名 open-fd alias 跨 `clone3()/wait4()`、`/proc/self/fd/<n>`、
    `execve()`、`execveat()`、`mmap(MAP_SHARED)`、`mmap(MAP_PRIVATE)` 和
    shared mapping fork 可见性等更后边界。
  - 下一处已探索自然边界已收敛到 `pipe2 + clone3()/wait4()` 匿名 IPC。
- `2026-04-27`
  - repo-generated block-rootfs `/init + post-init` 路径与 fourth-stage cleanup
    runtime guardrail 冻结通过。
  - `C1 / P4-prep-2 memory observation / shadow cache` 第一刀完成，并接入
    `execution_profile`、debug JSON 和 probe 摘要。
- `2026-04-22`
  - `xv6` 在真实 `virtio-blk` board path 上稳定到 shell。
  - Linux-facing `flat image + payload + set_gpr + linux_sbi_shim + repo DTB`
    foundation 进入主线。
- `2026-04-12`
  - `P4-prep-1` 完成，`bus / memory_region` 合同收口为统一事实来源。

## 当前仍然有效的风险 / 限制

- Linux 当前 checkpoint 线仍然在持续产出高信号 userland contract，
  因此它仍是近端 blocker，而不是低收益重复挖掘。
- `shadow_cache` 虽然已经有稳定 guardrail，但证据仍不足以证明：
  “继续深挖 Linux 只会得到低收益重复”，所以还不能激活 Wave 4。
- 仓库默认位置仍不携带真实 Linux `Image`；因此 Linux runtime guardrail
  继续保持 opt-in，但它本身已经在显式提供真实 `Image` 时冻结通过。
- `xv6 / Linux` 的 pipeline-side workload 观测还不够稳定，当前更可靠的
  workload 观测仍主要落在 pipeline vector CNN 和 functional `xv6/linux_proto`。
- `debug/frontend`、`pipeline` 和 guest runtime 都已形成可维护边界，但后续
  仍要避免真实 bug 修复把职责重新揉回大文件。

## 下一步

1. 从 `unlinkat-open-fd-mmap-shared-fork-parent-readback-ok` 之后继续收窄
   `pipe2 + clone3()/wait4()` 匿名 IPC，或找到更自然的后续 userland 边界。
2. 继续守住 `xv6` shell、Linux probe、`kernel_alpha`、debug CLI、
   `make test` 和 `make test-pipeline` 这些稳定 guardrail。
3. 只有在下面两条同时成立时，才考虑建议激活 Wave 4：
   - Linux 当前 checkpoint 线已经不再是近端 blocker
   - `shadow_cache / observation / representative workload` 证据已经足够稳定

## 验证基线

- `cd myCPU && make test`
- `cd myCPU && make test-pipeline`
- `cd frontend && node --test`

如果改动集中在 Linux / `xv6` workload harness、probe 或 runtime guardrail，
还应至少关注：

- `cd myCPU && make test-host-run_debug_cli_probe`
- `cd myCPU && make test-host-xv6_boot_smoke`
- `cd myCPU && make test-host-xv6_shell_smoke`
- `cd myCPU && make run-workload-xv6`
- `cd myCPU && python3 -m unittest tests.host.run_debug_cli_probe_test.RunDebugCliProbeTest.test_make_build_workload_linux_proto_block_mode_builds_post_init_smoke_elf`
- `cd myCPU && MYCPU_RUN_LINUX_PROTO_RUNTIME=1 MYCPU_LINUX_PROTO_RUNTIME_IMAGE=<Image> python3 -m unittest tests.host.run_debug_cli_probe_test.RunDebugCliProbeTest.test_linux_proto_block_mode_runtime_reaches_fourth_stage_when_requested`
