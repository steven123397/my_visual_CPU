# AGENTS.md

适用于 guest runtime 与 demo，遵循上级 [AGENTS.md](../AGENTS.md)。

- 通用 runtime 能力与 demo 入口编排分离。
- Course OS 与 Linux compat 旁路保持职责边界。
- 改动时验证受影响的 unit / guest smoke，并保留相关成功与错误行为。
