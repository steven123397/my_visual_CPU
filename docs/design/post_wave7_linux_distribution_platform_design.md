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
- `MYCPU_LINUX_DISTRO_RUNTIME_PROFILE=filesystem_consistency | tty_login_probe`（可选）

当前已存在的真实 opt-in 目标包括：

- `test-host-run_debug_cli_probe_linux_distribution_runtime`
- `test-host-run_debug_cli_probe_linux_distribution_filesystem`
- `test-host-run_debug_cli_probe_linux_distribution_tty_login`

这些 target 只能证明当前声明的 shell command / filesystem consistency / 等价 serial TTY
prompt contract，不等同于完整发行版矩阵、init 管理的 getty、密码 login、完整 process
control、跨 reboot 持久性或完整 F/D 浮点支持。

## 能力分解

标准发行版平台按 5 个长期能力面拆分，具体执行顺序和 checklist 由
[../plan/post_wave7_linux_distribution_platform_longterm_plan.md](../plan/post_wave7_linux_distribution_platform_longterm_plan.md)
维护：

1. **TTY / login / console 语义**
   从 `init=/bin/sh` smoke 扩到 serial TTY / getty / login 相关语义。当前已声明 BusyBox
   getty autologin 到等价 serial TTY prompt 的 opt-in 合同，但尚未声明 init 管理的 getty
   或密码 login 完整支持。
2. **signal / timer / process 控制**
   覆盖真实 shell 脚本控制流、`sleep`、子进程、`wait`、返回码和基础 signal 语义。当前还只证明
   多命令 shell 与文件系统一致性 smoke。
3. **文件系统与块设备耐久性**
   从 `/tmp` 会话内一致性扩到 ext4 / virtio-blk 的 sync、rename、目录遍历、较大文件和可选
   reset / reboot 后一致性。当前不声明跨 reboot 持久性。
4. **curated 发行版矩阵**
   以显式外部资产维护 Alpine、Debian/RISC-V 等 curated matrix。当前只有外部 Alpine
   rootfs 的正向证据，不开放任意镜像支持。
5. **ISA / platform 合同补齐**
   系统收口真实发行版触发到的 F/D、`fcsr`、FS state、DTB `riscv,isa`、hwcap 和平台设备
   广告。当前 FP load-store 只是越过 musl loader 的最小合同。

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
- 如果真实 rootfs 写入测试直接操作原资产，可能污染后续验证；需要使用临时副本。

## 当前有效性说明

- 当前有效 / 历史语境：当前有效。
- 当前执行计划以
  [../plan/post_wave7_linux_distribution_platform_longterm_plan.md](../plan/post_wave7_linux_distribution_platform_longterm_plan.md)
  为准。
- 当前状态以
  [../status/linux_distribution_platform_status.md](../status/linux_distribution_platform_status.md)
  和 [../status/mainline_status.md](../status/mainline_status.md) 为准。
