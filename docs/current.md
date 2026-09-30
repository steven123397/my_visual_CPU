# 当前状态

- 所在分支：`main`。
- 核对基点：`8e00982`。
- 工作范围：声明 Wiki 维护规则，移除重复导航与子目录开发指引。
- 已具备能力：项目是已可运行的 RISC-V 模拟器原型，包含 Course OS、pipeline、AI 设备和浏览器 Lab。
- 用户决定：待办使用 GitHub Issues；本轮先建立入口，不全量迁移旧文档。
- 用户决定：旧文档维护规则全部废除，根和子树 `AGENTS.md` 从最小内容重建。
- 用户决定：项目不再定位为课程项目；Course OS 代码仍作为 guest runtime 和验证资产。
- 已清理：`docs/background/`、`docs/showcase/`、`docs/status/`、`docs/plan/` 和根目录两份旧规划。
- 已迁移：网页实际使用的 4 张截图进入 `frontend/app/assets/`，页面入口改用当前状态和 GitHub Issues。
- 已保留：28 份旧设计资料，统一标记为待核实；初步分析保存在 `8e00982:docs/index.md`。
- 用户决定：架构与技术契约迁入 GitHub Wiki，移除 `docs/index.md` 和子目录 `AGENTS.md`。
- 已核实：仓库没有已跟踪的 CI / GitHub workflow 配置；Wiki 未启用，当前账号有管理员权限。
- Wiki 尚未创建页面或发布；Codespaces 暂无专用配置，是否采用尚未决定。
- 已保存：接手前的三份文档已提交为 `f70b0d0`。
- 工作树：三个旧分线已移除，提交均已被 `main` 包含；现在只有主工作树。
- 本轮验证：`cd frontend && node --test --test-name-pattern='GET /docs' tests/debug_server.test.mjs` 通过；`git diff --check` 通过。
- 上轮验证：`cd frontend && node --test` 通过，169 项通过、1 项外部 Linux 场景跳过；包含截图服务与旧路径失效检查。
- 历史验证：本会话曾在 `27fb764` 上运行 `cd myCPU && make test-fast-smoke` 通过，非本轮重新验证。
- 链接策略：被删除资料的设计引用指向 `32596a6` 历史快照；该提交尚未推送，GitHub 链接需推送后才能访问。

## 阻断与已知缺口

本轮无阻断。完整 C++ 回归和外部 Linux / 发行版 runtime 本轮未重跑。
旧设计正文存在重复和失效状态表述，尚未逐专题对照实现；历史远端部署状态也未刷新。
旧规划不自动成为当前开发任务，恢复开发的产品优先级尚未确定。

## 下一步

启用 Wiki 并建立首个页面后，选定第一个设计迁移专题，建议先核实 MMIO 或 pipeline 契约。
逐专题对照实现，提炼仍有效的边界，删除失效内容；需执行规划时再通过 `nk-plan` 建立新 Plan。
本轮没有 Plan、Issue 认领或外部写入。
