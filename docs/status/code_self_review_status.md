# 代码自检状态

## 文档定位

本文档用于承接以下仍然有效的自检结论与后续收口结果：

- `2026-03-24` 的系统性代码自检
- `2026-03-31` 的 `interactive_os terminal` 专项复检
- `2026-03-31` 晚些时候落地的 terminal 输入与会话同步修正

它只保留当前仍有价值的结论、风险和后续顺序，不再维护早期逐项审查流水账。

## 关联文档

- 相关设计：
  - [design/regression_completion_criteria.md](../design/regression_completion_criteria.md)
  - [design/debug_frontend_integration.md](../design/debug_frontend_integration.md)
  - [design/minimal_interactive_os_design.md](../design/minimal_interactive_os_design.md)
- 相关状态：
  - [status/mainline_status.md](mainline_status.md)
  - [status/kernel_alpha_status.md](kernel_alpha_status.md)
- 重要已完成计划归档：
  - [plan/history_plan.md#phase1-hardening-regressions-plan](../plan/history_plan.md#phase1-hardening-regressions-plan)

## 当前结论

- 早期触及 reference path correctness 底线的那批问题，已经基本完成第一轮修复和回归化。
- guest runtime、`kernel_alpha` 基线和 `interactive_os` guest monitor 当前整体边界已经比早期稳定得多。
- 当前自检焦点已经从“ISA / MMU / loader 基础 correctness”进一步转移到“host/frontend 调试链路的长会话压力、跨语言预算长期一致性与功能面控制”；真实 `debug server + mycpu --debug-cli` 端到端 smoke 已经落地。
- 就 `interactive_os terminal` 这条链路而言，guest 侧 monitor 本身没有发现新的 CPU 设计、特权语义或 MMU 合同错误；当前活跃风险主要集中在 browser frontend、Node debug server 和 host-side `debug_cli` 的压力与集成场景，而不是先前那批已识别的会话竞态、terminal tail 或协议拆分问题。

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
- `interactive_os terminal` 的 host/frontend 链路也已补上本轮同步修正：
  - `debug_server` 当前已使用 session 级串行化与 generation 失效保护，`load/reset/run/terminal-input` 不再让旧请求污染新会话。
  - terminal 增量同步已和 snapshot 广播解耦，等待回显 / prompt 时会合并 WebSocket 更新，不再把一次输入放大成整页重绘风暴。
  - server / frontend 已共享控制字符投影与有界 projected tail，当前不再沿用“raw tail 先截断、渲染时再解释控制序列”的旧路径。
  - `debug_protocol` 已拆成命令解码、响应序列化与 `CLI loop` 三块，`debug_server` 也已拆成 `DebugCliSession`、server runtime 与 HTTP / WebSocket 入口；调试链路当前不再继续由单个大文件同时承担子进程、协议、terminal 跟踪和路由职责。
  - 真实 `debug server + mycpu --debug-cli` 端到端 smoke 已接入 Node 门禁；Node 侧 session timeout、interactive boot/command 和 terminal advance 预算也已收口到 `frontend/server/debug_budget.mjs`。
  - C++ 侧 `debug_session.cpp` 的 `step_commit` budget 与 `interactive_terminal_smoke.cpp` 的 interactive boot/command budget 也已统一收口到 `myCPU/src/debug/debug_budget.h`。
  - 上述边界已由 `frontend/tests/debug_server.test.mjs`、`frontend/tests/terminal_input_pump.test.mjs`、`frontend/tests/terminal_projection.test.mjs`、`frontend/tests/terminal_render.test.mjs` 与 `myCPU/tests/host/interactive_terminal_smoke.cpp` 守住。

## 2026-03-31 `interactive_os terminal` 专项复检与后续收口结论

本轮复检覆盖：

- browser frontend
- `frontend/server/debug_server.mjs`
- host-side `debug_session / debug_protocol`
- guest `console_input / monitor / monitor_commands / interactive_os`

结论如下：

- guest monitor 这一侧结构相对干净，职责边界基本清晰。
- 这一轮识别出的主要问题，确实集中在 host/frontend 的 session 生命周期、terminal 增量同步和 UI 渲染耦合，而不是 guest 命令实现本身。
- 随后的 terminal 输入与会话同步修正已经把上一版识别出的几条核心风险压成稳定门禁；当前剩余工作更多转向协议稳健性、长会话压力验证和功能面控制，而不是继续处理同一批已知竞态。

### 本轮已同步收口、不再作为当前 blocker 跟踪的问题

1. **server 侧 session 串行化与旧请求失效。**
   `debug_server` 当前已经引入 session 级 action queue、generation guard 和 run loop token；`load/reset/run/terminal-input` 的互斥与旧请求失效不再只靠前端忽略旧响应。

2. **terminal 输入触发的 snapshot / WebSocket 广播风暴。**
   当前服务端等待回显 / prompt 时，terminal delta 与 snapshot 已分离处理，相关路径会合并增量更新；`debug_server` Node 测试已明确守住“等待输出收敛时最多只广播一次 snapshot / terminal update”的合同。

3. **server / frontend terminal tail 的无界增长与重复状态。**
   当前 terminal 状态已收口到共享 projection state 和有界 projected tail，原来那条“服务端累积全量 UART 文本、前端再维护另一份 raw tail”的退化模式已经退出当前主路径。

4. **raw tail 先截断、后投影导致的显示边界不稳。**
   当前 server / frontend 已统一改为“先做控制字符投影，再保留有界 tail”，相关边界已由 projection / render 测试覆盖。

### 当前仍有效的高优先级风险

1. **[建议关注] terminal 链路仍缺更长会话与更高吞吐压力验证。**
   现有 Node / host smoke 已经能把“会话替换污染、广播风暴、有界 tail 与控制字符投影”压成稳定红灯，但它们仍主要覆盖单会话、最小交互和有限输出量。对更长时间 `run`、更高频输入输出和真实浏览器交互时序的压力验证仍然偏少。

2. **[建议关注] 调试链路预算常量当前已分别在 Node / C++ 侧完成收口，但仍不是跨语言单一事实来源。**
   当前 `frontend/server/debug_budget.mjs` 与 `myCPU/src/debug/debug_budget.h` 已分别统一各自语言内的 session / interactive 预算命名；但两侧仍是分语言维护，后续如果只改单边，仍有再次漂移风险。

3. **[建议关注] `debug/frontend` 的功能面仍需继续收住。**
   当前这条链路已经达到“教学演示可用”的最小状态。后续如果继续往断点、条件暂停、任意文件加载或更大 UI 功能面扩张，而不先收口协议与验证边界，很容易重新引入新的耦合和脆弱点。

## 当前建议顺序

1. 为 terminal 输入 / 输出链路补更长会话、持续 `run`、更高吞吐输出和真实浏览器交互节奏下的压力验证。
2. 继续维护 Node `debug_budget.mjs` 与 C++ `debug_budget.h` 的预算口径一致性；如果后续 budget 再变化，应优先同步两侧命名入口，而不是重新引入裸数字。
3. 继续维持 terminal delta、snapshot 和 UI 渲染之间已经形成的边界，不让新功能把它们重新耦合回去。
4. `debug/frontend` 后续仍以教学演示可用为边界，避免在现有门禁尚未继续增强前盲目扩功能面。

## 本轮复检依据

本轮专项复检之后，已新鲜确认通过：

- `cd frontend && npm test`
- `cd frontend && node --test`
- `cd frontend && node --test frontend/tests/debug_server_e2e.test.mjs`
- `cd myCPU && make test-host-interactive_terminal_smoke`
- `cd myCPU && make test-host-debug_cli_smoke`
- `cd myCPU && make test-host-debug_protocol_command_smoke`

这些门禁当前能证明：

- terminal 最小交互链路仍然可用；
- 上一版识别出的会话替换污染、terminal 广播风暴、buffer / tail 投影，以及 `debug_protocol / debug_server` 的单文件耦合问题，当前已经进入自动化门禁；
- 当前文档里保留的风险，主要转向长会话压力、真实端到端集成和功能面控制，而不是现有 smoke 已经直接失败的 correctness 红灯。

## 当前建议入口

如果下一轮要继续处理这些问题，建议优先阅读：

- [status/mainline_status.md](mainline_status.md)
- [design/debug_frontend_integration.md](../design/debug_frontend_integration.md)
- [design/minimal_interactive_os_design.md](../design/minimal_interactive_os_design.md)
- [myCPU/AGENTS.md](../../myCPU/AGENTS.md)
- [myCPU/guest/AGENTS.md](../../myCPU/guest/AGENTS.md)
