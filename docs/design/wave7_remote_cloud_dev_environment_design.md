# Wave 7 远端云服务器开发与验证环境设计

## 文档定位

本文档记录主线 `Wave 7` 在另一台云服务器上承接完整开发/验证环境的当前有效设计边界。

它回答：

- 为什么这件事不是“把本机前端搬上公网”，而是“把本地完整开发/验证环境迁移到远端单机”。
- 远端服务器需要承接哪些能力：`myCPU` 构建、frontend/debug-cli、Linux Image/rootfs、AI profile、Spike 差分。
- 远端环境中的进程模型、资产白名单、资源边界、服务入口和运维边界应该如何收口。
- 哪些能力属于远端开发/验证环境，哪些仍不属于 Wave 7 的默认公网承诺。

本文档不记录执行 checklist。具体实施步骤写入 `docs/plan/`，当前状态以
[../status/mainline_status.md](../status/mainline_status.md) 为准。

## 关联文档

- 状态文档：
  - [../status/mainline_status.md](../status/mainline_status.md)
- 当前活跃计划：
  - [../plan/wave7_remote_cloud_dev_environment_plan.md](../plan/wave7_remote_cloud_dev_environment_plan.md)
- 相关设计：
  - [wave7_productization_and_showcase_design.md](wave7_productization_and_showcase_design.md)
  - [debug_frontend_integration.md](debug_frontend_integration.md)
  - [spike_differential_validation_design.md](spike_differential_validation_design.md)

## 背景与问题

当前本地工作区已经不只是一个“可打开首页和控制台的前端项目”。它依赖一整套本地开发/验证条件：

- `myCPU` C/C++ 构建与测试工具链
- `frontend/server/debug_server.mjs` 与 `mycpu --debug-cli`
- `linux_proto_console` 所需的 Linux `Image`、DTB、rootfs 和最小 shell smoke
- `mycpu --ai-profile-manifest` 的 AI 参数化小模型白名单模板
- `Spike` 外部差分联调及相关 host smoke

用户现在明确要求把“本地开发的所有内容”搬到另一台云服务器上，包括 Linux Image 和 Spike 差分联调。这意味着目标不是“部署一个只读产品站点”，而是让远端单机具备接近本地工作站的开发/验证能力，并在其上继续承接 `/`、`/console`、`/docs` 的 Wave 7 产品入口。

因此，这项工作必须同时解决两类问题：

1. **远端环境可用性**：toolchain、依赖、资产、目录、进程管理、可重复启动和最小 smoke。
2. **远端暴露边界**：frontend/debug-cli/Linux console/AI profile/Spike 联调在远端服务器上如何保持白名单、资源上限和可回收性。

## 目标

- 在另一台远端云服务器上复现本地开发/验证所需的核心能力，而不是只迁移静态前端页面。
- 远端环境至少承接以下能力：
  - `myCPU` 构建与 host/unit/guest 回归
  - frontend `/`、`/console`、`/docs`
  - `mycpu --debug-cli` 与 Node debug server
  - Linux serial console 所需 `Image`、DTB、rootfs 和最小命令 smoke
  - AI 参数化小模型白名单 profile
  - Spike 差分联调
- 为远端环境固定清晰的目录约定、资产白名单、systemd 进程模型、反代入口和最小运维动作。
- 在不改变当前 reference-first、白名单 workload、server-side AI graph 校验边界的前提下，让远端环境可持续承接 Wave 7 剩余工作。

## 非目标

- 不在这一轮把远端环境扩成多机集群、Kubernetes 或自动弹性调度体系。
- 不在这一轮开放任意用户 kernel/rootfs/model 上传。
- 不在这一轮把 Linux console、AI profile 或 Spike 差分变成完全公开、无限制的公网 API。
- 不在这一轮承诺标准 Debian/Alpine/RISC-V 发行版支持已经完成；远端环境首先复现当前本地已验证的资产和路径。
- 不在这一轮改变 frontend 技术栈、debug-cli 协议、Spike 差分设计边界或默认 backend。

## 约束与边界

- 远端目标机器是**另一台云服务器**，不是开发者本机；相关路径、systemd、反代、证书和数据目录都必须按远端单机约定设计。
- 后续与远端部署、运维、资产放置、service 启停、反代验证直接相关的工作，应默认在**远端服务器上的仓库 checkout** 中完成，由远端环境里的 Codex 读取同一份仓库并直接执行命令；本地工作区不再承担这些 server-specific 改动的执行现场。
- 本地工作区当前只承担三类动作：整理可提交的仓库内容、补通用文档/模板、在需要时做提交与版本 tag。任何只对当前本机有效、会扰乱本地开发环境的 server-specific 配置，不应继续堆在本地执行。
- 远端环境首先是**开发/验证环境**，其次才是可被域名访问的产品入口；因此 `/console` 的开放策略必须服从 session、资源和资产白名单边界。
- Linux `Image`、DTB、rootfs、Spike 可执行文件和可选工具链都视为远端服务器上的受控资产，不从浏览器或 HTTP API 上传。
- AI 参数化小模型继续只走 server-side whitelist，不接受浏览器提交自定义 graph package。
- Spike 差分继续保持“离线 external oracle”定位；它可以在远端服务器上安装并运行，但不应被 `/console` 或公网 API 直接暴露给匿名用户。
- 远端 frontend/debug-cli 仍以当前单机架构为基础：Node debug server 调度本机 `mycpu --debug-cli` 子进程，而不是引入新的远程调度层。

## 方案

### 结构设计

远端云服务器采用“单机完整开发/验证环境 + 受控产品入口”的结构：

```text
Remote Cloud Server
  /srv/apps/my_visual_CPU/
    repo/                 # 仓库工作副本
    runtime-assets/       # Linux Image / dtb / rootfs / white-listed assets
    toolchains/           # 可选的本地工具链或指向系统工具链
    logs/                 # frontend / debug / smoke logs
    tmp/                  # debug-cli / AI profile / smoke 临时目录

  systemd
    mycpu-frontend.service

  nginx
    /        -> frontend product site
    /console -> frontend console
    /docs    -> frontend docs
    /api/*   -> local Node debug server
    /ws      -> local WebSocket endpoint
```

推荐把远端单机分成四个职责层：

1. **Repo + Build Layer**
   - 仓库 checkout
   - `myCPU` 构建输出
   - frontend server 运行代码
   - `make test`、`make test-pipeline`、`node --test`

2. **Runtime Asset Layer**
   - Linux `Image`
   - Linux rootfs / block image
   - DTB 或相关 payload
   - AI profile 白名单模板运行所需最小输入资产
   - Spike 可执行文件与版本定位

3. **Service Layer**
   - Node debug server 作为本机受控服务运行
   - systemd 负责 restart、env、日志入口、ulimit、工作目录
   - nginx 负责域名、HTTPS、静态入口与 WebSocket 反代

4. **Verification Layer**
   - 远端 smoke 命令
   - Linux console opt-in e2e
   - AI profile smoke
   - Spike differential smoke
   - 部署后健康检查

### 远端执行工作流

远端环境的默认工作流固定为：

1. 本地仓库只负责把通用实现、文档和模板提交到 git。
2. 远端服务器通过 `git pull` 获取最新仓库内容。
3. 后续所有与远端环境强绑定的动作，都在远端 checkout 内执行：
   - 放置 Linux `Image/rootfs/DTB`
   - 安装或绑定 Spike
   - 写入 `.env` 或 systemd override
   - 启动/重启 frontend service
   - 验证 nginx、WebSocket、Linux console、AI profile、Spike differential
4. 若某个改动只对远端部署有效，但值得沉淀为通用模板或脚本，则在远端验证通过后再回写仓库。

这意味着“能通过 git 传递到远端的内容”和“必须在远端手工准备的运行资产”要严格分开：前者进仓库，后者留在远端服务器。

### 远端目录与资产约定

建议固定以下远端目录约定：

```text
/srv/apps/my_visual_CPU/repo
/srv/apps/my_visual_CPU/runtime-assets/linux/Image
/srv/apps/my_visual_CPU/runtime-assets/linux/rootfs.ext4
/srv/apps/my_visual_CPU/runtime-assets/linux/linux.dtb
/srv/apps/my_visual_CPU/runtime-assets/spike/bin/spike
/srv/apps/my_visual_CPU/logs/
/srv/apps/my_visual_CPU/tmp/
```

对应环境变量建议固定为：

- `MYCPU_LINUX_PROTO_CONSOLE_IMAGE=/srv/apps/my_visual_CPU/runtime-assets/linux/Image`
- `MYCPU_LINUX_PROTO_RUNTIME_IMAGE=/srv/apps/my_visual_CPU/runtime-assets/linux/Image`
- `SPIKE_PATH=/srv/apps/my_visual_CPU/runtime-assets/spike/bin/spike`
- `MYCPU_RUNTIME_TMPDIR=/srv/apps/my_visual_CPU/tmp`

如果远端机器使用系统安装的 Spike，也可以不单独维护 `runtime-assets/spike/bin/spike`，但部署脚本和 smoke 必须仍支持显式 `SPIKE_PATH`，避免依赖 shell 环境的偶然配置。

### 进程模型

远端服务保持单机、受控、易排障的模型：

- `nginx` 负责 TLS、域名和反代
- `mycpu-frontend.service` 负责启动 `node frontend/server/debug_server.mjs`
- Node debug server 继续本机 spawn `mycpu --debug-cli`
- Linux console route 继续在本机读受控 `Image/rootfs`
- AI profile route 继续在本机执行 `mycpu --ai-profile-manifest`

公网访问不应直接触达 `mycpu --debug-cli`。`mycpu --debug-cli` 只作为 Node debug server 的本地子进程存在。

### 资源与安全边界

远端环境虽然要承接完整开发/验证能力，但仍必须固定最小资源边界：

- 单 session 最大运行时长
- 单 session 最大 step/run 次数
- terminal buffer 上限
- 日志大小和轮转
- 临时目录生命周期
- AI profile 单次执行超时
- Linux console boot wait 超时
- 并发 session 数上限

当前推荐策略是：

- `/` 和 `/docs` 可公开
- `/console` 可公开，但只允许现有白名单 demo/workload
- Linux console 仍仅使用服务器本地白名单 `Image/rootfs`
- AI 参数化模板仍只允许 server-side whitelist
- Spike 差分仅通过远端 shell / CI / 运维脚本运行，不暴露为 HTTP 能力

### 接口 / 数据 / 契约

远端部署不应改变现有产品接口语义，但要新增远端部署契约：

1. **配置契约**
   - frontend service 必须通过环境变量读取 Linux `Image`、可选 rootfs、Spike 路径和临时目录
   - 这些路径在远端必须是稳定绝对路径

2. **启动契约**
   - systemd service 必须在 repo 根目录或明确工作目录启动
   - 服务只监听 `127.0.0.1:<port>`，由 nginx 反代

3. **资产契约**
   - Linux/Spike 等运行资产不写入仓库
   - 资产由远端部署脚本或运维文档说明其目标位置
   - 缺失资产时继续沿用当前 fail-closed 诊断语义

4. **验证契约**
   - 远端环境必须能运行最小 smoke，而不是只做到“服务能启动”
   - 至少包括 frontend health、Linux console gating、AI profile、Spike differential smoke
   - smoke 的真实执行地点是远端服务器，不要求本地继续承担部署侧验证

### 验证思路

文档与脚本落地后，至少守住：

- `git diff --check`
- `cd frontend && node --test`

如果开始落远端部署脚本、环境变量和服务配置，应补：

- `cd myCPU && make test-host-debug_cli_smoke`
- `cd myCPU && make test-host-interactive_terminal_smoke`
- `cd myCPU && make test-host-ai_accelerator_profile_smoke`
- `cd myCPU && make test-host-spike_differential_smoke`

远端环境 ready 后，建议新增一组显式 smoke：

- 远端 `frontend` 健康检查：`curl /`、`curl /docs`、`curl /api/tests`
- 远端 Linux console gating / ready 检查
- 远端 AI whitelist template run
- 远端 `SPIKE_PATH=... make test-host-spike_differential`
- 如有真实 Linux `Image/rootfs`，远端 opt-in Linux console e2e

## 风险与取舍

- 把完整开发环境搬到远端，会引入“资产管理”和“公网入口”两套问题；如果不先固定目录、白名单和进程边界，后续很容易退化成只能人工 SSH 排障的不可维护环境。
- Linux `Image/rootfs`、Spike 和工具链都可能偏大或有许可证/来源约束；因此仓库内应只保存契约、脚本和路径约定，不提交运行资产本体。
- 如果过早要求 `/console` 完全公网开放，Linux console 和 AI profile 会先暴露资源和安全问题；因此远端环境应先以开发/验证优先，公网暴露策略后置。
- 如果远端机器与本地 CPU 架构、系统库或工具链不同，JIT host smoke、Spike 路径、Node 版本和交叉编译工具链都可能出现环境偏差；部署文档必须把这些前提写清楚。

## 当前有效性说明

- 当前有效 / 历史语境：当前有效。
- 当前结果以 [../status/mainline_status.md](../status/mainline_status.md) 为准。
