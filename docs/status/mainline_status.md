# 主线状态

## 文档定位

本文档只记录当前 `main` 分支的稳定快照、少量关键历史节点、当前仍有效的风险和下一步。

执行过程、阶段性 checklist 和专项落地细节统一归档到 [../plan/history_plan.md](../plan/history_plan.md)；更细的优先级判断见 [project_priority_roadmap.md](project_priority_roadmap.md)。

## 关联文档

- 相关设计：
  - [../design/regression_completion_criteria.md](../design/regression_completion_criteria.md)
  - [../design/debug_frontend_integration.md](../design/debug_frontend_integration.md)
  - [../design/minimal_interactive_os_design.md](../design/minimal_interactive_os_design.md)
  - [../design/phase3_ooo_execution_model_design.md](../design/phase3_ooo_execution_model_design.md)
  - [../design/pipeline_speculation_contracts.md](../design/pipeline_speculation_contracts.md)
  - [../design/vector_ml_workload_direction_design.md](../design/vector_ml_workload_direction_design.md)
  - [../design/phase4_preparation_design.md](../design/phase4_preparation_design.md)
  - [../design/future_expansion_roadmap_design.md](../design/future_expansion_roadmap_design.md)
  - [../design/xv6_linux_jit_mainline_design.md](../design/xv6_linux_jit_mainline_design.md)
- 相关状态：
  - [project_priority_roadmap.md](project_priority_roadmap.md)
  - [kernel_alpha_status.md](kernel_alpha_status.md)
  - [xv6_linux_jit_status.md](xv6_linux_jit_status.md)
- 当前计划：
  - 当前无活跃计划；Wave 1 与 `C1 / P4-prep-2` 已归档到 [../plan/history_plan.md](../plan/history_plan.md)
- 已完成计划归档：
  - [../plan/history_plan.md#phase4-prep2-memory-observation-shadow-cache-plan](../plan/history_plan.md#phase4-prep2-memory-observation-shadow-cache-plan)
  - [../plan/history_plan.md#xv6-linux-jit-wave1-plan](../plan/history_plan.md#xv6-linux-jit-wave1-plan)
  - [../plan/history_plan.md#phase4-prep1-bus-memory-region-plan](../plan/history_plan.md#phase4-prep1-bus-memory-region-plan)
  - [../plan/history_plan.md#vector-v4-plan](../plan/history_plan.md#vector-v4-plan)
  - [../plan/history_plan.md#vector-frontend-visualization-plan](../plan/history_plan.md#vector-frontend-visualization-plan)
  - [../plan/history_plan.md#spike-external-differential-validation-plan](../plan/history_plan.md#spike-external-differential-validation-plan)
  - [../plan/history_plan.md#p2-validation-gap-backfill-round-2](../plan/history_plan.md#p2-validation-gap-backfill-round-2)

## 当前快照

- 当前仓库已经是一个已可运行的模拟器原型，不是纯设计稿。
- `phase1-stable`（`283aee6`）对应的 Phase 1 核心 bring-up 冻结基线已经形成，`functional` reference path、`kernel_alpha` 正向与 9 条负向回归都已稳定接通。
- `pipeline`、`make test-pipeline`、`debug_session / protocol`、本地 Node 调试服务和浏览器前端都已经正式接入主线，不再是待合入功能。
- `P1` 结构收口与 `P2` 首轮验证补洞已经完成；当前不再只是继续做“默认延续线收口”，而是自 `2026-04-21` 起正式把标准 OS bring-up 切换线提升为当前 active program。
- 当前近端主线已经明确切到 `RV64A + virtio 平台 + CSR / privilege 补全 + xv6-riscv bring-up`，对应的当前设计、状态和归档摘要分别见 [../design/xv6_linux_jit_mainline_design.md](../design/xv6_linux_jit_mainline_design.md)、[xv6_linux_jit_status.md](xv6_linux_jit_status.md) 和 [../plan/history_plan.md#xv6-linux-jit-wave1-plan](../plan/history_plan.md#xv6-linux-jit-wave1-plan)。
- 此次切主线并不放弃默认延续线：`V4`、`P4-prep-1`、`kernel_alpha`、`debug/frontend`、Spike 外部差分和现有回归矩阵继续作为新主线的 correctness / observation guardrail。
- `向量扩展 + ML workload` 仍保留为默认延续线和代表性 workload corpus，不再是当前唯一主线，但仍继续为 profile / observation 和更后续 `Phase 4` 判断提供信号。
- `Phase 4` 当前只正式打开准备性入口：`P4-prep-1` 已完成，`bus / memory region` 已成为统一事实来源；`C1 / P4-prep-2 memory observation / shadow cache` 第一刀也已完成，当前只做观测和 workload 证据收集，不改变 guest 可见语义，也不意味着完整 `cache / DMA / multicore` 已进入主线实施阶段。
- Spike 外部差分验证已经形成一条独立离线 oracle，当前处于维护态，主要服务 reference correctness 疑点排查，而不是新的默认主门禁。
- `2026-04-22` 当前主线切换的第一轮 A / B / C / D foundation 已按默认顺序整合到主工作树：A 已把 `RV64A + CSR / privilege` contract 变成主线事实来源，B 已接入 `virtio-mmio + virtqueue + virtio-blk` foundation，C 已把 external `xv6-riscv` workload harness 与刷新后的 `xv6_boot_smoke` 接进主线，D 已把 `execution_profile` 与 profile guardrail 接进默认回归。
- 同日也已完成第一轮 B / C post-integration follow-up：PLIC 现按 `xv6` 约定把 `virtio=1`、`UART=10` 分开接线，`Machine` / CLI / debug CLI / workload probe 已支持 block transport 选择，`mycpu_virt` board 已切到 `virtio-blk`，`xv6_boot_smoke` / `run-workload-xv6` 现已走真实 `virtio` board path。
- 同日第一轮 post-integration correctness findings 也已关闭：普通 `store` 现在会正确打破 `LR/SC` reservation，translation-fault memory access 也会进入 `execution_profile` 的 fault 统计；随后同日的 bug-driven A / B follow-up 先把 `xv6` 推过旧的 early-boot trap，再把真实 `virtio-blk` board path 推到 shell，并落下了 Linux-facing `flat/payload/set_gpr + linux_proto profile` foundation。主线优先级也因此继续收敛到：把 `xv6` shell 守成稳定 guardrail，并把真实 Linux 资产与 `DTB/chosen/cmdline` 接到现有 foundation 上。
- `2026-04-24` 又补上一层更窄的 Linux harness follow-up：`linux_proto` 的 repo-generated `dtb` 现在不再在缺 `rootfs.cpio` 时提前被 Make 依赖卡死，同日也补上了 repo-generated 最小 `rootfs.cpio` `/init` fallback；因此 `make run-workload WORKLOAD_NAME=linux_proto` 会继续进入 probe 入口，并在默认工作区只针对缺失 `Image` 统一 fail-closed。当前主线因此能更干净地区分“资产未落位”和“真实 Linux bring-up blocker”。
- 同日也已把 Linux bring-up 的两处近端兼容性缺口关掉：functional 路径现已补上最小 `RV64C`，而 modern `virtio-mmio` 也已补齐 `VIRTIO_F_VERSION_1`。在本地 `CONFIG_RISCV_ISA_C=y` Linux `Image` 上，当前 repo-generated `dtb + initrd` 路径已能稳定通过 `virtio_blk` 探测、枚举 `vda`，并继续推进到 `/init reached`。
- 同日也把下一步 block-rootfs 入口收口成显式 harness 合同：`linux_proto` profile 现支持 `LINUX_PROTO_DISK` / `LINUX_PROTO_BOOTARGS` alias，`mycpu_virt.dts` 会按当前 bootargs 强制重生成。当前默认 `disk=none` 路径下，Linux 会稳定枚举 `0 B` 的 `vda`；因此更后的真实 blocker 已收敛成“提供非空磁盘镜像 + 切 bootargs 到 block root”，而不再是 `RVC` 或 modern virtio 探测。
- `2026-04-25` 又把 block-rootfs bring-up 本身收口成 repo-generated fallback：`linux_proto` 现支持 `LINUX_PROTO_ROOTFS_MODE=block`，会在缺外部 Linux 磁盘镜像时默认生成最小 `rootfs.ext4`，并自动切到 `root=/dev/vda rw rootfstype=ext4 rootwait init=/init`；同日 `mycpu_virt.dts` 在 block 模式下也改成只按实际 payload 的 initrd 生成 `chosen.initrd`，避免旧 `rootfs.cpio` 大小泄漏进 DTB。随后 repo-generated `/init` 也已收口成可观察的最小用户态闭环：它现在会容忍 block-rootfs 下已预先挂载的 `devtmpfs`，并通过 staged marker 依次暴露 `stage=console-opened`、`stage=rootfs-rw-ok`、`stage=proc-readable` 与 `stage=sys-readable`；同日又把 repo-generated second-stage ELF `/post-init-smoke` 接进同一条 rootfs，让 `/init` 在最小 smoke 之后显式 `execve()` 到真实 post-init 用户态。基于本地 `CONFIG_RISCV_ISA_C=y` Linux `Image`，当前 block-rootfs 路径已稳定推进到 `virtio_blk virtio0: [vda] 16384 ...`、`EXT4-fs (vda)`、`VFS: Mounted root`、`devtmpfs: mounted`、`Run /init as init process`、`mycpu linux initrd: stage=console-opened`、`mycpu linux initrd: stage=rootfs-rw-ok`、`mycpu linux initrd: stage=proc-readable`、`mycpu linux initrd: stage=sys-readable`、`mycpu linux initrd: /init reached`、`mycpu linux initrd: stage=execve-post-init` 与 `mycpu linux userland: post-init reached`；因此活跃 blocker 已进一步从“缺非空磁盘镜像”与“最小 `/init` 输出不完整”收窄到 `post-init reached` 之后的下一处更后 userland checkpoint。
- `2026-04-27` 又把这条 block-rootfs second-stage userland baseline 继续推进一刀：在既有 `renameat2` 目录项更新与 cleanup contract 之后，repo-generated third-stage ELF `/post-init-exec-smoke` 现在会先回到父目录再做一次 `getdents64` 目录遍历，要求已被 `unlinkat(AT_REMOVEDIR)` 清掉的 `post-init-dir-smoke` 目录项不再可见，并通过 `stage=unlinkat-parent-dirent-gone` 暴露到 UART；随后又立刻复用同一个目录名重新 `mkdirat()`，确认该名字已经可以重新分配，并通过 `stage=mkdirat-dir-name-reusable` 暴露到 UART；最后再 `chdir()` 进入这个重建目录，确认旧的 `nested.txt` / `renamed.txt` 都已不可见，并通过 `stage=mkdirat-reused-dir-empty` 暴露到 UART。当前会稳定暴露 `mycpu linux userland: stage=file-readable`、`stage=rootfs-rw-roundtrip-ok`、`stage=fork-child-wrote`、`stage=parent-wait4-ok`、`stage=execve-third-stage`、`stage=mkdir-chdir-ok`、`stage=nested-file-roundtrip-ok`、`stage=getdents64-nested-visible`、`stage=fstatat-nested-stat-ok`、`stage=renameat2-syscall-ok`、`stage=renameat2-nested-ok`、`stage=renameat2-dirent-updated`、`stage=renameat2-cleanup-ok`、`stage=unlinkat-parent-dirent-gone`、`stage=mkdirat-dir-name-reusable`、`stage=mkdirat-reused-dir-empty`、`stage=mkdirat-reused-dir-dot-only`、`stage=mkdirat-reused-dir-parent-stat-ok`、`stage=third-stage-reached` 与 `post-init reached`。因此当前更近的活跃 blocker 已继续收敛到 multi-stage post-init exec + path-resolution + getdents64 目录遍历 + `fstatat` 元数据读回 + `renameat2`/`unlinkat` 目录项可见性 + `mkdirat` 名字复用 + 重建目录空视图之后的下一处更后 userland checkpoint。
- `2026-04-27` 同日完成 `C1 / P4-prep-2 memory observation / shadow cache` 第一刀：`ExecutionMemoryObservation` 已携带可选物理地址，`ExecutionProfile` 已聚合全局与 region 级 shadow cache 统计，debug JSON 与 `run_debug_cli_probe` 文本摘要已暴露只读观测面；这一路径只用于 cacheable RAM line 的 workload 观测和 MMIO / fault / non-cacheable bypass 统计，不改变 guest 可见语义。
- `2026-04-27` 后续又把第一组可回归 workload 观测收窄到 pipeline vector CNN：`vector_cnn_smoke` 现在会要求 pipeline 路径暴露非零 RAM `shadow_cache` 信号，并锁住全局统计与 RAM region 统计一致。同日 follow-up 也已把 `functional` backend 的最小 `ExecutionProfile` 观测接到 debug snapshot，并继续补到 `LR/SC/AMO` atomic traffic；默认 `run-workload-xv6` 现会在真实 `virtio-blk` board path 上稳定导出包含 atomic traffic 在内的非零 `profile / shadow-cache` 信号，`xv6_boot_smoke` 也已锁住当前 5000-step functional baseline（`memory=1570`、`shadow_cache line_accesses=1515 hits=1495 misses=20 bypasses=55`）与 RAM region 一致性，`test-host-run_debug_cli_probe` 则进一步锁住真实 `run_debug_cli_probe.py + --debug-cli + xv6` 的同一组 summary 输出。`xv6` 的 pipeline probe 仍会落入当前 pipeline 不支持的陷阱路径，因此当前稳定的 shadow-cache baseline 仍以 pipeline vector CNN 为准，`xv6` 则转为 stable functional observation path。

## 关键历史节点

- `2026-04-22`
  - 完成第一轮 `A -> B -> C -> D` 主线整合。
  - `xv6_boot_smoke` 已从旧的 `mhartid` illegal trap 口径刷新到 post-A 的 early-boot checkpoint。
  - `execution_profile_smoke` 已接入默认 `make test` / `make test-pipeline`。
  - 第一轮 post-integration correctness findings 已关闭：普通 `store` 会正确失效 `LR/SC` reservation，translation-fault memory observation 已计入 `execution_profile`。
  - 已完成 B / C follow-up：PLIC source wiring 拆分、`Machine` block transport 选择、`mycpu_virt` board profile 切到 `virtio-blk`，`xv6_boot_smoke` / `run-workload-xv6` 开始消费真实 `virtio` board path。
  - 同日进一步的 A / B bug-driven follow-up 也已完成：A 已补齐 `pmpcfg0/pmpaddr0/menvcfg/stimecmp` 最小 contract，B 已补齐 `xv6 uartinit()` 所需的 UART 16550 bring-up contract；`xv6` 先稳定到 5000-cycle boot-banner / allocator-warmup checkpoint，随后又推进到真实 `virtio-blk` board path 下的 shell。
  - 同日也已把 Linux-facing boot contract foundation 接进主线：generic `flat/payload/set_gpr`、probe summary 的 `payloads/gpr-seeds` 输出、`DebugSession reset` replay，以及 `linux_proto` board/profile 级 boot layout dry-run。
  - 这一轮验证已覆盖 `python3 tests/host/run_debug_cli_probe_test.py`、`make test-host-run_debug_cli_probe`、`make test-host-debug_protocol_command_smoke`、`make test-unit-machine_loader_reset`、`make test-host-debug_cli_smoke`、`make test-host-virtio_blk_smoke`、`make test-host-xv6_boot_smoke`、`make test-host-xv6_shell_smoke`、`make run-workload-xv6`、`make test`、`make test-pipeline` 与 `cd frontend && node --test`。
- `2026-04-24`
  - 已修正 `linux_proto` 的 `dtb` 生成规则：缺 `rootfs.cpio` 时不再直接报 `No rule to make target`。
  - 已补上 repo-generated 最小 `rootfs.cpio` `/init` fallback；默认工作区现在不再把外部 `initrd` 当成必需输入。
  - `make run-workload WORKLOAD_NAME=linux_proto` 现在会继续走到 probe 层，并 fail-closed 列出缺失 `Image` 文件。
  - functional 路径已补上最小 `RV64C`，modern `virtio-mmio` 已补齐 `VIRTIO_F_VERSION_1`，`virtio_blk` 探测不再因 modern feature 缺失而失败。
  - `linux_proto` profile 已新增 `LINUX_PROTO_DISK` / `LINUX_PROTO_BOOTARGS` alias，且 `mycpu_virt.dts` 会随 bootargs 变更强制重生成。
  - 这一轮验证已覆盖 `make build-workload WORKLOAD_NAME=linux_proto LINUX_PROTO_EXTERNAL_DIR=/tmp/mycpu-linux-missing`、`make WORKLOAD_NAME=linux_proto LINUX_PROTO_EXTERNAL_DIR=/tmp/mycpu-linux-missing run-workload`、更新后的 `run_debug_cli_probe` Linux profile 单测、`make test-unit-virtio_mmio_contract`、`make test-host-virtio_blk_smoke`、`make test`、`make test-pipeline` 与本地 `CONFIG_RISCV_ISA_C=y` Linux `Image` 的 `/init reached` probe。
- `2026-04-25`
  - `linux_proto` 现已支持 `LINUX_PROTO_ROOTFS_MODE=block`，并在缺外部 Linux 磁盘镜像时默认生成 repo-owned 最小 `rootfs.ext4`；block 模式会自动切到 `root=/dev/vda rw rootfstype=ext4 rootwait init=/init`。
  - `mycpu_virt.dts` 在 block 模式下会把 `chosen.initrd` 收口成零长度占位，而不是复用旧 `rootfs.cpio` 的大小。
  - repo-generated `/init` 现在会容忍 block-rootfs 路径下已预先挂载的 `devtmpfs`，并通过 staged marker 把最小用户态闭环收口成可观察 checkpoint；同日又补上一层更窄的 post-init smoke：在 console 打通后做一次 ext4 `rootfs` 写入/读回一致性检查、把 `procfs` 挂载与 `/proc/cmdline` 可读性纳入最小 `/init` 路径，再继续把 `sysfs` 挂载与 `/sys/devices/system/cpu/online` 可读性也纳入同一条 baseline。随后又修正了 `linux_mininit` 的 staged-marker 长度常量，让 `console-opened / rootfs-rw-ok / proc-readable / sys-readable / /init reached` 这些 UART 输出不再把 trailing `NUL` 一起写出；这轮继续把 repo-generated second-stage ELF `/post-init-smoke` 接到同一条 rootfs，并让 `/init` 在完成最小 smoke 后显式 `execve()` 到它；随后 post-init userland 也已补上只读文件 smoke 和 first writable-rootfs round-trip。基于本地 `CONFIG_RISCV_ISA_C=y` Linux `Image`，repo-generated `rootfs.ext4` 路径已稳定推进到 `EXT4-fs (vda)`、`VFS: Mounted root (ext4 filesystem) on device 254:0.`、`devtmpfs: mounted`、`Run /init as init process`、`mycpu linux initrd: stage=console-opened`、`mycpu linux initrd: stage=rootfs-rw-ok`、`mycpu linux initrd: stage=proc-readable`、`mycpu linux initrd: stage=sys-readable`、`mycpu linux initrd: /init reached`、`mycpu linux initrd: stage=execve-post-init`、`mycpu linux userland: stage=file-readable`、`mycpu linux userland: stage=rootfs-rw-roundtrip-ok` 与 `mycpu linux userland: post-init reached`。
  - 这一轮验证已覆盖 `make build-workload WORKLOAD_NAME=linux_proto LINUX_PROTO_ROOTFS_MODE=block LINUX_PROTO_IMAGE=/tmp/mycpu-linux-build-riscv64-linux-gnu/arch/riscv/boot/Image`、`python3 -m unittest tests.host.run_debug_cli_probe_test.RunDebugCliProbeTest.test_make_build_workload_linux_proto_block_mode_builds_post_init_smoke_elf`、`make test-host-run_debug_cli_probe`、`make test`、`make test-pipeline`，以及本地 `CONFIG_RISCV_ISA_C=y` Linux `Image` 的 `python3 workloads/run_debug_cli_probe.py ... --uart-wait "mycpu linux userland: stage=mkdirat-dir-name-reusable" 300000000` block-rootfs probe。
- `2026-04-26`
  - repo-generated second-stage ELF `/post-init-smoke` 现已继续补上最小 `clone3/fork-like -> child write -> parent wait4` process lifecycle smoke，并新增 `stage=execve-third-stage` marker 与 `execve()` 到 repo-generated third-stage ELF `/post-init-exec-smoke` 的最小 contract。
  - 基于本地 `CONFIG_RISCV_ISA_C=y` Linux `Image`，当前 repo-generated `rootfs.ext4` 路径会在 `mycpu linux userland: stage=file-readable`、`stage=rootfs-rw-roundtrip-ok`、`stage=fork-child-wrote`、`stage=parent-wait4-ok` 之后继续稳定命中 `stage=execve-third-stage`、`stage=mkdir-chdir-ok`、`stage=nested-file-roundtrip-ok`、`stage=getdents64-nested-visible`、`stage=fstatat-nested-stat-ok`、`stage=renameat2-syscall-ok`、`stage=renameat2-nested-ok`、`stage=renameat2-dirent-updated`、`stage=renameat2-cleanup-ok`、`stage=unlinkat-parent-dirent-gone`、`stage=mkdirat-dir-name-reusable`、`stage=third-stage-reached` 与 `post-init reached`。
  - 这一轮验证已覆盖 `python3 -m unittest tests.host.run_debug_cli_probe_test.RunDebugCliProbeTest.test_make_build_workload_linux_proto_block_mode_builds_post_init_smoke_elf`、`make test-host-run_debug_cli_probe`、`make build-workload WORKLOAD_NAME=linux_proto LINUX_PROTO_ROOTFS_MODE=block LINUX_PROTO_IMAGE=/tmp/mycpu-linux-build-riscv64-linux-gnu/arch/riscv/boot/Image`、`make test`、`make test-pipeline`，以及本地 `CONFIG_RISCV_ISA_C=y` Linux `Image` 的 `python3 workloads/run_debug_cli_probe.py ... --uart-wait "mycpu linux userland: stage=mkdirat-dir-name-reusable" 300000000` 与 `--uart-wait "mycpu linux userland: post-init reached" 300000000` 两次 block-rootfs probe。
- `2026-04-21`
  - 正式把 `future_expansion_roadmap_design.md` 中的标准 OS bring-up 切换线提升为当前 active program。
  - 新增 `xv6 / Linux / JIT` 主线 design / status / wave 1 plan，并按 4 个独立 worktree 启动并行工作流。
  - 当天晚些时候 4 个 worktree 的第一轮 handoff 全部收齐，当前主线进入“按 ownership 整合已完成 foundation / harness”的阶段。
- `2026-04-12`
  - 完成 `P4-prep-1`，`Bus` 已统一暴露 `RAM / MMIO / unmapped` 与保守 region 属性。
  - 向量 / CNN 可视化正式接入 `debug/frontend`。
- `2026-04-10` 到 `2026-04-11`
  - `V-lite` `V0 ~ V4` 及第一轮更窄 hardening 落地，形成固定 `conv -> relu` 的最小 CNN-style guest 闭环与最小 vector-aware pipeline 边界。
- `2026-04-07`
  - Spike 外部差分扩到第一批 device-free `Sv39 / page fault` final-state subset，并补上 returning trap handler 的 first-trap checkpoint。
- `2026-04-05`
  - decode 级 `BlockedByUnresolvedStore` 边界收窄完成，且主线已明确：当前不主动继续扩大更激进的 `issue / replay / speculation`。
  - `debug/frontend` 补上更窄的长会话、session replacement 与 terminal 输入压力验证。
- `2026-04-04`
  - `P1` 结构收口与 `P2` 首轮验证补洞完成两轮收口，新增 loader 单测、guest smoke 窄单测、真实 debug e2e smoke 与 pipeline smoke 拆分。

## 当前仍然有效的风险 / 限制

- `debug/frontend` 当前已经够用，但它的正式定位仍然是“教学演示可用 + 最小工程调试”，不应顺势扩成通用调试器。
- 当前 `pipeline` 已具备最小真实 `OoO execute`，但仍是单发射、顺序退休、保守 replay 的克制形态；当前没有足够证据支持继续主动扩大更激进的 `issue / replay / speculation`。
- 当前并行整合阶段虽已结束，但后续 `xv6` 暴露的 blocker 仍会跨 ISA、platform、guest workload 三类边界；必须继续按 A / B / C / D ownership 分类，避免回到 `main` 工作区后重新变成“谁顺手谁修”。
- 当前 `xv6-riscv` 虽已在真实 `virtio-blk` board path 上推进到 shell，但这条线更多已经变成稳定 workload guardrail；当前仍不能把这件事误读成 Linux 或 `JIT / DBT` 已接近本轮交付。
- 当前 `V4` 虽已落地，但仍刻意不扩到向量 load/store path、lane 模型、vector rename 或更重 memory speculation；在继续 hardening 与 workload 观察之前，直接抢跑更重 `Phase 4` 的性价比仍然偏低。
- 当前 `P4-prep-1` 与 `C1 / P4-prep-2` 都只是准备性收口：`shadow_cache` 目前只提供读侧观测和统计，不代表真实 cache / DMA / multicore 已进入正式实施阶段；当前 `xv6` 默认 functional probe 已具备稳定的 workload 观测出口，但 `xv6 / Linux` 若要拿到更像未来 cache 评估的 pipeline-side memory signal，仍需要后续单独收口 pipeline bring-up gap。
- guest runtime 的 `vm*`、`trap*`、`kernel_bringup`、`kernel_runtime` 等边界已经比早期清晰得多，但后续仍要防止真实 bug 修复把职责重新揉回大文件。
- 当前 `linux_proto` 已经具备 `linux_sbi_shim + payload/GPR seed + repo-generated dtb/chosen/cmdline + optional disk/bootargs alias + repo-generated block-rootfs fallback` 的 Linux-facing foundation；在显式提供真实 `Image` 时，这条路径已能稳定推进到 `Unpacking initramfs...`、`devtmpfs: initialized`、`xor`、repo-generated initramfs `/init reached`，以及 repo-generated ext4 block-rootfs 的 `mycpu linux initrd: stage=rootfs-rw-ok`、`stage=proc-readable`、`stage=sys-readable`、`stage=execve-post-init`、`mycpu linux userland: stage=file-readable`、`stage=rootfs-rw-roundtrip-ok`、`stage=fork-child-wrote`、`stage=parent-wait4-ok`、`stage=execve-third-stage`、`stage=mkdir-chdir-ok`、`stage=nested-file-roundtrip-ok`、`stage=getdents64-nested-visible`、`stage=fstatat-nested-stat-ok`、`stage=renameat2-syscall-ok`、`stage=renameat2-nested-ok`、`stage=renameat2-dirent-updated`、`stage=renameat2-cleanup-ok`、`stage=unlinkat-parent-dirent-gone`、`stage=mkdirat-dir-name-reusable`、`stage=mkdirat-reused-dir-empty`、`stage=mkdirat-reused-dir-dot-only`、`stage=mkdirat-reused-dir-parent-stat-ok`、`stage=third-stage-reached` 与 `post-init reached`。当前默认工作区已经通过 repo-generated 最小 `rootfs.cpio` 与 `rootfs.ext4` 去掉了外部 `initrd` / disk 镜像依赖，因此近端剩余的外部输入主要收窄为 kernel `Image`。
- `linux_sbi_shim` 现在还会把 Linux 接管 `stvec` 之前的 unexpected early `M/S` trap 直接打印到 UART；它先把一颗本地新构建 `Image` 的入口失败明确收窄到 `RVC`，随后 functional 路径又已补上最小 `RV64C`。当前真正仍未关闭的 bring-up blocker 已不再是 `RVC`、modern virtio feature、block-rootfs 挂载本身，或最小 `/init` 的 console / rootfs-rw / procfs / sysfs 闭环；当前更后、也更值得继续收窄的 gap 已更新为“post-init file-read/write + process lifecycle + multi-stage exec + path-resolution + getdents64 目录遍历 + `fstatat` 元数据读回之后的下一处 userland / platform checkpoint”，而不是继续回头处理已关闭的 console prefix 问题。
- 当前 `Machine` 默认 block transport 仍保持 `simple_storage` 以守住既有 guest / debug 路径；真实 `virtio` path 需要 workload / CLI 显式选择，这条兼容性策略当前是有意保留的。
- A 已经补齐第一轮 `RV64A + CSR / privilege` foundation，但这不代表 `xv6` 后续会用到的全部 timer / privilege contract 都已落齐；`pmp*`、`menvcfg`、`stimecmp` 等缺口仍可能继续暴露。

## 下一步

1. 把 `xv6` shell 继续守成真实 `virtio-blk` board path 的稳定 guardrail，并只按真实 bug 或明确收益补更窄 smoke。
2. 在现有 `flat/payload/set_gpr`、`LINUX_PROTO_ROOTFS_MODE=block` 与 repo-generated `rootfs.ext4` foundation 之上，继续沿真实 Linux `Image` 路径把最小 `console-opened -> rootfs-rw-ok -> proc-readable -> sys-readable -> /init reached -> file-readable -> rootfs-rw-roundtrip-ok -> fork-child-wrote -> parent-wait4-ok -> execve-third-stage -> mkdir-chdir-ok -> nested-file-roundtrip-ok -> getdents64-nested-visible -> fstatat-nested-stat-ok -> renameat2-syscall-ok -> renameat2-nested-ok -> renameat2-dirent-updated -> renameat2-cleanup-ok -> unlinkat-parent-dirent-gone -> mkdirat-dir-name-reusable -> mkdirat-reused-dir-empty -> mkdirat-reused-dir-dot-only -> mkdirat-reused-dir-parent-stat-ok -> third-stage-reached -> post-init reached` baseline 推向更后的 userland checkpoint，并冻结 multi-stage post-init exec + path-resolution + getdents64 目录遍历 + `fstatat` 元数据读回 + `renameat2`/`unlinkat` 目录项可见性 + `mkdirat` 名字复用 + 重建目录空视图之后的下一处稳定 blocker。
3. A / B 后续都保持围绕 Linux bring-up 的 bug-driven hardening：`RVC`、UART 8250、modern `virtio-mmio`、block-rootfs 挂载，以及最小 `/init` console / rootfs-rw / procfs / sysfs 闭环的近端 contract 已经补齐；接下来随着真实 Linux 暴露更后的 userland / tty / platform 缺口，再补最窄 contract，不主动扩大无关 ISA / device 面。
4. D 线继续作为主线 guardrail：优先用 `execution_profile`、`shadow_cache` 观测、debug CLI、`run_debug_cli_probe` 与既有 workload smoke 锁住 `xv6 / virtio / Linux profile` 路径的行为变化；当前至少要同时守住默认 functional `run-workload-xv6` 的非零 profile/shadow-cache，以及 pipeline vector CNN 的 RAM shadow-cache baseline。
5. 继续把 `pipeline`、guest runtime、`kernel_alpha` 十条基线、`debug/frontend` 和 Spike 外部差分限定在当前已接入、可验证的范围内维护，不让新主线反向污染 reference path。

## 验证基线

- `cd myCPU && make test`
- `cd myCPU && make test-pipeline`
- `cd frontend && node --test`

如果改动集中在 loader、guest smoke orchestration 或调试链路，至少额外关注：

- `cd myCPU && make test-unit-binary_loader`
- `cd myCPU && make test-unit-machine_loader_reset`
- `cd myCPU && make test-unit-supervisor_demo_smoke`
- `cd myCPU && make test-unit-user_program_smoke`
- `cd myCPU && make test-host-run_debug_cli_probe`
- `cd myCPU && make test-host-debug_cli_smoke`
- `cd myCPU && make test-host-interactive_terminal_smoke`
- `cd myCPU && make test-host-virtio_blk_smoke`
- `cd myCPU && make test-host-xv6_boot_smoke`
- `cd myCPU && make test-host-xv6_shell_smoke`
- `cd myCPU && make run-workload-xv6`
