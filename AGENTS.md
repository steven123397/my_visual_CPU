# AGENTS.md

myCPU 是一个已可运行的 RISC-V 系统模拟器原型。

## 工作入口

- 开发流程采用用户的 NexusKit skill 体系，用户明确要求优先。
- 先读 [docs/current.md](docs/current.md)；开发指引只维护本文件。
- 设计取舍、非琐碎实现或排障时，按主题定向检索 `docs/solutions/`；
  复用适用结果。目录不存在或无匹配时照常推进。
- `CONCEPTS.md` 在首个合格术语产生时建立，届时补齐导航。
- 待办使用 GitHub Issues：`steven123397/my_visual_CPU`。

## 项目边界

- 共享 `InstructionSemantics + functional backend` 是 ISA 真值来源。
- 不提交构建产物；验证与支持声明以实际证据为准。
- 旧文档维护规则全部废除，旧文档仅作参考，不要求更新、补写或归档。
- 项目指引从最小内容维护，只随实际需要增加已确认的长期约束。
- 日常 CI 配置覆盖全部默认 unit、核心 host 和前端默认测试；完整回归配置为手动运行。发布与云端验收状态见 `docs/current.md`。
- 本地按改动运行相关验证；广泛回归可复用对应版本的云端证据，记录 run ID、attempt、实际 checkout SHA 与跳过项。失败、取消、跳过和未运行不能记为通过。

## 架构 Wiki

- 当前架构与技术契约由 [GitHub Wiki](https://github.com/steven123397/my_visual_CPU/wiki) 承载，不在主仓库维护重复副本。
- 页面按模块组织：职责与范围、结构与数据流、对外契约、关键取舍、实现与验证入口；无内容的章节省略。
- 修改已记录的职责、接口或行为契约时，同步维护相关页面；内部实现变化不强制更新。
- Wiki 使用独立的 `.wiki.git` 仓库，通过 Git 编辑、提交和推送；维护前拉取最新内容，关联对应代码提交与验证证据。
- Wiki 已启用。页面内容需对照实现，不把提案或历史验证当作现行能力；未发布的页面修改需在 current 中说明。
- 决策经验放 `docs/solutions/`，待办放 GitHub Issues，工作进度放 `docs/current.md`。

<!-- CODEGRAPH_START -->
## CodeGraph

仓库存在 `.codegraph/` 时，理解或定位代码先用 CodeGraph：
MCP `codegraph_explore`，或 CLI `codegraph explore` / `codegraph node`。
不存在索引时跳过，不自行建立索引。
<!-- CODEGRAPH_END -->
