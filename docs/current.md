# 当前状态

- 所在分支：`main`。
- 核对基点：`8bd2d5e`；技术实现基点：公开提交 `27fb764`。
- 工作范围：完成全部旧设计提炼与原生 GitHub Wiki 发布，清除主仓库旧设计并同步入口。
- 已具备能力：项目是已可运行的 RISC-V 模拟器原型，包含 guest runtime、pipeline、AI 设备和浏览器 Lab；本轮没有改变实现能力。
- 用户决定：采用 NexusKit，待办使用 GitHub Issues；只维护最小根 `AGENTS.md`，旧文档规则全部废除。
- 用户决定：项目不再定位为课程项目，Course OS 仍是运行与验证资产；架构由 Wiki 承载，无主仓库副本。
- 已发布：[GitHub Wiki](https://github.com/steven123397/my_visual_CPU/wiki)，提交 `e75397b3845d70778061177c2e28c01132f52dd9`，16 个专题页、首页和迁移记录，共 18 页。
- 已转换：28 份旧设计全部有处理去向；[迁移记录](https://github.com/steven123397/my_visual_CPU/wiki/Migration) 保留逐文件对照与公开 Git 历史入口。
- 已清理：旧 background/showcase/status/plan/design、两份根规划、docs/index 和子目录 AGENTS；网页使用的 4 张截图在 frontend/app/assets。
- 已同步：根 AGENTS 声明 Wiki 维护规则，README 与前端文档入口改为已发布页面。
- 本轮验证：`cd frontend && node --test` 为 169 通过、1 外部 Linux 场景跳过；`cd myCPU && make test-verification-layers`、`git diff --check` 通过。
- 静态检查：18 页 Wiki 内链、66 源码链接、56 Make target 解析和 28 文件映射通过；不代表这些 Make target 全部实跑。
- 审查：[docs/reviews/main-wiki-migration.md](reviews/main-wiki-migration.md)，四个专项零发现，明确记录覆盖限制。
- 工作树：三个旧分线已移除，提交均被 main 包含，现在只有主工作树；主仓库本轮提交仅保存本地，Wiki 已独立推送。
- 接手前的未提交文档已保存为 `f70b0d0`。

## 阻断与已知缺口

本轮无阻断。完整 C++ 回归、慢速 guest、外部 Linux / OSComp / Spike 与远端部署本轮未重跑；历史验证不作新运行证明。
旧规划不自动成为当前开发任务，恢复开发的产品优先级尚未确定。当前无 CI / GitHub workflow，Codespaces 暂无专用配置。

## 下一步

由用户确定下一开发需求，再按 NexusKit 建立 Issue 或 Plan。CI、workflow 与 Codespaces 后续另行讨论，本轮没有新建 Issue 或实施计划。
