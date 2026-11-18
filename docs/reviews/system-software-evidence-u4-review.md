# 审查记录：系统软件取证 U4

- 范围：U4 报告、fork host/guest、独立构建脚本、`u4-*` 文本证据；基线 `e4ab34b`。排除 U5 草稿、外来 current 与 PR review。
- 路径：常规；correctness、testing、project-standards、learnings、adversarial 及 simplify 的 reuse/code-quality/efficiency 分批只读分析；容量限制下部分代理在另一轮承担独立角色。实际运行完成后针对性复查全部映射、生命周期、退出状态和哈希。
- 快照：报告 SHA-256 `1f027a8d225f2f4654c1cf5715afd636ff971a823cff8212112a112bda301f60`；host `8e4039362dde5c76165530a6ff0a0afc7853d009808dac99eeb780d5f09df8ae`；guest `88b625705023b416be4b43acf14fe1317419d4fe776b22a62c98ed7fc139efb8`；build script `9cc6662bd1c904cce7a7e4949c41e27731972c2d333fb5dedfeb4c215db0d7a2`。
- 轮次：第 1 轮与最终证据复核，2026-10-02；待修 0 / 已修复 1 / 遗留-转 Issue 0。

## R1-F01：启动 stdout 误记为空

- 严重度：P3；状态：已修复。
- 原表格称 stdout/stderr 均为空；实际 stdout 含 `xv6 kernel is booting`，stderr 为空。correctness 与 standards 命中同因，合并为一条。
- 主会话修正表格；全新 `u4_validator` 独立读取原始字节、JSON 与哈希确认事实及修正。通过结论和耗时未改变。

## 结论与边界

范围内无未解决交付问题。三项实际运行退出 0；完整 shell 的 16 条命令和独立普通 fork 的 10 条事件均核对。原始整数精确解析确认 SATP；before/after 映射、两次真实 store、ZOMBIE/status37、wait 后槽清理与两页 freelist 证据相互一致。

主会话重新核对 242 项 U4 源文件、14 项逐运行资产指纹不变；standards 另核对 518 项源/资产/工具/日志指纹。`bash -n u4-build.sh`、JSON 解析通过。暂存后默认 `git diff --cached --check` 提示原始 boot stdout 的末尾空行；保留原始字节及哈希，不将该提示记为默认检查通过。排除该原始日志的格式检查通过；长实验由本单元实际运行，审查复用其结果，没有机械重跑。

仅 functional、单 hart、固定 xv6、两张用户页；不证明 pipeline、COW、完整 OS 或全部资源回收。RAM PTE 不替代实时 TLB；独立测试适配不代表生产协议/前端获得能力。历史首次构建旧源码未留存、成功构建耗时未采、逐运行 HEAD 未采等限制已披露。没有产品修改或远端动作。
