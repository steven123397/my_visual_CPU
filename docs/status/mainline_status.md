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
  - [../design/wave5_cache_memory_system_design.md](../design/wave5_cache_memory_system_design.md)
  - [../design/xv6_linux_jit_mainline_design.md](../design/xv6_linux_jit_mainline_design.md)
- 相关状态：
  - [kernel_alpha_status.md](kernel_alpha_status.md)
  - [npu_tpu_accelerator_status.md](npu_tpu_accelerator_status.md)
  - [code_reself_status.md](code_reself_status.md)
- 当前计划：
  - 暂无主线活跃计划；继续推进 `Wave 5` 时先新建 `docs/plan/` 计划。
- 已完成计划归档：
  - [../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-d-l1d-hardening-plan](../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-d-l1d-hardening-plan)
  - [../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-c-l1d-observation-guardrail-plan](../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-c-l1d-observation-guardrail-plan)
  - [../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-b-minimal-l1d-plan](../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-b-minimal-l1d-plan)
  - [../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-a-signal-contract-plan](../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-a-signal-contract-plan)
  - [../plan/history_plan.md#mainline-wave4-ai-accelerator-slices-plan](../plan/history_plan.md#mainline-wave4-ai-accelerator-slices-plan)
  - [../plan/history_plan.md#mainline-roadmap-rewrite-and-linux-checkpoint-closure-plan](../plan/history_plan.md#mainline-roadmap-rewrite-and-linux-checkpoint-closure-plan)
  - [../plan/history_plan.md#phase4-prep2-memory-observation-shadow-cache-plan](../plan/history_plan.md#phase4-prep2-memory-observation-shadow-cache-plan)
  - [../plan/history_plan.md#xv6-linux-jit-wave1-plan](../plan/history_plan.md#xv6-linux-jit-wave1-plan)

## 目标 / 主题

当前主线仍围绕 `reference-first`、真实 workload bring-up 和可观察性收口展开。
`Wave 3` 已按真实实现现状收口：Linux fourth-stage checkpoint 线冻结在
`timerfd-one-shot-readback-ok`，后续不再默认继续扩同类 syscall breadth。
主线 `Wave 4` 的 AI accelerator A/B/C 三段切片已经完成并归档。

当前 active wave 仍是 `Wave 5 / cache / memory-system`。`Slice A / signal +
contract` 已完成：已有 `shadow_cache` 证据已经补上一条 pipeline-side `xv6`
memory observation guardrail，后续最小 L1 data cache 的第一版合同也已固定为
RAM-only、write-through、no dirty write-back，并对 MMIO / unmapped / side-effect、
atomic / fence 和 DMA interaction 采用保守口径。`Slice B / minimal executable L1D`
也已完成：当前已有默认关闭、可显式启用、RAM-only、write-through、no dirty
write-back 的最小 L1D 执行模型，并通过 `AddressSpace` 仅接入 data load/store。
instruction fetch、page walk、atomic、MMIO、unmapped 和 side-effect region 继续
bypass L1D。`Slice C / L1D opt-in observation + guardrail` 也已完成：当前 debug
snapshot 顶层暴露只读 `l1_data_cache` counters，`run_debug_cli_probe.py --l1d`
提供显式 opt-in probe guardrail；默认执行路径仍不打开 L1D，既有 `shadow_cache`
字段语义不变。`Slice D / L1D hardening` 也已完成：当前 L1D 对跨 cache line
store 会在 bypass 后失效重叠 line，store miss 固定为 write-through + no-allocate
并可观察为 miss，non-cacheable / side-effect / unmapped / refill fault 路径和
atomic、page-walk、instruction fetch 继续 bypass L1D；默认 `make test` /
`make test-pipeline` 仍不打开 L1D。这些结果仍不代表 full cache、DMA coherence、
multicore、JIT 或 AI accelerator 后续专项已经启动。当前暂无主线活跃计划。

## 当前状态

- 当前仓库已经是一个已可运行的模拟器原型，不是纯设计稿。
- `phase1-stable`（`283aee6`）仍是 Phase 1 冻结参考点。
- `xv6-riscv` 已在真实 `virtio-blk` board path 上稳定到 shell，当前主要承担
  workload guardrail 角色，不再是近端 blocker。
- Linux `linux_proto` 的 repo-generated block-rootfs fourth-stage harness
  已扩展并冻结到：
  - `mycpu linux userland: stage=timerfd-one-shot-readback-ok`
- 这条冻结边界之前，fourth-stage 已连续跨过多类 Linux userland 合同；
  当前 baseline 已不只覆盖文件/映射边界、进程 control-plane、路径 mutation、
  ready-queue 和 ancillary fd passing，还覆盖了：
  - `sendmsg(2)` + `recvmsg(2)` + `SCM_RIGHTS` 的 UNIX ancillary fd passing /
    sender-exit / parent readback 合同
  - `copy_file_range(2)` 的 kernel-side file copy / dst readback 合同
  - `splice(2)` 的 file -> pipe -> file zero-copy transfer / dst readback 合同
  - `statx(2)` 的 relative-dirfd metadata ABI / mask / size-mode readback 合同
  - `inotify_init1(2)` + `inotify_add_watch(2)` + `read(2)` 的 fs-notify queue /
    close-write event 合同
  - `timerfd_create(2)` + `timerfd_settime(2)` + `read(2)` 的 one-shot timer queue /
    expiration readback 合同
- 仓库默认位置仍不携带真实 Linux `Image`；因此 `timerfd` 边界在默认门禁里由
  build/string/probe 合同锁住，真实 `Image + rootfs.ext4` runtime 仍是 opt-in
  验证项。后续如果要把它作为发布级 runtime 断言，必须显式提供 `Image`
  重新跑对应 runtime guardrail。
- `P4-prep-1` 和 `C1 / P4-prep-2 memory observation / shadow cache` 已完成。
  当前稳定的观测 guardrail 主要是：
  - pipeline vector CNN 的 `shadow_cache` RAM baseline
  - functional `xv6` 的稳定 profile / `shadow_cache` baseline
  - functional `linux_proto` dummy-payload observation baseline
- `debug/frontend`、`kernel_alpha` 十条基线、`make test` / `make test-pipeline`
  和现有 workload smoke 都已进入维护态。
- 当前 active wave 是 `Wave 5 / cache / memory-system`，不是继续深挖 Linux
  checkpoint 的 `Wave 3`，也不是 AI accelerator 后续专项。
- 主线 `Wave 4` 的 AI accelerator 切片 A 已完成：`bounded dynamic shape`
  已从 `dynamic GEMM / FC-like` 扩到现有 op family 的正向或 fail-closed 合同，
  并新增 `dynamic_tiny_model` 动态小模型 workload。
- 主线 `Wave 4` 的 AI accelerator 切片 B 已完成：`timed-simple` profile 现在把
  tile setup 归入 `stall_cycles`，debug snapshot 保持 aggregate-only schema，
  frontend 已为 `guest_ai_accel_demo` 增加 workload presentation 与 AI accelerator
  只读 counters 面板。
- 主线 `Wave 4` 的 AI accelerator 切片 C stretch 已完成：新增静态 `fp32`
  row-wise `Softmax`，并新增 `tiny_attention_static` host workload，固定验证
  `gemm -> softmax -> gemm` 的最小 attention-like profile 闭环。
- 主线 `Wave 5` 的 `Slice A / signal + contract` 已完成：pipeline-side memory
  signal 固定为 `run_debug_cli_probe` 的 `xv6 --backend pipeline` 5000-cycle
  probe；该 guardrail 只证明 memory observation / `shadow_cache` 输出合同，不证明
  pipeline 已完整 boot `xv6`。
- 主线 `Wave 5` 的 `Slice B / minimal executable L1D` 已完成：新增
  `SimpleL1DataCache`，以默认关闭的 `Machine` 开关显式启用，`AddressSpace`
  只在 data load/store 绑定且启用时走 L1D；默认 reference path 不变，且继续
  保持 RAM-only、write-through、no dirty write-back、MMIO / side-effect bypass、
  atomic / fence 保守处理，以及 DMA 不透明 coherence 的边界。
- 主线 `Wave 5` 的 `Slice C / L1D opt-in observation + guardrail` 已完成：默认关闭
  的 L1D 现在有顶层 `l1_data_cache` debug snapshot 只读 counters，
  `run_debug_cli_probe.py --l1d` 可显式打开 L1D 并输出 `l1d-cache:` 摘要；默认
  执行路径不变，既有 `shadow_cache` 字段语义不变。
- 主线 `Wave 5` 的 `Slice D / L1D hardening` 已完成：本轮固定跨 cache line
  store bypass 后失效重叠 line、store miss write-through + no-allocate、fault
  refill 不污染 cache line，以及 non-cacheable / side-effect / unmapped、
  atomic、page-walk、instruction fetch 和默认路径继续 bypass / 默认关闭的合同。

## 当前优先级

1. 如继续推进 `Wave 5`，先新建活跃计划；下一刀可以围绕更窄的 L1D 观察消费、
   frontend 只读展示或继续 hardening 展开，不得直接跳 write-back /
   DMA coherence / multicore / JIT / I-cache / cache maintenance instruction。
2. AI accelerator 的 `INT4 / training / MobileNet / Linux-facing NPU driver /
   real DMA overlap / multi outstanding queue` 等后续专项不得改写主线 `Wave 5`
   定位。
3. Linux `timerfd-one-shot-readback-ok` 作为 `Wave 3` 冻结边界进入守成；除非真实
   runtime 重新暴露新 blocker，不再继续向当前 fourth-stage smoke 追加同类
   `open-fd / mmap / pipe / futex / socketpair / openat2 / pidfd / signalfd /
   renameat2 / eventfd / epoll / sendmsg / recvmsg / SCM_RIGHTS / copy_file_range /
   splice / statx / inotify / timerfd` 微分支。
4. 把 `xv6` shell、Linux probe、`kernel_alpha`、`pipeline`、debug CLI 和
   现有回归矩阵守成稳定 guardrail。
5. 继续积累 `shadow_cache / observation / representative workload` 证据；后续
   `Wave 5` follow-up 也不得提前承诺 write-back、DMA coherence、multicore 或 JIT。

## 关键历史节点

- `2026-04-29`
  - `Wave 3` 按当前真实实现收口：Linux checkpoint 线冻结到
    `timerfd-one-shot-readback-ok`，后续不再继续扩同类 Linux syscall breadth；
    `Wave 4` 随后激活并完成 AI accelerator 三段切片。
  - 同日为主线 `Wave 4` 打开 AI accelerator 三段切片计划：先做 `bounded dynamic
    shape / workload`，再做 profile / frontend 观察面，最后把 `Softmax + tiny
    static attention` 作为可降级 stretch；训练、`INT4`、MobileNet、Linux-facing
    NPU driver 等剩余远期目标后移到 AI accelerator 后续专项阶段。
  - 主线 `Wave 4` 的 AI accelerator 切片 A 已完成：`bounded dynamic shape`
    已从 `dynamic GEMM / FC-like` 扩到现有 op family 的正向或 fail-closed 合同，
    并新增 `dynamic_tiny_model` 动态小模型 workload。
  - 主线 `Wave 4` 的 AI accelerator 切片 B 已完成：profile / timing attribution
    已把 tile setup 归到 `stall_cycles`，host profile 与 guest debug counters
    稳定暴露 aggregate 字段；frontend 的 `guest_ai_accel_demo` workload card 和
    平台组 AI accelerator panel 已接入 `KMVAI`、queue、scratchpad、DMA bytes、
    device / dma / compute / stall cycles 与 utilization 等只读字段。
  - 主线 `Wave 4` 的 AI accelerator 切片 C stretch 已完成：新增静态 `fp32`
    row-wise `Softmax` 和 `tiny_attention_static` workload，固定验证
    `gemm -> softmax -> gemm` 的最小 attention-like 闭环；它不是完整 attention /
    Transformer runtime。
  - 主线 `Wave 4` 变更已提交为 `b11d10a`
    `feat(主线): 收口 Wave 4 AI accelerator 切片`。
  - 主线 `Wave 5 / cache / memory-system` 的 `Slice A / signal + contract` 已完成并
    归档：
    [../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-a-signal-contract-plan](../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-a-signal-contract-plan)。
    这一刀新增 pipeline-side `xv6 --backend pipeline` memory observation guardrail，
    继续保持 Linux 真实 `Image` runtime 为 opt-in，并固定后续最小 L1D 的
    RAM-only、write-through、no dirty write-back、bypass side-effect region 与 DMA
    不透明 coherence 合同。
  - 同日完成主线 `Wave 5` `Slice B / minimal executable L1D` 并归档：
    [../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-b-minimal-l1d-plan](../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-b-minimal-l1d-plan)。
    这一刀新增默认关闭、可显式启用、RAM-only、write-through 的最小 L1D 执行模型，
    只接入 data load/store；instruction fetch、page walk、atomic、MMIO、
    unmapped 和 side-effect region 继续 bypass L1D。
  - 同日完成主线 `Wave 5` `Slice C / L1D opt-in observation + guardrail` 并归档：
    [../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-c-l1d-observation-guardrail-plan](../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-c-l1d-observation-guardrail-plan)。
    这一刀新增顶层 `l1_data_cache` debug snapshot 只读 counters，并给
    `run_debug_cli_probe.py` 增加显式 `--l1d` guardrail；默认路径仍关闭 L1D，
    既有 `shadow_cache` 输出合同不变。
  - 同日完成主线 `Wave 5` `Slice D / L1D hardening` 并归档：
    [../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-d-l1d-hardening-plan](../plan/history_plan.md#mainline-wave5-cache-memory-system-slice-d-l1d-hardening-plan)。
    这一刀固定跨 cache line store bypass 后失效重叠 line、store miss
    write-through + no-allocate、non-cacheable / side-effect / unmapped / refill
    fault 不污染 cache state，以及 atomic、page-walk、instruction fetch 继续
    bypass L1D；默认 `make test` / `make test-pipeline` 仍不打开 L1D。
  - 这次收口把 `xv6 / Linux` pipeline-side memory signal 明确降级为
    `Wave 5 / cache` 前置证据，而不是阻塞 `Wave 4` 的硬门槛；当前 `Wave 4`
    依赖的观测证据来自 pipeline vector CNN、functional `xv6`、functional
    `linux_proto` dummy-payload 以及现有 pipeline guest demos。
  - Linux fourth-stage baseline 已推进到
    `timerfd-one-shot-readback-ok`。
  - 这一轮先后新增验证了：
    `pidfd_open + ppoll + waitid(P_PIDFD)` 新式进程句柄合同，
    `rt_sigprocmask + signalfd4` 的 `SIGCHLD` 阻塞 / 消费 / child-exit readback，
    `renameat2(RENAME_EXCHANGE)` 的原子路径交换 / reopen / readback，
    `eventfd2 + epoll_create1 + epoll_ctl + epoll_pwait` 的 ready-queue / counter readback，
    `sendmsg + recvmsg + SCM_RIGHTS` 的 ancillary fd passing / parent readback，
    `copy_file_range` 的 kernel-side file copy / dst readback，
    `splice(file -> pipe -> file)` 的 zero-copy transfer / dst readback，
    `statx(relative dirfd)` 的 metadata ABI / size-mode readback，
    `inotify` 的 close-write fs-notify queue event 合同，
    以及 `timerfd` 的 one-shot timer queue expiration 合同。
  - 这说明 Linux checkpoint 线已经给出足够高信号的后续 userland contract；
    当前继续扩同类 marker 的边际收益低于切到 `Wave 4`。
- `2026-04-28`
  - Linux fourth-stage baseline 已推进到
    `openat2-self-beneath-readback-ok`。
  - 这一轮先后新增验证了：
    `pipe2` 匿名 IPC、`futex + MAP_SHARED|MAP_ANONYMOUS` 跨进程同步、
    `AF_UNIX socketpair` child-write / parent-readback / EOF，
    以及 `openat2(2)` dirfd-relative self-reopen / ELF header readback。
  - 其中 `socketpair` 和 `openat2` 说明 Linux checkpoint 线仍在持续产出新的高信号
    userland contract，而不是回到匿名 IPC 或旧 `open-fd / mmap` 的同类微分支。
  - 这也说明 Linux checkpoint 线当时仍在持续产出高信号边界，当时还不到切
    `Wave 4` 的时机。
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

- 仓库默认位置仍不携带真实 Linux `Image`；因此 `timerfd` 冻结点的发布级 runtime
  断言仍需要开发者显式提供 `Image` 后重新运行 opt-in guardrail。默认门禁只能证明
  harness、构建、marker 和 dummy-payload observation 没有回退。
- pipeline-side `xv6` memory observation 已有稳定 guardrail，但它只是
  `shadow_cache` / memory profile 信号，不是 pipeline 完整 boot `xv6` 的支持声明。
- `Wave 5` `Slice B / C / D` 完成不代表完整 cache / DMA / multicore 已完成；当前
  只落地默认关闭、RAM-only、write-through、no dirty write-back 的最小 L1D 执行模型，
  显式 opt-in 的 L1D debug/probe 观察面，以及若干 L1D 边界 hardening 合同。
- `Softmax + tiny static attention` 已作为 `Wave 4` 后段 stretch 完成，但它只覆盖
  最小静态 `fp32` row-wise softmax 和极小 attention-like profile 闭环；不要把它
  写成完整 attention、动态 sequence length、KV-cache 或 Transformer runtime。
- `debug/frontend`、`pipeline` 和 guest runtime 都已形成可维护边界，但后续
  仍要避免真实 bug 修复把职责重新揉回大文件。

## 下一步

1. 如继续推进 `Wave 5`，先新建活跃计划；下一刀候选是更窄的 L1D 观察消费、
   frontend 只读展示或继续 hardening，不直接扩成 write-back / DMA coherence。
2. 继续把 pipeline-side `xv6` memory observation、functional `xv6`、Linux
   dummy/probe、pipeline `vector_cnn` 和现有 debug CLI 输出作为 cache 前置 guardrail。
3. AI accelerator 后续若继续推进 `INT4 / training / MobileNet / Linux-facing NPU
   driver / real DMA overlap / multi outstanding queue`，应另开本方向专项 plan，并
   明确不占用主线 `Wave 5`。
4. Wave 4 AI accelerator 的完成记录统一见
   [../plan/history_plan.md#mainline-wave4-ai-accelerator-slices-plan](../plan/history_plan.md#mainline-wave4-ai-accelerator-slices-plan)。
5. 显式提供真实 Linux `Image` 时，补跑 `timerfd-one-shot-readback-ok` runtime
   guardrail；未提供 `Image` 时，不把该项写成默认已证明。
6. 继续守住 `xv6` shell、Linux probe、`kernel_alpha`、debug CLI、
   `make test` 和 `make test-pipeline` 这些稳定 guardrail。
7. 不继续向当前 Linux fourth-stage smoke 追加同类 syscall 微分支；如果真实 runtime
   暴露新 blocker，再按 blocker 驱动回补最窄 Linux guardrail。

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
