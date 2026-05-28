# Post-Wave 7 标准 Linux 发行版平台设计

## 文档定位

本文档记录 `Wave 7` 阶段性收口之后，本地工作区重新打开的
`标准 Linux 发行版平台` 主线的当前有效设计边界。

它回答：

- 这条线为什么不再继续沿 `linux_proto` fourth-stage marker 横向扩展。
- 当前已经具备哪些真实发行版运行证据。
- 标准发行版平台按哪些能力层分解。
- 哪些能力属于这条主线，哪些仍不能提前写成“已经支持标准发行版”。

本文档不记录执行 checklist。执行步骤写入
[../plan/post_wave7_linux_distribution_platform_longterm_plan.md](../plan/post_wave7_linux_distribution_platform_longterm_plan.md)，
当前状态以 [../status/linux_distribution_platform_status.md](../status/linux_distribution_platform_status.md)
和 [../status/mainline_status.md](../status/mainline_status.md) 为准。

## 关联文档

- 状态文档：
  - [../status/linux_distribution_platform_status.md](../status/linux_distribution_platform_status.md)
  - [../status/mainline_status.md](../status/mainline_status.md)
- 当前计划：
  - [../plan/post_wave7_linux_distribution_platform_longterm_plan.md](../plan/post_wave7_linux_distribution_platform_longterm_plan.md)
- 已完成计划：
  - [../plan/history_plan.md#post-wave7-linux-distribution-platform-plan](../plan/history_plan.md#post-wave7-linux-distribution-platform-plan)
- 相关设计：
  - [wave7_productization_and_showcase_design.md](wave7_productization_and_showcase_design.md)
  - [platform_mmio_contract.md](platform_mmio_contract.md)
  - [wave7_remote_cloud_dev_environment_design.md](wave7_remote_cloud_dev_environment_design.md)

## 背景与问题

当前 Linux 主线已经越过“能不能启动真实内核”的阶段。此前 `linux_proto` fourth-stage
checkpoint 冻结在 `timerfd-one-shot-readback-ok`；`Wave 7` 也已经把受控
`linux_proto_console` 接入 `/console`，显式提供真实 `Image` 后可以进入
`mycpu-linux# ` mini shell。

`2026-05-05` 之后，这条线又获得了更强的真实外部发行版证据：

- 外部 Alpine ext4 rootfs + 静态 `/init` 可以进入 `mycpu-distro# `，执行
  `cat /etc/os-release` 并观察到 `ID=alpine`。
- 外部 Alpine ext4 rootfs + 动态 `init=/bin/sh` 可以进入真实 BusyBox `~ # `，
  执行 `cat /etc/os-release` 并回到 prompt。
- 同一动态 BusyBox shell 会话已经通过多命令 smoke 和 `filesystem_consistency`
  profile，覆盖 `/tmp` 目录创建、文件写入、追加、读回、长度检查、删除、目录移除和后续
  shell 存活。
- 同一动态 BusyBox shell 会话已经通过 `tty_login_probe` profile，覆盖 TTY 工具盘点、
  `tty` / `stty` / `setsid` 往返，以及 BusyBox `getty -n -l /bin/sh -L 115200 ttyS0 vt100`
  到等价 serial TTY prompt 后的后续输入输出往返。
- 同一动态 BusyBox shell 会话已经通过 `process_control` profile，覆盖 `sleep 1`、
  后台子进程与 `wait` 返回码、`trap` / `kill -TERM $$` 最小 signal 处理，以及 `||` /
  `&&` / `;` 和退出码读回的基础 shell 控制流。
- 外部 Alpine rootfs 临时副本已经通过 `filesystem_persistence` profile，覆盖同会话 ext4
  目录创建、文件写入、`sync` 可见路径、rename overwrite、目录遍历、64 KiB 文件写读和清理。
- 同一动态 BusyBox shell 会话已经通过 `fs_state_guardrail` profile，覆盖
  `awk 'BEGIN{print sqrt(2)}'` 的真实用户态浮点执行、`sleep 1` timer roundtrip、
  后台子进程 `awk` + `wait` 返回码往返，以及回到同一 shell 后再次执行 `awk`
  浮点格式化 / `sqrt(2)*sqrt(2)` 读回；它证明的是 Linux trap / timer /
  child-process return 之后，后续用户态 FP 路径仍能继续正确执行，而不是任意时点
  `mstatus/sstatus.FS` snapshot 都必须保持 `DIRTY`。
- 外部 Debian 13 (`trixie`) riscv64 ext4 rootfs 已建立 curated opt-in 路线：当前通过
  外部 Linux `Image`、外部 Debian ext4、`init=/mycpu-debian-init`、serial wrapper prompt
  `mycpu-debian# ` 和 `cat /etc/os-release -> ID=debian` 形成 Debian shell 第一刀正向证据。
  这条 Debian 路线当前是 curated wrapper shell contract，不等同于“原生 Debian `/bin/sh` /
  getty / login` 已完整稳定”。
- 为越过 musl loader，当前已补 FPR raw state、标准 `flw/fld/fsw/fsd` load-store
  语义和 RVC `c.fld/c.fsd` / `c.fldsp/c.fsdsp` 解码。

因此，这条线后续的主要价值已经不在于继续补几个同类 syscall marker，而在于把当前
“能跑真实动态 shell 的点证据”推进成分层、可重复、可观察的发行版平台证据链。

## 目标

- 以当前 `linux_proto` bring-up 和外部 Alpine rootfs 证据为起点，推进到标准
  Debian / Alpine / RISC-V 发行版镜像级平台。
- 固定“标准发行版级支持”所需的 guest 可见平台合同、runtime 资产合同和 opt-in
  验证合同。
- 用真实发行版用户态驱动平台 gap，而不是用展示层文案或单点 marker 替代能力声明。
- 保持 `reference-first`、真实镜像 opt-in、fail-closed 和默认回归不依赖外部资产。

## 非目标

- 不把这条线等同于 `Wave 7` 远端云服务器部署；部署继续在远端 checkout 中推进。
- 不开放任意用户上传 kernel / rootfs / dtb。
- 不承诺图形桌面、网络栈、完整包管理或“等同 QEMU 全功能平台”。
- 不把默认 backend 切到 `pipeline` 或 JIT。
- 不把 JIT、multicore、coherence 或 AI accelerator 并入本线。
- 不把 repo 自带 `linux_proto/rootfs.ext4` 当作标准发行版证据。
- 不在 CLI / probe 层稳定前新增 frontend distro route。

## 稳定边界

- 共享 `InstructionSemantics + functional backend` 仍是 Linux bring-up 的语义真值来源。
- `pipeline`、未来 JIT 和其他执行形态只能消费共享语义，不复制 ISA 解释。
- 仓库默认仍不提交标准发行版运行资产；真实 `Image`、rootfs、可选 initramfs 和相关产物
  由开发者或远端环境显式提供。
- 真实发行版 runtime guardrail 必须显式 opt-in，并在缺少外部 rootfs 时 fail-closed。
- 前端 `/console` 只能消费已有 session / runtime 合同，不反向定义 Linux 平台语义。

## 分层架构

这条主线按 5 层组织能力与证据：

1. **发行版资产合同层**
   固定外部 kernel `Image`、block rootfs、bootargs、prompt、profile 和 expected output
   的组织方式。默认仓库不携带真实发行版资产。

2. **runtime / probe 合同层**
   复用 `linux_proto` 加载路径、debug CLI 和 `run_debug_cli_probe`，把验证目标从
   mini shell marker 提升为真实动态 shell、TTY/login、process、filesystem 和 distro
   matrix。

3. **guest 可见平台合同层**
   围绕 UART、virtio-blk、CLINT/PLIC、timer、signal、TTY、MMU、页故障和文件系统路径补齐
   Linux 真实用户态会消费的能力。

4. **ISA / platform 广告层**
   保证 F/D、`fcsr`、FS state、DTB `riscv,isa`、hwcap 等广告与真实实现一致，不诱导用户态
   进入未实现路径。

5. **展示层**
   frontend 只在 CLI / probe 合同稳定后消费能力；展示入口不是平台语义事实来源。

## 当前运行合同

当前 opt-in runtime 入口仍以环境变量显式描述：

- `MYCPU_RUN_LINUX_DISTRO_RUNTIME=1`
- `MYCPU_LINUX_DISTRO_RUNTIME_IMAGE=/path/to/Image`
- `MYCPU_LINUX_DISTRO_RUNTIME_ROOTFS=/path/to/rootfs.ext4`
- `MYCPU_LINUX_DISTRO_RUNTIME_BOOTARGS='...'`（可选）
- `MYCPU_LINUX_DISTRO_RUNTIME_PROMPT='...'`
- `MYCPU_LINUX_DISTRO_RUNTIME_COMMAND='...'`
- `MYCPU_LINUX_DISTRO_RUNTIME_EXPECT='...'`
- `MYCPU_LINUX_DISTRO_RUNTIME_COMMANDS='command=>expected\n...'`（可选）
- `MYCPU_LINUX_DISTRO_RUNTIME_PROFILE=filesystem_consistency | tty_login_probe | process_control | filesystem_persistence | fs_state_guardrail`（可选）

当前已存在的真实 opt-in 目标包括：

- `test-host-run_debug_cli_probe_linux_distribution_runtime`
- `test-host-run_debug_cli_probe_linux_distribution_filesystem`
- `test-host-run_debug_cli_probe_linux_distribution_tty_login`
- `test-host-run_debug_cli_probe_linux_distribution_process_control`
- `test-host-run_debug_cli_probe_linux_distribution_filesystem_persistence`
- `test-host-run_debug_cli_probe_linux_distribution_fs_state_guardrail`

这些 target 只能证明当前声明的 shell command / filesystem consistency / 等价 serial TTY
prompt / process-control / 同会话 ext4 persistence smoke contract，不等同于完整发行版矩阵、
init 管理的 getty、密码 login、完整 process control、跨 reboot 持久性或完整 F/D 浮点支持。

## 能力分解

标准发行版平台按 5 个长期能力面拆分，具体执行顺序和 checklist 由
[../plan/post_wave7_linux_distribution_platform_longterm_plan.md](../plan/post_wave7_linux_distribution_platform_longterm_plan.md)
维护：

1. **TTY / login / console 语义**
   从 `init=/bin/sh` smoke 扩到 serial TTY / getty / login 相关语义。当前已声明 BusyBox
   getty autologin 到等价 serial TTY prompt 的 opt-in 合同，但尚未声明 init 管理的 getty
   或密码 login 完整支持。
2. **signal / timer / process 控制**
   覆盖真实 shell 脚本控制流、`sleep`、子进程、`wait`、返回码和基础 signal 语义。当前已声明
   BusyBox shell 可见的最小 process / timer / signal profile，但尚未声明完整 signal
   delivery、作业控制、多进程压力或长期 timer 稳定性。
3. **文件系统与块设备耐久性**
   从 `/tmp` 会话内一致性扩到 ext4 / virtio-blk 的 sync、rename、目录遍历、较大文件和可选
   reset / reboot 后一致性。当前已声明临时 rootfs 副本上的同会话 ext4 persistence profile；
   仍不声明跨 reboot / reset 后读回。
4. **curated 发行版矩阵**
   以显式外部资产维护 Alpine、Debian/RISC-V 等 curated matrix。当前 Alpine 已有动态
   BusyBox `/bin/sh`、filesystem、TTY/login、process-control 和同会话 ext4 persistence
   证据；Debian 13 (`trixie`) 已有 curated wrapper shell `ID=debian` 证据，不开放任意镜像支持。
5. **ISA / platform 合同补齐**
   系统收口真实发行版触发到的 F/D、`fcsr`、FS state、DTB `riscv,isa`、hwcap 和平台设备
   广告。当前已经从 FP load-store 最小合同继续推进到 DTB `riscv,isa` 广告一致性和一串
   真实 Alpine `awk` 热路径上的 `0x53` move / convert / arithmetic / compare 子集，
   当前已进一步覆盖到 `fcvt.w.d` 参与的整数格式化路径，以及 `fle.d` / `fneg.d`
   参与的基础比较与负数格式化路径，并新增一组最小 `fcsr` 异常标志合同：`fdiv.d`
   divide-by-zero 会把 `CSR_FFLAGS.DZ` 写回到 guest 可见 alias，`fsqrt.d sqrt(-1.0)`
   会把 `CSR_FFLAGS.NV` 写回到 guest 可见 alias，`fsqrt.d sqrt(2.0)` 会把
   `CSR_FFLAGS.NX` 写回到 guest 可见 alias，`fmul.d 1e308*1e308` / `1e-308*1e-308`
   会把 `CSR_FFLAGS.OF` / `CSR_FFLAGS.UF` 写回到 guest 可见 alias；同时，针对 Linux 在 DT 路线下会向
   `/proc/cpuinfo` 无条件补出 `zicntr` / `zihpm` 的行为，当前已把
   `hpmcounter3-31` / `mhpmcounter3-31` / `mhpmevent3-31` 收成最小合法 CSR 合同，并补上
   `hpmcounter3-31 -> mhpmcounter3-31` alias 一致性，以免 guest 在 capability 视图上
   广告了 `zihpm` 却一读就陷入 illegal instruction；同时，当前也开始把 guest-visible
   `auxv` / `AT_HWCAP` 视图纳入同一条证据链，用真实 Alpine rootfs 上的
   `od -An -tx8 -w16 /proc/self/auxv` 固化当前 `AT_HWCAP=0x112d`；同时也把外部 Alpine
   rootfs 里的 `/bin/busybox` userland ABI 纳入同一条证据链，用离线提取 + host `readelf`
   固化 `Flags: 0x5, RVC, double-float ABI` 和
   `Tag_RISCV_arch: rv64...f...d...c...`。这说明当前 guest-visible capability 广告与
   外部 Alpine 用户态 ABI 事实已经可以被分别、稳定地观察，不再需要从单条 `awk` 路径反推；
   同时，Linux 当前的 `AT_HWCAP=0x112d` 也已经能追到明确根因：它是从 DT
   `riscv,isa = rv64imafdc_zicsr_zifencei` 里的单字母扩展直接折算出来的 `IMAFDC`，
   因此当前这一段工作不再是解释 “IMAC vs lp64d/imafdc” mismatch，而是保持
   DTB / boot log / `/proc/cpuinfo` / `auxv` 与真实外部 userland ABI 持续对齐；
   当前 host / pipeline 侧也已经把最小 `FS` / `SD` 合同接回 `mstatus/sstatus`，并在浮点
   提交后置 `FS=DIRTY`；但真实 Linux guest 在 trap / interrupt / context-switch
   路径里会按自身约定把 `FS` 清回 `INITIAL/CLEAN`，因此 real-rootfs `FS state`
   guardrail 还不能靠单次 snapshot 断言完成，当前也不应把这类不稳定 snapshot 保留成
   自动化 opt-in target；因此本轮只把一条“FP 用户态执行 -> trap/timer/child-process
   roundtrip -> 后续 FP 用户态仍正确”的最小 real-rootfs guardrail 固化下来，而不把
   “任意时点 snapshot 必须为 DIRTY” 错当成完成定义；
   另外，离线外部 userland 静态面已经证明 `/bin/busybox` 与 `ld-musl` 还会继续用到
   `flt.d`、`fcvt.wu.d`、`fcvt.lu.d`、`fcvt.d.wu`、`fcvt.d.lu`，以及更远的 `fmadd.d/fmsub.d/fnmsub.d` 和
   单精度 `*.s` 子集；因此阶段后续不应只靠动态 `awk` 碰撞来“猜”缺口，而要把
   外部 userland 静态面当成真实需求上界，继续做最小合同收口。当前这条原则已经推进到第一组
   三输入 double FMA：`decode/Insn` 已新增 `rs3`，`InstructionSemantics/SemanticInputs`
   已能携带第三个源操作数，`floating_ops` 已接回 `fmadd.d` / `fmsub.d` / `fnmsub.d`
   的最小 shared-semantics 合同，pipeline execute 也已按最小路径把 `rs3` 从 FPR 读入执行；
   同轮还继续补了静态面里已经出现的 `fsgnj.d` / `fsgnjn.d` / `fabs.d` alias，并且没有继续依赖
   “浮点消费者直接读当前 `core.fpr` 就够了”这个脆弱前提：pipeline frontend 现在会在 older
   未提交浮点写仍在 ROB 中时阻止年轻浮点消费者进入执行，从而避免 `fld -> fadd.d` 这类真实
   userland 路径读到 stale FPR。本轮又把静态面里已经出现的第一对单精度 convert
   `fcvt.d.s` / `fcvt.s.d` 接回 shared semantics，并把它们纳入同一条 older FP pending
   分类，保证后续 `fcvt.* -> 双精度消费者` 不会绕过这条最小 stall 合同。
   随后又补上了同一批静态 userland 路径里的 `fcvt.s.w`，并修正 pipeline decode/hazard
   对它的入口分类：这条指令必须被当成“读 GPR rs1、写 FPR rd”的 int-to-single convert，
   不能落回默认 `reads_rs1=false` 的路径，否则 `fcvt.s.w -> fcvt.d.s` 这类链路会把真实
   `a0` 输入误读成 `x0`。
   同轮又把 `fcvt.w.s` 接回 shared semantics 与 pipeline 整数写回路径，先只覆盖
   signed single-to-int32 这条最小 convert 路径，不提前扩成 `fcvt.wu.s/l.s/lu.s` 整批
   single-to-int 家族。
   本轮又继续把 `fcvt.l.s` 接回 shared semantics 与 pipeline 整数写回路径，先只覆盖
   signed single-to-int64 这条最小 convert 路径，当前仍不把 `fcvt.wu.s/lu.s` 写成已完成能力。
   本轮又继续把 `fcvt.wu.s` 接回 shared semantics 与 pipeline 整数写回路径，先只覆盖
   unsigned single-to-int32 这条最小 convert 路径，当前仍不把 `fcvt.lu.s` 或完整 unsigned
   single-to-int 家族写成已完成能力。
   本轮又继续把 `fcvt.lu.s` 接回 shared semantics 与 pipeline 整数写回路径，先只覆盖
   unsigned single-to-int64 这条最小 convert 路径；当前这组 single-to-int 合同仍不代表
   rounding / exception / 全家族语义已经收口。
   本轮又继续把同一静态面里的 `fsgnj.s` 接回 shared semantics，先只覆盖单精度 sign-copy
   这一条最小合同；随后又继续把 `fsgnjn.s` 和 `fabs.s` alias 接回 shared semantics、
   FPR 源分类、older-FP pending 分类和 host / pipeline smoke，收掉单精度 sign-injection
   的最小第二刀；本轮又继续补上 `fsgnjx.s` 和 `fneg.s` alias 的 host / pipeline 正向证据，
   把单精度 sign-injection 四条最小合同收齐；本轮又继续把外部 `ld-musl` 静态面里高频出现的
   `fmv.w.x / fmv.x.w` 接回 shared semantics、pipeline 入口分类和 host / pipeline smoke，
   收掉单精度 bit-move family 的最小第一刀；本轮又继续把同一静态面里明确出现的
   `feq.s / flt.s / fle.s` 接回 shared semantics、FPR 源分类和 host / pipeline smoke，
   收掉单精度 compare family 的最小第一刀；本轮又继续把 `fcvt.s.l` 接回 shared semantics、
   pipeline 入口分类和 host / pipeline smoke，并用 `1ULL << 40` 这类真实 64-bit 输入把
   它同 `fcvt.s.w` 显式区分开，收掉 single int64-to-float convert 的最小第一刀；随后又把
   `fcvt.s.lu` 接回 shared semantics、pipeline 入口分类和 host / pipeline smoke，先只覆盖
   unsigned int64-to-float 这条最小 single convert 路径；本轮又继续把 `fcvt.s.wu`
   接回 shared semantics、pipeline 入口分类和 host / pipeline smoke，先只覆盖
   unsigned int32-to-float 这条最小 single convert 路径，并显式要求它继续走
   “读 GPR rs1、写 FPR rd”的 int-to-float 合同，避免被错误路由成 FPR 源读取；本轮又继续把 `fsqrt.s` 的正常路径
   接回 shared semantics、FPR 源分类和 host / pipeline smoke，先只覆盖
   `sqrt(2.25f) -> 1.5f` 这条最小 single sqrt 合同；随后又补上
   `fsqrt.s sqrt(-1.0f) -> CSR_FFLAGS.NV` 与 `fsqrt.s sqrt(2.0f) -> CSR_FFLAGS.NX`
   的 host / pipeline 正向证据，并确认它们都会保留 guest 可见 rounding-mode bits；
   随后又把 `fmul.s max*max -> CSR_FFLAGS.OF` 和 `fmul.s min*min -> CSR_FFLAGS.UF`
   的 host / pipeline 正向证据补齐；当前仍不把这扩写成更大的 `.s` 异常矩阵。
   本轮又继续把单精度基础二元算术第一刀 `fadd.s` / `fsub.s` / `fmul.s` / `fdiv.s`
   接回 shared semantics、FPR 源分类、older-FP pending 分类和 host / pipeline smoke，
   先只覆盖普通有限值加减乘，以及 `fdiv.s` 的正常路径、divide-by-zero `CSR_FFLAGS.DZ`、
   `0.0f / 0.0f -> CSR_FFLAGS.NV`、`1.0f / 3.0f -> CSR_FFLAGS.NX` 这组最小单精度除法异常路径；
   当前仍不把 single FMA、整批 `.s` 异常标志或更完整 NaN 角落语义写成已完成能力。
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
   现在也都有 host / pipeline 正向证据，并同样保留 guest 可见 rounding-mode bits；当前仍不把这扩写成
   更完整 `.s` 异常标志矩阵、NaN 角落语义或完整 single arithmetic 已完成能力。
   随后又把 `fclass.s` 接回 shared semantics 和 pipeline 整数写回路径，先只覆盖真实已验证的
   quiet-NaN / 正 normal 分类，不提前扩成 `fclass.d` 或完整分类矩阵。
   本轮又继续把 `fclass.d` 接回 shared semantics 和 pipeline 整数写回路径，先只覆盖真实已验证的
   quiet-NaN / 正 normal double 分类，不提前扩成完整 `fclass` family 支持。
   本轮又继续把 `fmax.d / fmin.d` 接回 shared semantics，先只覆盖普通有限 double 值的
   max/min 选择路径，不提前扩成对应 `.s` 版本或更完整 NaN 角落语义。随后又把同一静态面里
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
   现在也都有 host / pipeline 正向证据，并同样保留 guest 可见 rounding-mode bits。又继续把单精度 `fmax.s / fmin.s` 接回 shared semantics、
   older-FP pending 分类和 host / pipeline smoke，先只覆盖普通有限 single 值的 min/max 选择路径；
   本轮又继续把 compare/minmax 的最小 NaN/invalid flag 合同接回 shared semantics 与
   pipeline：`feq.s` 在 quiet-NaN 下返回 unordered `0` 且不置 `NV`，`feq.s` 在
   signaling-NaN 下返回 unordered `0` 且把 `CSR_FFLAGS.NV` 写回到 guest 可见 alias；
   `flt.s` / `fle.s` 在 NaN 下返回 unordered `0` 且把 `CSR_FFLAGS.NV` 写回；`fmin.s`
   / `fmax.s` 在单侧 NaN 下返回非 NaN 操作数、在双侧 quiet-NaN 下返回 canonical NaN，
   并在 signaling-NaN 路径上把 `CSR_FFLAGS.NV` 写回。随后又把同一套最小合同补到
   double compare/minmax：`feq.d` 在 quiet-NaN 下返回 unordered `0` 且不置 `NV`，
   在 signaling-NaN 下返回 unordered `0` 且把 `CSR_FFLAGS.NV` 写回；`flt.d` /
   `fle.d` 在 NaN 下返回 unordered `0` 且把 `CSR_FFLAGS.NV` 写回；`fmin.d` /
   `fmax.d` 在单侧 NaN 下返回非 NaN 操作数、在双侧 quiet-NaN 下返回 canonical
   double NaN，并在 signaling-NaN 路径上把 `CSR_FFLAGS.NV` 写回。当前仍不把这扩写成
   完整 IEEE754 compare/minmax 角落语义已完成能力。
   本轮又继续把 `fcvt.w.s` / `fcvt.w.d` 的动态 rounding / `fcsr` 最小合同接回 shared
   semantics 与 pipeline：`rm=111(dyn)` 现在会真实读取 guest `frm`，并在
   `frm=RUP` 下的 `7.5f -> 8`、`frm=RDN` 下的 `-3.5 -> -4` 这类可区分的非精确
   float-to-int convert 路径上把 `CSR_FFLAGS.NX`
   写回到 guest 可见 `fcsr` alias，同时保留原有 rounding-mode bits；当前仍不把这扩写成
   完整 `.s/.d -> {w,wu,l,lu}` 越界 / NaN / 饱和结果矩阵已经全部收口。随后又把
   同一套动态 rounding / `fcsr` 合同补到 `fcvt.l.s` / `fcvt.l.d`：
   `rm=111(dyn)` 现在在 `frm=RUP` / `frm=RDN` 下也会真实读取 guest `frm`，并用
   `7.5f -> 8`、`-3.5 -> -4` 这类可区分样例把 `CSR_FFLAGS.NX` 写回到 guest 可见
   `fcsr` alias，同时保留 rounding-mode bits；随后又把同一条动态 rounding 合同补到
   `fcvt.lu.s` / `fcvt.lu.d`：`rm=111(dyn)` 现在在 `frm=RMM` 下也会按
   round-to-nearest, ties-to-max-magnitude 处理 `3.5f -> 4` 与 `3.25 -> 3`，
   并把 `CSR_FFLAGS.NX` 写回到 guest 可见 `fcsr` alias，同时保留 rounding-mode
   bits；当前仍不把这扩写成完整 float-to-int rounding family 已完成。本轮又继续把
   同一条 float-to-int helper 的 `RMM` 第一刀接回 `fcvt.wu.s` / `fcvt.wu.d`：
   `rm=111(dyn)` 现在在 `frm=RMM` 下会按 round-to-nearest, ties-to-max-magnitude
   处理 `3.25f` / `3.25 -> 3` 与 `3.5f` / `3.5 -> 4`，并把 `CSR_FFLAGS.NX`
   写回到 guest 可见 `fcsr` alias，同时保留 rounding-mode bits；当前仍不把这扩写成
   完整 float-to-int rounding family 已完成。本轮又继续把 `fcvt.wu.s` / `fcvt.wu.d`
   的 RV64 结果形状合同收回到 shared semantics 与 pipeline：当前 `WU` family 会把
   32-bit unsigned 结果按 RV64 `X` 寄存器写回规则做符号扩展，因此
   `4294967295 -> 0xffffffffffffffff`；同时也补了 `.s/.d` 路径上
   `qNaN/-1.0 -> 0` 与 `+inf/qNaN -> UINT32_MAX` 的最小 invalid clipping 路径，并确认它只置
   `CSR_FFLAGS.NV`、不额外置 `NX`。随后又把同一条 invalid clipping 合同补到
   `fcvt.lu.s` / `fcvt.lu.d`：`qNaN/-1.0 -> 0`、`+inf/qNaN -> UINT64_MAX` 现在都有
   host / pipeline 正向证据，并同样只置 `NV`、不额外置 `NX`；本轮再把
   `fcvt.w.s` / `fcvt.w.d` 的 signed int32 invalid clipping 证据补齐：`+inf/qNaN -> INT32_MAX`、
   `-inf -> INT32_MIN` 现在也都有 host / pipeline 正向证据，并同样只置 `NV`、
   不额外置 `NX`；随后又把同一条 signed invalid clipping 合同补到
   `fcvt.l.s` / `fcvt.l.d`：`+inf/qNaN -> INT64_MAX`、`-inf -> INT64_MIN` 现在也都有
   host / pipeline 正向证据，并同样只置 `NV`、不额外置 `NX`；本轮又继续把
   `fcvt.d.l` / `fcvt.d.lu` 的 int64-to-double 动态 rounding / `fcsr` 最小合同接回
   shared semantics 与 pipeline：`rm=111(dyn)` 现在在 `frm=RUP` / `frm=RDN` 下也会
   真实读取 guest `frm`，并用 `2^53+1` 这类不能被 binary64 精确表示的 64-bit 整数样例，
   固化 `9007199254740994.0` / `9007199254740992.0` 两条可区分结果，同时把
   `CSR_FFLAGS.NX` 写回到 guest 可见 `fcsr` alias，并保留 rounding-mode bits。当前仍不把这扩写成
   完整 `.s/.d -> {w,wu,l,lu}` 越界 / NaN / 饱和结果矩阵已经收口。
   当前剩余重心已经不再是 double FMA opcode 缺口，而是更系统的单精度 `*.s` 尾项、
   更完整异常标志矩阵，以及更完整的三源 forwarding / rename / hazard 建模。
   但仍未完成完整 F/D、异常标志、FS state 和用户态 capability 收口。

## 提交与阶段边界

五个阶段都较大，提交边界按阶段完成态控制：

- 每个阶段彻底完成后允许提交一次。
- 中间 slice 默认不自动提交。
- 如果阶段因为真实 blocker 无法完成，可以记录 blocker 和验证证据，但不把部分进展自动提交成阶段完成。
- 阶段提交前必须核对 staged 范围，且只提交本阶段相关文件。
- 不自动 push。

## 验证思路

- 文档层：
  - `git diff --check`
- 默认回归：
  - `cd myCPU && make test`
  - `cd myCPU && make test-pipeline`
  - `cd frontend && node --test`
- Linux 定向门禁：
  - `cd myCPU && make test-host-run_debug_cli_probe`
  - 阶段对应的真实 opt-in target
  - `cd myCPU && make test-host-xv6_boot_smoke`
  - `cd myCPU && make test-host-xv6_shell_smoke`
- ISA / pipeline 相关变更：
  - 补对应 host / unit / pipeline smoke
  - 至少跑相关窄门禁和 `make test-pipeline`

## 风险与取舍

- 如果继续沿 fourth-stage smoke 横向补 marker，会延后真正的平台 gap 盘点。
- 如果过早开放任意用户镜像上传，会把资产管理、安全和资源问题提前混入 bring-up。
- 如果先做 frontend distro route，会把 UI 文案和未稳定的平台能力耦合起来。
- 如果过早广告 F/D 或 hwcap 能力，真实用户态库可能进入未实现路径。
- 如果只看 guest `AT_HWCAP` / `/proc/cpuinfo` 而不看真实外部用户态 ABI，容易把
  capability 广告层与真实用户态 ABI 之间的错层问题误判成单纯 `awk` 指令缺口；这也是此前
  `IMAC` 广告与 `lp64d/imafdc` BusyBox 事实不一致时暴露出来的具体风险。
- 如果把真实 Linux `FS state` 语义简化成“任意时点 snapshot 必须保持 DIRTY”，会把
  内核 trap / interrupt / context-switch 自身对 `FS` 的整理语义误判成模拟器 FPU 语义缺失。
- 如果真实 rootfs 写入测试直接操作原资产，可能污染后续验证；需要使用临时副本。

## 当前有效性说明

- 当前有效 / 历史语境：当前有效。
- 当前执行计划以
  [../plan/post_wave7_linux_distribution_platform_longterm_plan.md](../plan/post_wave7_linux_distribution_platform_longterm_plan.md)
  为准。
- 当前状态以
  [../status/linux_distribution_platform_status.md](../status/linux_distribution_platform_status.md)
  和 [../status/mainline_status.md](../status/mainline_status.md) 为准。
