# Wave 7 远端云服务器开发与验证环境计划

> **文档状态：** 执行中

## 文档定位

本文档用于记录主线 `Wave 7` 在另一台云服务器上承接完整开发/验证环境的具体落地步骤、当前做到哪一步，以及完成后需要如何回写状态文档并归档。

## 关联文档

- 来源设计：
  - [../design/wave7_remote_cloud_dev_environment_design.md](../design/wave7_remote_cloud_dev_environment_design.md)
- 目标状态：
  - [../status/mainline_status.md](../status/mainline_status.md)

## 目标

- 为另一台远端云服务器建立一套可重复的 myCPU 开发/验证环境，而不是只部署本地前端页面。
- 让远端单机承接以下能力：
  - `myCPU` 构建与测试
  - frontend `/`、`/console`、`/docs`
  - `mycpu --debug-cli`
  - Linux serial console 所需 `Image/rootfs/DTB`
  - AI 参数化小模型白名单 profile
  - Spike 差分联调
- 明确后续 server-specific 变更默认在远端服务器上的仓库 checkout 中执行，不继续扰乱本地开发环境。

## 完成定义

- 仓库内有正式设计、计划、状态和索引入口。
- 仓库内有远端部署所需的目录约定、配置样例、启动方式和运维说明。
- 仓库内有远端单机的服务脚本或配置支架，至少覆盖 frontend service 与反代入口。
- 仓库内有远端 smoke/验证脚本或文档化命令，覆盖 frontend、Linux console、AI profile 和 Spike 差分。
- `mainline_status.md` 已记录该子任务的 active 目标、边界和剩余风险。

## 任务

### 任务 1：整理远端单机环境契约

**文件：**
- 创建：
  - `docs/design/wave7_remote_cloud_dev_environment_design.md`
- 修改：
  - `docs/status/mainline_status.md`
  - `docs/index.md`

- [ ] **步骤 1：** 固定远端机器的角色、目录约定、运行资产位置和环境变量契约。
- [ ] **步骤 2：** 记录 `/`、`/docs`、`/console`、Linux `Image/rootfs`、AI profile、Spike 的边界。
- [ ] **步骤 3：** 在主线状态文档中把该任务列为 Wave 7 当前下一步之一。

### 任务 2：补远端部署支架

**文件：**
- 创建：
  - `deploy/README.md`
  - `deploy/env/mycpu-frontend.env.example`
  - `deploy/systemd/mycpu-frontend.service`
  - `deploy/nginx/mycpu.conf`
- 修改：
  - `README.md`

- [ ] **步骤 1：** 新增 `deploy/` 子树，固定远端单机部署目录和文件组织。
- [ ] **步骤 2：** 提供 frontend service 的 systemd 样例和 env 样例。
- [ ] **步骤 3：** 提供 nginx 反代样例，覆盖 `/`、`/console`、`/docs`、`/api/*` 和 WebSocket。
- [ ] **步骤 4：** 明确这些文件是供远端 checkout 使用的模板，不要求在本地机器实际启用。

### 任务 3：补远端运行资产与验证说明

**文件：**
- 创建：
  - `deploy/scripts/remote_smoke.sh`
  - `deploy/scripts/prepare_runtime_dirs.sh`
- 修改：
  - `deploy/README.md`
  - `README.md`

- [ ] **步骤 1：** 固定 Linux `Image/rootfs/DTB`、Spike、日志和临时目录的远端准备方式。
- [ ] **步骤 2：** 提供最小远端 smoke：frontend、Linux gating、AI profile、Spike differential。
- [ ] **步骤 3：** 明确哪些资产不进仓库、需要由运维或开发者手动放置。
- [ ] **步骤 4：** 明确 smoke 的执行地点是远端服务器，不要求在本地继续承接部署侧验证。

### 任务 4：验证与回写

**文件：**
- 修改：
  - `docs/status/mainline_status.md`
  - `docs/plan/history_plan.md`
  - `docs/index.md`

- [ ] **步骤 1：** 运行本轮触达范围内的前端和文档相关验证。
- [ ] **步骤 2：** 在 `mainline_status.md` 中更新当前状态、剩余风险和下一步。
- [ ] **步骤 3：** 完成后把结果归档到 `history_plan.md` 并删除本计划文件。

## 完成态回写要求

- 全部 checklist 必须勾完。
- 对应 `status` 文档必须增加：
  - 完成结果摘要
  - 关键历史节点
  - 仍然有效的剩余风险（如果有）
- 需要把“完成时间 + 完成内容 + 必要时的一两句过程摘要”追加到 `docs/plan/history_plan.md`。
- 归档完成后，删除原计划文件，不再长期保留完成态 checklist。
