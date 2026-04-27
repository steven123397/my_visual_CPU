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
  - 当前无活跃计划；Wave 1 已归档到 [../plan/history_plan.md#xv6-linux-jit-wave1-plan](../plan/history_plan.md#xv6-linux-jit-wave1-plan)
- 已完成计划：
  - [../plan/history_plan.md#xv6-linux-jit-wave1-plan](../plan/history_plan.md#xv6-linux-jit-wave1-plan)
  - 其他历史归档统一见 [../plan/history_plan.md](../plan/history_plan.md)

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
- C 线的 external workload harness 已进入主线：`xv6-riscv` 外部源码树、board profile、profile make glue、`run-workload-xv6` / `smoke-workload-xv6`、`xv6_boot_smoke` 与 `xv6_shell_smoke` 已可直接使用；当前 `mycpu_virt` board profile 已切到 `virtio-blk`，真实 `virtio-mmio + virtio-blk` board path 已稳定到 shell prompt，并且 `xv6_shell_smoke` 已锁住 `ls`、`cat README`、`wc README`、`grep qemu README | wc`、root/nested 路径文件创建/读回/删除、`forktest` 与 `stressfs`，因此这条线当前已经从“post-banner gap finder”提升为“真实 board path 的稳定 shell 里程碑”。
- D 线的 `execution_profile`、debug CLI profile 导出、面向 `memory_region` 的读侧观测合同，以及 `C1 / P4-prep-2` 的 `shadow_cache` 观测面都已进入主线；`test-host-execution_profile_smoke` 已补进默认 `make test` / `make test-pipeline` guardrail。同日 follow-up 也已把 `functional` backend 的最小 `ExecutionProfile` 观测接到 debug snapshot，并继续纳入 `LR/SC/AMO` atomic traffic；默认 `run-workload-xv6` 现在会稳定打印更完整的非零 `profile / shadow-cache`，`xv6_boot_smoke` 会锁住当前 functional 5000-step baseline 与 RAM shadow-cache 一致性，`test-host-run_debug_cli_probe` 也会锁住真实 probe summary 出口；这两条 xv6 observation guardrail 现已提升到默认 `make test` / `make test-pipeline`。
- A / D 的第一轮 post-integration hardening 也已补齐：普通 `store` 现在会正确失效 `LR/SC` reservation，delegated page/access fault 也会进入 `execution_profile` 的 `unmapped` fault observation；当前新增 gap 也因此不再是已知 architected correctness 缺口，而是 `xv6` 在真实 `virtio` board path 上继续前进时会暴露出的下一处 bring-up blocker。
- 同日进一步的 bug-driven A / B follow-up 也已把 `xv6` 推过旧的 early-boot trap：A 已补齐 `pmpcfg0/pmpaddr0/menvcfg/stimecmp` 的最小合法 contract，B 已把 UART 扩到 `xv6 uartinit()` 需要的 `LCR/FCR/DLAB` 与 RX/TX interrupt identity；`run-workload-xv6` 现在也会直接打印 machine/supervisor trap 视图。
- 当前 Linux-facing 的最小 boot foundation 也已进入主线：`run_debug_cli_probe.py`、普通 CLI、debug CLI、`Machine` 和 `DebugSession` 现在都支持通用的 `flat image + payload + set_gpr` 合同；probe summary 会直接打印 `payloads:` 与 `gpr-seeds:`；`linux_proto` workload profile 当前会以 `linux_sbi_shim.bin@0x80000000` 作为主镜像，并稳定导出 `Image@0x80200000`、`dtb@0x87f00000`、`initrd@0x84000000`、`a0=hartid`、`a1=dtb` 与 `a2=kernel entry`。同日也已把板级 `DTB/chosen/cmdline` 合同收口进仓库：`mycpu_virt.dts.in` 会生成默认 `mycpu_virt.dtb`，显式携带 `chosen.bootargs`、`linux,initrd-start/end` 与 `timebase-frequency=100MHz`。因此当前这条主线的活跃 blocker 已不再是“能否把 xv6 推到 shell”，而是：如何沿真实 Linux boot path 继续把 checkpoint 从 initramfs unpack 推向 rootfs / init。
- 同日也已把这条 Linux-facing blocker 再收窄一层：当前主工作区里仍不存在 `external/linux-riscv/arch/riscv/boot/Image` 与 `external/linux-riscv/rootfs.cpio` 这 2 个真实 Linux 资产；但板级 `DTB/chosen/cmdline` 已不再依赖外部 `mycpu_virt.dtb`，而是由仓库内模板和 `dtc` 规则默认生成。`run_debug_cli_probe.py` / `make run-workload WORKLOAD_NAME=linux_proto` 现在会在缺 `Image`、`initrd` 或显式 payload 输入时 fail-closed，直接打印缺失文件列表，而不是继续把问题伪装成 probe / simulator 行为异常。
- 同日也已把这条 Linux-facing bring-up 再往前推进三刀：先补上一层最小 `linux_sbi_shim`，把 Linux 从旧的 `ecall from M-mode -> mtvec=0` blocker 推进到真实 `S-mode` early boot；随后又把 PLIC contract 从“只认 `virtio=1` / `UART=10`”收口成“`1..10` contiguous source window 合法、仅 `1/10` 会被真实设备拉高”，因此 `__plic_init()` 不再因写 `priority[2]` 触发 `store access fault`；最后再把 board DTB 固定到仓库生成的 `100MHz` timebase 合同。当前在真实外部 `Image + initrd` 与仓库生成 `dtb` 资产下，Linux 已能稳定打印到 `Unpacking initramfs...`，随后继续推进到 `devtmpfs: initialized`、`workingset`、`jitterentropy` 与 `xor: measuring software checksum speed` checkpoint。
- 同日也补了一层更窄的 probe harness follow-up：`DebugSession::run_until_uart_contains()` 现在会按 UART 尾部增量搜索，而不是在每个 cycle 对整段 UART 缓冲做全文扫描；当前 `linux_proto` 的 `100M` `uart-wait` 已能在约 `1m30s` wall-clock 内稳定等到 `xor: measuring software checksum speed`，避免把长 Linux UART 日志误诊成 debug harness 自身的观测瓶颈。
- 同日继续把 probe 推到 `cycle=100000000` 与更长 wall-clock 预算后，当前 `100MHz` board DTB 路径下已不再像旧 `1MHz` external DTB 那样很快落入 `soft lockup` / `workqueue lockup`；Linux 会继续前进到 `devtmpfs: initialized` 之后的更后面阶段，但在 `devtmpfs: mounted` / `VFS: Mounted root` 之前仍未拿到更稳定 checkpoint。当前冻结下来的更近判断是：活跃 blocker 更像 `devtmpfs: initialized` 之后到 rootfs / init 之间的“纯慢 / 时间基准-吞吐耦合”或后续 boot contract 缺口，而不是新的 page/access fault。
- `2026-04-24` 又补上一层更窄的 Linux harness follow-up：`linux_proto` 的 repo-generated `mycpu_virt.dts/.dtb` 现在不再对缺失的 `rootfs.cpio` 保持 Make 级硬依赖；同日又补上 repo-generated 最小 `rootfs.cpio` `/init` fallback，因此默认工作区已不再要求外部 `initrd` 才能形成完整 boot 合同。当前在缺 `Image` 的场景下，`build-workload` 会生成带真实 initrd window 的 `chosen`，随后由 `run_debug_cli_probe.py` / `make run-workload WORKLOAD_NAME=linux_proto` 在 probe 入口统一 fail-closed 并列出缺失文件。当前这条 bring-up 线因此不再被 Make 依赖错误提前卡死，也把默认工作区的外部缺口从 `Image + initrd` 收窄成 `Image` 单项。
- 同日也补上一层更窄的 Linux bring-up 诊断与兼容性收口：`linux_sbi_shim` 现在会在 Linux 接管 `stvec` 之前，把 unexpected early `M/S` trap 的 `cause/epc/tval` 直接打到 UART；随后又补齐 Linux 8250 需要的 UART `MCR/MSR`、functional `RV64C` 最小语义，以及 modern `virtio-mmio` 必需的 `VIRTIO_F_VERSION_1`。基于这层收口，本地 `CONFIG_RISCV_ISA_C=y` Linux `Image` 已能在 repo-generated `dtb + initrd` 路径上稳定推进到 `Run /init as init process` 与 `mycpu linux initrd: /init reached`，并且 `virtio_blk` 已不再拒绝探测，而是稳定枚举出 `vda`。
- 同日也把 block-rootfs 入口合同再往前推进一刀：`linux_proto` profile 现在额外支持 `LINUX_PROTO_DISK` 与 `LINUX_PROTO_BOOTARGS` alias，并把 `mycpu_virt.dts` 改成每次按当前 bootargs 重新生成，避免旧 DTS 缓存掩盖新的 `chosen.bootargs`。当前无磁盘输入场景下，Linux 已会稳定枚举 `virtio_blk virtio0: [vda] 0 512-byte logical blocks (0 B/0 B)`；因此新的近端 blocker 已从 `RVC` / modern virtio 探测，收敛成“需要一份非空 block-rootfs 磁盘镜像，以及与之匹配的 `root=/dev/vda` 类 bootargs”。
- `2026-04-25` 又把这条 block-rootfs bring-up 线本身收口成 repo-generated fallback：`linux_proto` 现支持 `LINUX_PROTO_ROOTFS_MODE=block`，会在缺外部 Linux 磁盘镜像时默认生成最小 `rootfs.ext4`，并自动切到 `root=/dev/vda rw rootfstype=ext4 rootwait init=/init`；同日 `mycpu_virt.dts` 在 block 模式下也改成只按实际 payload 的 initrd 生成 `chosen.initrd`，避免旧 `rootfs.cpio` 大小泄漏进 DTB。随后 repo-generated `/init` 也已收口成可观察的最小用户态闭环：它现在会容忍 block-rootfs 下已预先挂载的 `devtmpfs`，并通过 staged marker 暴露 `stage=console-opened`、`stage=rootfs-rw-ok`、`stage=proc-readable` 与 `stage=sys-readable`；同日又补上 repo-generated second-stage ELF `/post-init-smoke`，让 `/init` 在这些 smoke 之后显式 `execve()` 到真实 post-init 用户态。随后 post-init userland 也已补上静态文件读取与 writable-rootfs round-trip。基于本地 `CONFIG_RISCV_ISA_C=y` Linux `Image`，当前 repo-generated block-rootfs 路径已稳定推进到 `virtio_blk virtio0: [vda] 16384 ...`、`EXT4-fs (vda)`、`VFS: Mounted root (ext4 filesystem) on device 254:0.`、`devtmpfs: mounted`、`Run /init as init process`、`mycpu linux initrd: stage=console-opened`、`mycpu linux initrd: stage=rootfs-rw-ok`、`mycpu linux initrd: stage=proc-readable`、`mycpu linux initrd: stage=sys-readable`、`mycpu linux initrd: /init reached`、`mycpu linux initrd: stage=execve-post-init`、`mycpu linux userland: stage=file-readable`、`mycpu linux userland: stage=rootfs-rw-roundtrip-ok` 与 `mycpu linux userland: post-init reached`；因此活跃 blocker 已进一步从“缺非空 block-rootfs 磁盘镜像”与“最小 `/init` 输出不完整”收窄到 first writable-rootfs post-init round-trip 之后的下一处更后 userland checkpoint。
- `2026-04-27` 又把这条 block-rootfs second-stage userland baseline 继续往前推进一刀：repo-generated third-stage ELF `/post-init-exec-smoke` 现在会在 `renameat2` cleanup 之后回到父目录再做一次 `getdents64` 目录遍历，要求 `unlinkat(AT_REMOVEDIR)` 清理掉的 `post-init-dir-smoke` 目录项已经从根目录视图消失，并通过 `stage=unlinkat-parent-dirent-gone` 暴露到 UART；随后又立刻对同一路径重新 `mkdirat()`，要求这个目录名已经可以重新分配，并通过 `stage=mkdirat-dir-name-reusable` 暴露到 UART；最后再 `chdir()` 进入重建目录，确认旧的 `nested.txt` / `renamed.txt` 都已不可见，并通过 `stage=mkdirat-reused-dir-empty` 暴露到 UART。基于本地 `CONFIG_RISCV_ISA_C=y` Linux `Image`，当前 repo-generated block-rootfs 路径会在 `stage=file-readable`、`stage=rootfs-rw-roundtrip-ok` 之后继续稳定命中 `stage=fork-child-wrote`、`stage=parent-wait4-ok`、`stage=execve-third-stage`、`stage=mkdir-chdir-ok`、`stage=nested-file-roundtrip-ok`、`stage=getdents64-nested-visible`、`stage=fstatat-nested-stat-ok`、`stage=renameat2-syscall-ok`、`stage=renameat2-nested-ok`、`stage=renameat2-dirent-updated`、`stage=renameat2-cleanup-ok`、`stage=unlinkat-parent-dirent-gone`、`stage=mkdirat-dir-name-reusable`、`stage=mkdirat-reused-dir-empty`、`stage=mkdirat-reused-dir-dot-only`、`stage=mkdirat-reused-dir-parent-stat-ok`、`stage=third-stage-reached` 与 `post-init reached`；因此活跃 blocker 已进一步收窄到 multi-stage post-init exec + path-resolution + getdents64 目录遍历 + `fstatat` 元数据读回 + `renameat2`/`unlinkat` 目录项可见性 + `mkdirat` 名字复用 + 重建目录空视图之后的下一处更后 userland checkpoint。

## 关键历史节点

- `2026-04-24`
  - 已修正 `linux_proto` 的 `dtb` 生成入口：缺 `rootfs.cpio` 时不再在 Make 依赖阶段直接报 `No rule to make target`。
  - 已补上 repo-generated 最小 `rootfs.cpio` `/init` fallback；当外部 `rootfs.cpio` 缺失时，`linux_proto` 现在会自动回落到仓库内生成的 initramfs。
  - `make run-workload WORKLOAD_NAME=linux_proto` 现在会继续进入 probe 入口，并稳定打印缺失 `Image` 文件；默认工作区不再把 `initrd` 作为额外缺口。
  - `linux_sbi_shim` 现在会把 unexpected early `M/S` trap 的 `cause/epc/tval` 直接打印到 UART；这层诊断先把本地 Linux bring-up 收窄到 `RVC` 入口问题，随后 functional 路径又已补上最小 `RV64C` 语义并重新通过 Linux bring-up。
  - 同日也已补齐 Linux 8250 需要的 UART `MCR/MSR` 与 modern `virtio-mmio` 需要的 `VIRTIO_F_VERSION_1`；本地 `CONFIG_RISCV_ISA_C=y` Linux `Image` 现在会稳定枚举 `virtio_blk virtio0: [vda] 0 512-byte logical blocks (0 B/0 B)`，并继续推进到 `/init reached`。
  - `linux_proto` profile 现在额外支持 `LINUX_PROTO_DISK` / `LINUX_PROTO_BOOTARGS` alias，且 `mycpu_virt.dts` 会在 `build-workload` 时按当前 bootargs 强制重生成；这把 block-rootfs 的下一步入口收口成“提供非空磁盘镜像 + 切 bootargs 到 block root”。
  - 本轮验证已覆盖 `python3 -m unittest tests.host.run_debug_cli_probe_test.RunDebugCliProbeTest.test_make_run_workload_linux_proto_derives_boot_contract_from_profile`、`python3 -m unittest tests.host.run_debug_cli_probe_test.RunDebugCliProbeTest.test_make_build_workload_linux_proto_generates_fallback_initrd_and_dtb`、`python3 -m unittest tests.host.run_debug_cli_probe_test.RunDebugCliProbeTest.test_make_run_workload_linux_proto_forwards_optional_disk_alias`、`python3 -m unittest tests.host.run_debug_cli_probe_test.RunDebugCliProbeTest.test_make_build_workload_linux_proto_honors_bootargs_alias`、`python3 -m unittest tests.host.run_debug_cli_probe_test.RunDebugCliProbeTest.test_make_run_workload_linux_proto_reports_missing_assets_via_probe`、`make test-unit-virtio_mmio_contract`、`make test-host-virtio_blk_smoke`、`make test-host-run_debug_cli_probe`、`make test`、`make test-pipeline` 与本地 `CONFIG_RISCV_ISA_C=y` Linux `Image` 的 `/init reached` probe。
- `2026-04-25`
  - `linux_proto` 现已支持 `LINUX_PROTO_ROOTFS_MODE=block`，并在缺外部 Linux disk 镜像时默认生成 repo-owned 最小 `rootfs.ext4`；block 模式会自动切到 `root=/dev/vda rw rootfstype=ext4 rootwait init=/init`。
  - `mycpu_virt.dts` 在 block 模式下会把 `chosen.initrd` 收口成零长度占位，而不是复用旧 `rootfs.cpio` 的大小。
  - repo-generated `/init` 现在会容忍 block-rootfs 路径下已预先挂载的 `devtmpfs`，并通过 staged marker 把最小用户态闭环收口成可观察 checkpoint；同日又补上一层更窄的 post-init smoke：在 console 打通后做一次 ext4 `rootfs` 写入/读回一致性检查，把 `procfs` 挂载与 `/proc/cmdline` 可读性也纳入最小 `/init` 路径，再继续把 `sysfs` 挂载与 `/sys/devices/system/cpu/online` 可读性纳入同一条 baseline。随后又修正了 `linux_mininit` 的 staged-marker 长度常量，让 `console-opened / rootfs-rw-ok / proc-readable / sys-readable / /init reached` 这些 UART 输出不再把 trailing `NUL` 一起写出；这轮继续把 rootfs 里的 repo-generated second-stage ELF `/post-init-smoke` 也接入，让 `/init` 在最小 smoke 之后显式 `execve()` 到更真实的 post-init 用户态；随后 second-stage 也已补上 `post-init-data.txt` 只读 smoke、`create -> write -> read -> unlink` 的 writable-rootfs round-trip，以及 `clone3/fork-like -> child write -> parent wait4` process lifecycle。当前 third-stage 也继续补上最小 `mkdir -> chdir -> create/write/read/unlink nested file` 的目录/路径解析 contract，并进一步补上基于 `getdents64` 的最小目录项遍历合同、基于 `fstatat` 的最小目录项元数据读回合同，以及基于 `renameat2`/`unlinkat` 的最小目录项可见性合同、`mkdirat` 名字复用合同与 reused-dir 空视图合同。基于本地 `CONFIG_RISCV_ISA_C=y` Linux `Image`，repo-generated `rootfs.ext4` 路径已稳定推进到 `EXT4-fs (vda)`、`VFS: Mounted root (ext4 filesystem) on device 254:0.`、`devtmpfs: mounted`、`Run /init as init process`、`mycpu linux initrd: stage=console-opened`、`mycpu linux initrd: stage=rootfs-rw-ok`、`mycpu linux initrd: stage=proc-readable`、`mycpu linux initrd: stage=sys-readable`、`mycpu linux initrd: /init reached`、`mycpu linux initrd: stage=execve-post-init`、`mycpu linux userland: stage=file-readable`、`mycpu linux userland: stage=rootfs-rw-roundtrip-ok`、`mycpu linux userland: stage=fork-child-wrote`、`mycpu linux userland: stage=parent-wait4-ok`、`mycpu linux userland: stage=execve-third-stage`、`mycpu linux userland: stage=mkdir-chdir-ok`、`mycpu linux userland: stage=nested-file-roundtrip-ok`、`mycpu linux userland: stage=getdents64-nested-visible`、`mycpu linux userland: stage=fstatat-nested-stat-ok`、`mycpu linux userland: stage=renameat2-syscall-ok`、`mycpu linux userland: stage=renameat2-nested-ok`、`mycpu linux userland: stage=renameat2-dirent-updated`、`stage=renameat2-cleanup-ok`、`stage=unlinkat-parent-dirent-gone`、`stage=mkdirat-dir-name-reusable`、`stage=mkdirat-reused-dir-empty`、`mycpu linux userland: stage=third-stage-reached` 与 `mycpu linux userland: post-init reached`。
  - 本轮验证已覆盖 `python3 -m unittest tests.host.run_debug_cli_probe_test.RunDebugCliProbeTest.test_make_build_workload_linux_proto_block_mode_builds_post_init_smoke_elf`、`make test-host-run_debug_cli_probe`、`make build-workload WORKLOAD_NAME=linux_proto LINUX_PROTO_ROOTFS_MODE=block LINUX_PROTO_IMAGE=/tmp/mycpu-linux-build-riscv64-linux-gnu/arch/riscv/boot/Image`、`make test`、`make test-pipeline`，以及本地 `CONFIG_RISCV_ISA_C=y` Linux `Image` 的 `python3 workloads/run_debug_cli_probe.py ... --uart-wait "mycpu linux userland: stage=mkdirat-reused-dir-parent-stat-ok" 300000000` 和 `--uart-wait "mycpu linux userland: post-init reached" 300000000` 两次 block-rootfs probe。
- `2026-04-22`
  - 已按 `A -> B -> C -> D` 顺序完成第一轮主工作树整合。
  - `xv6_boot_smoke` 已从旧的 `mhartid` illegal trap 口径刷新到 post-A 的 early-boot checkpoint。
  - `execution_profile_smoke` 已接入默认 `make test` / `make test-pipeline`。
  - 第一轮 post-integration correctness findings 已关闭：普通 `store` 会正确打破 `LR/SC` reservation，faulting memory access 也会被 profile 统计。
  - 已完成 B / C follow-up：PLIC source wiring 拆分、`Machine` block transport 选择、`mycpu_virt` board profile 切到 `virtio-blk`，`xv6_boot_smoke` / `run-workload-xv6` 开始消费真实 `virtio` board path。
  - 同日进一步的 A / B bug-driven follow-up 也已完成：`xv6` 已越过旧的 early-boot trap，先稳定到 5000-cycle `S` mode boot-banner / allocator-warmup checkpoint，随后又在真实 `virtio-blk` board path 上推进到 shell，并把 shell smoke 扩到常用用户态 + 文件系统路径。
  - 同日也已落下 Linux-facing boot foundation：generic `flat/payload/set_gpr` 合同、probe summary 的 `payloads/gpr-seeds` 输出、`DebugSession reset` 对 post-load payload/GPR seed 的 replay、`linux_sbi_shim`、repo-owned `mycpu_virt.dts.in -> mycpu_virt.dtb` board contract，以及把 Linux 推到 `Unpacking initramfs...` / `xor: measuring software checksum speed` 的第一处更后 boot checkpoint。
  - 这一轮验证已覆盖 `python3 tests/host/run_debug_cli_probe_test.py`、`make test-host-run_debug_cli_probe`、`make test-host-debug_protocol_command_smoke`、`make test-unit-machine_loader_reset`、`make test-host-debug_cli_smoke`、`make test-host-xv6_boot_smoke`、`make test-host-xv6_shell_smoke`、`make run-workload-xv6`、`make test`、`make test-pipeline` 与 `cd frontend && node --test`。
- `2026-04-21`
  - 正式决定从“默认延续线优先”切到“标准 OS bring-up 线为当前主线”。
  - 新增 `xv6 / Linux / JIT` 主线 design / status / wave 1 plan。
  - 确认按 4 个独立 worktree / 4 个独立对话并行推进。
  - 4 个 worktree 的第一轮 handoff 全部收齐：A 已落地首轮 `RV64A` foundation，B 已落地 `virtio` foundation，C 已落地 external workload harness，D 已落地 execution profile / observation foundation。

## 当前仍然有效的风险 / 限制

- `xv6-riscv` 预期会暴露大量 CSR、trap、timer、interrupt、storage / block、platform contract 细节缺口；当前仍无法精确预估这批缺口的规模。
- `xv6-riscv` bring-up 线虽然已经接入主线，并且当前已在真实 `virtio-blk` board path 上稳定到 shell，但这不代表 Linux 近在眼前；`xv6` 现在更多是稳定 workload guardrail 和 bug finder，而不是当前活跃 blocker 本身。
- 当前 `Machine` 默认 block transport 仍保持 `simple_storage` 以守住既有 guest / debug 路径；真实 `virtio-blk` 路径现在需要由 workload profile、CLI 或 debug CLI 显式选择，这条兼容性策略当前是有意保留的。
- “不做短寿命最小实现”会显著提高本轮对抽象边界的要求；如果控制不好，容易出现过度设计。当前必须持续用 `xv6`、未来 `Linux` 和未来 `JIT / DBT` 三个真实复用目标来约束抽象范围。
- 当前 `pipeline`、`V4` 和 `P4-prep-1` 已经具备可用结构边界，但这并不意味着可以直接跳到更重的 cache / DMA / multicore 或更激进 speculation；这些仍应在本轮主线站稳之后再决定。
- 当前最大的活跃 blocker 已不再是 `S-mode entry / SBI`、PLIC priority window、`RVC` 入口兼容性、modern `virtio-mmio` 的 `VIRTIO_F_VERSION_1` 探测、“是否有一份可挂载的 block-rootfs 磁盘镜像”，或 block-rootfs 下最小 `/init` 的 console / rootfs-rw / procfs / sysfs 闭环；这些最小 contract 已能把 Linux 稳定送到 repo-generated initramfs `/init reached`，并把 repo-generated ext4 block-rootfs 路径推进到 `EXT4-fs`、`VFS: Mounted root`、`devtmpfs: mounted`、`Run /init as init process`、`mycpu linux initrd: stage=console-opened`、`mycpu linux initrd: stage=rootfs-rw-ok`、`mycpu linux initrd: stage=proc-readable`、`mycpu linux initrd: stage=sys-readable`、`mycpu linux initrd: /init reached`、`mycpu linux userland: stage=file-readable`、`stage=rootfs-rw-roundtrip-ok`、`stage=fork-child-wrote`、`stage=parent-wait4-ok`、`stage=execve-third-stage`、`stage=mkdir-chdir-ok`、`stage=nested-file-roundtrip-ok`、`stage=getdents64-nested-visible`、`stage=fstatat-nested-stat-ok`、`stage=renameat2-syscall-ok`、`stage=renameat2-nested-ok`、`stage=renameat2-dirent-updated`、`stage=renameat2-cleanup-ok`、`stage=unlinkat-parent-dirent-gone`、`stage=mkdirat-dir-name-reusable`、`stage=mkdirat-reused-dir-empty`、`stage=mkdirat-reused-dir-dot-only`、`stage=mkdirat-reused-dir-parent-stat-ok`、`stage=third-stage-reached` 与 `post-init reached`。当前更近、也更活跃的 gap 已收敛成：如何在这个 repo-generated 最小 `rootfs.ext4` baseline 之上继续冻结 multi-stage post-init exec + path-resolution + getdents64 目录遍历 + `fstatat` 元数据读回 + `renameat2`/`unlinkat` 目录项可见性 + `mkdirat` 名字复用 + 重建目录空视图之后的下一处稳定用户态 checkpoint，而不是重新回头处理已关闭的 root mount / console prefix 问题。
- 当前 `linux_proto` 已经不再只是 boot contract dry-run profile；在显式提供外部 `Image` 时，它已能默认生成 board DTB、回落到 repo-generated initramfs、按需生成 repo-generated `rootfs.ext4`，并把真实 Linux 带到 `Unpacking initramfs...`、`devtmpfs: initialized`、`workingset`、`jitterentropy`、`xor`、initramfs `/init reached`，以及 block-rootfs 的 `mycpu linux initrd: stage=rootfs-rw-ok`、`stage=proc-readable`、`stage=sys-readable`、`stage=execve-post-init`、`mycpu linux userland: stage=file-readable`、`stage=rootfs-rw-roundtrip-ok`、`stage=fork-child-wrote`、`stage=parent-wait4-ok`、`stage=execve-third-stage`、`stage=mkdir-chdir-ok`、`stage=nested-file-roundtrip-ok`、`stage=getdents64-nested-visible`、`stage=fstatat-nested-stat-ok`、`stage=renameat2-syscall-ok`、`stage=renameat2-nested-ok`、`stage=renameat2-dirent-updated`、`stage=renameat2-cleanup-ok`、`stage=unlinkat-parent-dirent-gone`、`stage=mkdirat-dir-name-reusable`、`stage=mkdirat-reused-dir-empty`、`stage=mkdirat-reused-dir-dot-only`、`stage=mkdirat-reused-dir-parent-stat-ok`、`stage=third-stage-reached` 与 `post-init reached`。当前默认工作区已经通过 repo-generated 最小 `rootfs.cpio` 与 `rootfs.ext4` 去掉了外部 `initrd` / disk 镜像依赖，但仍缺真实 `Image` 输入资产；而 block-rootfs 侧当前已闭环的是“最小 `/init` + post-init file-read/write + process lifecycle + multi-stage exec + 目录/路径解析 + 目录项遍历 + 目录项元数据读回 + rename/unlink 后目录项视图更新 + 目录名复用 + 重建目录空视图 baseline”，不是更完整的 Linux 用户空间。
- 当前 Linux board contract 已推进到 `linux_sbi_shim + kernel/dtb/initrd payload + a0/a1/a2 seed + repo-generated chosen/initrd/timebase DTB + optional disk/bootargs alias`；后续如果 Linux 在 block-rootfs 之后继续暴露 gap，优先先判断是否需要再补更窄的 `virtio-blk` / mount / bootargs 合同，而不是回退到外部 DTB 手工维护。
- A 已经补上第一轮 `mhartid / misa.A / wfi / RV64A` foundation，但这不代表 `xv6` 后续会用到的全部 CSR / timer contract 都已齐备；更后面的 `pmp*`、`menvcfg`、`stimecmp` 等缺口仍可能继续暴露。

## 下一步

1. 把真实 `virtio-blk` board path 下的 `xv6` shell 里程碑继续守成稳定 guardrail，后续只按真实 bug 或明确收益补更窄 shell/userland/filesystem smoke，不再把“能否到 shell”当本轮主阻塞点。
2. 继续把真实 Linux `Image` 资产放到现有 harness 可消费的位置，并沿当前 `LINUX_PROTO_ROOTFS_MODE=block` + repo-generated `rootfs.ext4` 路径，在最小 `console-opened -> rootfs-rw-ok -> proc-readable -> sys-readable -> /init reached -> file-readable -> rootfs-rw-roundtrip-ok -> fork-child-wrote -> parent-wait4-ok -> execve-third-stage -> mkdir-chdir-ok -> nested-file-roundtrip-ok -> getdents64-nested-visible -> fstatat-nested-stat-ok -> renameat2-syscall-ok -> renameat2-nested-ok -> renameat2-dirent-updated -> renameat2-cleanup-ok -> unlinkat-parent-dirent-gone -> mkdirat-dir-name-reusable -> mkdirat-reused-dir-empty -> mkdirat-reused-dir-dot-only -> mkdirat-reused-dir-parent-stat-ok -> third-stage-reached -> post-init reached` baseline 之上继续冻结下一处稳定用户态 checkpoint 或 blocker；优先级已经从“把前缀补成完整 `/init reached`”切到“找出 multi-stage post-init exec + path-resolution + getdents64 目录遍历 + `fstatat` 元数据读回 + `renameat2`/`unlinkat` 目录项可见性 + `mkdirat` 名字复用 + 重建目录空视图之后真正还缺什么”。
3. A / B 后续都改成围绕 Linux bring-up 的 bug-driven hardening：`linux_sbi_shim`、PLIC contiguous source window、UART `MCR/MSR`、functional `RV64C`、modern `virtio-mmio VIRTIO_F_VERSION_1`、`100MHz` board DTB contract、block-rootfs 挂载，以及最小 `/init` console / rootfs-rw / procfs 闭环这几层 contract 都已经补上。接下来优先先判断更后面的 userland / tty / platform 路径暴露出的下一处 contract，而不是回头重复已关闭的 `RVC` / modern virtio / root mount / `/init` 前缀问题。
4. D 线继续作为读侧 guardrail：优先用 `execution_profile`、debug CLI、`run_debug_cli_probe` 和既有 workload smoke 锁住新引入的 `xv6 / virtio / Linux profile` 行为变化。当前至少要同时守住默认 functional `run-workload-xv6` 的非零 profile/shadow-cache，以及 pipeline vector CNN 的 RAM shadow-cache baseline，而不是新增一次性日志。

## 验证基线

- `cd myCPU && make test`
- `cd myCPU && make test-pipeline`
- `cd frontend && node --test`

本轮各 workstream 还应额外关注：

- Workstream A：`cd myCPU && make test-host-instruction_semantics_smoke`
- Workstream B：`cd myCPU && make test-unit-mmio_contract_matrix`、`cd myCPU && make test-host-virtio_blk_smoke`
- Workstream C：`cd myCPU && make test-host-run_debug_cli_probe`、`cd myCPU && make test-host-xv6_boot_smoke`、`cd myCPU && make test-host-xv6_shell_smoke`、`cd myCPU && make run-workload-xv6`
- Workstream D：`cd myCPU && make test-host-debug_cli_smoke`
