# 交互式终端桌面壳设计

## 文档定位

本文档用于把 `minimal_interactive_os` 的 Task 4 前端方案固定为可执行规格。它只覆盖浏览器壳层、Node 服务 terminal API 和前端状态机，不改动 guest / monitor 的职责边界。

本文档遵循已确认的三项交互决策：

- 终端为主舞台，调试器退居辅助区域；
- 调试信息使用可折叠侧栏，而不是常驻主视图；
- 键盘默认不直通 guest，用户点击终端后才进入输入态。

本文档不承担实时进度更新。当前进展请写入对应 `status` 文档。

## 关联文档

- 状态文档：
  - [status/mainline_status.md](/home/liangjiaqi/projects/my_visual_CPU/docs/status/mainline_status.md)
- 相关计划：
  - [plan/minimal_interactive_os_plan.md](/home/liangjiaqi/projects/my_visual_CPU/docs/plan/minimal_interactive_os_plan.md)

## 设计边界

本方案必须继续满足 `docs/design/minimal_interactive_os_design.md` 的约束：

- 不新增 framebuffer、图形设备或 guest 鼠标事件；
- 不把终端滚动缓冲并入 `DebugSnapshot`；
- 不把前端扩成通用 IDE 或窗口管理器；
- 鼠标只改变前端壳层状态，例如焦点、侧栏展开与按钮点击；
- guest 仍然只接收 UART 文本输入流。

## 页面结构

页面收口为 3 个主区域：

1. 顶部 `session bar`
   展示当前测试、backend、运行状态，以及 `Load / Run / Pause / Step / Reset` 控件。

2. 主舞台 `terminal stage`
   终端占据主要视区，负责展示 banner、prompt、命令回显和输出滚动缓冲。终端窗口拥有清晰的标题栏、状态提示和点击聚焦反馈。

3. 右侧 `debug inspector`
   调试器默认收起，展开后显示现有 summary、pipeline、events、devices、registers、csrs、bus。它保留当前调试能力，但不与终端输出耦合。

## 状态模型

前端状态在现有 snapshot/history 之外新增 `terminal` 与 `layout` 两个子状态：

- `terminal.buffer`
  当前终端完整文本缓冲，用于渲染滚动视图。
- `terminal.nextOffset`
  对应 server 侧 `uart_output(offset)` 的下一个读取偏移。
- `terminal.focused`
  当前终端是否已进入输入态。
- `terminal.connected`
  当前 session 是否已建立，可用于决定空态提示。
- `terminal.pendingInput`
  是否存在尚未完成的 terminal input 请求，用于避免重复发送。
- `layout.debugPanelOpen`
  调试侧栏展开 / 收起状态。

状态更新原则：

- snapshot 仍通过现有路径推进；
- terminal buffer 只通过 terminal API / terminal WebSocket 消息推进；
- reset / load 新会话时必须清空 terminal buffer 和 offset，避免跨 session 串味；
- 自动滚动只在用户仍停留底部附近时触发，不强行抢滚动位置。

## Server / API 设计

Node 服务维持独立的 session-level terminal 状态，并在现有 HTTP / WebSocket 接口上补两条窄接口：

- `POST /api/session/terminal-input`
  请求体：`{ text }`
  作用：将文本转发给当前 `DebugCliSession.uartInput(text)`。

- `POST /api/session/terminal-output`
  请求体：`{ offset }`
  响应体：`{ text, nextOffset }`
  作用：按 offset 拉取 UART 输出增量。

服务端行为约束：

- `load` / `reset` 后，服务端 terminal 状态回到空缓冲和 offset `0`；
- 成功执行 terminal input 后，服务端立即拉取一次 terminal output，并通过 WebSocket 广播 terminal 增量；
- WebSocket 保留 `snapshot` 广播，同时新增 `terminal` 消息类型，例如：
  - `{ type: "terminal", text, nextOffset, reset }`
- `terminal` 与 `snapshot` 是并行通道，不互相嵌套。

## 前端交互

### 终端

- 默认显示“点击终端开始输入”的提示。
- 点击终端容器后进入 focused 状态。
- focused 状态下只处理最小键集：
  - 可见 ASCII
  - `Enter`
  - `Backspace`
- 非最小键集直接忽略，不尝试实现 ANSI 控制或历史编辑。
- 每次按键都通过 terminal input API 发送最小文本片段，不在前端本地模拟整行 shell。

### 调试侧栏

- 默认收起，仅保留一个明确的展开按钮。
- 展开时显示现有调试面板，不改变 snapshot 结构和渲染组件职责。
- 收起 / 展开只改变布局，不应清空任何 snapshot 或 terminal 状态。

### 运行控制

- `Load` 成功后终端缓冲清空，并显示新会话空态。
- `Run / Pause / Step / Reset` 继续沿用现有 session 控制接口。
- 每次 session 状态变化后，前端可主动拉取 terminal output，以尽快同步 guest 新输出。

## 实现拆分

为避免把 Task 4 做成一次性大改，代码拆分为以下单元：

1. `frontend/server/debug_server.mjs`
   补 terminal API、session terminal offset / buffer 和 WebSocket terminal 广播。

2. `frontend/app/state.js`
   新增 terminal / layout 状态、键盘规范化和终端缓冲更新逻辑。

3. `frontend/app/components/terminal.js`
   负责终端主舞台和提示文案渲染，不掺入调试器面板拼装。

4. `frontend/app/render.js`
   编排 terminal stage、debug inspector 和现有 panels。

5. `frontend/app/app.js`
   处理 terminal 点击聚焦、键盘事件、load/run/pause/reset 后的 terminal 刷新。

6. `frontend/app/styles.css`
   调整为“终端主舞台 + 可折叠 inspector”布局，并保留现有视觉语言。

## 测试与验证

Task 4 的测试目标分 2 层：

1. `frontend/tests/debug_server.test.mjs`
   覆盖 terminal input/output API、load/reset 时的 terminal 状态清空，以及 WebSocket terminal 消息广播。

2. `frontend/tests/ui_state.test.mjs` 与新增 `frontend/tests/terminal_state.test.mjs`
   覆盖 terminal focus、键盘规范化、terminal buffer 追加、offset 推进和自动滚动判定。

实现完成后的验证命令保持为：

- `cd frontend && node --test`
- `cd myCPU && make test-host-interactive_terminal_smoke`

## 取舍说明

- 本方案刻意不实现可拖拽窗口、窗口层级或多终端会话。那会把 Task 4 从“最小桌面壳”扩成窗口管理器。
- terminal 输出选择独立通道，而不是塞回 snapshot，是为了保持协议边界清晰并避免快照无限膨胀。
- 点击后输入比默认直接抢键盘更稳妥，适合当前页面同时保留调试按钮和 inspector 操作。

## 当前有效性说明

- 当前有效 / 历史语境：历史语境，对应 `minimal_interactive_os` Task 4 前端实现规格。该功能已实现完成。
- 当前正式结果以 [status/mainline_status.md](/home/liangjiaqi/projects/my_visual_CPU/docs/status/mainline_status.md) 为准。
