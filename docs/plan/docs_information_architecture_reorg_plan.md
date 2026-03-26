# docs 文档治理重组实现计划

> **文档状态：** 已完成

> **完成态说明：** 本轮已经把 `docs/` 正式文档收口到 `background / design / plan / status + AGENTS.md + index.md`。当前治理规则以 [docs/AGENTS.md](/home/liangjiaqi/projects/my_visual_CPU/docs/AGENTS.md) 和 [docs/index.md](/home/liangjiaqi/projects/my_visual_CPU/docs/index.md) 为准，完成结果摘要已回写到 [status/mainline_status.md](/home/liangjiaqi/projects/my_visual_CPU/docs/status/mainline_status.md)。

> **面向 AI 代理的工作者：** 如需重演类似工作，仍应使用 superpowers:subagent-driven-development（推荐）或 superpowers:executing-plans。下文复选框结果仅保留历史执行记录。

**目标：** 把当前 `docs/` 重组为 `background / design / plan / status + AGENTS.md + index.md` 的稳定结构，并建立统一的模板、索引和维护规则。

**架构：** 保留 `background/request.md` 作为唯一背景文档，把 `contracts/` 并入 `design/`，把 `superpowers/` 与 `archive/` 中的计划和设计迁回正式目录，把完成态逻辑固化到 `plan/` 与 `status/` 的联动规则中。通过重写 `docs/AGENTS.md`、`docs/index.md` 和模板文件，形成后续 AI 与人工都能持续遵守的文档治理约束。

**技术栈：** Markdown、Git、仓库内现有文档体系

## 关联文档

- 来源设计：
  - 无。本计划直接落实已确认的文档治理规则。
- 目标状态：
  - [status/mainline_status.md](/home/liangjiaqi/projects/my_visual_CPU/docs/status/mainline_status.md)

## 实际结果

- 已建立 [design/template.md](/home/liangjiaqi/projects/my_visual_CPU/docs/design/template.md)、[plan/template.md](/home/liangjiaqi/projects/my_visual_CPU/docs/plan/template.md) 和 [status/template.md](/home/liangjiaqi/projects/my_visual_CPU/docs/status/template.md)。
- 已重写 [docs/AGENTS.md](/home/liangjiaqi/projects/my_visual_CPU/docs/AGENTS.md) 与 [docs/index.md](/home/liangjiaqi/projects/my_visual_CPU/docs/index.md)，收口目录职责、模板规则和索引关系。
- 已把原 `contracts / superpowers / archive / templates` 中仍需保留的正式文档迁回 `design / plan / status`。
- 已把完成态计划保留在 `plan/`，并把对应结果回写到相关 `status` 文档。

---

### 任务 1：建立新的 docs 信息架构与模板

**文件：**
- 创建：`docs/design/template.md`
- 创建：`docs/plan/template.md`
- 创建：`docs/status/template.md`
- 修改：`docs/AGENTS.md`
- 修改：`docs/index.md`

- [x] **步骤 1：按新规则建立 `design / plan / status` 模板**
- [x] **步骤 2：重写 `docs/AGENTS.md`，明确目录职责、创建条件、完成态回写规则与索引要求**
- [x] **步骤 3：重写 `docs/index.md`，把入口收敛到新四分法与主状态入口**

### 任务 2：迁移并重命名现有正式文档

**文件：**
- 修改：`docs/design/*`
- 修改：`docs/status/*`
- 修改：`readme.md`
- 修改：`AGENTS.md`
- 修改：`myCPU/AGENTS.md`
- 修改：`myCPU/guest/AGENTS.md`

- [x] **步骤 1：把 `contracts/`、`templates/`、`superpowers/`、`archive/` 中仍应保留的文档迁入正式目录**
- [x] **步骤 2：把主状态文档改成稳定命名，并统一更新所有仓库内链接**
- [x] **步骤 3：把 `design / plan / status` 中混杂的旧口径收敛到新的单一事实来源规则**

### 任务 3：处理历史计划与状态重复

**文件：**
- 修改：`docs/plan/*`
- 修改：`docs/status/kernel_alpha_status.md`
- 删除：不再保留的重复历史文件

- [x] **步骤 1：把 archive 中的历史计划迁入 `plan/` 并标记完成态**
- [x] **步骤 2：把 `kernel_alpha` 的历史过程摘要并入对应状态文档，删除重复历史文件**
- [x] **步骤 3：确认完成态计划都已在对应 `status` 中留下结果摘要**

### 任务 4：收尾与校验

**文件：**
- 复查：`docs/`
- 复查：仓库内所有 Markdown 链接引用

- [x] **步骤 1：移除空目录与废弃目录**
- [x] **步骤 2：运行仓库内路径引用扫描，确认不存在旧目录和旧文件名残留**
- [x] **步骤 3：用 `git diff --check` 做文档格式自检**
- [x] **步骤 4：把本计划按实际执行结果更新为完成态**
