# 标准 Linux 发行版平台状态

## 文档定位

本文档只记录 `Post-Wave 7 标准 Linux 发行版平台` 这条新主线的当前基线、少量关键历史节点、当前仍有效的限制和下一步。

它不维护逐条执行流水账；更细的实施过程统一回写到
[../plan/post_wave7_linux_distribution_platform_plan.md](../plan/post_wave7_linux_distribution_platform_plan.md)
和 [../plan/history_plan.md](../plan/history_plan.md)。

## 关联文档

- 相关设计：
  - [../design/post_wave7_linux_distribution_platform_design.md](../design/post_wave7_linux_distribution_platform_design.md)
  - [../design/xv6_linux_jit_mainline_design.md](../design/xv6_linux_jit_mainline_design.md)
  - [../design/future_expansion_roadmap_design.md](../design/future_expansion_roadmap_design.md)
- 相关状态：
  - [mainline_status.md](mainline_status.md)
- 当前活跃计划：
  - [../plan/post_wave7_linux_distribution_platform_plan.md](../plan/post_wave7_linux_distribution_platform_plan.md)

## 当前状态

- `2026-05-02` 已把 `Wave 7` 阶段性收口之后的 Linux 后续方向正式收口为
  `标准 Linux 发行版平台` 新主线，并补齐独立 `design / plan / status` 入口。
- 当前 Linux 已完成的稳定基线仍是：
  - Linux fourth-stage checkpoint 冻结到
    `timerfd-one-shot-readback-ok`
  - `/console` 已有 gated `linux_proto_console`，显式提供真实 `Image` 后可进入
    `mycpu-linux# ` 提示符
  - reset / terminate / health check / config UX / real-image opt-in e2e 已有稳定 guardrail
- 当前这条线的目标已经不再是“继续向 fourth-stage smoke 追加更多同类 marker”，而是
  推进到更接近 QEMU 使用体验的标准 Debian / Alpine / RISC-V 发行版镜像。
- 当前远端云服务器上的 `Wave 7` 剩余部署 / 运维工作继续在远端 checkout 中推进；
  本地工作区的 Linux 方向现在聚焦于平台合同、runtime 资产合同和发行版级 bring-up。
- 当前第一刀已经收敛为：先补 `外部发行版运行资产合同 + opt-in shell command smoke`，
  不先做新的 frontend distro route，也不继续扩第四阶段同类 syscall marker。
- 当前第一刀代码入口已经落下最小 opt-in shell contract：
  - `make test-host-run_debug_cli_probe_linux_distribution_runtime`
  - `MYCPU_RUN_LINUX_DISTRO_RUNTIME=1`
  - `MYCPU_LINUX_DISTRO_RUNTIME_IMAGE=/path/to/Image`
  - `MYCPU_LINUX_DISTRO_RUNTIME_ROOTFS=/path/to/rootfs.ext4`
  - `MYCPU_LINUX_DISTRO_RUNTIME_PROMPT='...'`
  - `MYCPU_LINUX_DISTRO_RUNTIME_COMMAND='...'`
  - `MYCPU_LINUX_DISTRO_RUNTIME_EXPECT='...'`
  它当前通过 `--debug-cli` 直接证明“到达 prompt -> 输入命令 -> 观察输出 -> 回到 prompt”，
  还没有提升成新的前端 distro route。
- 当前这条 opt-in shell contract 已做默认值分层：
  - repo 自带 `linux_proto/rootfs.ext4` 默认仍走 `help -> commands: help uptime exit`
  - 显式外部 rootfs 默认切到 `cat /etc/os-release -> ID=`
  这样既保留现有 mini shell guardrail，也让外部发行版资产默认落在更像标准用户态的一条
  高信号命令上。
- 当前这条真实发行版 runtime guardrail 也已做 fail-closed 收口：
  - `make test-host-run_debug_cli_probe_linux_distribution_runtime` 现在必须显式提供
    `MYCPU_LINUX_DISTRO_RUNTIME_ROOTFS=/path/to/rootfs.ext4`
  - 没有外部 rootfs 时，target 与 host unittest 都不会回落到 repo 自带
    `linux_proto/rootfs.ext4`
- `2026-05-05` 已拿到第一条真实外部 rootfs 正向证据：用外部 Alpine riscv64 ext4
  rootfs、外部 Linux `Image` 和 `init=/init` 跑通
  `make test-host-run_debug_cli_probe_linux_distribution_runtime`。该证据证明当前平台可以通过
  `virtio-blk` 挂载外部 Alpine ext4，执行外部静态 `/init`，到达 `mycpu-distro# `，
  输入 `cat /etc/os-release`，观察到 `ID=alpine`，并回到 prompt。
  这份 rootfs 是本机临时运行资产，不纳入仓库默认资产。

## 关键历史节点

- `2026-05-05`
  - 已在本机生成外部 Alpine riscv64 block rootfs，并用不依赖仓库内 `linux_proto/rootfs.ext4`
    的路径跑通第一条发行版 runtime smoke：
    `external Image + external Alpine ext4 + static /init -> mycpu-distro# -> cat /etc/os-release -> ID=alpine`。
  - 同轮诊断确认更早失败不是 kernel 启动、DTB、`virtio-blk` 或 ext4 mount 问题：
    UART 日志已到 `EXT4-fs (vda): mounted filesystem`、`VFS: Mounted root` 和
    `Run /init`。
  - 当前 blocker 已收窄为动态链接 Alpine BusyBox / musl loader 用户态路径：
    当 `/init` 依赖 `/bin/sh` / BusyBox 动态用户态时，当前预算内仍等不到
    `mycpu-distro# `。
- `2026-05-02`
  - `Post-Wave 7 标准 Linux 发行版平台` 新主线正式启动，并新增：
    - [../design/post_wave7_linux_distribution_platform_design.md](../design/post_wave7_linux_distribution_platform_design.md)
    - [../plan/post_wave7_linux_distribution_platform_plan.md](../plan/post_wave7_linux_distribution_platform_plan.md)
    - [linux_distribution_platform_status.md](linux_distribution_platform_status.md)
  - 同日已新增第一刀 opt-in shell command smoke 入口：
    `make test-host-run_debug_cli_probe_linux_distribution_runtime`。
    它显式消费外部 `Image/rootfs/prompt/command/expected output` 合同，用 `--debug-cli`
    证明“启动到 shell prompt、执行一条命令、观察期望输出、再次回到 prompt”。
  - 同日这条 shell contract 已进一步分层：repo 自带 rootfs 默认保持 `help`，
    外部 rootfs 默认切到 `cat /etc/os-release -> ID=`。
  - 同日 `linux_distribution_runtime` target 和 host unittest 已加上 fail-closed 约束：
    必须显式提供外部 `MYCPU_LINUX_DISTRO_RUNTIME_ROOTFS`，不再隐式复用 repo 自带 rootfs。
- `2026-05-01` 到 `2026-05-02`
  - `Wave 7` Linux console 已完成 interactive route、hardening、terminate、reset re-arm、
    config UX、health check 和真实 `Image` opt-in e2e。
- `2026-04-29`
  - Linux fourth-stage checkpoint 冻结到 `timerfd-one-shot-readback-ok`，主线不再默认追加同类 syscall breadth。

## 当前仍然有效的风险 / 限制

- 仓库默认位置仍不携带标准发行版 `Image`、rootfs 或相关运行资产；真实发行版级 runtime
  断言仍必须显式 opt-in。
- 当前 `linux_proto_console` 仍是最小 shell / prompt 路线，不等同于标准 Debian / Alpine /
  RISC-V 发行版环境。
- 当前 `linux_proto_console` 的外部资产合同仍不完整：前端只显式检查 kernel `Image`，
  而标准发行版所需的 rootfs、bootargs、prompt 和命令回显合同还没有被单独收口。
- 发行版级平台所需的动态链接用户态、长期交互 shell、TTY / signal / timer / 文件系统 /
  virtio 稳定性和长期运行 contract 还没有被拆成独立验证矩阵；其中动态链接
  Alpine BusyBox / musl loader 已经是当前最直接 blocker。
- 当前前端展示和 debug CLI 路线可以复用，但它们不是 guest 可见平台语义的事实来源。
- 如果把这条线退回成继续追加 `timerfd` 之后的同类 marker，会延后真正的平台 gap 盘点。

## 下一步

1. 把 `external Alpine ext4 + static /init` 作为第一条发行版 runtime 正向基线保留，
   不把它扩大解释成完整发行版用户态支持。
2. 下一刀优先定位动态链接 Alpine BusyBox / musl loader 路径，先确认卡点落在 ELF
   interpreter / PIE loader、用户态页故障、syscall、signal 还是串口交互。
3. 动态用户态路径跑通后，再选下一条更强的发行版级命令合同，例如真实 `/bin/sh`
   下的 `cat /etc/os-release` 或 `ls -l /bin/sh`。
4. 在动态用户态 smoke 稳定前，不新开 frontend distro route；`linux_proto_console` 继续只声明
   受控 mini shell guardrail。
5. 继续守住现有 `xv6`、Linux probe、`linux_proto_console`、`make test`、
   `make test-pipeline` 和 `frontend` Node tests 这些稳定 guardrail。

## 验证基线

- `cd myCPU && make test`
- `cd myCPU && make test-pipeline`
- `cd frontend && node --test`
- `cd myCPU && make test-host-run_debug_cli_probe`
- `cd myCPU && make test-host-xv6_boot_smoke`
- `cd myCPU && make test-host-xv6_shell_smoke`
- `cd myCPU && python3 -m unittest tests.host.run_debug_cli_probe_test.RunDebugCliProbeTest.test_make_build_workload_linux_proto_block_mode_builds_post_init_smoke_elf`
