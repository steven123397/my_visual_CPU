# 审查记录：main Wiki 迁移

- **范围**：主仓库 `8bd2d5e` 到本轮工作区（AGENTS、README、前端文档页及测试、28 份旧设计删除）；独立 Wiki 的 18 页草稿，对应发布提交 `e75397b3845d70778061177c2e28c01132f52dd9`。
- **意图**：将全部旧设计中仍有效的契约提炼至原生 GitHub Wiki；移除失效规划、重复状态和课程规则，保持根指引最小。
- **路径**：NexusKit 常规审查。
- **覆盖**：correctness、project-standards、testing、agent-native 四个叶子审查；主会话合并核对。只有说明文字、链接和测试断言机械迁移，无可精简代码，跳过 simplify 三视角；没有存留发现，按规则无需另派 validator。
- **审查轮次**：第 1 轮，2026-09-30。
- **概要**：待修 0 / 已修复 0 / 遗留-转 Issue 0。

## 结论与证据

范围内未发现需处理问题。correctness 核对了 UART、SimpleStorage、向量访存、调度、DBT executable memory / runtime harness 及 Linux 环境变量；规范审查以根 AGENTS 与用户最新决定为准，不应用已废除规则。

主会话验证：前端 `node --test` 为 169 通过、1 外部 Linux 场景跳过；`make test-verification-layers`、diff whitespace 检查通过。18 页内链、66 个公开版本源码链接、56 个 Make target 解析和 28 份旧设计去向检查通过。首次前端检查在删除完成前启动，旧路径 404 断言失败；删除完成后重跑通过，未以首次结果作为完成证据。

审查后仅补齐 Migration 的实际检查结果、current 与本报告，没有改变技术契约。Wiki 以固定公开代码提交 `27fb764` 提供源码与旧设计历史入口。发布后 `ls-remote` 与本地 Wiki HEAD 一致，Home 原始页面与 Pipeline 网页 HTTP 读取成功，网页包含专题正文与内部导航。

## 覆盖限制

没有独立穷举全部 28 份旧设计的每项细节、ISA / syscall / AI op × dtype 或所有 pipeline hazard；主会话已逐专题对照实现并提供完整文件映射。未跑完整 C++、慢速 guest、外部 Linux / OSComp / Spike、DBT 宿主执行或远端部署。发布后的版本与 HTTP 访问由主会话核对，不宣称浏览器视觉审查。
