# 标准 Linux 发行版平台状态

## 文档定位

本文档只记录 `Post-Wave 7 标准 Linux 发行版平台` 这条新主线的当前基线、少量关键历史节点、当前仍有效的限制和下一步。

它不维护逐条执行流水账；更细的实施过程统一回写到
[../plan/post_wave7_linux_distribution_platform_longterm_plan.md](../plan/post_wave7_linux_distribution_platform_longterm_plan.md)
和 [../plan/history_plan.md#post-wave7-linux-distribution-platform-plan](../plan/history_plan.md#post-wave7-linux-distribution-platform-plan)。

## 关联文档

- 相关设计：
  - [../design/post_wave7_linux_distribution_platform_design.md](../design/post_wave7_linux_distribution_platform_design.md)
- 相关状态：
  - [mainline_status.md](mainline_status.md)
- 当前活跃计划：
  - [../plan/post_wave7_linux_distribution_platform_longterm_plan.md](../plan/post_wave7_linux_distribution_platform_longterm_plan.md)
- 已完成计划：
  - [../plan/history_plan.md#post-wave7-linux-distribution-platform-plan](../plan/history_plan.md#post-wave7-linux-distribution-platform-plan)

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
- `2026-05-05` 已拿到两条真实外部 Alpine rootfs 正向证据：
  - `init=/init` 静态 `/init` 路线：`virtio-blk` 挂载外部 Alpine ext4，执行外部静态
    `/init`，到达 `mycpu-distro# `，输入 `cat /etc/os-release`，观察到 `ID=alpine`，
    并回到 prompt。
  - `init=/bin/sh` 动态 BusyBox / musl 路线：经 debug CLI 等到真实 BusyBox `~ # `
    prompt，输入 `cat /etc/os-release`，观察到 `ID=alpine`，并回到 prompt。
  - 同一动态 BusyBox shell 会话的多命令路线：逐条执行
    `cat /etc/os-release`、`ls -l /bin/sh` 和 `/tmp` 写读回显，并且每条命令后都回到
    `~ # ` prompt。
  - 同一动态 BusyBox shell 会话的文件系统一致性路线：
    `test-host-run_debug_cli_probe_linux_distribution_filesystem` 使用
    `MYCPU_LINUX_DISTRO_RUNTIME_PROFILE=filesystem_consistency`，覆盖 `/tmp` 目录创建、
    文件写入、追加、读回、长度检查、删除、目录移除和后续 shell 存活。
  - 同一动态 BusyBox shell 会话的 TTY / login 第一阶段路线：
    `test-host-run_debug_cli_probe_linux_distribution_tty_login` 使用
    `MYCPU_LINUX_DISTRO_RUNTIME_PROFILE=tty_login_probe`，覆盖外部 Alpine rootfs 中
    `getty` / `login` / `stty` / `setsid` / `tty` 工具盘点、`tty` / `stty` / `setsid`
    往返，以及 BusyBox `getty -n -l /bin/sh -L 115200 ttyS0 vt100` 到等价 serial TTY
    prompt 后的后续输入输出往返。
  - 同一动态 BusyBox shell 会话的 process / timer / signal 第一阶段路线：
    `test-host-run_debug_cli_probe_linux_distribution_process_control` 使用
    `MYCPU_LINUX_DISTRO_RUNTIME_PROFILE=process_control`，覆盖 `sleep 1` timer 可见行为、
    后台子进程与 `wait` 返回码、`trap` / `kill -TERM $$` 最小 signal 处理，以及
    `||` / `&&` / `;` 和退出码读回的 shell 控制流。
  - 外部 Alpine rootfs 临时副本上的文件系统耐久性第一阶段路线：
    `test-host-run_debug_cli_probe_linux_distribution_filesystem_persistence` 使用
    `MYCPU_LINUX_DISTRO_RUNTIME_PROFILE=filesystem_persistence`，先把外部 rootfs 复制到
    `/tmp` 临时 ext4 副本，再覆盖 ext4 同会话目录创建、文件写入、`sync` 可见路径、
    rename overwrite、目录遍历、64 KiB 文件写读和清理。
  这份 rootfs 是本机临时运行资产，不纳入仓库默认资产；动态路线当前只声明最小
  shell command / 文件系统一致性 / 等价 serial TTY prompt / process-control / 同会话
  ext4 persistence smoke contract，不声明完整发行版矩阵、init 管理的 getty、密码 login、
  完整 signal 子系统、跨 reboot 持久性或完整 F/D 浮点算术支持。

## 关键历史节点

- `2026-05-05`
  - 已在本机生成外部 Alpine riscv64 block rootfs，并用不依赖仓库内 `linux_proto/rootfs.ext4`
    的路径跑通第一条发行版 runtime smoke：
    `external Image + external Alpine ext4 + static /init -> mycpu-distro# -> cat /etc/os-release -> ID=alpine`。
  - 同轮诊断确认更早失败不是 kernel 启动、DTB、`virtio-blk` 或 ext4 mount 问题：
    UART 日志已到 `EXT4-fs (vda): mounted filesystem`、`VFS: Mounted root` 和
    `Run /init`。
  - 修复前 blocker 已收窄为动态链接 Alpine BusyBox / musl loader 用户态路径：
    当 `/init` 依赖 `/bin/sh` / BusyBox 动态用户态时，当时预算内仍等不到
    `mycpu-distro# `。
  - 同日继续定位确认该动态 blocker 的第一崩点不是 rootfs / ext4 / `virtio-blk`，
    而是 musl loader `__setjmp` 中的 compressed `c.fsd` 原始 FPR 保存路径。已补
    FPR raw state、标准 `flw/fld/fsw/fsd` load-store 语义和 RVC `c.fld/c.fsd` /
    `c.fldsp/c.fsdsp` 解码，并保持既有自定义 vector `0x07/0x27` `funct3=0` 路径。
  - 修复后已用外部 Alpine riscv64 ext4 rootfs、外部 Linux `Image` 和
    `init=/bin/sh` 跑通真实动态 BusyBox shell：
    `~ # -> cat /etc/os-release -> ID=alpine -> ~ #`。动态 BusyBox / musl loader
    不再是当前近端 blocker。
  - 同日把 `linux_distribution_runtime` opt-in guardrail 扩成可选多命令序列合同：
    `MYCPU_LINUX_DISTRO_RUNTIME_COMMANDS` 可逐行声明 `command=>expected`，同一 shell
    会话内每条命令都必须观察到期望输出并回到 prompt。外部 Alpine 动态 `/bin/sh`
    已通过 `cat /etc/os-release`、`ls -l /bin/sh` 和 `/tmp` 写读三条命令。
  - 同日继续把长期交互 / 文件系统一致性 smoke 收口成独立 opt-in target：
    `make test-host-run_debug_cli_probe_linux_distribution_filesystem`。该 target 复用真实
    外部 `Image/rootfs/bootargs/prompt` 合同，设置
    `MYCPU_LINUX_DISTRO_RUNTIME_PROFILE=filesystem_consistency`，并已在外部 Alpine
    动态 `/bin/sh` 下通过 `/tmp` 目录创建、文件写入、追加、读回、长度检查、删除、
    目录移除和后续 shell 存活。
  - 同日完成长线阶段 1 的 TTY / login / console 第一刀：新增
    `make test-host-run_debug_cli_probe_linux_distribution_tty_login`，保持缺外部 rootfs
    fail-closed，并用 `MYCPU_LINUX_DISTRO_RUNTIME_PROFILE=tty_login_probe` 在真实外部
    Alpine 动态 `/bin/sh` 下证明 `/dev/ttyS0` 可见、TTY 工具可用、`tty` / `stty` /
    `setsid` 路线可交互，以及 BusyBox `getty -n -l /bin/sh -L 115200 ttyS0 vt100`
    能到达等价 serial TTY prompt 并完成后续输入输出往返。
  - 同日完成长线阶段 2 的 signal / timer / process 第一刀：新增
    `make test-host-run_debug_cli_probe_linux_distribution_process_control`，保持缺外部 rootfs
    fail-closed，并用 `MYCPU_LINUX_DISTRO_RUNTIME_PROFILE=process_control` 在真实外部
    Alpine 动态 `/bin/sh` 下证明 `sleep 1`、后台 `sleep` 子进程 + `wait` 返回码、
    `trap` / `kill -TERM $$` 和基础 shell 控制流均能在同一会话内观察到期望输出并回到
    `~ # ` prompt。
  - 同日完成长线阶段 3 的文件系统与块设备耐久性第一刀：新增
    `make test-host-run_debug_cli_probe_linux_distribution_filesystem_persistence`，保持缺外部
    rootfs fail-closed，并强制复制外部 Alpine rootfs 到 `/tmp` 临时 ext4 副本后运行；
    真实 smoke 已在同一动态 `/bin/sh` 会话内通过目录创建、文件写入、`sync`、rename
    overwrite、目录遍历、64 KiB 文件写读和清理。
- `2026-05-02`
  - `Post-Wave 7 标准 Linux 发行版平台` 新主线正式启动，并新增：
    - [../design/post_wave7_linux_distribution_platform_design.md](../design/post_wave7_linux_distribution_platform_design.md)
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
- 发行版级平台所需的 virtio 稳定性和长期运行 contract 还没有被拆成完整验证矩阵；
  动态 BusyBox / musl loader 已有最小 shell command、`/tmp` 文件系统一致性、等价
  serial TTY prompt 和 process / timer / signal 最小矩阵正向证据，但这不等同于完整
  发行版级支持。
- 当前 process / timer / signal 第一阶段只证明 BusyBox shell 可见的 `sleep`、子进程
  `wait`、`trap` / `kill` 和控制流；还不声明完整 signal delivery 语义、作业控制、
  多进程压力或长期 timer 稳定性。
- 当前 filesystem persistence 第一阶段只证明临时外部 rootfs 副本上的同会话 ext4 读写、
  `sync`、rename、目录遍历和较大文件一致性；还不声明跨 reboot / reset 后读回或 host
  镜像 flush 完整语义。
- 当前 TTY / login 第一阶段只证明 BusyBox getty autologin 到 `/bin/sh` 的等价 serial
  TTY prompt；还不声明 init 管理的 `/etc/inittab` getty、密码 login、PAM / shadow
  登录流或多终端会话支持。
- 当前新增的是 FPR 原始状态与 FP load/store 的最小合同，足以越过 musl loader 的
  `c.fsd` 保存现场路径；它还不是完整 F/D arithmetic、FS dirty state、`fcsr` 或 DTB
  ISA 字符串收口。
- 当前前端展示和 debug CLI 路线可以复用，但它们不是 guest 可见平台语义的事实来源。
- 如果把这条线退回成继续追加 `timerfd` 之后的同类 marker，会延后真正的平台 gap 盘点。

## 下一步

当前下一步按
[../plan/post_wave7_linux_distribution_platform_longterm_plan.md](../plan/post_wave7_linux_distribution_platform_longterm_plan.md)
执行，长线拆成五个阶段：

1. curated 发行版矩阵。
2. ISA / platform 合同补齐。

执行期间继续保留 `external Alpine ext4 + static /init`、`external Alpine ext4 + dynamic /bin/sh`
单命令、多命令 smoke、同一动态 shell 会话内的 `/tmp` 文件系统一致性 smoke、
`tty_login_probe` 等价 serial TTY prompt smoke、`process_control` 最小矩阵 smoke 和
`filesystem_persistence` 同会话 ext4 persistence smoke 作为正向基线；不要把它们扩大解释成
完整发行版用户态支持。五个阶段都较大，每个阶段彻底完成后才提交一次；其他中间 slice
不自动提交。

## 验证基线

- `cd myCPU && make test`
- `cd myCPU && make test-pipeline`
- `cd frontend && node --test`
- `cd myCPU && make test-host-run_debug_cli_probe`
- `cd myCPU && make test-host-run_debug_cli_probe_linux_distribution_tty_login`
- `cd myCPU && make test-host-run_debug_cli_probe_linux_distribution_process_control`
- `cd myCPU && make test-host-run_debug_cli_probe_linux_distribution_filesystem`
- `cd myCPU && make test-host-run_debug_cli_probe_linux_distribution_filesystem_persistence`
- `cd myCPU && make test-host-xv6_boot_smoke`
- `cd myCPU && make test-host-xv6_shell_smoke`
- `cd myCPU && python3 -m unittest tests.host.run_debug_cli_probe_test.RunDebugCliProbeTest.test_make_build_workload_linux_proto_block_mode_builds_post_init_smoke_elf`
