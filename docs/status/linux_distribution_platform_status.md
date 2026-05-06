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
- `2026-05-05` 已完成 curated 发行版矩阵第一刀：
  - 新增 `MYCPU_LINUX_DISTRO_RUNTIME_DISTRO=alpine|debian` 的 curated matrix 入口，固定
    distro-specific `Image/rootfs/bootargs/prompt/command/expected` 环境变量约定。
  - `make test-host-run_debug_cli_probe_linux_distribution_curated_alpine_shell` 继续复用外部
    Alpine 动态 `/bin/sh` shell 基线，默认 `init=/bin/sh`、`~ # `、
    `cat /etc/os-release -> ID=alpine`。
  - `make test-host-run_debug_cli_probe_linux_distribution_curated_debian_shell` 新增外部 Debian 13
    (`trixie`) riscv64 curated route：当前通过外部 Linux `Image`、外部 Debian ext4、
    `init=/mycpu-debian-init`、serial wrapper prompt `mycpu-debian# ` 和
    `cat /etc/os-release -> ID=debian` 形成 shell 第一刀正向证据。
  - `make test-host-run_debug_cli_probe_linux_distribution_curated_matrix` 会顺序运行 Alpine /
    Debian 两条 curated shell target；缺少各自外部 rootfs 时保持 fail-closed。
  - Debian 当前只声明 `shell` profile；Alpine 继续声明 `shell`、`filesystem_consistency`、
    `tty_login_probe`、`process_control` 和 `filesystem_persistence`。

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
  - 同日继续推进长线阶段 5 的 ISA / platform 合同补齐：
    - 修正 DTB `riscv,isa` 广告从硬编码 `rv64ima_zicsr_zifencei` 改成与当前实现一致的
      `rv64imac_zicsr_zifencei`，并新增
      `make test-host-run_debug_cli_probe_linux_distribution_curated_alpine_isa_advertisement`
      真实 opt-in target，用 Alpine boot log 里的
      `riscv: base ISA extensions ...` / `ELF capabilities ...` 验证当前广告集合为
      `{a,c,i,m}`。
    - host 侧已把真实 Alpine `awk` 热路径上踩到的 `0x53` 子集继续补到
      `fmv.d.x` / `fmv.x.d` / `fmv.d` / `fcvt.d.w`、`frm` / `fcsr` aliases、
      `fmul.d` / `fadd.d` / `fsub.d` / `fdiv.d` / `fsqrt.d` / `fcvt.w.d` / `fcvt.l.d` /
      `fcvt.d.l` / `feq.d` / `fle.d` / `fneg.d`，并补了对应的
      `instruction_semantics_smoke` / `pipeline_backend_smoke` 红绿回归。
    - 同轮把最小 `fcsr` 异常标志合同继续接回真实执行路径：`fdiv.d 7.0 / 0.0` 现在会把
      `CSR_FFLAGS.DZ` 写回到 guest 可见 `fcsr` alias，`fsqrt.d sqrt(-1.0)` 现在会把
      `CSR_FFLAGS.NV` 写回到 guest 可见 `fcsr` alias；随后又补上了
      `fsqrt.d sqrt(2.0) -> CSR_FFLAGS.NX`、`fmul.d 1e308*1e308 -> CSR_FFLAGS.OF` 和
      `fmul.d 1e-308*1e-308 -> CSR_FFLAGS.UF` 的 host / pipeline 回归。真实 Alpine
      BusyBox `awk` opt-in smoke 也已扩到 rounding / overflow / underflow 路径并重新通过。
    - 同轮把一条最小 `zihpm` capability 合同接回 guest 可见路径：Linux DT 路线在挂载
      procfs 后会把 `zicntr` / `zihpm` 补进 `/proc/cpuinfo` 的 `isa` 串，因此当前已把
      `hpmcounter3-31` / `mhpmcounter3-31` / `mhpmevent3-31` 从非法 CSR 收成最小合法
      CSR，并补上 `hpmcounter3-31 -> mhpmcounter3-31` alias 一致性；对应 host
      `atomic_semantics_smoke` 回归与
      `test-host-run_debug_cli_probe_linux_distribution_curated_alpine_proc_cpuinfo_isa_view`
      真实 Alpine procfs smoke 已新增并通过。当前真实外部 Alpine rootfs 已通过
      `mount -t proc proc /proc -> grep '^isa' /proc/cpuinfo -> rv64imac_zicntr_zicsr_zifencei_zihpm`。
    - 同轮把一条最小 guest-visible `hwcap` 合同接回用户态探测路径：真实外部 Alpine rootfs
      现在已通过
      `test-host-run_debug_cli_probe_linux_distribution_curated_alpine_auxv_hwcap_view`，
      用 `mount -t proc proc /proc -> od -An -tx8 -w16 /proc/self/auxv` 固化
      `AT_HWCAP=0x1105` 的当前 guest 视图，避免后续 `hwcap` 广告在没有真实 rootfs 证据时漂移。
    - 同轮新增一条离线 Alpine userland ABI guardrail：
      `test-host-run_debug_cli_probe_linux_distribution_curated_alpine_busybox_userland_abi_view`
      直接从外部 Alpine ext4 rootfs 提取 `/bin/busybox`，并用 host `readelf` 固化
      `Flags: 0x5, RVC, double-float ABI` 与
      `Tag_RISCV_arch: rv64...f...d...c...`。这说明当前外部 Alpine BusyBox 用户态本身是
      `double-float ABI` / `rv64imafdc...` 构建；它和 guest-visible `AT_HWCAP=0x1105`
      (`IMAC`) 已经形成一条可重复观察的 capability gap 证据链，后续不再需要靠猜测判断。
      同轮还补充了一次离线静态指令盘点：`/bin/busybox` 与 `/lib/ld-musl-riscv64.so.1`
      明确还会用到 `flt.d`、`fcvt.wu.d`、`fcvt.lu.d`，以及更远的 `fmadd.d/fmsub.d/fnmsub.d`
      和一批单精度 `*.s` 指令；因此这条主线当前的近端工作不再是“猜测 BusyBox 还会不会踩
      新路径”，而是沿真实外部 userland 静态面和动态 smoke 逐步收口最小缺口。
    - 同轮还确认了当前 `AT_HWCAP=0x1105` 的根因路径：Linux RISC-V `elf_hwcap`
      就是从 DT `riscv,isa` 单字母扩展集合折算而来，所以当前 guest-visible `IMAC`
      与 `mycpu_virt.dts` 里的 `rv64imac_zicsr_zifencei` 是一致的，不是内核额外“漂移”
      出来的视图；真正的矛盾点仍然是“guest 广告 IMAC，但外部 Alpine BusyBox userland
      是 lp64d/imafdc”。
    - 同轮把一条最小 `FS state` 合同接回 host / pipeline 执行路径：`mstatus/sstatus`
      现在已新增 `FS` / `SD` 位模型，浮点 load/store 与 `0x53` 浮点指令提交后会把
      `FS=DIRTY` 置回 guest 可见 CSR。对应 host 红绿已补到
      `atomic_semantics_smoke`、`instruction_semantics_smoke` 和
      `pipeline_backend_smoke`。真实 Alpine rootfs 上则暂未保留自动化
      `FS state` guardrail：现有 runtime snapshot 采样经常正好落在 Linux S-mode trap /
      中断路径，内核会按自身约定把 `FS` 从用户态 dirty 整理回 `INITIAL/CLEAN`，
      因此当前只有 host / pipeline 侧正向证据，real-rootfs `FS state`
      仍是当前近端 blocker，而不是已完成能力。
    - 真实 Alpine BusyBox `awk` opt-in smoke 已从单条表达式扩成基础浮点矩阵，当前已能稳定
      跑通 `1.5+2.25 -> 3.75`、`if ((1.5+2.25)==3.75) -> 11`、`7/2 -> 3.5`、
      `printf "%d", 7/2 -> 3`、`printf "%u", 7/2 -> 3`、`printf "%x", 15/2 -> 7`、
      `if (1.5 < 2.25) -> 1`、`if (2.25 <= 2.25) -> 1`、`if (2.25 > 1.5) -> 1`、
      `printf "%u", -1 -> 4294967295`、`printf "%x", -1 -> ffffffff`、
      `printf "%.0f", 2.5/3.5 -> 2/4`、`printf "%.1f", 2.25/2.35/-2.35 -> 2.2/2.4/-2.4`、
      `1e308*1e308 -> inf`、`1e-308*1e-308 == 0`、`sqrt(2) -> 1.41421` 和
      `sqrt(-1) -> nan`；同轮 host / pipeline 侧还新增了 `flt.d`、`fcvt.wu.d`、
      `fcvt.lu.d`、`fcvt.d.wu`、`fcvt.d.lu` 的最小合同，避免下一步外部 userland
      继续沿 double compare / unsigned convert 路径撞上最基础的非法指令；本轮又继续把离线
      静态面里已经出现的最小三输入 double FMA 子集 `fmadd.d` / `fmsub.d` / `fnmsub.d`
      接回 shared semantics 与 pipeline：`Insn/decode` 已显式保留 `rs3`，`SemanticInputs`
      已能携带第三个浮点源，host / pipeline smoke 也已新增并通过对应三条 double FMA 路径；
      同轮还继续把静态面里已经出现的 `fsgnj.d` / `fsgnjn.d` / `fabs.d` alias 收成最小
      double sign-injection 合同，并在 pipeline 侧补上一个更关键的真实依赖门禁：
      当 older `fld` 或其他未提交的浮点写仍在 ROB 中时，年轻浮点消费者现在不会再直接从
      `core.fpr` 偷读 stale 值；对应 `fld -> fadd.d` 的 pipeline load-use smoke 已新增并通过。
      本轮又把离线静态面里已经出现的第一对单精度 convert `fcvt.d.s` / `fcvt.s.d`
      接回 shared semantics 与 pipeline smoke，并把它们纳入 older FP pending 分类，
      避免后续 `fcvt.* -> 双精度消费者` 这类链路绕过同一条最小 stall 合同。
      随后又沿同一条静态 userland 路径补上 `fcvt.s.w`：`floating_ops` 现已支持
      `funct7=0x68 rs2=0` 的 int-to-single convert，pipeline decode/hazard 入口也已把
      `fcvt.s.w` 重新归类成“读 GPR rs1、写 FPR rd”的指令，避免 `fcvt.s.w -> fcvt.d.s`
      这种真实链路把 `a0` 误当成 `x0` 或让年轻消费者读到 stale FPR。
      本轮又继续把 `fcvt.w.s` 接回 shared semantics 与 pipeline 整数写回路径，先只覆盖
      signed single-to-int32 这条最小 convert 路径；当前仍不把这扩写成
      `fcvt.wu.s/l.s/lu.s` 或更完整 single-to-int 家族已经完成。
      本轮又继续把 `fcvt.l.s` 接回 shared semantics 与 pipeline 整数写回路径，先只覆盖
      signed single-to-int64 这条最小 convert 路径；当前仍不把 `fcvt.wu.s/lu.s` 或更完整
      single-to-int 家族写成已完成能力。
      本轮又继续把 `fcvt.wu.s` 接回 shared semantics 与 pipeline 整数写回路径，先只覆盖
      unsigned single-to-int32 这条最小 convert 路径；当前仍不把 `fcvt.lu.s` 或完整 unsigned
      single-to-int 家族写成已完成能力。
      本轮又继续把 `fcvt.lu.s` 接回 shared semantics 与 pipeline 整数写回路径，先只覆盖
      unsigned single-to-int64 这条最小 convert 路径；当前这组 single-to-int 合同仍不代表
      rounding / exception / 全家族语义已经收口。
      本轮又继续把外部 `ld-musl` 静态面里明确出现的 `fsgnj.s` 接回 shared semantics 与
      pipeline smoke，作为单精度 sign-injection 的第一刀；随后又继续把 `fsgnjn.s`
      和 `fabs.s` alias 接回 shared semantics、FPR 源分类、older-FP pending 分类和
      host / pipeline smoke，收掉单精度 sign-injection 的最小第二刀；本轮又继续补上
      `fsgnjx.s` 和 `fneg.s` alias 的 host / pipeline 正向证据，把单精度 sign-injection
      四条最小合同收齐；本轮又继续把外部 `ld-musl` 静态面里高频出现的 `fmv.w.x / fmv.x.w`
      接回 shared semantics、pipeline 入口分类和 host / pipeline smoke，收掉单精度
      bit-move family 的最小第一刀；本轮又继续把同一静态面里明确出现的
      `feq.s / flt.s / fle.s` 接回 shared semantics、FPR 源分类和 host / pipeline smoke，
      收掉单精度 compare family 的最小第一刀；本轮又继续把 `fcvt.s.l` 接回 shared semantics、
      pipeline 入口分类和 host / pipeline smoke，并用 `1ULL << 40` 这类真实 64-bit 输入把
      它同 `fcvt.s.w` 显式区分开，收掉 single int64-to-float convert 的最小第一刀；随后又把
      `fcvt.s.lu` 接回 shared semantics、pipeline 入口分类和 host / pipeline smoke，先只覆盖
      unsigned int64-to-float 这条最小 single convert 路径；随后又把 `fcvt.s.wu`
      接回 shared semantics、pipeline 入口分类和 host / pipeline smoke，先只覆盖 unsigned
      int32-to-float 这条最小 single convert 路径，并显式守住“读 GPR rs1、写 FPR rd”的
      int-to-float 合同；对应 `instruction_semantics_smoke` / `pipeline_backend_smoke`
      已新增并通过。本轮又继续把 `fsqrt.s` 的正常路径
      接回 shared semantics、FPR 源分类和 host / pipeline smoke，先只覆盖
      `sqrt(2.25f) -> 1.5f` 这条最小 single sqrt 合同；随后又补上
      `fsqrt.s sqrt(-1.0f) -> CSR_FFLAGS.NV` 与 `fsqrt.s sqrt(2.0f) -> CSR_FFLAGS.NX`
      的 host / pipeline 正向证据，并确认它们都会保留 guest 可见 rounding-mode bits；
      随后又把 `fmul.s max*max -> CSR_FFLAGS.OF` 和 `fmul.s min*min -> CSR_FFLAGS.UF`
      的 host / pipeline 正向证据补齐；当前仍不把整批 `.s` 异常矩阵写成已完成能力。
      本轮又继续把单精度基础二元算术第一刀 `fadd.s` / `fsub.s` / `fmul.s` / `fdiv.s`
      接回 shared semantics、FPR 源分类、older-FP pending 分类和 host / pipeline smoke，
      先只覆盖普通有限值加减乘，以及 `fdiv.s` 的正常路径、divide-by-zero `CSR_FFLAGS.DZ`、
      `0.0f / 0.0f -> CSR_FFLAGS.NV`、`1.0f / 3.0f -> CSR_FFLAGS.NX` 这组最小单精度除法异常路径；
      对应 `instruction_semantics_smoke` / `pipeline_backend_smoke` 已新增并通过。
      当前仍不把 single FMA、更完整 `.s` 异常标志矩阵、NaN 角落语义或更大单精度运算面写成已完成能力。
      随后又把单精度三输入 FMA 第一刀 `fmadd.s` / `fmsub.s` / `fnmsub.s` / `fnmadd.s`
      接回 shared semantics、FPR 源分类、older-FP pending 分类和 host / pipeline smoke，
      先只覆盖普通有限 single 值 fused multiply-add/subtract 路径；随后又补上
      `fmadd.s inf*0+1 -> CSR_FFLAGS.NV` 与 `fmadd.s 1*1+2^-25 -> CSR_FFLAGS.NX`
      的 host / pipeline 正向证据，并确认它们都会保留 guest 可见 rounding-mode bits；随后又补上
      `fmadd.s max*max+0 -> CSR_FFLAGS.OF` 与 `fmadd.s min*min+0 -> CSR_FFLAGS.UF`
      的 host / pipeline 正向证据；随后又把同一套最小异常标志合同补到 `fmsub.s`：
      `fmsub.s inf*0-1 -> CSR_FFLAGS.NV`、`fmsub.s 1*1-2^-25 -> CSR_FFLAGS.NX`、
      `fmsub.s max*max-0 -> CSR_FFLAGS.OF`、`fmsub.s min*min-0 -> CSR_FFLAGS.UF`
      现在也都有 host / pipeline 正向证据，并同样保留 guest 可见 rounding-mode bits；随后又把同一套最小异常标志合同补到 `fnmsub.s`：
      `fnmsub.s -(inf*0)+1 -> CSR_FFLAGS.NV`、`fnmsub.s -(1*1)+2^-25 -> CSR_FFLAGS.NX`、
      `fnmsub.s -(max*max)+0 -> CSR_FFLAGS.OF`、`fnmsub.s -(min*min)+0 -> CSR_FFLAGS.UF`
      现在也都有 host / pipeline 正向证据，并同样保留 guest 可见 rounding-mode bits；随后又把同一套最小异常标志合同补到 `fnmadd.s`：
      `fnmadd.s -(inf*0)-(-1) -> CSR_FFLAGS.NV`、`fnmadd.s -(1*1)-(-2^-25) -> CSR_FFLAGS.NX`、
      `fnmadd.s -(max*max)-(-0) -> CSR_FFLAGS.OF`、`fnmadd.s -(min*min)-(-0) -> CSR_FFLAGS.UF`
      现在也都有 host / pipeline 正向证据，并同样保留 guest 可见 rounding-mode bits；对应
      `instruction_semantics_smoke` / `pipeline_backend_smoke` 已新增并通过。当前仍不把这扩写成
      更完整 `.s` 异常标志矩阵、NaN 角落语义或完整 single arithmetic 已完成能力。
      随后又把同一静态面里高频出现的 `fclass.s` 接回 shared semantics 与 pipeline smoke，
      先只覆盖 quiet-NaN 和正 normal 这类真实已验证分类路径；当前仍不把这扩写成
      `fclass.d` 或完整单精度分类矩阵已经完成。
      本轮又继续把 `fclass.d` 接回 shared semantics 与 pipeline smoke，先只覆盖
      quiet-NaN 和正 normal 这类真实已验证 double 分类路径；当前仍不把这扩写成
      完整 `fclass` family 已经完成。
      本轮又继续把外部 `ld-musl` 静态面里明确出现的 `fmax.d / fmin.d` 接回 shared
      semantics 与 pipeline smoke，先只覆盖普通有限 double 值的 max/min 选择路径；
      当前仍不把这扩写成对应 `.s` 版本或更完整 NaN 角落语义已经完成。随后又把同一静态面里
      明确还缺的 `fnmadd.d` 接回 shared semantics、older-FP pending 分类和 host / pipeline smoke，
      把 double FMA 第一组四条三输入路径补齐；随后又补上
      `fmadd.d inf*0+1 -> CSR_FFLAGS.NV` 与 `fmadd.d 1*1+2^-54 -> CSR_FFLAGS.NX`
      的 host / pipeline 正向证据，并确认它们都会保留 guest 可见 rounding-mode bits；随后又补上
      `fmadd.d max*max+0 -> CSR_FFLAGS.OF` 与 `fmadd.d min*min+0 -> CSR_FFLAGS.UF`
      的 host / pipeline 正向证据；随后又把同一套最小异常标志合同补到 `fmsub.d`：
      `fmsub.d inf*0-1 -> CSR_FFLAGS.NV`、`fmsub.d 1*1-2^-54 -> CSR_FFLAGS.NX`、
      `fmsub.d max*max-0 -> CSR_FFLAGS.OF`、`fmsub.d min*min-0 -> CSR_FFLAGS.UF`
      现在也都有 host / pipeline 正向证据，并同样保留 guest 可见 rounding-mode bits；随后又把同一套最小异常标志合同补到 `fnmsub.d`：
      `fnmsub.d -(inf*0)+1 -> CSR_FFLAGS.NV`、`fnmsub.d -(1*1)+2^-54 -> CSR_FFLAGS.NX`、
      `fnmsub.d -(max*max)+0 -> CSR_FFLAGS.OF`、`fnmsub.d -(min*min)+0 -> CSR_FFLAGS.UF`
      现在也都有 host / pipeline 正向证据，并同样保留 guest 可见 rounding-mode bits；随后又把同一套最小异常标志合同补到 `fnmadd.d`：
      `fnmadd.d -(inf*0)-(-1) -> CSR_FFLAGS.NV`、`fnmadd.d -(1*1)-(-2^-54) -> CSR_FFLAGS.NX`、
      `fnmadd.d -(max*max)-(-0) -> CSR_FFLAGS.OF`、`fnmadd.d -(min*min)-(-0) -> CSR_FFLAGS.UF`
      现在也都有 host / pipeline 正向证据，并同样保留 guest 可见 rounding-mode bits；本轮又继续把单精度 `fmax.s / fmin.s` 接回 shared
      semantics、older-FP pending 分类和 host / pipeline smoke，先只覆盖普通有限 single 值的
      min/max 选择路径；随后又继续把 compare/minmax 的最小 NaN/invalid flag 合同接回
      shared semantics 与 pipeline：`feq.s` 在 quiet-NaN 下返回 unordered `0` 且不置
      `NV`，在 signaling-NaN 下返回 unordered `0` 且把 `CSR_FFLAGS.NV` 写回；
      `flt.s` / `fle.s` 在 NaN 下返回 unordered `0` 且把 `CSR_FFLAGS.NV` 写回；
      `fmin.s` / `fmax.s` 在单侧 NaN 下返回非 NaN 操作数、在双侧 quiet-NaN 下返回
      canonical NaN，并在 signaling-NaN 路径上把 `CSR_FFLAGS.NV` 写回；对应
      `instruction_semantics_smoke` / `pipeline_backend_smoke` 已新增并通过；随后又把同一套
      最小合同补到 double compare/minmax：`feq.d` 在 quiet-NaN 下返回 unordered `0`
      且不置 `NV`，在 signaling-NaN 下返回 unordered `0` 且把 `CSR_FFLAGS.NV` 写回；
      `flt.d` / `fle.d` 在 NaN 下返回 unordered `0` 且把 `CSR_FFLAGS.NV` 写回；
      `fmin.d` / `fmax.d` 在单侧 NaN 下返回非 NaN 操作数、在双侧 quiet-NaN 下返回
      canonical double NaN，并在 signaling-NaN 路径上把 `CSR_FFLAGS.NV` 写回；对应
      `instruction_semantics_smoke` / `pipeline_backend_smoke` 已新增并通过。随后又把
      `fcvt.w.s` / `fcvt.w.d` 的 `rm=111(dyn)` 路径接回 shared semantics 与 pipeline：
      现在会真实读取 guest `frm`，并在 `frm=RUP` 下的 `7.5f -> 8`、`frm=RDN`
      下的 `-3.5 -> -4` 这类可区分的非精确 float-to-int convert 路径上把
      `CSR_FFLAGS.NX` 写回到 guest 可见 `fcsr` alias，
      同时保留 rounding-mode bits；对应 `instruction_semantics_smoke` /
      `pipeline_backend_smoke` 已新增并通过。随后又把同一套动态 rounding / `fcsr`
      合同补到 `fcvt.l.s` / `fcvt.l.d`：`rm=111(dyn)` 现在在 `frm=RUP` /
      `frm=RDN` 下也会真实读取 guest `frm`，并用 `7.5f -> 8`、`-3.5 -> -4`
      这类可区分样例把 `CSR_FFLAGS.NX` 写回到 guest 可见 `fcsr` alias，同时保留
      rounding-mode bits；随后又把同一条动态 rounding 合同补到
      `fcvt.lu.s` / `fcvt.lu.d`：`rm=111(dyn)` 现在在 `frm=RMM` 下也会按
      round-to-nearest, ties-to-max-magnitude 处理 `3.5f -> 4` 与 `3.25 -> 3`，
      并把 `CSR_FFLAGS.NX` 写回到 guest 可见 `fcsr` alias，同时保留 rounding-mode
      bits；对应 `instruction_semantics_smoke` / `pipeline_backend_smoke`
      已新增并通过。本轮又继续把同一条 float-to-int helper 的
      `RMM` 第一刀接回 `fcvt.wu.s` / `fcvt.wu.d`：`rm=111(dyn)` 现在在
      `frm=RMM` 下会按 round-to-nearest, ties-to-max-magnitude 处理
      `3.25f` / `3.25 -> 3` 与 `3.5f` / `3.5 -> 4`，并把 `CSR_FFLAGS.NX`
      写回到 guest 可见 `fcsr` alias，同时保留 rounding-mode bits；对应
      `instruction_semantics_smoke` / `pipeline_backend_smoke` 已新增并通过。本轮又继续把
      `fcvt.wu.s` / `fcvt.wu.d` 的 RV64 结果形状合同收回到 shared semantics 与 pipeline：
      当前 `WU` family 会把 32-bit unsigned 结果按 RV64 `X` 寄存器写回规则做符号扩展，
      因此 `4294967295 -> 0xffffffffffffffff`；同时也补了 `.s/.d` 路径上
      `qNaN/-1.0 -> 0` 与 `+inf/qNaN -> UINT32_MAX` 的最小 invalid clipping 路径，并确认它只置
      `CSR_FFLAGS.NV`、不额外置 `NX`；随后又把同一条 invalid clipping 合同补到
      `fcvt.lu.s` / `fcvt.lu.d`：`qNaN/-1.0 -> 0`、`+inf/qNaN -> UINT64_MAX` 现在都有
      host / pipeline 正向证据，并同样只置 `NV`、不额外置 `NX`；本轮再把
      `fcvt.w.s` / `fcvt.w.d` 的 signed int32 invalid clipping 证据补齐：`+inf/qNaN -> INT32_MAX`、
      `-inf -> INT32_MIN` 现在也都有 host / pipeline 正向证据，并同样只置
      `NV`、不额外置 `NX`；随后又把同一条 signed invalid clipping 合同补到
      `fcvt.l.s` / `fcvt.l.d`：`+inf/qNaN -> INT64_MAX`、`-inf -> INT64_MIN` 现在也都有
      host / pipeline 正向证据，并同样只置 `NV`、不额外置 `NX`；对应
      `instruction_semantics_smoke` / `pipeline_backend_smoke` 已新增并通过。当前仍不把这扩写成完整 `.s/.d -> {w,wu,l,lu}`
      越界 / NaN / 饱和结果矩阵已经完成。当前剩余重心已经不再是 double FMA opcode 缺口，而是单精度 `*.s`
      尾项、更完整异常/rounding/NaN 角落语义，以及 real-rootfs `FS state` / capability 收口。
      动态路径已从最早的 musl loader `fmv.d.x` illegal instruction，
      前移到 BusyBox 后续 `F/D compare/convert` 与更完整的异常 / rounding / capability
      尾项，不再停留在早期 DTB/loader/首条 F/D move 缺口。
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
- Debian curated route 当前依赖外部 rootfs 上的 `mycpu-debian-init` wrapper shell 合同；
  还不声明原生 Debian `/bin/sh`、init 管理的 getty 或密码 login 已完整稳定。
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
  `c.fsd` 保存现场路径；当前又进一步补齐到 `fmv.d.x` / `fmv.x.d` / `fmv.d` /
  `fcvt.d.w` / `fmul.d` / `fadd.d` / `fsub.d` / `fcvt.l.d` / `fcvt.d.l` / `feq.d`
  和 DTB `riscv,isa` 广告一致性；当前已额外覆盖 `fdiv.d` 的最小 `CSR_FFLAGS.DZ`、
  `fsqrt.d sqrt(-1.0)` 的最小 `CSR_FFLAGS.NV`、`fsqrt.d sqrt(2.0)` 的最小
  `CSR_FFLAGS.NX`、`fmul.d 1e308*1e308` 的最小 `CSR_FFLAGS.OF` 和
  `fmul.d 1e-308*1e-308` 的最小 `CSR_FFLAGS.UF` 写回合同，并补上了与 Linux
  `/proc/cpuinfo` `isa` 视图一致的最小 `hpmcounter3-31` / `mhpmcounter3-31` /
  `mhpmevent3-31` CSR 合同、`hpmcounter3-31 -> mhpmcounter3-31` alias 一致性，以及
  `AT_HWCAP=0x1105` 的当前 guest-visible `auxv` 视图；同时，离线 Alpine BusyBox
  userland ABI guardrail 已确认 `/bin/busybox` 本身是 `double-float ABI` /
  `rv64...f...d...c...` 构建。当前这条线已经有“guest advertises IMAC while userland binary
  is lp64d/imafdc” 的明确证据；当前 host / pipeline 的最小 `FS=DIRTY` 合同也已落下，
  但真实 Alpine rootfs 上仍未形成稳定 `FS state` 正向 guardrail，因此目前没有把它保留成
  自动化 opt-in target；它仍然不是完整
  F/D arithmetic、FS dirty state、
  完整 `fcsr` 异常标志或 hwcap / 用户态 capability 收口。
- 当前前端展示和 debug CLI 路线可以复用，但它们不是 guest 可见平台语义的事实来源。
- 如果把这条线退回成继续追加 `timerfd` 之后的同类 marker，会延后真正的平台 gap 盘点。

## 下一步

当前下一步按
[../plan/post_wave7_linux_distribution_platform_longterm_plan.md](../plan/post_wave7_linux_distribution_platform_longterm_plan.md)
执行，长线拆成五个阶段：

1. ISA / platform 合同补齐。

执行期间继续保留 `external Alpine ext4 + static /init`、`external Alpine ext4 + dynamic /bin/sh`
单命令、多命令 smoke、同一动态 shell 会话内的 `/tmp` 文件系统一致性 smoke、
`tty_login_probe` 等价 serial TTY prompt smoke、`process_control` 最小矩阵 smoke 和
`filesystem_persistence` 同会话 ext4 persistence smoke 作为正向基线；不要把它们扩大解释成
完整发行版用户态支持。当前近端 blocker 已从 DTB `riscv,isa` 广告不一致和最早
`F/D move/convert` 缺失，推进到 BusyBox `awk` 动态路径的更完整异常 / rounding /
capability 尾项；`fdiv.d -> CSR_FFLAGS.DZ`、`fsqrt.d sqrt(-1.0) -> CSR_FFLAGS.NV`、
`fsqrt.d sqrt(2.0) -> CSR_FFLAGS.NX`、`fmul.d 1e308*1e308 -> CSR_FFLAGS.OF`、
`fmul.d 1e-308*1e-308 -> CSR_FFLAGS.UF`、`/proc/cpuinfo isa` 视图所需的最小
`zihpm` CSR 合同、`AT_HWCAP=0x1105` 的当前 guest-visible `auxv` 视图，以及 Alpine BusyBox
`double-float ABI` / `rv64...f...d...c...` 的 userland ABI 事实都已不再是“未知项”；当前 blocker
已经收窄到如何让 guest-visible capability 广告、真实 Linux trap/return 路径下的 `FS state`，
以及这条真实 userland ABI 事实收口一致。
五个阶段都较大，每个阶段彻底完成后才提交一次；其他中间 slice 不自动提交。

## 验证基线

- `cd myCPU && make test`
- `cd myCPU && make test-pipeline`
- `cd frontend && node --test`
- `cd myCPU && make test-host-run_debug_cli_probe`
- `cd myCPU && make test-host-run_debug_cli_probe_linux_distribution_tty_login`
- `cd myCPU && make test-host-run_debug_cli_probe_linux_distribution_process_control`
- `cd myCPU && make test-host-run_debug_cli_probe_linux_distribution_filesystem`
- `cd myCPU && make test-host-run_debug_cli_probe_linux_distribution_filesystem_persistence`
- `cd myCPU && make test-host-run_debug_cli_probe_linux_distribution_curated_alpine_proc_cpuinfo_isa_view`
- `cd myCPU && make test-host-run_debug_cli_probe_linux_distribution_curated_alpine_auxv_hwcap_view`
- `cd myCPU && make test-host-run_debug_cli_probe_linux_distribution_curated_alpine_busybox_userland_abi_view`
- `cd myCPU && make test-host-atomic_semantics_smoke`
- `cd myCPU && make test-host-xv6_boot_smoke`
- `cd myCPU && make test-host-xv6_shell_smoke`
- `cd myCPU && python3 -m unittest tests.host.run_debug_cli_probe_test.RunDebugCliProbeTest.test_make_build_workload_linux_proto_block_mode_builds_post_init_smoke_elf`
