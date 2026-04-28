# AGENTS.md

## 适用范围

本文件适用于 [docs](.) 子树下的全部正式技术文档。

## 目录结构

当前 `docs/` 只保留以下正式层次：

- [index.md](index.md)
  面向读者的文档入口和阅读顺序。
- [background](background)
  项目背景、原始目标和外部上下文。
- [design](design)
  长期有效的设计边界、阶段设计、模块契约和取舍说明。
- [plan](plan)
  执行计划、checklist 和完成态归档。
- [status](status)
  当前状态、风险、优先级和下一步。
- [showcase](showcase)
  项目展示材料、展示截图和 HTML 预览页。

## 文档分工

- [AGENTS.md](../AGENTS.md)
  只保留仓库总览、全局约定、默认 workflow 和验证基线。
- [myCPU/AGENTS.md](../myCPU/AGENTS.md)
  只保留 simulator 主体的方法、局部规则和验证要求。
- [myCPU/guest/AGENTS.md](../myCPU/guest/AGENTS.md)
  只保留 guest runtime 的局部规则和验证要求。
- [README.md](../README.md)
  面向读者的项目概览，保持简洁，不承载实时状态流水账。
- [showcase](showcase)
  面向展示与汇报，不作为实时状态或执行计划来源。

## 文档角色规则

- `background/`
  只回答“为什么有这个项目 / 这条路线”。
- `design/`
  只回答“边界是什么、为什么这么设计”。
- `plan/`
  只回答“怎么落地、checklist 到哪一步”。
- `status/`
  只回答“当前是什么状态、当前优先级是什么、还有什么风险、下一步是什么”。
- `showcase/`
  只回答“对外怎么展示项目”。

## 单一事实来源规则

- 仓库级实时主线状态、当前优先级、active wave 和近端 blocker，只写在 [status/mainline_status.md](status/mainline_status.md)。
- 不再维护并行的仓库级主线状态文档，例如 `project_priority_roadmap.md` 或 `xv6_linux_jit_status.md` 这一类拆分。
- 专项状态文档只在确有独立跟踪价值时保留，例如 `kernel_alpha`、`NPU / TPU-like` 或 review findings。
- `AGENTS.md` 只定义规则、方法、范围和验证要求，不复制实时状态正文。
- `index.md` 只做导航，不重复状态内容。
- 已完成计划统一归档到 [plan/history_plan.md](plan/history_plan.md)，不在 `status` 或 `AGENTS` 里长期保留执行 checklist。

## 新文档创建条件

- `background/`
  只有用户明确要求新增背景文档时才创建。
- `design/`
  当出现新的模块、阶段、平台契约、设计重构或需要长期保存的方案边界时创建。
- `plan/`
  当任务需要分阶段执行、checklist、验收点或归档记录时创建。
- `status/`
  当某个独立子系统需要长期跟踪实时状态时创建。
  仓库主线默认继续使用稳定文件名 [status/mainline_status.md](status/mainline_status.md)，不要再拆平行主线状态文档。
- `showcase/`
  当项目展示、答辩、README 截图或 HTML 汇报页需要集中素材时维护。

## 模板要求

- 新增设计文档默认先套用 [design/template.md](design/template.md)。
- 新增计划文档默认先套用 [plan/template.md](plan/template.md)。
- 新增状态文档默认先套用 [status/template.md](status/template.md)。
- `background/` 和 `showcase/` 当前不提供固定自动模板。

## 命名与完成态规则

- 长期维护的主状态文档优先使用稳定文件名，不把日期放进文件名。
- 代码审查 / 修改 findings 默认使用稳定文件名 [status/code_reself_status.md](status/code_reself_status.md)。
- 计划文档完成时必须：
  - 把 checklist 勾完
  - 在对应 `status` 文档中回写结果摘要
  - 把“完成时间 + 完成内容 + 过程摘要”追加到 [plan/history_plan.md](plan/history_plan.md)
  - 删除原计划文件
- 已完成但仍保留的设计文档，要在开头明确“当前有效”或“历史语境”。

## 索引要求

- 每份 `design` 文档都应链接：
  - 对应 `status`
  - 当前活跃 `plan`，或 [plan/history_plan.md](plan/history_plan.md) 中对应的历史条目
- 每份 `plan` 文档都应链接：
  - 来源 `design`
  - 目标 `status`
- 每份 `status` 文档都应链接：
  - 相关 `design`
  - 当前活跃 `plan`
  - 重要已完成计划在 [plan/history_plan.md](plan/history_plan.md) 中的归档条目
- 新增、重命名或删除正式文档后，必须同步更新 [index.md](index.md)。

## 文档维护规则

- 根目录和子树 `AGENTS.md` 不要膨胀成实现细节或状态流水账全集。
- 长篇设计、审查、方案和契约文档放在 `docs/`，不要堆进 README。
- 不要让 `design / plan / status` 同时维护同一件事的实时表述。
- 如果某份状态文档开始堆积完整执行 checklist，应把执行细节下沉到 `plan/`。
- 如果某份设计文档开始承担实时进度更新，应把实时部分收口到 `status/`。
- 每部分任务完成时，自动更新对应 `status`；如果 `plan` 已完成，还应同步归档到 `history_plan.md`。
