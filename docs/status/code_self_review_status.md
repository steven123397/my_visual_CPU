# 代码自检状态

## 文档定位

本文档用于承接两轮仍然有效的自检结论：

- `2026-03-24` 的系统性代码自检
- `2026-03-31` 的 `interactive_os terminal` 专项复检

它只保留当前仍有价值的结论、风险和后续顺序，不再维护早期逐项审查流水账。

## 关联文档

- 相关设计：
  - [design/regression_completion_criteria.md](/home/liangjiaqi/projects/my_visual_CPU/docs/design/regression_completion_criteria.md)
  - [design/debug_frontend_integration.md](/home/liangjiaqi/projects/my_visual_CPU/docs/design/debug_frontend_integration.md)
  - [design/minimal_interactive_os_design.md](/home/liangjiaqi/projects/my_visual_CPU/docs/design/minimal_interactive_os_design.md)
- 相关状态：
  - [status/mainline_status.md](/home/liangjiaqi/projects/my_visual_CPU/docs/status/mainline_status.md)
  - [status/kernel_alpha_status.md](/home/liangjiaqi/projects/my_visual_CPU/docs/status/kernel_alpha_status.md)
- 重要已完成计划：
  - [plan/phase1-hardening-regressions_plan.md](/home/liangjiaqi/projects/my_visual_CPU/docs/plan/phase1-hardening-regressions_plan.md)

## 当前结论

- 早期触及 reference path correctness 底线的那批问题，已经基本完成第一轮修复和回归化。
- guest runtime、`kernel_alpha` 基线和 `interactive_os` guest monitor 当前整体边界已经比早期稳定得多。
- 当前自检焦点已经从“ISA / MMU / loader 基础 correctness”转移到“host/frontend 调试链路的并发、性能和协议稳健性”。
- 就 `interactive_os terminal` 这条链路而言，guest 侧 monitor 本身没有发现新的 CPU 设计、特权语义或 MMU 合同错误；主要风险集中在 browser frontend、Node debug server 和 host-side `debug_cli`。

## 已完成的主要收口

`2026-03-24` 那轮系统性自检识别出的高优先级 correctness 缺口，目前已完成第一轮收口：

- 非法整数保留编码不再误执行，已补 asm 回归。
- `DIV/REM/DIVW/REMW` 的 `INT_MIN / -1` 边界不再依赖宿主未定义行为，已补 asm 回归。
- ELF loader 已支持 pure-BSS `PT_LOAD` 的 `zero-fill`，并补上 segment / reject / header 单元回归。
- `Bus` / device 的区间冲突和非法访问宽度防御已完成第一轮收口，host-side MMIO contract matrix 已接入。
- guest runtime 已完成 `vm*`、`trap*`、`kernel_runtime`、`user_task*`、`user_program*` 的第一轮职责拆分与单元门禁扩充。
- `interactive_os / monitor / vm_debug` 已完成第一轮 post-Phase1 hardening：
  - `peek` 走 `vm_debug_read()` 的只读校验路径，不再因为 monitor 命令本身制造 guest fault。
  - `monitor_commands` 的参数与错误路径已有单元回归。
  - `interactive_terminal_smoke` 已覆盖 functional / pipeline 下的最小交互闭环。

## 2026-03-31 `interactive_os terminal` 专项复检结论

本轮复检覆盖：

- browser frontend
- `frontend/server/debug_server.mjs`
- host-side `debug_session / debug_protocol`
- guest `console_input / monitor / monitor_commands / interactive_os`

结论如下：

- guest monitor 这一侧结构相对干净，职责边界基本清晰。
- 当前性能与稳定性问题，主要不是 guest 命令实现“写成屎山”，而是 host/frontend 在 session 生命周期、terminal 增量同步和 UI 渲染上的耦合过重。
- 现有测试是绿的，但它们还没有把这些结构性风险压成稳定红灯。

### 当前仍有效的高优先级风险

1. **[必须修复] server 侧缺少 session 串行化。**
   `currentSession/currentSnapshot/currentTerminalBuffer/currentTerminalOffset/runTimer` 当前由多个 HTTP handler 和 `setInterval(async ...)` 并发访问。前端的 `terminalInputPump.reset()` 只能忽略旧响应，不能阻止旧请求继续改服务端状态或继续发 websocket，这会造成 `load/reset/run/terminal-input` 之间的真实竞态和跨会话污染。

2. **[建议修改] terminal 输入链路仍会放大成 snapshot 广播风暴。**
   当前服务端为等待回显 / prompt 会循环推进 `stepCommit()` 并广播 snapshot；前端收到 terminal 或 snapshot 消息后又会整页 `paint()`。这会把一次按键放大成高频 websocket 消息和全页面重绘，是当前 `interactive_os terminal` 越用越慢的主要结构性原因。

3. **[建议修改] server 侧 terminal buffer 仍然无界增长。**
   `currentTerminalBuffer` 当前主要只是为 prompt 检测服务，但仍在累积全量 UART 文本。这与前端此前已经修过的 buffer 膨胀问题是同类退化模式，也让 server / frontend 之间出现了不必要的重复状态。

4. **[建议修改] terminal raw tail 先截断、后投影，显示边界还不稳。**
   前端当前先保留原始 UART 文本尾部，再在渲染时处理 `\b/\r`。一旦截断点落在控制序列边界，终端起始处就可能保留脏字符或出现与逻辑状态不一致的显示残留。

5. **[建议修改] `debug_protocol` 仍是手写最小 JSON line parser。**
   当前协议解析依赖字符串查找与有限 escape 集合，足以支撑现阶段 demo，但对更复杂字段、协议演进和异常输入的容错都偏弱。terminal 输入这轮已经因为 escape 处理暴露过一次问题，这块仍然是后续容易反复出 bug 的边界。

## 当前建议顺序

1. 先把 `debug_server` 改成 session 级 single-flight / generation 模型，真正收住 `load/reset/run/terminal-input` 的互斥和旧请求失效。
2. 把 terminal 增量同步与 snapshot 广播解耦，避免“一个字符触发整页重绘”。
3. 给 server 侧 terminal buffer 加上有界 tail，并把前端 buffer cap 从“raw tail”改成“对控制序列友好”的保留策略。
4. 如果 `debug_cli` 协议继续扩展，再把手写 parser 收口成更稳的统一 codec，而不是继续补零散 escape case。

## 本轮复检依据

本轮专项复检之后，已新鲜确认通过：

- `cd frontend && npm test`
- `cd myCPU && make test-host-interactive_terminal_smoke`
- `cd myCPU && make test-host-debug_cli_smoke`

这些门禁当前能证明：

- terminal 最小交互链路仍然可用；
- 这轮文档中列出的风险主要是并发、长会话和结构性性能问题，而不是现有 smoke 已经直接失败的 correctness 红灯。

## 当前建议入口

如果下一轮要继续处理这些问题，建议优先阅读：

- [status/mainline_status.md](/home/liangjiaqi/projects/my_visual_CPU/docs/status/mainline_status.md)
- [design/debug_frontend_integration.md](/home/liangjiaqi/projects/my_visual_CPU/docs/design/debug_frontend_integration.md)
- [design/minimal_interactive_os_design.md](/home/liangjiaqi/projects/my_visual_CPU/docs/design/minimal_interactive_os_design.md)
- [myCPU/AGENTS.md](/home/liangjiaqi/projects/my_visual_CPU/myCPU/AGENTS.md)
- [myCPU/guest/AGENTS.md](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/AGENTS.md)
