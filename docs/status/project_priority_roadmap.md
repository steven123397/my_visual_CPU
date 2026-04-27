# 当前项目优先级路线图

## 文档定位

本文档只保留当前仍然开放的优先级判断，不重复记录已经完成的整段执行过程。

它不替代 [mainline_status.md](mainline_status.md)。`mainline_status` 负责回答“现在主线是什么状态”，本文档负责回答“下一轮最值得做什么，以及什么不该抢跑”。

## 关联文档

- 相关设计：
  - [../design/regression_completion_criteria.md](../design/regression_completion_criteria.md)
  - [../design/debug_frontend_integration.md](../design/debug_frontend_integration.md)
  - [../design/minimal_interactive_os_design.md](../design/minimal_interactive_os_design.md)
  - [../design/phase3_ooo_execution_model_design.md](../design/phase3_ooo_execution_model_design.md)
  - [../design/pipeline_speculation_contracts.md](../design/pipeline_speculation_contracts.md)
  - [../design/vector_ml_workload_direction_design.md](../design/vector_ml_workload_direction_design.md)
  - [../design/npu_tpu_accelerator_direction_design.md](../design/npu_tpu_accelerator_direction_design.md)
  - [../design/phase4_preparation_design.md](../design/phase4_preparation_design.md)
  - [../design/future_expansion_roadmap_design.md](../design/future_expansion_roadmap_design.md)
  - [../design/xv6_linux_jit_mainline_design.md](../design/xv6_linux_jit_mainline_design.md)
- 相关状态：
  - [mainline_status.md](mainline_status.md)
  - [kernel_alpha_status.md](kernel_alpha_status.md)
  - [xv6_linux_jit_status.md](xv6_linux_jit_status.md)
- 当前计划：
  - 当前无活跃计划；`C1 / P4-prep-2 memory observation / shadow cache` 已归档到 [../plan/history_plan.md#phase4-prep2-memory-observation-shadow-cache-plan](../plan/history_plan.md#phase4-prep2-memory-observation-shadow-cache-plan)
- 已完成计划归档：
  - [../plan/history_plan.md#phase4-prep2-memory-observation-shadow-cache-plan](../plan/history_plan.md#phase4-prep2-memory-observation-shadow-cache-plan)
  - [../plan/history_plan.md#xv6-linux-jit-wave1-plan](../plan/history_plan.md#xv6-linux-jit-wave1-plan)
  - [../plan/history_plan.md#phase4-prep1-bus-memory-region-plan](../plan/history_plan.md#phase4-prep1-bus-memory-region-plan)
  - [../plan/history_plan.md#vector-v4-plan](../plan/history_plan.md#vector-v4-plan)
  - [../plan/history_plan.md#vector-frontend-visualization-plan](../plan/history_plan.md#vector-frontend-visualization-plan)
  - [../plan/history_plan.md#spike-external-differential-validation-plan](../plan/history_plan.md#spike-external-differential-validation-plan)

## 当前判断

- `P0` correctness 修补、`P1` 结构收口和 `P2` 首轮验证补洞都已经完成；自 `2026-04-21` 起，当前已经不再停留在“继续评估要不要切主线”，而是正式激活了标准 OS bring-up 切换线。
- 当前主线的优先级判断已经改为：先落 `RV64A + virtio + CSR / privilege 补全 + xv6-riscv workload harness / bring-up`，并让这轮结构决策直接服务后续 `Linux` 与 `JIT / DBT`。
- 默认延续线没有被丢弃：`V4`、`P4-prep-1`、`kernel_alpha`、`debug/frontend` 与既有回归矩阵继续作为当前主线的 guardrail、观测基础与回归支架。
- `future_expansion_roadmap_design.md` 仍然是路线菜单；当前真正已经激活的执行方案，以 [xv6_linux_jit_status.md](xv6_linux_jit_status.md)、[mainline_status.md](mainline_status.md) 和 [../plan/history_plan.md#xv6-linux-jit-wave1-plan](../plan/history_plan.md#xv6-linux-jit-wave1-plan) 为准。
- `2026-04-23` 也已把独立 `MMIO NPU / TPU-like` tensor accelerator 方向收口成正式 design / status，并完成 Wave 1 foundation 归档：它采用静态子图、`scratchpad + DMA`、host / guest 共用设备 ABI 的路线，但当前仍只作为未来候选方向保留，不覆盖 `xv6 / Linux` 这条已激活主线。
- `2026-04-27` 已完成 `C1 / P4-prep-2 memory observation / shadow cache` 第一刀；它仍然只负责观测与 workload 证据收集，不改变 guest 可见语义，也不替代当前 `xv6 / Linux` 主线。
- `2026-04-22` 第一轮 A / B / C / D foundation 与首轮 B / C post-integration follow-up 都已进入主工作树；同日进一步的 A / B bug-driven follow-up 先把 `xv6` 推过旧的 early-boot trap，随后又在真实 `virtio-blk` board path 上稳定到 shell，并落下了 Linux-facing `flat/payload/set_gpr + linux_proto profile` foundation；后续同日的 Linux bring-up follow-up 又补上最小 `linux_sbi_shim`、PLIC contiguous source window contract，以及 repo-generated `mycpu_virt.dtb` `chosen/cmdline/timebase` 合同，把真实 Linux 推到 `Unpacking initramfs...`、`devtmpfs: initialized` 与 `xor: measuring software checksum speed` checkpoint。`2026-04-24` 这条线又进一步补上 repo-generated 最小 `rootfs.cpio` `/init` fallback，把默认工作区的外部缺口从 `Image + initrd` 收窄到 `Image`；同日 `linux_sbi_shim` 也补上 early `M/S` trap UART 诊断，functional 路径补上最小 `RV64C`，modern `virtio-mmio` 也补齐 `VIRTIO_F_VERSION_1`。`2026-04-25` 又补上 `LINUX_PROTO_ROOTFS_MODE=block` 与 repo-generated `rootfs.ext4` fallback，并把 repo-generated block-rootfs `/init` + post-init 路径收口到 `mycpu linux initrd: stage=console-opened`、`stage=rootfs-rw-ok`、`stage=proc-readable`、`stage=sys-readable`、`stage=execve-post-init`、`mycpu linux userland: stage=file-readable` 与 `stage=rootfs-rw-roundtrip-ok`；`2026-04-26` 又先把同一条 second-stage userland baseline 继续推进到 `stage=fork-child-wrote`、`stage=parent-wait4-ok` 与 `post-init reached`，随后再补上 `stage=execve-third-stage`、`stage=mkdir-chdir-ok`、`stage=nested-file-roundtrip-ok`、`stage=getdents64-nested-visible`、`stage=fstatat-nested-stat-ok`、`stage=renameat2-syscall-ok`、`stage=renameat2-nested-ok`、`stage=renameat2-dirent-updated`、`stage=renameat2-cleanup-ok` 与 `stage=third-stage-reached` 的 third-stage `execve() + path-resolution + VFS metadata` chain；`2026-04-27` 又把同一条链继续推进到 `stage=unlinkat-parent-dirent-gone`、`stage=mkdirat-dir-name-reusable` 与 `stage=mkdirat-reused-dir-empty`，把 `renameat2` cleanup 之后父目录项已消失、同名目录已可重新创建、且重建目录对旧 `nested.txt/renamed.txt` 已是空视图的最小合同也冻结下来。因此“下一轮最值得做什么”的答案已进一步收敛到：继续用 `xv6` shell 当 guardrail，并把真实 Linux 从“最小 `/init` + rootfs/proc/sys + post-init file-read/write + process lifecycle + multi-stage exec + 目录/路径解析 + 目录项遍历 + 目录项元数据读回 + rename/unlink 后目录项视图更新 + 目录名复用 + 重建目录空视图 已闭环”继续推到更后的用户态 checkpoint。

## 当前优先级

### 1. 常态维护仍是默认前提

- 继续维护 reference correctness 矩阵，不让 illegal / MMIO / ELF / CSR / Sv39 合同回退。
- 继续守住 `kernel_alpha` 十条 guest 基线、`guest_supervisor_demo` 与当前 debug / frontend 链路。
- 继续把新增 bug 的最小持久回归补到已有门禁中，而不是重新打开低收益的大规模回归扩面。

### 2. 用 `xv6` shell 守住真实 `virtio` board path，同时把近端重点转向 Linux-facing foundation

- PLIC source wiring、`Machine` block transport 选择与 `mycpu_virt` board profile 切换都已完成，`xv6` 已开始真实消费 `virtio-mmio + virtio-blk` contract。
- 当前最窄、最值钱的下一步不再是继续证明 `xv6` 能不能到 shell，而是把已经稳定的 shell/userland/filesystem 路径作为 guardrail，用它约束后续 Linux-facing 变更。
- 这条线仍然应该优先保持“可复用的 external workload 入口 + 稳定 smoke”，而不是为了快跑演示去写一次性特判。
- `Linux` 后续会直接复用这层 harness / profile 结构；当前已经落下 generic `flat/payload/set_gpr`、`linux_proto`、最小 `linux_sbi_shim`、repo-generated `dtb/chosen/cmdline/timebase` contract、repo-generated 最小 `rootfs.cpio` `/init` fallback，以及 `LINUX_PROTO_ROOTFS_MODE=block` 下的 repo-generated `rootfs.ext4` fallback。当前真实 `Image + initrd` 已能推进到 `Unpacking initramfs...`、`devtmpfs: initialized`、`workingset`、`jitterentropy`、`xor` 与 `/init reached`；而真实 `Image + block-rootfs` 也已推进到 `EXT4-fs`、`VFS: Mounted root`、`devtmpfs: mounted`、`Run /init as init process`、`mycpu linux initrd: stage=console-opened`、`stage=rootfs-rw-ok`、`stage=proc-readable`、`stage=sys-readable`、`stage=execve-post-init`、`mycpu linux userland: stage=file-readable`、`stage=rootfs-rw-roundtrip-ok`、`stage=fork-child-wrote`、`stage=parent-wait4-ok`、`stage=execve-third-stage`、`stage=mkdir-chdir-ok`、`stage=nested-file-roundtrip-ok`、`stage=getdents64-nested-visible`、`stage=fstatat-nested-stat-ok`、`stage=renameat2-syscall-ok`、`stage=renameat2-nested-ok`、`stage=renameat2-dirent-updated`、`stage=renameat2-cleanup-ok`、`stage=unlinkat-parent-dirent-gone`、`stage=mkdirat-dir-name-reusable`、`stage=mkdirat-reused-dir-empty`、`stage=mkdirat-reused-dir-dot-only`、`stage=mkdirat-reused-dir-parent-stat-ok`、`stage=third-stage-reached` 与 `post-init reached`。同日 probe harness 侧的长 UART wait 也已收口成增量搜索，`RVC` 与 modern `virtio-mmio` 探测问题也已关闭；下一步更该沿这条真实 boot path 收口 multi-stage post-init exec + path-resolution + getdents64 目录遍历 + `fstatat` 元数据读回 + `renameat2`/`unlinkat` 目录项可见性 + `mkdirat` 名字复用 + 重建目录空视图之后的更后 userland checkpoint，而不是继续在 `xv6` 自身 shell 用例上横向扩面。

### 3. A / B 两条 contract 线都进入 bug-driven 支撑位

- `virtio` 这条线当前不再缺 transport / queue / board wiring；后续只随着 `xv6` 暴露的新平台缺口补最小 contract，不主动扩大更多 `virtio` 设备或额外 platform 特性。
- A 线同样不再是“先要不要合”的问题，而是随着真实 workload 暴露新的缺口，最小化补上 `pmp*`、`menvcfg`、`stimecmp` 等后续 contract；当前这类新缺口更可能来自后续 Linux bring-up，而不是 `xv6` 到 shell 之前的路径。
- 这两条线都要继续保证单一事实来源：ISA 语义仍归 `InstructionSemantics`，platform contract 仍归统一 `Machine / device` 边界。
- 一旦新缺口能被更窄的 asm / host smoke 固化，就继续沿当前 hardening 路线补最小回归。

### 4. observation / profile foundation + 默认延续线 guardrail 仍是并行必须项

- 当前主线虽然切到标准 OS bring-up，但 `V4`、`P4-prep-1`、`kernel_alpha`、`interactive_os` 和 debug / pipeline workload 仍然要继续守住。
- 当前更值得并行保留的，不只是单纯的 `V4` hardening，还包括面向 `Linux / JIT / DBT` 的 execution profile / observation foundation；这条线现在也已经开始给 `run_debug_cli_probe`、payload/gpr seed summary 和 Linux boot profile dry-run 提供读侧支架。
- 这条线的目标是为后续 hot-path 定位、memory behavior 观察、cache 评估和 JIT 候选路径选择提供证据，而不是现在就抢跑真实 JIT。
- 当前 D 线已经整合进主线，并把 `execution_profile_smoke` 接进默认 `make test` / `make test-pipeline`；`C1 / P4-prep-2` 也已把 shadow-cache 观测接到 `execution_profile`、debug JSON 和 probe 摘要。下一步更应该用这组读侧观测锁住新出现的 `xv6 / virtio / Linux` 路径，而不是反向抢占 A/B/C 的 contract ownership。

### 5. Spike 外部差分与更激进 `Phase 3` 继续维持条件触发

- 当前 Spike 线最有价值的角色，仍然是在 reference correctness 出现疑点时提供外部 oracle。
- 当前没有证据支持优先重开更激进的 `issue / replay / speculation`；即使以后重开，也应先有真实 workload hotspot，再看是否值得先做 issue decoupling。

## 当前明确不优先做的事

1. 不直接抢跑 `Linux` 完整 bring-up。
2. 不在当前 foundation 还没站稳前，直接实现 `JIT / 动态二进制翻译`。
3. 不为 `xv6` 先造一批短寿命、只服务单一 demo 的 special case 或 Makefile 特判。
4. 不在当前单发射 + coarse replay 基线上，继续主动扩大更激进的 `Phase 3` issue / replay / memory disambiguation。
5. 不在 `xv6` foundation 与 workload 观测都还不稳定之前，直接抢跑更重的 `Phase 4` cache / DMA / multicore / coherence。
6. 不把 `debug/frontend` 顺势扩成更大的产品功能面或通用调试器。
7. 不把“真实 `virtio` board path 已到 shell”或“Linux 已推进到最小 block-rootfs `console-opened -> rootfs-rw-ok -> proc-readable -> sys-readable -> /init reached -> file-readable -> rootfs-rw-roundtrip-ok -> fork-child-wrote -> parent-wait4-ok -> execve-third-stage -> mkdir-chdir-ok -> nested-file-roundtrip-ok -> getdents64-nested-visible -> fstatat-nested-stat-ok -> renameat2-syscall-ok -> renameat2-nested-ok -> renameat2-dirent-updated -> renameat2-cleanup-ok -> unlinkat-parent-dirent-gone -> mkdirat-dir-name-reusable -> mkdirat-reused-dir-empty -> mkdirat-reused-dir-dot-only -> mkdirat-reused-dir-parent-stat-ok -> third-stage-reached -> post-init reached`”误判成“Linux bring-up 已接近完成”；当前离更完整的 block-rootfs 用户空间、真实 post-init workload 和更后 userland checkpoint 仍有明显距离，而不是 `xv6` shell 自身继续扩面就能自然解决。
8. 不把独立 `NPU / TPU-like` AI accelerator 模块误读成当前近端主线；当前它已经完成 Wave 1 foundation 和首轮 hardening，但仍是未来候选方向，真正继续扩大时仍应单开对应 `status / plan`，而不是并入当前 `xv6 / Linux` 主线执行。

## 如需新开计划

1. 当前主线的 Wave 1 已归档到 [../plan/history_plan.md#xv6-linux-jit-wave1-plan](../plan/history_plan.md#xv6-linux-jit-wave1-plan)；后续细化如果仍需要新专项，再按 status / history 的口径继续切分。
2. 如果 Workstream A / B / C / D 任一条线后续需要再细分成第二层专项，新增计划也应继续挂到 [xv6_linux_jit_status.md](xv6_linux_jit_status.md)，不要重新分裂事实来源。
3. 当前已经验证真实 `Image + initrd + repo-generated dtb/chosen/cmdline + linux_sbi_shim` 路径能稳定进入 Linux `devtmpfs: initialized` 与 initramfs unpack 之后的更后 boot 阶段，也已验证 `Image + repo-generated rootfs.ext4 + block bootargs` 路径能稳定推进到 `mycpu linux initrd: stage=rootfs-rw-ok`、`stage=proc-readable`、`stage=sys-readable`、`stage=execve-post-init`、`mycpu linux userland: stage=file-readable`、`stage=rootfs-rw-roundtrip-ok`、`stage=fork-child-wrote`、`stage=parent-wait4-ok`、`stage=execve-third-stage`、`stage=mkdir-chdir-ok`、`stage=nested-file-roundtrip-ok`、`stage=getdents64-nested-visible`、`stage=fstatat-nested-stat-ok`、`stage=renameat2-syscall-ok`、`stage=renameat2-nested-ok`、`stage=renameat2-dirent-updated`、`stage=renameat2-cleanup-ok`、`stage=unlinkat-parent-dirent-gone`、`stage=mkdirat-dir-name-reusable`、`stage=mkdirat-reused-dir-empty`、`stage=mkdirat-reused-dir-dot-only`、`stage=mkdirat-reused-dir-parent-stat-ok`、`stage=third-stage-reached` 与 `post-init reached`；但只有在进一步把这个最小 post-init `exec + file + writable-rootfs + process lifecycle + multi-stage exec + path-resolution + getdents64 目录遍历 + fstatat 元数据读回 + renameat2/unlinkat 目录项可见性 + mkdirat 名字复用 + 重建目录空视图` baseline 之后的下一处 userland checkpoint 或 blocker 冻结下来，才考虑新开更完整的 `Linux` bring-up 专项计划。
4. 只有在 profile / hot-path / workload 证据足够明确之后，才考虑新开 `JIT / DBT` 专项计划。
5. 如果未来出现真实 `Phase 3` stall hotspot、`debug/frontend` bug 或 Spike correctness 缺口，再围绕具体问题单开最小专项计划。
