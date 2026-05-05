# Post-Wave 7 标准 Linux 发行版平台设计

## 文档定位

本文档记录 `Wave 7` 阶段性收口之后，本地工作区重新打开的
`标准 Linux 发行版平台` 新主线的当前有效设计边界。

它回答：

- 为什么当前 Linux 基线已经足以支撑“发行版级平台”这条新主线启动。
- 这条线和 `Wave 7` 的 Linux console 展示、远端服务器部署之间如何分工。
- 后续需要围绕哪些 guest 可见平台合同、runtime 资产合同和验证合同推进。
- 哪些能力属于这条新主线，哪些仍不应被提前写成“已经支持标准发行版”。

本文档不记录执行 checklist。具体实施步骤写入 `docs/plan/`，当前状态以
[../status/linux_distribution_platform_status.md](../status/linux_distribution_platform_status.md)
和 [../status/mainline_status.md](../status/mainline_status.md) 为准。

## 关联文档

- 状态文档：
  - [../status/linux_distribution_platform_status.md](../status/linux_distribution_platform_status.md)
  - [../status/mainline_status.md](../status/mainline_status.md)
- 相关计划：
  - [../plan/post_wave7_linux_distribution_platform_plan.md](../plan/post_wave7_linux_distribution_platform_plan.md)
- 相关设计：
  - [xv6_linux_jit_mainline_design.md](xv6_linux_jit_mainline_design.md)
  - [future_expansion_roadmap_design.md](future_expansion_roadmap_design.md)
  - [wave7_productization_and_showcase_design.md](wave7_productization_and_showcase_design.md)
  - [wave7_remote_cloud_dev_environment_design.md](wave7_remote_cloud_dev_environment_design.md)

## 背景与问题

当前仓库的 Linux 相关能力已经不再是“能不能起第一阶段”的问题。主线 `Wave 3`
已经把真实 Linux fourth-stage checkpoint 冻结到
`timerfd-one-shot-readback-ok`，`Wave 7` 也已经把 `linux_proto_console`
接入前端 `/console`，能在显式提供真实 `Image` 时进入 `mycpu-linux# ` 提示符并完成
最小命令往返。

但这套能力仍然建立在一条刻意保守的边界上：repo 默认不携带真实 Linux `Image`，
console 路线仍是受控 demo，用户态是最小 shell smoke，不是标准 Debian / Alpine /
RISC-V 发行版环境。继续沿 `timerfd` 线追加同类 syscall 微分支，已经不再是当前最有价值
的投入。

因此，`Wave 7` 阶段性收口之后，本地工作区应把 Linux 方向重新定义为
`标准 Linux 发行版平台` 新主线：目标不再是“再多过几条 fourth-stage marker”，而是
推进到更接近 QEMU 使用体验的标准发行版镜像 bring-up、长期交互 shell 和动态链接用户态。

## 目标

- 以当前 `linux_proto` bring-up 基线为起点，推进到标准 Debian / Alpine / RISC-V
  发行版镜像级平台。
- 固定“标准发行版级支持”所需的 guest 可见平台合同、runtime 资产合同和最小 smoke。
- 明确 Linux kernel、rootfs、console、virtio、timer、TTY、信号、文件系统和长期运行稳定性
  的分层边界，避免把所有问题都揉进单条 smoke。
- 保持 `reference-first`、真实镜像 opt-in 和 fail-closed 的方法论，不用展示层或前端叙事
  代替平台能力声明。

## 非目标

- 不把这条线等同于 `Wave 7` 远端云服务器部署；部署继续在远端 checkout 中推进。
- 不在第一刀开放任意用户上传 kernel / rootfs / dtb。
- 不在第一刀承诺图形桌面、网络栈、包管理完整可用或“等同 QEMU 全功能平台”。
- 不把默认 backend 切到 `pipeline` 或 JIT，也不把 JIT / multicore / coherence 问题并入
  本线。

## 约束与边界

- 共享 `InstructionSemantics + functional backend` 仍是 Linux bring-up 的语义真值来源。
- 仓库默认仍不提交标准发行版运行资产；真实 `Image`、rootfs、可选 initramfs 和相关产物
  继续由开发者或远端环境显式提供。
- 当前 `linux_proto_console`、`run_debug_cli_probe.py` 和既有 Linux runtime smoke
  是这条线的事实基础，不再另起一套并行 harness。
- 前端 `/console` 只能消费已有 session / runtime 合同，不应反向定义 Linux 平台语义。
- 这条线需要独立 `design / plan / status`，不能继续复用 `Wave 7` 展示文档承载实时推进。

## 方案

### 结构设计

这条新主线按 4 层推进：

1. **发行版资产合同层**
   - 固定标准 kernel、rootfs、可选 initramfs、DTB 和启动参数的组织方式。
   - 明确哪些资产由仓库生成、哪些资产由外部提供、哪些路径通过环境变量显式注入。

2. **guest 可见平台合同层**
   - 围绕 Linux 实际会消费的 UART、virtio-blk、timer、PLIC/CLINT、中断、MMU、页表、
     trap 和文件系统读写路径补齐缺口。
   - 把“发行版级平台 gap”从当前 `timerfd` checkpoint 线里拆出来，避免 marker 与平台能力
     混在一起。

3. **runtime / harness 层**
   - 复用当前 `linux_proto` 加载路径、debug CLI、front-end console 和 opt-in runtime
     smoke，但把验证目标从“最小 shell marker”提升为“长期交互 shell + 常用用户态工具”。
   - 统一 prompt、boot marker、超时、reset/terminate 和最小命令 smoke 的合同。

4. **验证与展示层**
   - 继续区分默认门禁和 opt-in 真实镜像门禁。
   - 默认门禁锁住构建、装载、字符串、probe 和 fail-closed 诊断；真实发行版级断言通过
     opt-in runtime smoke 单独证明。

### 第一刀切片

当前第一刀不优先做 `/console` 里的新发行版卡片，也不继续沿 `timerfd` 线追加同类 marker。
第一刀先收口为：

1. **外部发行版运行资产合同**
   - 明确外部 kernel `Image`、外部 block rootfs、可选 bootargs，以及期望 shell prompt /
     命令回显的注入方式。
   - 继续复用仓库现有的 `linux_sbi_shim`、生成 DTB 和 `virtio-blk` 板级合同，不先引入新的
     frontend / 产品叙事入口。

2. **opt-in shell command smoke**
   - 在 host CLI / probe 层证明“能启动到指定 prompt，并执行一条动态链接用户态命令，再回到 prompt”。
   - 这条 smoke 的价值高于继续补 `timerfd` 之后的同类 syscall marker，因为它直接证明
     发行版级 shell contract，而不是只证明第四阶段最小用户态 smoke。

3. **frontend distro route 延后**
   - 在外部运行资产合同和 shell command smoke 证明之前，不单独新增 `linux_distro_console`
     一类前端路线。
   - 当前 `/console` 中的 `linux_proto_console` 继续承担受控 mini shell guardrail，
     不被提前改写成“已经支持标准发行版”。

### 当前 gap 盘点

从当前 `linux_proto` 基线走向标准 Debian / Alpine / RISC-V 发行版镜像，当前最直接的 gap
不是 CPU 核心语义，而是下面几层合同还没有正式收口：

1. **运行资产合同缺口**
   - 当前前端 manifest 只显式要求 `MYCPU_LINUX_PROTO_CONSOLE_IMAGE`，而 rootfs、DTB、
     bootargs、prompt 仍默认绑定到 repo 内的 `linux_proto` mini shell 语义。

2. **shell 合同缺口**
   - 当前 e2e 只证明 `mycpu-linux# ` 和 `help / uptime / exit`，还没有形成“标准发行版 shell
     可执行一条动态链接用户态命令并回到 prompt”的独立门禁。

3. **验证分层缺口**
   - 当前真实镜像 smoke 主要集中在前端 Linux console 和 `run_debug_cli_probe.py` 的
     fourth-stage marker 断言；还没有单独的“发行版级 shell contract”层。

4. **后续平台 gap**
   - TTY / login 语义、长期 uptime、动态链接用户态工具、文件系统一致性、signal / timer /
     交互稳定性都属于后续第二阶段问题，不适合在第一刀里和外部资产合同一起处理。

### 接口 / 数据 / 契约

- **镜像合同**
  - 第一阶段仍使用显式环境变量指向真实 `Image`、rootfs 和可选 DTB。
  - 运行资产路径必须是稳定绝对路径，不通过浏览器上传。
  - 当前第一刀建议把 kernel `Image`、block rootfs、可选 bootargs、shell prompt、
    测试命令和期望输出都收口成显式 contract，而不是继续把 rootfs / prompt 固定写死在
    `linux_proto_console` 里。
  - 当前建议的 opt-in contract 变量是：
    - `MYCPU_RUN_LINUX_DISTRO_RUNTIME=1`
    - `MYCPU_LINUX_DISTRO_RUNTIME_IMAGE=/path/to/Image`
    - `MYCPU_LINUX_DISTRO_RUNTIME_ROOTFS=/path/to/rootfs.ext4`
    - `MYCPU_LINUX_DISTRO_RUNTIME_BOOTARGS='...'`（可选）
    - `MYCPU_LINUX_DISTRO_RUNTIME_PROMPT='...'`
    - `MYCPU_LINUX_DISTRO_RUNTIME_COMMAND='...'`
    - `MYCPU_LINUX_DISTRO_RUNTIME_EXPECT='...'`
  - 默认命令合同按 rootfs 来源分层：
    - repo 自带 `linux_proto/rootfs.ext4` 仍默认 `help -> commands: help uptime exit`
    - 显式外部 rootfs 默认切到 `cat /etc/os-release -> ID=`
  - 这样可以保持现有 mini shell guardrail 不回退，同时让“外部发行版资产”默认落在更像标准
    发行版用户态的一条高信号命令上，且比只看 `uname` 更接近真实用户空间与 rootfs 身份识别。
  - `linux_distribution_runtime` 这条真实发行版 runtime guardrail 现在必须显式提供外部
    `MYCPU_LINUX_DISTRO_RUNTIME_ROOTFS`，不允许回落到 repo 自带 `linux_proto/rootfs.ext4`。
    后者只保留给 mini shell / fourth-stage guardrail，不再被视为“发行版级 runtime”资产。
  - `2026-05-05` 的第一条正向证据限定为
    `external Alpine ext4 + static /init + cat /etc/os-release -> ID=alpine`。
    该证据证明外部 block rootfs、ext4 mount、`virtio-blk` 和从 rootfs 执行 `/init`
    的路径可用，但不把动态链接发行版用户态声明为已支持。

- **session / console 合同**
  - 成功标志从“到达最小 shell prompt”开始，但需要逐步扩展到动态链接用户态命令、
    长时交互和 reset 后可重建状态。
  - `Load / Run / Pause / Reset / Terminate` 仍复用现有会话 API，不新起并行入口。
  - 第一刀先在 CLI / probe 层把“到达 prompt -> 输入命令 -> 观察输出 -> 回到 prompt”
    收口成 opt-in smoke，再决定是否把同样合同提升到前端路由。

- **guest 可见平台合同**
  - 不把单个 syscall 通过 smoke 证明等同于“发行版级支持”。
  - 需要为块设备、TTY、定时器、信号、文件系统、页故障、长期 uptime 和交互稳定性建立
    更直接的门禁。

- **验证合同**
  - 维持默认仓库无真实发行版资产时的 fail-closed 语义。
  - 真实镜像验证继续显式 opt-in，并产出稳定 prompt / 命令 / 输出证据，而不是只看内核日志。

### 验证思路

- 文档层：
  - `git diff --check`
- 默认回归：
  - `cd myCPU && make test`
  - `cd myCPU && make test-pipeline`
  - `cd frontend && node --test`
- Linux 定向门禁：
  - `cd myCPU && make test-host-run_debug_cli_probe`
  - `cd myCPU && make test-host-xv6_boot_smoke`
  - `cd myCPU && make test-host-xv6_shell_smoke`
  - `cd myCPU && python3 -m unittest tests.host.run_debug_cli_probe_test.RunDebugCliProbeTest.test_make_build_workload_linux_proto_block_mode_builds_post_init_smoke_elf`
- 真实发行版运行时断言：
  - 第一刀优先新增“显式提供外部 `Image/rootfs` 后，boot 到指定 prompt、执行一条命令、再次观察
    prompt”的 opt-in shell command smoke，不写成默认已证明。

## 风险与取舍

- 如果继续沿现有 fourth-stage smoke 横向补 marker，会延后真正的发行版平台 gap 盘点。
- 如果过早开放任意用户镜像上传，会把资产管理、安全和资源问题提前混入 bring-up。
- 如果把这条线和 `Wave 7` 远端部署混做一件事，会让本地平台合同和远端运维合同再次纠缠。
- 标准发行版级平台的能力面天然比当前 `linux_proto` 更宽，必须分阶段明确“第一条发行版级 shell 基线”
  和“更完整用户态能力”之间的边界。
- 如果在外部运行资产合同稳定前就先做 frontend distro route，会把 manifest、健康诊断、
  UI 文案和真实平台能力再次耦合起来，后续更难收口。
- 当前外部 Alpine rootfs 已经能走到 kernel `Run /init` 并在静态 `/init` 下完成
  smoke；动态 BusyBox / musl loader 路径仍是近端 blocker。后续设计讨论不应再把
  这个问题归类为“缺少 rootfs 资产”或“kernel / virtio-blk 未挂载”。

## 当前有效性说明

- 当前有效 / 历史语境：当前有效。
- 当前结果与下一步以
  [../status/linux_distribution_platform_status.md](../status/linux_distribution_platform_status.md)
  和 [../status/mainline_status.md](../status/mainline_status.md) 为准。
