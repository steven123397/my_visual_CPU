# 审查记录：系统软件取证 U1

- 范围：`c253ddad1862a8e222c74d633f9269390e418624` 到 U1 文档及生成文本证据；current 仅纳入本单元状态、环境、下一步与残留说明。
- 意图：固定取证现场、构建来源和问题覆盖，提供 U2 前置条件；不修改产品语义，不把构建当作产品测试通过。
- 路径：常规。主会话直接派发 correctness、project-standards、learnings 三个独立只读子代理；各自复核同一快照，均返回零发现。
- 覆盖：事实与 JSON 对应、工具/入口/预算、源码与资产指纹、AGENTS/Plan 约束、xv6 工具链经验。纯文档及生成证据无可精简产品代码，跳过精简角色；没有存留发现，按规则无需 validator。
- 轮次：第 1 轮，2026-10-02。
- 概要：待修 0 / 已修复 0 / 遗留-转 Issue 0。范围内未发现需处理问题。

## 快照与验证

审查主体报告 SHA-256 为 `5af609eff4546d704e4874e3ad62a8bd212235e74fa1d509a788bf63de1c99dd`；inventory 为 `e5cd3539b5883a884d2471836cdfc1616eacd20c13a109c9473f4d068a542a9a`；build 为 `98fc84b13bc9b5270511c77a11724bc6e7b947d73091117dbe3514fb05466a14`。current 工作树快照为 `66ac195f57dd3c1a057f241a0646da80a5d214f4f8d5c7362227ad2065ed8336`，其中其他会话 diff 明确排除。

correctness 代理独立重算源码总指纹、已跟踪脏补丁、host 构建日志哈希，并核对当前源码和重建资产无变化；同时走查测试入口与预算。standards 代理对照根 AGENTS 和 Plan；learnings 代理核对 [xv6 工具链经验](../solutions/build-errors/xv6-ci-toolchain-baselines-and-budget.md)。输出均符合 JSON 契约，没有丢弃项或候选冲突。

主会话确定性检查记录于 [u1-validation.json](system-software-evidence-c253dda/u1-validation.json)，含完整 argv/cwd 与结果：JSON 解析、10 个相对链接、651 个源码哈希、15 个重建资产哈希、源码总指纹和脏补丁哈希全部通过；`git diff --check` 退出 0。提交整理仅为报告追加验证/审查入口、current 增加残留说明；这些新增链接及提交范围由主会话再核对，不外推为新一轮独立审查。

## 覆盖限制

host 强制构建缺少精确墙钟耗时和显式 timeout，证据如实标明；不能用于耗时比较。历史 CI 未重新读取远端，不作为当前行为通过证据。真实 Spike 默认入口不可用；产品测试和 guest 未执行，Q1–Q6、普通 fork、观察副作用仍待后续单元。

外来 `docs/reviews/2026-10-01-pr14-17-pre-merge.md`、`docs/reviews/2026-10-01-remote-pr-pre-merge.md` 和 current 审查补记不纳入审查或提交。没有 PR/远端契约变更，未调用其他条件角色；本报告不声称独立全面产品审查。
