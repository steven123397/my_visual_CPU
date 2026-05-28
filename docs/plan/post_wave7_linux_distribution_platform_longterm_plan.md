# Post-Wave 7 标准 Linux 发行版平台长线计划

> **文档状态：** 执行中

## 文档定位

本文档用于承接 `Post-Wave 7 标准 Linux 发行版平台` 主线的长期执行。它不是短期单点修复计划，而是把后续较长时间内要推进的五个大阶段、每阶段完成定义、提交边界和验证要求固定下来，便于新会话通过 `/goal` 长时间持续工作。

本计划只记录执行顺序、阶段 checklist 和提交/验证规则；长期设计边界以 [../design/post_wave7_linux_distribution_platform_design.md](../design/post_wave7_linux_distribution_platform_design.md) 为准，当前状态以 [../status/linux_distribution_platform_status.md](../status/linux_distribution_platform_status.md) 和 [../status/mainline_status.md](../status/mainline_status.md) 为准。

## 关联文档

- 来源设计：
  - [../design/post_wave7_linux_distribution_platform_design.md](../design/post_wave7_linux_distribution_platform_design.md)
- 目标状态：
  - [../status/linux_distribution_platform_status.md](../status/linux_distribution_platform_status.md)
  - [../status/mainline_status.md](../status/mainline_status.md)
- 已完成基线：
  - [history_plan.md#post-wave7-linux-distribution-platform-plan](history_plan.md#post-wave7-linux-distribution-platform-plan)

## 当前基线

- Linux 发行版运行时能力基线提交：`15daeea822654dc95702e5ad9058ef01d80417d1`
  `feat(Linux发行版): 跑通动态用户态与文件系统smoke`。
- 本计划不记录临时 `git status`、`ahead/behind` 计数或工作区脏状态；新会话启动时必须重新执行 `git status --short --branch` 和 `git log -1 --oneline`，以当时 HEAD 和状态文档为准。
- 当前正向证据包括：
  - `external Alpine ext4 + static /init -> mycpu-distro# -> cat /etc/os-release -> ID=alpine`
  - `external Alpine ext4 + dynamic /bin/sh -> ~ # -> cat /etc/os-release -> ID=alpine`
  - 同一动态 BusyBox shell 会话内的多命令 smoke
  - `filesystem_consistency` profile，覆盖 `/tmp` 目录创建、文件写入、追加、读回、长度检查、删除、目录移除和后续 shell 存活
- 当前仍不声明：完整发行版矩阵、TTY/login、完整 signal/timer/process 控制、跨 reboot 持久性、完整 F/D 算术、`fcsr`、FS dirty state、DTB `riscv,isa` / hwcap 完整收口、frontend distro route、任意用户镜像上传。
- 阶段 1 已完成第一刀正向收口：`tty_login_probe` 使用真实外部 Alpine rootfs 证明 BusyBox
  getty autologin 到等价 serial TTY prompt，并完成后续输入输出往返；当前仍不声明 init 管理的
  getty、密码 login 或多终端完整支持。
- 阶段 2 已完成第一刀正向收口：`process_control` 使用真实外部 Alpine rootfs 证明 `sleep`、
  后台子进程与 `wait` 返回码、`trap` / `kill` 和基础 shell 控制流；当前仍不声明完整
  signal delivery、作业控制、多进程压力或长期 timer 稳定性。
- 阶段 3 已完成第一刀正向收口：`filesystem_persistence` 使用外部 Alpine rootfs 临时副本
  证明同会话 ext4 `sync`、rename overwrite、目录遍历和 64 KiB 文件写读；当前仍不声明
  跨 reboot / reset 后读回。
- 阶段 4 已完成第一刀正向收口：`fs_state_guardrail` 使用真实外部 Alpine rootfs 证明
  `awk` 浮点用户态路径在 `sleep 1` timer roundtrip、后台子进程 `awk` + `wait` 返回码之后
  仍能继续正确执行；当前仍不声明任意时点 `mstatus/sstatus.FS` snapshot 必须保持 `DIRTY`，
  也不声明完整 Linux `FS state` 可见合同已经收口。

## 总目标

把当前 “外部 Alpine ext4 + 动态 `/bin/sh` + 文件系统一致性 smoke” 推进成一个 reference-first、可观察、可调试的小型 QEMU-like RISC-V Linux 平台实验室。

这条线的完整形态不是“再多过几个 syscall marker”，而是形成分层证据链：

1. curated 外部发行版资产矩阵；
2. 真实 guest 可见平台合同；
3. 真实用户态驱动的 ISA / platform gap 收口；
4. 默认回归与 opt-in 真实镜像验证分离；
5. 前端只消费已经稳定的 CLI / probe 合同。

## 执行总原则

- 不回退到继续追加 `linux_proto` fourth-stage 同类 syscall marker。
- 不把 repo 自带 `linux_proto/rootfs.ext4` 当作标准发行版证据。
- 不先做 frontend distro route；只有 CLI / probe 层稳定后再考虑前端。
- 不声明完整发行版支持，除非有真实外部 `Image/rootfs` 的 opt-in 运行证据。
- 每个实现 slice 优先 TDD：先补 host / probe 红灯，再实现，再跑真实 opt-in smoke。
- 共享 `InstructionSemantics + functional backend` 仍是语义真值来源；`pipeline` 只消费共享语义。
- 每个大阶段彻底完成后才提交一次；其他中间 slice 不自动提交。
- 阶段提交前必须核对 `git diff --cached --name-only`，只提交本阶段相关文件。
- 不自动 push。

## 阶段提交规则

这五个阶段都很大，提交边界按阶段完成态控制：

- 阶段 1 完成后允许提交一次。
- 阶段 2 完成后允许提交一次。
- 阶段 3 完成后允许提交一次。
- 阶段 4 完成后允许提交一次。
- 阶段 5 完成后允许提交一次。

除非用户另有明确指令，中间只更新工作区和状态文档，不自动 commit。若某阶段因为真实 blocker 无法完成，可以记录 blocker、验证证据和下一步，但不把“部分进展”自动提交成阶段完成。

## 通用验证基线

每个阶段至少执行：

- `git diff --check`
- `cd myCPU && make test-host-run_debug_cli_probe`
- 阶段对应的真实 opt-in runtime target
- `cd myCPU && make test-pipeline`
- `cd myCPU && make test`

如果触及 frontend 或 `/console`：

- `cd frontend && node --test`

如果触及 ISA / pipeline 语义：

- 补对应 host / unit / pipeline smoke
- 至少跑相关窄门禁和 `make test-pipeline`

## 阶段 1：TTY / login / console 语义

> **阶段状态：** 已完成第一刀正向收口；阶段提交项见 checklist。

### 目标

从当前 `init=/bin/sh` smoke 推进到更标准的 serial TTY / login 体验。优先证明 getty/login 或明确定位它所需的 TTY / termios / session / controlling terminal 缺口。

### 主要文件

- 修改：
  - `myCPU/tests/host/run_debug_cli_probe_test.py`
  - `myCPU/Makefile`
  - `docs/status/linux_distribution_platform_status.md`
  - `docs/design/post_wave7_linux_distribution_platform_design.md`（必要时）
- 可能修改：
  - `myCPU/workloads/linux_proto/profile.mk`
  - `myCPU/workloads/run_debug_cli_probe.py`
  - `myCPU/src/devices/uart16550.*`
  - `myCPU/src/platform/machine.*`
  - `frontend/*`（仅当 CLI / probe 层稳定后）

### Checklist

- [x] 调查当前 Alpine rootfs 中 `getty`、`login`、`stty`、`setsid`、`tty` 等工具可用性。
- [x] 如果 Alpine rootfs 不足，评估使用外部 Debian rootfs 或临时 Alpine rootfs 副本，不提交 rootfs 资产。
  当前 Alpine rootfs 已具备所需工具；本阶段未引入或提交 rootfs 资产。
- [x] 新增 opt-in profile，例如 `MYCPU_LINUX_DISTRO_RUNTIME_PROFILE=tty_login_probe`。
- [x] 新增 make target，例如 `test-host-run_debug_cli_probe_linux_distribution_tty_login`，保持 fail-closed。
- [x] 用 host 单测覆盖 profile 解析、make target 和缺 rootfs 的 fail-closed 行为。
- [x] 尝试通过 `bootargs`、`init` 包装脚本或 rootfs 临时副本进入 getty/login 或等价 TTY prompt。
  本阶段使用 `init=/bin/sh` 进入动态 BusyBox shell，再以
  `setsid /sbin/getty -n -l /bin/sh -L 115200 ttyS0 vt100` 验证等价 serial TTY prompt。
- [x] 若 getty/login 无法跑通，必须定位到具体 blocker：TTY 设备、termios、session、controlling terminal、signal、fork/exec 还是 shell prompt settling。
  本阶段选择正向完成路径；剩余限制是尚未声明 init 管理的 getty 或密码 login。
- [x] 回写 status：记录正向证据或 blocker，不把 blocker 写成支持声明。
- [x] 阶段完成后运行通用验证基线。
- [x] 阶段完成后提交一次。

### 完成定义

满足以下二选一：

- 正向完成：真实外部 rootfs opt-in smoke 能到达 getty/login 或等价 serial TTY prompt，并完成至少一次输入输出往返；或
- blocker 完成：已用真实外部 rootfs 稳定复现并定位 TTY/login 近端 blocker，文档记录具体缺口和下一阶段可执行任务。

## 阶段 2：signal / timer / process 控制矩阵

> **阶段状态：** 已完成第一刀正向收口；阶段提交项见 checklist。

### 目标

把 shell 从“能跑命令”推进到“能跑基本脚本控制流”。优先覆盖 `sleep`、子进程、`wait`、返回码、简单 signal / trap。

### 主要文件

- 修改：
  - `myCPU/tests/host/run_debug_cli_probe_test.py`
  - `myCPU/Makefile`
  - `docs/status/linux_distribution_platform_status.md`
- 可能修改：
  - `myCPU/src/trap.cpp`
  - `myCPU/src/arch/*`
  - `myCPU/src/exec/*`
  - `myCPU/src/platform/*`
  - `myCPU/src/devices/clint.*`
  - `myCPU/src/devices/plic.*`

### Checklist

- [x] 新增 `process_control` profile。
- [x] 覆盖 `sleep 1; echo ok` 或更短可控 timer 合同。
  本阶段使用 `sleep 1; printf 'sleep-ok'`。
- [x] 覆盖后台子进程和 `wait` 返回码。
  本阶段使用 `sh -c 'sleep 1; exit 7' & pid=$!; wait $pid` 并读回 `wait-status:7`。
- [x] 覆盖 `trap` / `kill` 的最小 shell 可见行为。
  本阶段使用 `trap 'printf trap-hit' TERM; kill -TERM $$`。
- [x] 覆盖简单脚本控制流：`&&`、`;`、退出码读回。
  本阶段覆盖 `false || ...; true && ...; false; printf "$?"`。
- [x] 对每个失败点区分 syscall、timer、signal delivery、wait、shell 内建或 prompt settling。
  本阶段走正向完成路径；若后续失败，profile 中每条命令已按能力面拆分。
- [x] 只在真实发行版 smoke 需要时补最小诊断程序；不要回到 fourth-stage marker 扩展。
  本阶段未新增 guest 诊断程序，也未回退到 fourth-stage marker。
- [x] 回写 status 的 process / timer / signal 证据矩阵。
- [x] 阶段完成后运行通用验证基线。
- [x] 阶段完成后提交一次。

### 完成定义

真实外部 rootfs 的同一 shell 会话能稳定通过 process / timer / signal 最小矩阵，并且每条命令都观察到期望输出后回到 prompt；或者已经定位不可继续的真实 blocker 并写清楚缺口。

## 阶段 3：文件系统与块设备耐久性

> **阶段状态：** 已完成第一刀正向收口；阶段提交项见 checklist。

### 目标

从 `/tmp` 短命一致性推进到 ext4 / virtio-blk 更强合同，包括 `sync/fsync`、rename、目录遍历、较大文件读写和可选 reset / reboot 后一致性。

### 主要文件

- 修改：
  - `myCPU/tests/host/run_debug_cli_probe_test.py`
  - `myCPU/Makefile`
  - `docs/status/linux_distribution_platform_status.md`
- 可能修改：
  - `myCPU/src/devices/virtio_blk.*`
  - `myCPU/src/devices/virtqueue.*`
  - `myCPU/src/mem/*`
  - `myCPU/src/platform/machine.*`

### Checklist

- [x] 新增 `filesystem_persistence` profile。
- [x] 真实验证使用外部 rootfs 临时副本，不破坏原始 rootfs。
  本阶段 target 使用 `mktemp` 创建 `/tmp/mycpu-distro-rootfs.*.ext4` 并复制外部 rootfs，
  运行结束后清理临时副本。
- [x] 覆盖 `sync` / `fsync` 可见路径。
  本阶段使用 BusyBox `sync <file> 2>/dev/null || sync` 覆盖可见同步路径。
- [x] 覆盖 rename overwrite、目录遍历、较大文件写读。
  本阶段覆盖 rename overwrite、`find ... | sort` 目录遍历和 64 KiB 文件写读。
- [x] 评估 reset / reboot 后是否能读回预期内容。
  本阶段先完成同会话 ext4 persistence；跨 reboot / reset 后读回保留为后续风险。
- [x] 若跨 reboot 失败，区分 guest sync、virtio-blk flush、host image 写入、reset 重装载语义。
  本阶段未声明跨 reboot 支持；后续若推进，需要单独区分这些路径。
- [x] 回写 status：区分同会话 `/tmp`、同会话 ext4、跨 reboot 持久性。
- [x] 阶段完成后运行通用验证基线。
- [x] 阶段完成后提交一次。

### 完成定义

真实外部 rootfs 副本能通过文件系统耐久性 smoke，至少证明同会话 ext4 读写一致性；若推进到跨 reboot，则必须给出重启后读回证据。任何未完成能力必须保留为明确风险。

## 阶段 4：curated 发行版矩阵

> **阶段状态：** 已完成第一刀正向收口；阶段提交项见 checklist。

### 目标

从 Alpine 单点扩到 curated distro matrix。先支持 Alpine + Debian/RISC-V 两条外部 rootfs 路线，不开放任意镜像支持。

### 主要文件

- 修改：
  - `myCPU/tests/host/run_debug_cli_probe_test.py`
  - `myCPU/Makefile`
  - `docs/status/linux_distribution_platform_status.md`
  - `docs/design/post_wave7_linux_distribution_platform_design.md`
  - `docs/index.md`（必要时）
- 可能新增：
  - `docs/status/linux_distribution_asset_matrix.md`（只有矩阵足够复杂时）

### Checklist

- [x] 设计外部资产路径约定：kernel `Image`、rootfs、bootargs、prompt、profile、expected。
- [x] Alpine 保持当前基线。
- [x] 新增 Debian/RISC-V rootfs opt-in contract。
- [x] 每个发行版只声明已验证的 profile：shell、filesystem、process、tty 等。
- [x] 默认测试不因缺少外部资产失败。
- [x] opt-in target 缺资产时必须 fail-closed。
- [x] 文档记录每个发行版的当前支持矩阵和缺口。
- [x] 阶段完成后运行通用验证基线。
- [x] 阶段完成后提交一次。

### 完成定义

至少 Alpine + Debian 两条 curated 外部 rootfs 路线有明确 env、bootargs、prompt、profile 和验证命令；每条路线的支持能力以真实 opt-in smoke 为准，不写成任意镜像支持。

## 阶段 5：ISA / platform 合同补齐

### 目标

把真实发行版触发到的 ISA / platform gap 系统收口。当前 FP load/store 只是越过 musl loader 的最小合同，不等于完整 F/D 支持。

### 主要文件

- 修改：
  - `myCPU/src/arch/*`
  - `myCPU/src/isa/*`
  - `myCPU/src/exec/*`
  - `myCPU/src/decode.c`
  - `myCPU/tests/host/*`
  - `docs/status/linux_distribution_platform_status.md`
  - `docs/design/post_wave7_linux_distribution_platform_design.md`
- 可能修改：
  - `myCPU/workloads/linux_proto/*`
  - DTB / ISA 字符串生成相关路径

### Checklist

- [ ] 评估完整 F/D arithmetic 的真实需求和最小测试矩阵。
- [ ] 评估 `fcsr`、异常标志、rounding mode、FS dirty state 的 guest 可见合同。
- [ ] 评估 DTB `riscv,isa`、hwcap、用户态库能力探测之间的一致性。
- [ ] 补 functional host / unit tests；必要时补 pipeline smoke。
  当前已经固化 `fmv.d.x` / `fmv.x.d` / `fmv.d` / `fadd.d` / `fsub.d` / `fmul.d` /
  `fcvt.d.w` / `fcvt.w.d` / `fcvt.l.d` / `fcvt.d.l` / `feq.d` / `fle.d` / `fneg.d`
  以及 `fcsr` alias，且已有真实 Alpine BusyBox `awk` 基础浮点矩阵 opt-in smoke
  （加法、条件比较、除法、`%d/%u/%x` 整数格式化、负数格式化、平方根）；当前又补上了
  `fdiv.d 7.0 / 0.0` 的最小 `CSR_FFLAGS.DZ`、`fsqrt.d sqrt(-1.0)` 的最小
  `CSR_FFLAGS.NV`、`fsqrt.d sqrt(2.0)` 的最小 `CSR_FFLAGS.NX` 以及
  `fmul.d 1e308*1e308` / `1e-308*1e-308` 的最小 `CSR_FFLAGS.OF` / `CSR_FFLAGS.UF`
  host / pipeline 回归，并确认真实 Alpine BusyBox `awk` opt-in smoke 已扩到
  `sqrt(-1) -> nan`、`1e308*1e308 -> inf` 和 `1e-308*1e-308 == 0` 且未回退；当前还补上了
  `hpmcounter3-31` / `mhpmcounter3-31` / `mhpmevent3-31` 的最小 host CSR 合同，以及
  `hpmcounter3-31 -> mhpmcounter3-31` alias 一致性，并用真实 Alpine procfs smoke 与
  `test-host-run_debug_cli_probe_linux_distribution_curated_alpine_proc_cpuinfo_isa_view` 固化了
  `mount -t proc proc /proc -> grep '^isa' /proc/cpuinfo -> rv64imafdc_zicntr_zicsr_zifencei_zihpm`；
  当前还新增了 `test-host-run_debug_cli_probe_linux_distribution_curated_alpine_auxv_hwcap_view`，
  用真实 Alpine rootfs 上的 `od -An -tx8 -w16 /proc/self/auxv` 固化当前
  `AT_HWCAP=0x112d` 的 guest-visible 视图；当前还新增了
  `test-host-run_debug_cli_probe_linux_distribution_curated_alpine_busybox_userland_abi_view`，
  用离线提取 rootfs 中 `/bin/busybox` + host `readelf` 固化
  `double-float ABI` 与 `Tag_RISCV_arch: rv64...f...d...c...` 的真实外部用户态 ABI 事实；
  当前又补充了一次离线静态指令盘点，确认外部 Alpine `/bin/busybox` 与
  `/lib/ld-musl-riscv64.so.1` 仍会用到 `flt.d`、`fcvt.wu.d`、`fcvt.lu.d`、
  `fcvt.d.wu`、`fcvt.d.lu`，
  以及更远的 `fmadd.d/fmsub.d/fnmsub.d/fnmadd.d` 和一批单精度 `*.s` 路径；同轮也把
  `flt.d`、`fcvt.wu.d`、`fcvt.lu.d`、`fcvt.d.wu`、`fcvt.d.lu` 的最小 shared semantics /
  pipeline 合同接了回来；本轮又继续把这条静态面里的第一组三输入 double FMA
  `fmadd.d` / `fmsub.d` / `fnmsub.d` 接回 shared semantics 与 pipeline，并补上
  `decode.rs3` / `SemanticInputs.rs3v` 的最小通路和对应 host / pipeline smoke；同轮还把
  `fsgnj.d` / `fsgnjn.d` / `fabs.d` alias 收成最小 double sign-injection 合同，并新增
  `fld -> fadd.d` 的 pipeline float load-use smoke，逼出并补上 older 未提交浮点写在 ROB
  中时的最小 consumer stall；本轮又把静态面里已经出现的第一对单精度 convert
  `fcvt.d.s` / `fcvt.s.d` 接回 shared semantics 与 pipeline smoke，并把它们纳入
  older FP pending 分类；随后又继续把 `fcvt.s.w` 接回 shared semantics，并修正
  pipeline decode/hazard 对它的入口分类，确保 `fcvt.s.w -> fcvt.d.s` 这类静态 userland
  链路会真正读取 GPR `rs1` 而不是误落到默认路径；本轮又继续把 `fcvt.w.s` 接回
  shared semantics 与 pipeline 整数写回路径，先只覆盖 signed single-to-int32 这条最小
  convert 路径；本轮又继续把 `fcvt.l.s` 接回 shared semantics 与 pipeline 整数写回路径，
  先只覆盖 signed single-to-int64 这条最小 convert 路径；本轮又继续把 `fcvt.wu.s`
  接回 shared semantics 与 pipeline 整数写回路径，先只覆盖 unsigned single-to-int32
  这条最小 convert 路径；本轮又继续把 `fcvt.lu.s` 接回 shared semantics 与 pipeline
  整数写回路径，先只覆盖 unsigned single-to-int64 这条最小 convert 路径；本轮又继续把 `fsgnj.s` 接回
  shared semantics 与 pipeline smoke，先只覆盖单精度 sign-copy 这条最小路径；随后又继续把
  `fsgnjn.s` 和 `fabs.s` alias 接回 shared semantics、FPR 源分类、older-FP pending 分类和
  host / pipeline smoke，收掉单精度 sign-injection 的最小第二刀；本轮又继续补上
  `fsgnjx.s` 和 `fneg.s` alias 的 host / pipeline 正向证据，把单精度 sign-injection 四条
  最小合同收齐；本轮又继续把外部 `ld-musl` 静态面里高频出现的 `fmv.w.x / fmv.x.w`
  接回 shared semantics、pipeline 入口分类和 host / pipeline smoke，收掉单精度 bit-move
  family 的最小第一刀；本轮又继续把同一静态面里明确出现的 `feq.s / flt.s / fle.s`
  接回 shared semantics、FPR 源分类和 host / pipeline smoke，收掉单精度 compare family
  的最小第一刀；本轮又继续把 `fcvt.s.l` 接回 shared semantics、pipeline 入口分类和
  host / pipeline smoke，并用 `1ULL << 40` 这类真实 64-bit 输入把它同 `fcvt.s.w`
  显式区分开，收掉 single int64-to-float convert 的最小第一刀；随后又把 `fcvt.s.lu`
  接回 shared semantics、pipeline 入口分类和 host / pipeline smoke，先只覆盖 unsigned
  int64-to-float 这条最小 single convert 路径；随后又把 `fcvt.s.wu` 接回
  shared semantics、pipeline 入口分类和 host / pipeline smoke，先只覆盖 unsigned
  int32-to-float 这条最小 single convert 路径，并显式守住“读 GPR rs1、写 FPR rd”的
  int-to-float 合同；本轮又继续把 `fsqrt.s`
  的正常路径接回 shared semantics、FPR 源分类和 host / pipeline smoke，先只覆盖
  `sqrt(2.25f) -> 1.5f` 这条最小 single sqrt 合同；随后又补上
  `fsqrt.s sqrt(-1.0f) -> CSR_FFLAGS.NV` 与 `fsqrt.s sqrt(2.0f) -> CSR_FFLAGS.NX`
  的 host / pipeline 正向证据，并确认它们都会保留 guest 可见 rounding-mode bits；随后又把
  `fmul.s max*max -> CSR_FFLAGS.OF` 和 `fmul.s min*min -> CSR_FFLAGS.UF` 的
  host / pipeline 正向证据补齐；随后又把
  `fadd.s` / `fsub.s` / `fmul.s` / `fdiv.s` 接回 shared semantics、FPR 源分类、older-FP pending
  分类和 host / pipeline smoke，先只覆盖普通有限值加减乘，以及 `fdiv.s`
  的正常路径、divide-by-zero `CSR_FFLAGS.DZ`、`0.0f / 0.0f -> CSR_FFLAGS.NV`、
  `1.0f / 3.0f -> CSR_FFLAGS.NX` 这组最小单精度除法异常路径；当前仍不把这扩写成
  single FMA、整批 `.s` 异常标志矩阵或更完整 NaN 角落语义已经完成；随后又把
  `fmadd.s` / `fmsub.s` / `fnmsub.s` / `fnmadd.s` 接回 shared semantics、FPR 源分类、
  older-FP pending 分类和 host / pipeline smoke，先只覆盖普通有限 single 值
  fused multiply-add/subtract 路径；随后又补上 `fmadd.s inf*0+1 -> CSR_FFLAGS.NV`
  与 `fmadd.s 1*1+2^-25 -> CSR_FFLAGS.NX` 的 host / pipeline 正向证据，并确认它们都会保留
  guest 可见 rounding-mode bits；随后又补上 `fmadd.s max*max+0 -> CSR_FFLAGS.OF`
  与 `fmadd.s min*min+0 -> CSR_FFLAGS.UF` 的 host / pipeline 正向证据；随后又把同一套最小异常标志合同补到
  `fmsub.s`：`fmsub.s inf*0-1 -> CSR_FFLAGS.NV`、`fmsub.s 1*1-2^-25 -> CSR_FFLAGS.NX`、
  `fmsub.s max*max-0 -> CSR_FFLAGS.OF`、`fmsub.s min*min-0 -> CSR_FFLAGS.UF`
  现在也都有 host / pipeline 正向证据，并同样保留 guest 可见 rounding-mode bits；随后又把同一套最小异常标志合同补到
  `fnmsub.s`：`fnmsub.s -(inf*0)+1 -> CSR_FFLAGS.NV`、`fnmsub.s -(1*1)+2^-25 -> CSR_FFLAGS.NX`、
  `fnmsub.s -(max*max)+0 -> CSR_FFLAGS.OF`、`fnmsub.s -(min*min)+0 -> CSR_FFLAGS.UF`
  现在也都有 host / pipeline 正向证据，并同样保留 guest 可见 rounding-mode bits；随后又把同一套最小异常标志合同补到
  `fnmadd.s`：`fnmadd.s -(inf*0)-(-1) -> CSR_FFLAGS.NV`、`fnmadd.s -(1*1)-(-2^-25) -> CSR_FFLAGS.NX`、
  `fnmadd.s -(max*max)-(-0) -> CSR_FFLAGS.OF`、`fnmadd.s -(min*min)-(-0) -> CSR_FFLAGS.UF`
  现在也都有 host / pipeline 正向证据，并同样保留 guest 可见 rounding-mode bits；当前仍不把这扩写成更完整 `.s` 异常标志矩阵、
  NaN 角落语义或完整 single arithmetic 已完成；随后又把
  `fclass.s` 接回 shared semantics 与 pipeline 整数写回路径，先只覆盖真实已验证的
  quiet-NaN / 正 normal 分类；本轮又继续把 `fclass.d` 接回 shared semantics 与
  pipeline 整数写回路径，先只覆盖真实已验证的 quiet-NaN / 正 normal double 分类；
  本轮又继续把 `fmax.d / fmin.d` 接回 shared semantics 与 pipeline smoke，先只覆盖
  普通有限 double 值的 max/min 选择路径；随后又把同一静态面里明确还缺的 `fnmadd.d`
  接回 shared semantics、older-FP pending 分类和 host / pipeline smoke，把 double FMA
  第一组四条三输入路径补齐；随后又补上 `fmadd.d inf*0+1 -> CSR_FFLAGS.NV`
  与 `fmadd.d 1*1+2^-54 -> CSR_FFLAGS.NX` 的 host / pipeline 正向证据，并确认它们都会保留
  guest 可见 rounding-mode bits；随后又补上 `fmadd.d max*max+0 -> CSR_FFLAGS.OF`
  与 `fmadd.d min*min+0 -> CSR_FFLAGS.UF` 的 host / pipeline 正向证据；随后又把同一套最小异常标志合同补到
  `fmsub.d`：`fmsub.d inf*0-1 -> CSR_FFLAGS.NV`、`fmsub.d 1*1-2^-54 -> CSR_FFLAGS.NX`、
  `fmsub.d max*max-0 -> CSR_FFLAGS.OF`、`fmsub.d min*min-0 -> CSR_FFLAGS.UF`
  现在也都有 host / pipeline 正向证据，并同样保留 guest 可见 rounding-mode bits；随后又把同一套最小异常标志合同补到
  `fnmsub.d`：`fnmsub.d -(inf*0)+1 -> CSR_FFLAGS.NV`、`fnmsub.d -(1*1)+2^-54 -> CSR_FFLAGS.NX`、
  `fnmsub.d -(max*max)+0 -> CSR_FFLAGS.OF`、`fnmsub.d -(min*min)+0 -> CSR_FFLAGS.UF`
  现在也都有 host / pipeline 正向证据，并同样保留 guest 可见 rounding-mode bits；随后又把同一套最小异常标志合同补到
  `fnmadd.d`：`fnmadd.d -(inf*0)-(-1) -> CSR_FFLAGS.NV`、`fnmadd.d -(1*1)-(-2^-54) -> CSR_FFLAGS.NX`、
  `fnmadd.d -(max*max)-(-0) -> CSR_FFLAGS.OF`、`fnmadd.d -(min*min)-(-0) -> CSR_FFLAGS.UF`
  现在也都有 host / pipeline 正向证据，并同样保留 guest 可见 rounding-mode bits；本轮又继续把同一静态面里的 `fmax.s / fmin.s` 接回
  shared semantics、older-FP pending 分类和 host / pipeline smoke，先只覆盖普通有限
  single 值的 min/max 选择路径；本轮又继续把 compare/minmax 的最小 NaN/invalid flag
  合同接回 shared semantics 与 pipeline：`feq.s` 在 quiet-NaN 下返回 unordered `0`
  且不置 `NV`，在 signaling-NaN 下返回 unordered `0` 且把 `CSR_FFLAGS.NV` 写回；
  `flt.s` / `fle.s` 在 NaN 下返回 unordered `0` 且把 `CSR_FFLAGS.NV` 写回；
  `fmin.s` / `fmax.s` 在单侧 NaN 下返回非 NaN 操作数、在双侧 quiet-NaN 下返回 canonical NaN，
  并在 signaling-NaN 路径上把 `CSR_FFLAGS.NV` 写回；随后又把同一套最小合同补到
  double compare/minmax：`feq.d` 在 quiet-NaN 下返回 unordered `0` 且不置 `NV`，
  在 signaling-NaN 下返回 unordered `0` 且把 `CSR_FFLAGS.NV` 写回；`flt.d` /
  `fle.d` 在 NaN 下返回 unordered `0` 且把 `CSR_FFLAGS.NV` 写回；`fmin.d` /
  `fmax.d` 在单侧 NaN 下返回非 NaN 操作数、在双侧 quiet-NaN 下返回 canonical
  double NaN，并在 signaling-NaN 路径上把 `CSR_FFLAGS.NV` 写回；随后又把
  `fcvt.w.s` / `fcvt.w.d` 的 `rm=111(dyn)` 路径接回 shared semantics 与 pipeline：
  现在会真实读取 guest `frm`，并在 `frm=RUP` 下的 `7.5f -> 8`、`frm=RDN`
  下的 `-3.5 -> -4` 这类可区分的非精确 float-to-int convert 路径上把
  `CSR_FFLAGS.NX` 写回到 guest 可见 `fcsr` alias，同时保留 rounding-mode
  bits；随后又把同一套动态 rounding / `fcsr` 合同补到 `fcvt.l.s` / `fcvt.l.d`：
  `rm=111(dyn)` 现在在 `frm=RUP` / `frm=RDN` 下也会真实读取 guest `frm`，并用
  `7.5f -> 8`、`-3.5 -> -4` 这类可区分样例把 `CSR_FFLAGS.NX` 写回到 guest 可见
  `fcsr` alias，同时保留 rounding-mode bits；随后又把同一条动态 rounding 合同补到
  `fcvt.lu.s` / `fcvt.lu.d`：`rm=111(dyn)` 现在在 `frm=RMM` 下也会按
  round-to-nearest, ties-to-max-magnitude 处理 `3.5f -> 4` 与 `3.25 -> 3`，
  并把 `CSR_FFLAGS.NX` 写回到 guest 可见 `fcsr` alias，同时保留 rounding-mode
  bits；本轮又继续把同一条 float-to-int helper 的 `RMM` 第一刀接回 `fcvt.wu.s` /
  `fcvt.wu.d`：`rm=111(dyn)` 现在在 `frm=RMM` 下会按 round-to-nearest,
  ties-to-max-magnitude 处理 `3.25f` / `3.25 -> 3` 与 `3.5f` / `3.5 -> 4`，
  并把 `CSR_FFLAGS.NX` 写回到 guest 可见 `fcsr` alias，同时保留 rounding-mode
  bits；本轮又继续把 `fcvt.wu.s` / `fcvt.wu.d` 的 RV64 结果形状合同收回到
  shared semantics 与 pipeline：当前 `WU` family 会把 32-bit unsigned 结果按 RV64
  `X` 寄存器写回规则做符号扩展，因此 `4294967295 -> 0xffffffffffffffff`；同时也补了
  `.s/.d` 路径上 `qNaN/-1.0 -> 0` 与 `+inf/qNaN -> UINT32_MAX` 的最小 invalid clipping 路径，并确认它只置
  `CSR_FFLAGS.NV`、不额外置 `NX`；随后又把同一条 invalid clipping 合同补到
  `fcvt.lu.s` / `fcvt.lu.d`：`qNaN/-1.0 -> 0`、`+inf/qNaN -> UINT64_MAX` 现在都有 host /
  pipeline 正向证据，并同样只置 `NV`、不额外置 `NX`；本轮再把 `fcvt.w.s` 的
  signed int32 invalid clipping 证据补齐：`+inf/qNaN -> INT32_MAX`、`-inf -> INT32_MIN`
  现在也都有 host / pipeline 正向证据，并同样只置 `NV`、不额外置 `NX`；随后又把
  同一条 signed invalid clipping 合同补到 `fcvt.l.s` / `fcvt.l.d`：
  `+inf/qNaN -> INT64_MAX`、`-inf -> INT64_MIN` 现在也都有 host / pipeline 正向证据，
  并同样只置 `NV`、不额外置 `NX`；本轮又继续把 `fcvt.d.l` / `fcvt.d.lu` 的
  int64-to-double 动态 rounding / `fcsr` 最小合同接回 shared semantics 与 pipeline：
  `rm=111(dyn)` 现在在 `frm=RUP` / `frm=RDN` 下也会真实读取 guest `frm`，并用
  `2^53+1` 这类不能被 binary64 精确表示的 64-bit 整数样例，固化
  `9007199254740994.0` / `9007199254740992.0` 两条可区分结果，同时把
  `CSR_FFLAGS.NX` 写回到 guest 可见 `fcsr` alias，并保留 rounding-mode bits；当前仍不把这扩写成完整 `.s/.d -> {w,wu,l,lu}`
  越界 / NaN / 饱和结果矩阵已经完成；
  当前仍不把 compare/minmax 这部分扩写成完整 IEEE754 角落语义已经完成；
  此外，当前 `AT_HWCAP=0x112d` 的根因也已明确：Linux 只是按 DT `riscv,isa` 的
  单字母集合折算出 `IMAFDC`，并没有额外广告不存在的扩展；
  当前还把最小 `mstatus/sstatus.FS` / `SD` 合同接回 host / pipeline 路径，浮点提交后会置
  `FS=DIRTY`，并已有 `atomic_semantics_smoke`、`instruction_semantics_smoke`、
  `pipeline_backend_smoke` 回归；但真实 Alpine `BusyBox awk` runtime 的 `FS state`
  采样仍会撞到 Linux trap / interrupt 路径把 `FS` 整理回 `INITIAL/CLEAN`，所以
  不应把“单次 snapshot 必须为 `DIRTY`”当作完成定义；本轮已新增
  `test-host-run_debug_cli_probe_linux_distribution_fs_state_guardrail`，用真实 rootfs 上的
  FP 用户态执行 -> timer / child-process roundtrip -> 后续 FP 用户态仍正确 这条最小
  正向证据替代不稳定 snapshot 断言；后续仍需继续评估更完整的 F/D arithmetic、异常和
  `FS state` 合同；本轮又把 `c.fld` / `c.fldsp` / `c.fsd` / `c.fsdsp` 纳入离线
  Alpine FP static-surface 支持集合，并用 `rvc_semantics_smoke` 固定 compressed FP 在
  `FS=Initial` 下复用现有 `fld/fsd` 语义；同轮修正 `ReorderBuffer` older-FP writer
  分类漂移，让 `fmadd.s` / `fmsub.s` / `fnmsub.s` / `fnmadd.s` 与 `fmin.d` / `fmax.d`
  阻塞同寄存器年轻 FP consumer，避免新增支持面绕过 pipeline stale-FPR guardrail。
- [ ] 不因 host unit 通过就声明发行版支持，必须回到真实 rootfs opt-in smoke 验证。
- [ ] 文档明确已支持、未支持、刻意不广告的 ISA 能力。
- [ ] 阶段完成后运行通用验证基线。
- [ ] 阶段完成后提交一次。

### 完成定义

发行版用户态不会因为错误 ISA 广告、缺失 F/D/fcsr/FS state 合同或 pipeline/functional 不一致而误判能力；相关 host / pipeline / real rootfs smoke 均有证据。

## 最终完成定义

五个阶段全部完成后，这条主线应达到：

- 至少 Alpine + Debian curated rootfs 有明确能力矩阵。
- 真实外部 rootfs 能通过 shell、TTY/login 或明确 TTY 边界、process/timer/signal、filesystem persistence 中已声明的 profile。
- ISA / platform 广告与实际能力一致，不诱导用户态走未实现路径。
- 默认测试和 opt-in 真实镜像验证分离清楚。
- 前端如有 distro route，只消费已稳定 CLI / probe 合同，不反向定义平台能力。

## 完成态回写要求

- 五个阶段全部 checklist 必须勾完，或未完成项必须转成新的后续计划 / blocker 状态。
- 对应 `status` 文档必须增加：
  - 完成结果摘要
  - 关键历史节点
  - 仍然有效的剩余风险
- 需要把“完成时间 + 完成内容 + 必要时的一两句过程摘要”追加到 `docs/plan/history_plan.md`。
- 归档完成后，删除本计划文件，不再长期保留完成态 checklist。
