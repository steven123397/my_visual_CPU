---
name: mycpu-quality-review
description: Use when reviewing or improving code quality in my_visual_CPU, including audits, simplification, deduplication, large-file boundary assessment, diagnostic-noise cleanup, review finding remediation, or behavior-preserving convergence refactors.
---

# myCPU Quality Review

本 skill 是 `my_visual_CPU` 的项目专用代码质量审查 / 收敛重构护栏。

它不是“看到乱就重写”的许可，也不是对全局 superpowers 的重复包装。

## 与现有体系的关系

- 先遵守根 `AGENTS.md`、最近子树 `AGENTS.md` 和 `docs/status/mainline_status.md`。
- 如果本轮涉及代码、正式文档、review finding、Git / worktree / commit 或治理文件，先使用 `project-governance-workflow`；本 skill 只负责审查 / 收敛 rubric，不接管仓库治理。
- 如果 `project-governance-workflow` 没有出现在当前可用 skills 列表，或其路径不可读，必须在报告中标出 skill 发现异常，并退回到根 `AGENTS.md`、最近子树 `AGENTS.md` 和 `docs/status/mainline_status.md` 的显式规则执行；不能假装已经应用治理 skill。
- 如果 `systematic-debugging`、`verification-before-completion`、`requesting-code-review` 或 `receiving-code-review` 同时触发，保留它们的调查、验证和反馈处理纪律，但不要机械升级成 worktree、全量矩阵或子代理，除非用户明确要求。

## 何时使用

以下场景适用：

- 全仓库或子树代码审查
- “这段代码太乱 / 太大 / 太重复，帮我收敛”
- review finding 整改
- 大文件拆分前的边界评估与落地
- 清理 ad-hoc diagnostics、临时 guardrail 或验证噪音
- 对现有实现做行为保持的结构瘦身

以下场景不适用：

- 单点 bugfix，且没有明显结构问题
- 纯 cosmetic rewrite
- 猜想式架构重写
- 新功能从零实现

## 模式

根据用户请求选择一个模式：

- `review-only`
  - 只读审查，不改文件
- `safe-fixes`
  - 只做高置信、行为保持的局部收敛
- `fix-and-validate`
  - 在 `safe-fixes` 基础上跑最小相关验证
- `strict-convergence`
  - 用户明确要求“收敛重构 / 系统整理 / 代码提纯”时使用

默认映射：

- 用户说“review / audit / 看一下 / 查一下” -> `review-only`
- 用户说“clean up / simplify / improve quality / 收敛重构” -> `fix-and-validate`

## 必读上下文

开始前至少读取：

1. 根 `AGENTS.md`
2. 目标子树 `AGENTS.md`；如果目标子树没有局部 `AGENTS.md`，记录“无局部规则”并继续
3. `docs/status/mainline_status.md`

按任务再补充：

- 代码审查 / review finding 整改：`docs/status/code_reself_status.md`
- 文档治理或状态口径：`docs/AGENTS.md`
- guest runtime：`myCPU/guest/AGENTS.md`

不要一次把整套 `design / plan / status` 灌进上下文，只读取直接相关文件。

## 不可破坏的边界

除非用户明确要求改 contract，否则必须守住：

- 共享 `InstructionSemantics + functional backend` 仍是 ISA 真值来源。
- `pipeline`、未来 `JIT` 和其他执行形态只能消费共享语义，不能复制 ISA 解释。
- `debug/frontend` 只能消费只读快照，不能反向变成执行语义来源。
- `design / plan / status` 分工保持严格分离。
- 不新增并行事实来源，不把同一状态抄写到多个正式文档。
- 不能为了“测试变绿”而弱化断言、降级 contract、放宽 fail-closed 行为或回避真实 regression。
- 不把一次性 workload / smoke 需求固化成长期特判。

## 审查优先级

按下面顺序看问题：

1. contract safety
   - shared semantics 被旁路或复制
   - backend 私有偷修
   - 测试 / harness 改动掩盖真实回归

2. structural convergence
   - 单文件承担多个无关职责
   - 参数层层传递、状态重复保存
   - 包装层、helper、adapter 比被包装逻辑更难理解

3. duplication and drift
   - copy-paste 逻辑只差细枝末节
   - AGENTS / status / README /脚本里重复维护同一事实
   - 多处手写同一验证选择逻辑

4. validation signal quality
   - 改动很窄却默认跑超宽矩阵
   - ad-hoc diagnostics 永久混入正常路径
   - 收尾报告引用旧验证而不是本轮 fresh evidence

5. clarity and waste
   - 死代码
   - 命名掩盖真实 contract
   - 深层嵌套和混合抽象层

6. efficiency
   - 只有在能明确说明性能浪费或路径冗余时才处理
   - 不做“也许更快”的猜测式优化

## 首选修复动作

优先做：

- 删除死代码
- 让重复逻辑回到现有公共 helper 或共享语义层
- 只在确实减少复杂度时提取一个聚焦 helper
- 按职责拆大文件，而不是按行数硬切
- 把窄而波动的验证选择收口到局部规则或引用文件，而不是继续堆在根规则中
- 把一次性诊断从长期正常路径里移走

避免做：

- 大规模 rename campaign
- 为了“看起来更高级”引入新抽象层
- 没有证据的性能重写
- 在 cleanup 名义下偷偷改行为

## 大文件拆分启发式

把文件视为“值得拆”仅当以下条件同时满足多个：

- 混合了两类以上主要职责
- 一次改动经常波及互不相关的片段
- helper 搜索困难，文件像杂物堆
- 验证范围难以判断，因为 contract 横跨多个主题

拆分时要求：

1. 尽量保持原有公开入口稳定
2. 按职责拆，不按“每 500 行一刀”拆
3. 不把 shared semantics / contract 核心边界切碎
4. 不扩大验证口径，除非拆分真的改变了边界

## findings 表达格式

默认用中文，并使用以下分级：

- `[必须修复]`
  - 正确性、contract、安全性、fail-closed、状态口径污染
- `[建议修改]`
  - 冗余、职责堆叠、大文件、验证噪音、明显低效
- `[仅记录]`
  - 长期观察项、值得后续单独立 plan 的问题
- `[问题]`
  - 需要作者或用户澄清意图

每条 finding 至少写清：

1. 影响范围
2. 为什么是问题
3. 建议动作
4. 建议验证

## 测试失败时的停机门

如果验证失败，不要立刻继续改实现。先把失败分类为且仅为以下之一：

- `backend_regression`
- `stale_tests`
- `unclear_contract`

处理规则：

- `backend_regression`
  - 修代码，回到 intended contract
- `stale_tests`
  - 只更新受影响测试，并明确说明旧期望为何过时
- `unclear_contract`
  - 先对照 `AGENTS.md`、相关 `design / status` 或用户意图澄清，再继续

绝不要因为着急收尾就放宽断言或降低测试门槛。

## 验证策略

使用 `references/validation-map.md` 选最小相关验证。

规则：

- `review-only` 默认不跑测试，只报告建议验证命令
- `safe-fixes` 和 `fix-and-validate` 运行最窄能证明改动正确的 gate
- 如果触及 `myCPU/AGENTS.md` 中定义的核心路径，至少守住对应基线
- 如果改动横跨执行路径、debug、workload 或 frontend/backend 边界，再逐层扩门
- 不拿旧日志、旧通过结果或“理论上应该通过”当完成证据

## 与 superpowers 的配合方式

对于本仓库，推荐的叠加关系是：

- `using-superpowers`
  - 负责纪律，不决定 repo-specific 质量标准
- `project-governance-workflow`
  - 负责轻重分级和仓库治理边界
- `mycpu-quality-review`
  - 负责本项目的代码质量审查与收敛 rubric
- `systematic-debugging`
  - 只在真的遇到红灯或异常行为时升级
- `requesting-code-review`
  - 用于实现完成后的额外独立审查
- `receiving-code-review`
  - 用于处理外部 review feedback

默认不要因为 superpowers 的通用话术就自动：

- 开 worktree
- 派子代理
- 写大 plan
- 跑全量矩阵

除非用户明确要求，或仓库治理判断确有必要。

## 输出要求

完成后至少报告：

1. 审查 / 修改范围
2. 发现的问题或实际做掉的收敛项
3. 守住了哪些边界
4. 实际运行了哪些验证
5. 哪些地方刻意没有动
6. 剩余风险或建议下一步
