# 审查记录：系统软件取证 U2

- 范围：U1 提交 `1b54928fc0f1d2c7e39d2c9c781be4e9a43babc7` 后新增 U2 报告、四份运行 JSON、权限及 Spike 诊断夹具、总报告入口和本单元 current 字段；排除 U3/U4 草稿及其他会话文件。
- 路径：常规；correctness、project-standards、testing 和 simplify 的 reuse、code-quality、efficiency 视角已完成只读分析。容量限制下分批派发：reuse 代理另轮承担 efficiency，standards 代理另轮承担 code-quality；主会话整合结果。新增真实 Spike 证据由 correctness 和 standards 针对性复核。
- 快照：最终 U2 报告 SHA-256 `02ff78643ea1e9351f927072f0c50452e71f0a535d239debb168dc8d5616b973`；权限夹具 `2787ded7cfefc2439373efd287a16912b40f6ae44c8af1fabedd2771a183a7c1`；Spike 夹具 `42902b4db5bc6b8aadd1404f5f10754a670f294405e24606f33dcb16e77f8ecf`。实际运行配置及其他内容指纹见 U2 JSON。
- 轮次：第 1 轮及针对性复核，2026-10-02；待修 0 / 已修复 1 / 遗留-转 Issue 0。

## R1-F01：场景数误写

- 严重度：P3；状态：已修复。
- 报告将 backend differential 场景数写成 27，实际构造并执行 26 个场景。
- correctness 指出后，全新独立 validator 对照源码重新计数确认 26；主会话修正表格，未改变测试或期望。

## 结论与边界

范围内没有未解决的交付问题。产品不修复是 Plan 明确边界，F-U2-001 为取证成果而非本单元待修条目。5 项真实参考 matched、5 项在 myCPU runner 内提前失败；复核实际二进制哈希及调用顺序，不将提前失败记为参考执行。四项 Sv39 budget 失败的根因未验证。

权限及 fence 证据已实际运行；部分跨页 tval 仅输出采样而非夹具断言，不能扩展成全后端证据。初次编译失败、原工具自测失败均保留；新夹具通过不能替代原工具全自测通过。主会话 `git diff --check` 通过。没有全 ISA、任意外设调度、DBT/JIT 或完整回归验收声明。
