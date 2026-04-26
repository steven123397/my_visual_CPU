# AGENTS.md

## 适用范围

本文件适用于 [docs](.) 子树下的全部正式技术文档。

## 目录结构

当前 `docs/` 只保留以下正式层次：

- [index.md](index.md)
  面向读者的文档入口和阅读顺序。
- [background](background)
  项目背景、原始目标和外部上下文。当前只保留 [background/request.md](background/request.md)。
- [design](design)
  结构设计、阶段设计、模块设计与长期有效的契约 / 收口标准。
- [plan](plan)
 具体实现计划、整改计划，以及统一的完成态归档。
- [status](status)
  当前状态跟踪文档。
- [showcase](showcase)
  项目展示材料、展示截图和 HTML 汇报预览页。

## 文档分工

根目录与子目录文档职责如下：

- [AGENTS.md](../AGENTS.md)
  只保留项目总览、全局约定、workflow、阶段规划和子目录索引。
- [myCPU/AGENTS.md](../myCPU/AGENTS.md)
  记录 simulator 主体的实现基线、局部规则和已知问题。
- [myCPU/guest/AGENTS.md](../myCPU/guest/AGENTS.md)
  记录 guest runtime 的实现基线、局部规则和已知问题。
- [README.md](../README.md)
  面向读者的项目概览，保持可读、简洁，不承载过细实现流水账。
- [showcase](showcase)
  面向汇报、答辩和 README 引用的展示材料目录，不作为实时状态或执行计划来源。

## 文档角色规则

- `background/`
  只回答“为什么有这个项目 / 这条路线”。不写实时状态、任务进度或执行 checklist。
- `design/`
  只回答“要做什么、边界是什么、为什么这么设计”。设计文档应向下关联对应 `status` 和 `plan`，但不承担实时进度更新。
- `plan/`
 只回答“怎么落地、任务做到哪一步”。执行中的计划文档负责 checklist 和执行步骤；完成态计划统一归档到 `history_plan.md`。
- `status/`
  只回答“当前是什么状态、还有什么风险、下一步是什么”。同一模块的实时进度只能以对应 `status` 文档为准。
  代码审查 / 修改任务的 findings 默认集中到 [status/code_reself_status.md](status/code_reself_status.md)，除非用户明确要求另开专项状态文档。
- `showcase/`
  只回答“对外怎么展示项目”。可以放展示文案、截图、录屏占位页和 HTML 预览，不承载实时状态、长期设计边界或执行 checklist。

## 单一事实来源规则

- 实时状态、当前风险、当前下一步，只写在 `status/`。
- 任务清单、勾选进度、执行结果，只写在 `plan/`。
- 设计目标、边界、取舍、契约和收口标准，只写在 `design/`。
- 项目背景和原始目标，只写在 `background/request.md`。
- 对外展示文案、截图和 HTML 预览，只放在 `showcase/`。
- `index.md` 只做导航，不重复承载同一主题的当前状态。
- `AGENTS.md` 只定义规则和少量当前主线摘要，不复制模块状态正文。

## 新文档创建条件

- `background/`
  当前默认不新增模板，也不作为 AI 自动扩展目录。只有用户明确要求新增背景文档时才创建。
- `design/`
  当出现新的模块、阶段、平台契约、设计重构、收口标准或需要长期保存的方案边界时创建。
- `plan/`
  当某份设计已获确认、某轮整改需要跟踪执行、或对话中已经形成可执行任务清单时创建。
- `status/`
  当某个模块、主线、子系统或专项审查需要持续跟踪实时状态时创建。代码自检报告也视为 `status` 文档。
- `showcase/`
  当项目展示、答辩、README 截图或 HTML 汇报页需要集中素材时维护；不为普通实现任务自动新增展示材料。

## 模板要求

- 新增设计文档默认先套用 [design/template.md](design/template.md)。
- 新增计划文档默认先套用 [plan/template.md](plan/template.md)。
- 新增状态文档默认先套用 [status/template.md](status/template.md)。
- `background/` 当前不提供自动模板。
- `showcase/` 当前不提供固定模板，优先复用已有 [showcase/README.md](showcase/README.md) 和 [showcase/preview.html](showcase/preview.html) 的结构。
- 如果计划文档不是从现有设计文档派生，而是直接来自用户确认的任务，也必须先套用计划模板，并在“来源设计”处明确写清来源。

## 命名与完成态规则

- 长期维护的主状态文档优先使用稳定文件名，不把日期放进文件名。
- 代码审查 / 修改 findings 默认使用稳定文件名 [status/code_reself_status.md](status/code_reself_status.md)。
- 不创建单独 `docs/archive/` 或 `docs/plan/archive/` 目录；完成态计划统一归档到 [plan/history_plan.md](plan/history_plan.md)。
- 计划文档完成时必须：
  - 把 checklist 勾完
  - 在对应 `status` 文档中回写结果摘要、完成情况和必要历史节点
  - 把“完成时间 + 完成内容 + 必要时的一两句过程摘要”追加到 [plan/history_plan.md](plan/history_plan.md)
  - 删除原计划文件，不再把完成态 checklist 长期保留在 `plan/`
- 已完成但仍保留的设计文档，要在开头明确“当前有效”或“历史语境”，避免旧设计被误读为当前待办。
- 不再创建 `docs/contracts/`、`docs/templates/`、`docs/archive/` 或 `docs/superpowers/` 这类平行正式目录。

## 索引要求

- 每份 `design` 文档都应链接：
  - 对应的 `status`
  - 当前活跃 `plan`，或 [plan/history_plan.md](plan/history_plan.md) 中对应的历史条目
- 每份 `plan` 文档都应链接：
  - 来源 `design`
  - 目标 `status`
- 每份 `status` 文档都应链接：
  - 相关 `design`
  - 当前活跃 `plan`
  - 重要已完成计划在 [plan/history_plan.md](plan/history_plan.md) 中的归档条目，或统一归档入口
- 新增或重命名正式文档后，必须同步更新 [index.md](index.md)。

## 文档维护规则

- 根目录 `AGENTS.md` 不要再次膨胀成实现细节全集。
- 具体实现方式、局部规则、存留问题优先写入对应子树的 `AGENTS.md`。
- `AGENTS.md` 可以保留少量“当前主线 / 下一步 / 局部边界”摘要，但详细状态仍以 `status/` 为准。
- 长篇设计、审查、方案和契约文档放在 `docs/`，不要堆进 README。
- README 要持续可读，尤其 guest 相关描述保持概览化，不要写成长串内部细节。
- 不要让 `design / plan / status` 同时维护同一件事的实时表述。
- 如果某份状态文档已经开始堆积完整执行 checklist，应把执行细节下沉到 `plan/`。
- 如果某份计划文档已经完成，应先回写 `status`，再归档到 `history_plan.md`，随后删除原文件。
- 如果某份设计文档开始承担实时进度更新，应把实时部分收口到 `status/`。
- 报告和总结文档需要清楚区分：
  - 项目 owner 既有已完成工作
  - 已落地的当前重构成果
  - 正在进行的下一步工作
  - 更远期阶段规划
- 每部分任务完成时，自动更新 `status/`；如果对应 `plan` 已完成，还应同步归档到 `history_plan.md` 并删除原计划文件。
