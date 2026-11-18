# 审查记录：系统软件取证 U3

- 范围：U3 报告、独立 observation host 夹具及 `u3-*.json`；基线 `d164cfc`。产品路径与 U1 的 651 项源码清单保持相同。排除 U4 草稿、其他会话 current 与 PR review。
- 路径：常规；correctness、project-standards、testing、simplify 的 reuse/code-quality/efficiency 均由只读子代理分批分析。容量限制下部分代理在独立轮次承担另一角色。存留发现由全新 `u3_validator` 独立复核；主会话处理后 standards 针对性确认。
- 快照：最终报告 SHA-256 `b93f53325bdc1d60bf155d996284f949a870445d3542b14edacac692a4f75983`；探针 `743d534132d56bfc5dcd452743146043f03da888f696dd8f4ecdef58967c9f2e`。原始记录及最终 guest 镜像内容/指纹见 U3 报告链接。
- 轮次：第 1 轮与针对性复核，2026-10-02；待修 0 / 已修复 1 / 遗留-转 Issue 0。

## R1-F01：历史失败缺少旧探针源码

- 严重度：P2；状态：已修复（证据声明修正；历史材料缺口仍保留）。
- E-U3-001–004 只保留旧 source_sha256 和输出；最终源码已经不同，哈希不能恢复旧全文。standards 指出后，全新 validator 确认不能由交付材料重建。
- 原全文无法恢复，采用审查建议的备选：两份旧 JSON 标 `historical-output-only`，报告明确其局限；不再宣称旧版本完整可重建。两项重要缺陷结论只依赖最终 E-U3-005/006 的可取得源码、控制组和记录。standards 已复核该修正，不把披露缺口说成旧源码已补齐。

## 结论与验证

范围内无未解决的交付问题。F-U3-001 的独立 RAM 签名确认 pipeline setter 分歧；F-U3-002 的 PTE/PPN 变化、flush 控制与规范的当前 PTE 检查要求吻合。二者为 Plan 要求记录的既有产品缺陷，本轮不修产品，不作为需要擅自扩大范围的待修交付项。

主会话已运行最终探针；复核者检查实际输出及源码，不把 `completed` 当全部行为通过。既有命令解析/profile 工具通过，debug CLI 超时及默认 Python 未运行仍保留。未动态验证 L1D、窗口溢出、代码页 A/D 或 xv6 是否触发这些缺陷。

确定性整理检查见 [u3-validation.json](system-software-evidence-c253dda/u3-validation.json)：651 项产品源码不变，五份输入 JSON 解析、报告相对链接、最终探针指纹通过；首次验证器误读自身空输出已记录并修正。`git diff --check` 通过。没有产品修复、协议更改或远端动作。
