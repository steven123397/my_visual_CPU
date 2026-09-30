# 当前状态

- 所在分支：`main`。
- 核对基点：`3cdc8b9`；技术实现基点：公开提交 `27fb764`。
- 工作范围：规划 GitHub Actions 日常 CI 与手动完整回归，减轻本机验证负担。
- 已具备能力：项目是已可运行的 RISC-V 模拟器原型，包含 guest runtime、pipeline、AI 设备和浏览器 Lab；本轮没有改变实现能力。
- 用户决定：采用 NexusKit，待办使用 GitHub Issues；只维护最小根 `AGENTS.md`，旧文档规则全部废除。
- 用户决定：项目不再定位为课程项目，Course OS 仍是运行与验证资产；架构由 Wiki 承载，无主仓库副本。
- 用户决定：接受接入 CI 的方向，先研究测试入口与实施方案。
- 已规划：[GitHub Actions Plan](plans/2026-09-30-2214-chore-github-actions-ci-plan.md)，U1 测试入口、U2 两份 workflow、U3 云端运行验收与指引同步。
- 已核实：GitHub Actions 已启用且默认 token 只读；xv6 已随仓库保存；前端默认 e2e 依赖真实 simulator 与 3 个 guest 映像。
- 方案：日常验证全部默认 unit、核心 host 和前端；完整回归手动触发。首轮构建并行、测试串行，缓存与定时任务后续评估。
- 已发布：[GitHub Wiki](https://github.com/steven123397/my_visual_CPU/wiki)，提交 `e75397b3845d70778061177c2e28c01132f52dd9`，16 个专题页、首页和迁移记录，共 18 页。
- 已转换：28 份旧设计全部有处理去向；[迁移记录](https://github.com/steven123397/my_visual_CPU/wiki/Migration) 保留逐文件对照与公开 Git 历史入口。
- 已清理：旧 background/showcase/status/plan/design、两份根规划、docs/index 和子目录 AGENTS；网页使用的 4 张截图在 frontend/app/assets。
- 已同步：根 AGENTS 声明 Wiki 维护规则，README 与前端文档入口改为已发布页面。
- 本轮规划证据：静态源码、Make 依赖、仓库 Actions 权限与官方文档调研；Plan 连贯性、可行性、范围、安全和失败传播自检通过，未运行构建或测试。
- 上轮迁移验证：前端 169 通过、1 外部 Linux 场景跳过；验证分层、链接及 diff 检查通过，范围见 [迁移审查](reviews/main-wiki-migration.md)。
- 工作树：三个旧分线已移除，提交均被 main 包含，现在只有主工作树；主仓库本轮提交仅保存本地，Wiki 已独立推送。
- 接手前的未提交文档已保存为 `f70b0d0`。

## 阻断与已知缺口

当前 Plan 与本地实施无阻断；仍无 workflow 配置，不能声称云端验证已工作。
U3 首次云端验收依赖主仓库发布授权和默认分支上的 workflow。干净 runner 兼容与冷构建耗时尚待实跑；不以本机旧产物或历史测试替代。
真实 Linux / OSComp / Spike、远端部署、定时运行与 Codespaces 未纳入本轮实施范围。

## 下一步

首个可实施单元为 Plan U1：建立全部默认 unit 与核心 CI 的 Make 入口。当前仅完成规划并本地保存，尚未实施或发布主仓库。
