# AGENTS.md

myCPU 是一个已可运行的 RISC-V 系统模拟器原型。

## 工作入口

- 开发流程采用用户的 NexusKit skill 体系，用户明确要求优先。
- 先读 [docs/current.md](docs/current.md)，再读目标子树的 `AGENTS.md`。
- 资料导航见 [docs/index.md](docs/index.md)。
- 设计取舍、非琐碎实现或排障时，按主题定向检索 `docs/solutions/`；
  复用适用结果。目录不存在或无匹配时照常推进。
- `CONCEPTS.md` 在首个合格术语产生时建立，届时补齐导航。
- 待办使用 GitHub Issues：`steven123397/my_visual_CPU`。

## 项目边界

- 共享 `InstructionSemantics + functional backend` 是 ISA 真值来源。
- 不提交构建产物；验证与支持声明以实际证据为准。
- 旧文档维护规则全部废除，旧文档仅作参考，不要求更新、补写或归档。
- 项目指引从最小内容维护，只随实际需要增加已确认的长期约束。

<!-- CODEGRAPH_START -->
## CodeGraph

仓库存在 `.codegraph/` 时，理解或定位代码先用 CodeGraph：
MCP `codegraph_explore`，或 CLI `codegraph explore` / `codegraph node`。
不存在索引时跳过，不自行建立索引。
<!-- CODEGRAPH_END -->
