# Post-Wave 7 标准 Linux 发行版平台计划

> **文档状态：** 执行中

## 文档定位

本文档用于记录 `Post-Wave 7 标准 Linux 发行版平台` 这条新主线如何落地、当前做到哪一步，以及完成后需要如何回写状态文档并归档。

## 关联文档

- 来源设计：
  - [../design/post_wave7_linux_distribution_platform_design.md](../design/post_wave7_linux_distribution_platform_design.md)
- 目标状态：
  - [../status/linux_distribution_platform_status.md](../status/linux_distribution_platform_status.md)
  - [../status/mainline_status.md](../status/mainline_status.md)

## 目标

- 把当前 `linux_proto` checkpoint / gated console 基线升级为 `标准 Linux 发行版平台`
  新主线的正式推进入口。
- 明确第一阶段应补哪些 guest 可见平台 gap、runtime 资产合同和最小发行版级 smoke。

## 完成定义

- 仓库内有正式 design / plan / status / index 入口。
- 当前 baseline、剩余 gap、第一阶段 runtime 资产合同和 smoke 目标已经文档化。
- 第一刀代码实施的主要文件边界和验证矩阵已经明确，可在本分支继续推进。
- `mainline_status.md` 已反映这条新主线已正式启动，但不与远端 `Wave 7` 部署工作混写。

## 任务

### 任务 1：收口标准发行版平台的 baseline 与边界

**文件：**
- 创建：
  - `docs/design/post_wave7_linux_distribution_platform_design.md`
  - `docs/status/linux_distribution_platform_status.md`
- 修改：
  - `docs/status/mainline_status.md`
  - `docs/index.md`

- [ ] **步骤 1：** 以当前 `timerfd-one-shot-readback-ok` checkpoint、`linux_proto_console`
  和 `Wave 7` 远端部署分工为基础，固定这条新主线的目标与非目标。
- [ ] **步骤 2：** 把“标准发行版平台”与“继续追加 fourth-stage marker”明确区分开。
- [ ] **步骤 3：** 在主线状态文档与索引中为这条新主线建立正式入口。

### 任务 2：盘点 guest 可见平台 gap 与第一阶段 contract

**文件：**
- 修改：
  - `docs/design/post_wave7_linux_distribution_platform_design.md`
  - `docs/status/linux_distribution_platform_status.md`
  - `myCPU/workloads/linux_proto/profile.mk`
  - `myCPU/workloads/run_debug_cli_probe.py`
  - `myCPU/Makefile`
  - `myCPU/tests/host/run_debug_cli_probe_test.py`

- [x] **步骤 1：** 列出从当前 `linux_proto` 基线走向标准 Debian / Alpine / RISC-V
  发行版镜像所需的 guest 可见平台 gap。
- [x] **步骤 2：** 明确第一阶段先守哪些 contract，例如 kernel / rootfs / DTB 资产、
  prompt、最小命令 smoke、reset / terminate / timeout 语义。
- [x] **步骤 3：** 把第一阶段暂不处理的能力面明确写成边界，例如任意用户镜像上传、
  图形桌面、网络或完整包管理生态。

当前结论：

- 第一刀不继续追加 fourth-stage syscall marker。
- 第一刀不先做新的 frontend distro route。
- 第一刀优先补 `外部发行版运行资产合同 + opt-in shell command smoke`。
- 第一刀的最小代码面优先落在 `linux_proto` workload profile、`run_debug_cli_probe.py`、
  对应 make 目标与 host unittest，而不是先改 `Machine` 执行语义。

### 任务 3：规划第一刀 runtime / harness 实施切片

**文件：**
- 修改：
  - `docs/plan/post_wave7_linux_distribution_platform_plan.md`
  - `myCPU/tests/host/run_debug_cli_probe_test.py`
  - `myCPU/workloads/linux_proto/profile.mk`
  - `myCPU/Makefile`

- [x] **步骤 1：** 明确第一刀代码实施优先级，是先做标准发行版资产合同，还是先做长期交互 shell smoke。
- [x] **步骤 2：** 给出需要补的最窄 host / frontend / runtime 门禁，避免一开始就跑完整发行版矩阵。
- [x] **步骤 3：** 保持现有 `xv6`、Linux probe、`linux_proto_console` 和 Node tests
  继续作为回归基线，不在未证明新能力前改写既有支持声明。

当前结论：

- 第一刀优先级：先补标准发行版运行资产合同，再用这组合同落一条 opt-in shell command smoke。
- 第一刀最窄门禁：
  - 默认门禁继续保持 `make test` / `make test-pipeline` / `node --test`
  - 新增 opt-in host runtime smoke，显式提供外部 `Image/rootfs`，等待指定 prompt，
    输入一条命令，观察期望输出，再次等待 prompt
- shell 合同默认值分层：
  - repo 自带 `linux_proto/rootfs.ext4` 继续使用 `help -> commands: help uptime exit`
  - 显式外部 rootfs 默认使用 `cat /etc/os-release -> ID=`
- fail-closed 约束：
  - `test-host-run_debug_cli_probe_linux_distribution_runtime` 必须显式提供
    `MYCPU_LINUX_DISTRO_RUNTIME_ROOTFS`
  - repo 自带 `linux_proto/rootfs.ext4` 只属于 mini shell guardrail，不再允许被这条发行版 runtime
    target 隐式复用
- 运行证据：
  - `2026-05-05` 已用外部 Alpine riscv64 ext4 rootfs 和外部 Linux `Image` 跑通
    `test-host-run_debug_cli_probe_linux_distribution_runtime`
  - 当前跑通版本使用 rootfs 内静态 `/init` 打印 `mycpu-distro# `，执行
    `cat /etc/os-release` 并输出 `ID=alpine`
  - 诊断已确认 kernel 能到 `Run /init`，ext4 和 `virtio-blk` 不是当前 blocker；
    动态 BusyBox / musl loader 用户态路径仍未跑通
- 延后项：
  - 新的 frontend distro route
  - 图形桌面、网络、完整包管理
  - 更宽的长期 uptime / 多命令交互矩阵

### 任务 4：验证与回写

**文件：**
- 修改：
  - `docs/status/linux_distribution_platform_status.md`
  - `docs/status/mainline_status.md`
  - `docs/plan/history_plan.md`

- [ ] **步骤 1：** 在每一轮实现后执行最窄验证，并在需要时扩到 `make test` /
  `make test-pipeline` / `node --test`。
- [x] **步骤 2：** 把新的 checkpoint、剩余风险和验证结果回写到 Linux 状态文档。
- [ ] **步骤 3：** 整条计划完成后，把结果归档到 `history_plan.md` 并删除本计划文件。

## 完成态回写要求

- 全部 checklist 必须勾完。
- 对应 `status` 文档必须增加：
  - 完成结果摘要
  - 关键历史节点
  - 仍然有效的剩余风险（如果有）
- 需要把“完成时间 + 完成内容 + 必要时的一两句过程摘要”追加到 `docs/plan/history_plan.md`。
- 归档完成后，删除原计划文件，不再长期保留完成态 checklist。
