
# 最小可交互 Monitor OS 实现计划

> **文档状态：** 已完成并通过总门禁验证

## 2026-03-27 当前进展

- 已完成 Task 1：
  - `Uart16550` 最小 RX 合同已接入；
  - guest 侧 `platform_uart_rx_ready()` / `platform_uart_getc()` 已打通；
  - `test-unit-uart_rx_contract` 已通过。
- 已完成 Task 2：
  - `debug_session` / `debug_protocol` 已支持 `uart_input(text)` 与 `uart_output(offset)`；
  - `step_commit` / `step_cycle` 已支持可选 `count`；
  - 为交互式 smoke 新增 `run_until_uart_contains` / `run_until_halt`；
  - JSON string 解析已支持 `\\r` / `\\n` / `\\t` 等基本转义。
- 已完成 Task 3 的最小闭环：
  - `interactive_os` 独立 guest 入口已接入；
  - 当前最小 monitor 已支持 banner / prompt / 可见 ASCII 回显 / `Enter` / `Backspace` / 固定行缓冲；
  - 当前命令集已支持 `help` / `echo` / `halt`；
  - `test-host-interactive_terminal_smoke`、`test-guest-interactive_os_demo`、`test-pipeline-guest-interactive_os_demo` 已通过。
- 已完成 Task 4：
  - `frontend/server` 已补 `terminal-input` / `terminal-output` API 与 terminal WebSocket 增量广播；
  - 浏览器前端已切成“终端主舞台 + 可折叠调试侧栏 + 点击后输入”桌面壳；
  - `cd frontend && node --test` 已通过。
- 已完成 Task 5：
  - monitor 已补 `time` / `uptime` / `disk info` / `disk read` / `regs` / `peek` / `pagewalk` / `pte dump`；
  - `tests/unit/monitor_commands.c` 与扩展后的 `interactive_terminal_smoke` 已接入；
  - `cd myCPU && make test-unit-monitor_commands` 已通过。
- 已完成 Task 6：
  - `interactive_os` 相关门禁已接入 `make test` / `make test-pipeline`；
  - 计划、状态、README 与 guest 入口说明已完成回写；
  - `mmio_access_faults` 已按 UART RX 新合同修正回归用例；
  - `guest_supervisor_demo` 已切到独立 timeout 预算，避免其既有高成本 smoke 继续误报超时；
  - `cd myCPU && make test`、`cd myCPU && make test-pipeline` 与 `cd frontend && node --test` 已通过。

## 文档定位

本文档用于把 [design/minimal_interactive_os_design.md](/home/liangjiaqi/projects/my_visual_CPU/docs/design/minimal_interactive_os_design.md) 中已经确认的“前端桌面壳 + guest 串口 monitor 内核”路线，拆成可执行的实现任务、验证门禁和完成态回写要求。

本文档只回答“怎么落地”，不重复维护设计边界或实时状态。设计边界仍以设计文档为准，实时进度以后续 `status` 文档回写为准。

## 关联文档

- 来源设计：
  - [design/minimal_interactive_os_design.md](/home/liangjiaqi/projects/my_visual_CPU/docs/design/minimal_interactive_os_design.md)
- 目标状态：
  - [status/mainline_status.md](/home/liangjiaqi/projects/my_visual_CPU/docs/status/mainline_status.md)
  - 如后续需要额外强调 `kernel_alpha` 冻结边界，再补充回写 [status/kernel_alpha_status.md](/home/liangjiaqi/projects/my_visual_CPU/docs/status/kernel_alpha_status.md)

## 目标

- 打通 `browser -> frontend/server -> mycpu --debug-cli -> UART RX -> guest monitor` 的正式输入链路。
- 在不扩图形硬件、不改 backend 语义边界的前提下，把当前前端扩成“桌面壳里的终端窗口”。
- 新增独立 `interactive_os` guest 入口，复用现有 `kernel_bringup`、`kernel_runtime`、`trap`、`vm`、`console`、`timer`、`storage` 基础设施。
- 先收口最小可交互闭环，再扩充 monitor 命令，使其覆盖基础交互、内核观察和 storage 只读探测。
- 为 host / device、debug protocol、frontend、guest demo 四层同时补齐可回归门禁。

## 完成定义

- `Uart16550` 新增最小 RX 合同，支持 host 注入输入字节流，且不破坏既有 TX / THRE 路径。
- `debug-cli` 新增显式 terminal I/O 命令，至少包含 `uart_input(text)` 与 `uart_output(offset)`，且不把完整终端缓冲塞回 `DebugSnapshot`。
- 前端新增终端窗口、焦点管理、键盘捕获、终端滚动缓冲和最小运行控制；鼠标点击仍只作用于壳层，不透传给 guest。
- `interactive_os` 作为新的 guest demo 独立存在，不把 `kernel_alpha` 扩成 monitor OS。
- `interactive_os` 至少支持：
  - banner / prompt
  - 可见 ASCII 回显
  - `Enter`
  - `Backspace`
  - 固定长度行缓冲
  - `help`、`echo`、`time`、`uptime`、`halt`
  - `regs`、`peek`、`pagewalk`、`pte dump`
  - `disk info`、`disk read`
- 新增并接入以下门禁：
  - `cd myCPU && make test-unit-uart_rx_contract`
  - `cd myCPU && make test-unit-monitor_commands`
  - `cd myCPU && make test-host-interactive_terminal_smoke`
  - `cd myCPU && make test-guest-interactive_os_demo`
  - `cd myCPU && make test-pipeline-guest-interactive_os_demo`
  - `cd frontend && node --test`
- 总门禁保持通过：
  - `cd myCPU && make test`
  - `cd myCPU && make test-pipeline`
  - `cd frontend && node --test`

## 分阶段收口

### Phase A：最小交互闭环

- 补 UART RX。
- 补 `debug-cli` terminal I/O。
- 新增 `interactive_os` skeleton、banner、prompt、回显、`help` / `echo` / `halt`。
- 通过 host-driven smoke 打通“加载 -> 看到 prompt -> 注入命令 -> 看到输出 -> halt”。

### Phase B：前端终端壳

- 补前端 terminal window、键盘捕获、terminal buffer 和 session-level output 同步。
- 保留现有 debug 面板，不把终端输出塞回 snapshot。

### Phase C：命令扩展与正式门禁

- 扩 `time` / `uptime` / `disk info` / `disk read` / `regs` / `peek` / `pagewalk` / `pte dump`。
- 接入 unit / host / guest / pipeline 回归。
- 回写 README、AGENTS、status 文档。

## 文件结构

### 新增文件

- `myCPU/tests/unit/uart_rx_contract.cpp`
  覆盖 `Uart16550` RX 队列、`LSR` 数据就绪位、消费后清空以及与既有 TX 路径不互相污染的合同。
- `myCPU/tests/unit/monitor_commands.c`
  覆盖 monitor 行编辑、命令分发、参数解析和输出语义片段。
- `myCPU/tests/host/interactive_terminal_smoke.cpp`
  通过 `--debug-cli` 驱动 `interactive_os`，验证 prompt、回显、命令输出、halt 和 backend 无关性。
- `myCPU/guest/include/console_input.h`
  guest 侧轮询式 UART 输入与行编辑 helper 声明。
- `myCPU/guest/include/monitor.h`
  monitor 主循环与运行入口声明。
- `myCPU/guest/include/monitor_commands.h`
  monitor 命令分发、parser 和结果枚举声明。
- `myCPU/guest/include/vm_debug.h`
  pagewalk / PTE dump 等 VM 调试 helper 声明。
- `myCPU/guest/kernel/console_input.c`
  轮询式 `rx_ready/getc`、行缓冲、退格和提交逻辑。
- `myCPU/guest/kernel/monitor.c`
  banner、prompt、monitor 主循环和命令执行编排。
- `myCPU/guest/kernel/monitor_commands.c`
  命令 parser、dispatcher 与基础命令实现。
- `myCPU/guest/kernel/monitor_format.c`
  monitor 输出格式化 helper，避免 `monitor.c` 继续膨胀。
- `myCPU/guest/kernel/vm_debug.c`
  只读 pagewalk / PTE dump helper。
- `myCPU/guest/interactive_os/main.c`
  新的独立 guest 入口。
- `frontend/app/components/terminal.js`
  终端窗口渲染与 terminal 结构拼装。
- `frontend/tests/terminal_state.test.mjs`
  终端焦点、键盘规范化、滚动缓冲状态逻辑测试。

### 重点修改文件

- `myCPU/include/platform_mmio.h`
  新增最小 UART RX 所需寄存器/bit 常量。
- `myCPU/src/devices/uart16550.h`
- `myCPU/src/devices/uart16550.cpp`
  实现 RX FIFO / queue、`LSR` 数据就绪位和 host 注入入口。
- `myCPU/src/debug/debug_session.h`
- `myCPU/src/debug/debug_session.cpp`
  新增 `uart_input` / `uart_output` 会话 API。
- `myCPU/src/debug/debug_protocol.cpp`
  新增 terminal I/O 命令。
- `myCPU/tests/host/debug_cli_smoke.cpp`
  扩到 `uart_output(offset)` / `uart_input(text)` 基本路径。
- `myCPU/guest/include/platform.h`
- `myCPU/guest/include/platform_drivers.inc`
- `myCPU/guest/lib/platform.S`
  新增 guest 侧 `platform_uart_rx_ready()` / `platform_uart_getc()`。
- `myCPU/guest/kernel/console.c`
  如需要，可把只写 console 保持薄封装，并把输入逻辑收口到 `console_input.c`。
- `myCPU/Makefile`
  接新 unit / host / guest 目标与总门禁。
- `frontend/server/debug_server.mjs`
  新增 terminal input/output API、session terminal buffer 和 WebSocket 广播。
- `frontend/app/index.html`
- `frontend/app/app.js`
- `frontend/app/api.js`
- `frontend/app/render.js`
- `frontend/app/state.js`
- `frontend/app/styles.css`
  扩 terminal window、交互状态和桌面壳呈现。
- `frontend/tests/debug_server.test.mjs`
- `frontend/tests/ui_state.test.mjs`
  扩 terminal API 和 terminal UI 相关门禁。
- `myCPU/AGENTS.md`
- `myCPU/guest/AGENTS.md`
- `readme.md`
- `docs/status/mainline_status.md`
  在实现完成后回写结果、门禁和当前边界。

## 任务

### 任务 1：补 UART RX 合同与 guest 侧接收 shim

**文件：**
- 创建：`myCPU/tests/unit/uart_rx_contract.cpp`
- 修改：`myCPU/include/platform_mmio.h`
- 修改：`myCPU/src/devices/uart16550.h`
- 修改：`myCPU/src/devices/uart16550.cpp`
- 修改：`myCPU/guest/include/platform.h`
- 修改：`myCPU/guest/include/platform_drivers.inc`
- 修改：`myCPU/guest/lib/platform.S`
- 修改：`myCPU/Makefile`

- [ ] **步骤 1：先写失败的 `uart_rx_contract.cpp`**

  覆盖以下合同：

  - 新实例初始 `LSR` 不带 RX 数据就绪位。
  - host 注入 `"AB"` 后，guest 连续两次读取分别得到 `'A'` 与 `'B'`。
  - RX 队列清空后，`LSR` 数据就绪位恢复未就绪。
  - RX 路径存在时，原有 `THR` 输出和 `output_size()` 行为不变。

- [ ] **步骤 2：注册并运行失败测试**

  运行：`cd myCPU && make test-unit-uart_rx_contract`

  预期：FAIL，表现为以下其中之一：

  - `Makefile` 还没有 `uart_rx_contract` 目标；
  - 缺少 `UART_REG_RBR` / `UART_LSR_DR` 常量；
  - 缺少 `inject_input(...)` 或等价 RX API。

- [ ] **步骤 3：实现最小 UART RX 合同**

  实现要求：

  - 在 `platform_mmio.h` 中补 `UART_REG_RBR` 和 `UART_LSR_DR`。
  - 在 `Uart16550` 中新增最小 RX 队列、`inject_input(...)` 和只读 helper。
  - `load(UART_REG_RBR)` 消费一个字节。
  - `load(UART_REG_LSR)` 正确反映 RX 就绪与既有 THRE/TEMT。
  - 只提供 polling 所需能力，不在这一任务引入 RX interrupt。
  - guest 侧平台 shim 提供 `platform_uart_rx_ready()` 与 `platform_uart_getc()`。

- [ ] **步骤 4：重新运行新单测**

  运行：`cd myCPU && make test-unit-uart_rx_contract`

  预期：PASS。

- [ ] **步骤 5：回归既有 MMIO / UART 相关门禁**

  运行：

  - `cd myCPU && make test-unit-mmio_contract_matrix`
  - `cd myCPU && make test-host-debug_cli_smoke`

  预期：PASS，证明 RX 补丁没有破坏既有 TX / snapshot 可观察性。

- [ ] **步骤 6：Commit**

  示例：`git commit -m "feat(uart): add minimal rx contract for interactive monitor"`

### 任务 2：扩 `DebugSession` / `debug_protocol` 的 terminal I/O

**文件：**
- 修改：`myCPU/src/debug/debug_session.h`
- 修改：`myCPU/src/debug/debug_session.cpp`
- 修改：`myCPU/src/debug/debug_protocol.cpp`
- 修改：`myCPU/tests/host/debug_cli_smoke.cpp`
- 修改：`myCPU/Makefile`

- [ ] **步骤 1：先扩失败的 `debug_cli_smoke.cpp`**

  新增两类断言：

  - 对已有 `hello` flat-binary，在运行后调用 `uart_output(0)` 能拿到完整输出和 `next_offset`。
  - 再次调用 `uart_output(next_offset)` 返回空增量。
  - `uart_input("abc")` 命令可成功写入当前会话，不改变既有 snapshot 命令结构。

- [ ] **步骤 2：运行 smoke，确认协议尚未支持 terminal I/O**

  运行：`cd myCPU && make test-host-debug_cli_smoke`

  预期：FAIL，报 `unknown command: uart_output`、`unknown command: uart_input` 或缺少相关字段。

- [ ] **步骤 3：实现 session-level terminal I/O API**

  实现要求：

  - `DebugSession` 暴露 `uart_input(text)`。
  - `DebugSession` 暴露 `uart_output(offset)`，返回 `{ next_offset, text }` 等价结构。
  - `debug_protocol` 新增对应 JSON line 命令。
  - `DebugSnapshot` 继续只保留 `output_size` / `recent_output` 这一类有限快照，不扩成无界终端日志。

- [ ] **步骤 4：重新运行 host smoke**

  运行：`cd myCPU && make test-host-debug_cli_smoke`

  预期：PASS。

- [ ] **步骤 5：确认 protocol 改动未破坏现有快照 / pipeline smoke**

  运行：

  - `cd myCPU && make test-host-pipeline_backend_smoke`
  - `cd myCPU && make test-host-backend_differential_smoke`

  预期：PASS。

- [ ] **步骤 6：Commit**

  示例：`git commit -m "feat(debug): add terminal io commands to debug cli"`

### 任务 3：新增独立 `interactive_os` 入口与最小 monitor 闭环

**文件：**
- 创建：`myCPU/guest/include/console_input.h`
- 创建：`myCPU/guest/include/monitor.h`
- 创建：`myCPU/guest/include/monitor_commands.h`
- 创建：`myCPU/guest/kernel/console_input.c`
- 创建：`myCPU/guest/kernel/monitor.c`
- 创建：`myCPU/guest/kernel/monitor_commands.c`
- 创建：`myCPU/guest/kernel/monitor_format.c`
- 创建：`myCPU/guest/interactive_os/main.c`
- 创建：`myCPU/tests/host/interactive_terminal_smoke.cpp`
- 修改：`myCPU/Makefile`

- [ ] **步骤 1：先写失败的 host-driven smoke**

  `interactive_terminal_smoke.cpp` 至少覆盖：

  - 通过 `--debug-cli` 加载 `guest/interactive_os.elf`。
  - 轮询 `uart_output(0)`，直到看到 boot banner 和 prompt。
  - 注入 `help\r`，断言有回显和帮助语义片段。
  - 注入 `echo hi\r`，断言输出 `hi`。
  - 注入 `halt\r`，断言 session 最终 `halted=true`。

- [ ] **步骤 2：运行 smoke，确认当前缺少 guest demo / make 目标**

  运行：`cd myCPU && make test-host-interactive_terminal_smoke`

  预期：FAIL，表现为以下其中之一：

  - 缺少 `guest/interactive_os.elf` 构建规则；
  - monitor 相关 guest 源文件不存在；
  - smoke 无法等到 prompt。

- [ ] **步骤 3：实现最小 monitor 闭环**

  实现要求：

  - `interactive_os/main.c` 使用独立入口，不修改 `kernel_alpha/main.c` 角色。
  - 复用 `kernel_runtime_run_bringup(...)`。
  - 首版默认采用 polling UART RX，不把 PLIC / RX interrupt 作为 monitor 前置条件。
  - 行编辑至少支持：
    - 可见 ASCII
    - `Enter`
    - `Backspace`
    - 固定长度行缓冲
  - 命令至少支持：
    - `help`
    - `echo`
    - `halt`
  - 启动后必须稳定打印 banner 和 prompt。

- [ ] **步骤 4：补 guest / host 目标并重新运行 smoke**

  运行：

  - `cd myCPU && make test-host-interactive_terminal_smoke`
  - `cd myCPU && make test-guest-interactive_os_demo`

  预期：PASS。

- [ ] **步骤 5：补 backend 双路径 smoke**

  运行：`cd myCPU && make test-pipeline-guest-interactive_os_demo`

  预期：PASS，说明 `interactive_os` 不依赖某个 backend 私有行为。

- [ ] **步骤 6：Commit**

  示例：`git commit -m "feat(guest): add interactive_os monitor skeleton"`

### 任务 4：把前端扩成终端桌面壳

**文件：**
- 创建：`frontend/app/components/terminal.js`
- 创建：`frontend/tests/terminal_state.test.mjs`
- 修改：`frontend/server/debug_server.mjs`
- 修改：`frontend/app/index.html`
- 修改：`frontend/app/app.js`
- 修改：`frontend/app/api.js`
- 修改：`frontend/app/render.js`
- 修改：`frontend/app/state.js`
- 修改：`frontend/app/styles.css`
- 修改：`frontend/tests/debug_server.test.mjs`
- 修改：`frontend/tests/ui_state.test.mjs`

**当前已确认的 Task 4 UI 决策：**

- 采用“终端主舞台 + 调试器可折叠侧栏”的混合式布局；
- 默认由终端占据主视觉区域，调试器作为右侧 inspector 按需展开；
- 键盘默认不直通 guest，用户点击终端后才进入输入态；
- 终端输出继续走独立 terminal buffer / offset，同步边界不并入 `DebugSnapshot`；
- 详细书面规格见：
  - `docs/superpowers/specs/2026-03-27-interactive-terminal-shell-design.md`

- [ ] **步骤 1：先写失败的 Node tests**

  至少覆盖：

  - `POST /api/session/terminal-input` 能发送文本到当前会话。
  - `POST /api/session/terminal-output` 或等价接口能返回 `{ nextOffset, text }`。
  - WebSocket 能广播 terminal 增量或 session terminal state。
  - 前端状态机能处理：
    - terminal focus
    - 可见 ASCII / `Enter` / `Backspace` 规范化
    - 自动滚动

- [ ] **步骤 2：运行 Node tests，确认当前只有只读调试面**

  运行：`cd frontend && node --test`

  预期：FAIL，表现为 404、缺少 terminal state 或 UI state 断言不成立。

- [ ] **步骤 3：实现 terminal server + terminal window**

  实现要求：

  - Node 服务维护 session-level terminal offset / buffer。
  - 浏览器输入通过专门 terminal API 下发，不复用 snapshot 接口。
  - WebSocket 继续广播 snapshot；terminal 增量可独立广播，也可并入 session terminal message。
  - 前端新增终端窗口、点击聚焦、键盘捕获、滚动缓冲。
  - 鼠标点击只作用于壳层状态，不产生 guest 鼠标事件。

- [ ] **步骤 4：重新运行 Node tests**

  运行：`cd frontend && node --test`

  预期：PASS。

- [ ] **步骤 5：与 host smoke 联动验证**

  运行：`cd myCPU && make test-host-interactive_terminal_smoke`

  预期：PASS，证明 server/front-end 方案没有反向要求 simulator 改协议边界。

- [ ] **步骤 6：Commit**

  示例：`git commit -m "feat(frontend): add terminal desktop shell for interactive os"`

### 任务 5：扩 monitor 命令到正式范围

**文件：**
- 创建：`myCPU/tests/unit/monitor_commands.c`
- 创建：`myCPU/guest/include/vm_debug.h`
- 创建：`myCPU/guest/kernel/vm_debug.c`
- 修改：`myCPU/guest/include/monitor_commands.h`
- 修改：`myCPU/guest/kernel/monitor.c`
- 修改：`myCPU/guest/kernel/monitor_commands.c`
- 修改：`myCPU/guest/kernel/monitor_format.c`
- 修改：`myCPU/tests/host/interactive_terminal_smoke.cpp`
- 修改：`myCPU/Makefile`

- [ ] **步骤 1：先写失败的 `monitor_commands.c`**

  覆盖以下点：

  - 命令 parser 能区分空行、未知命令、单参数和双参数命令。
  - `help` / `echo` / `time` / `uptime` / `halt`。
  - `disk info` / `disk read 0`。
  - `peek 0x80000000`。
  - `pagewalk 0x80000000`。
  - `pte dump`。
  - 关键测试只断言语义片段，不把整段终端输出当稳定 ABI。

- [ ] **步骤 2：运行 unit test，确认 parser / dispatcher 尚未完成**

  运行：`cd myCPU && make test-unit-monitor_commands`

  预期：FAIL，报缺少 parser / dispatcher / VM debug helper。

- [ ] **步骤 3：实现命令扩展**

  实现要求：

  - 保持 `monitor.c` 只负责主循环和 I/O 编排。
  - parser / dispatch 放在 `monitor_commands.c`。
  - pagewalk / PTE dump 只做只读调试，不把 VM internals 重新耦合回 monitor 主循环。
  - `peek` 必须做地址范围校验，避免 monitor 自己制造 guest fault。
  - `disk read` 首版只做只读探测，不引入写盘。

- [ ] **步骤 4：扩 host smoke 到完整命令面**

  把 `interactive_terminal_smoke.cpp` 扩成会依次注入：

  - `help`
  - `echo hi`
  - `time`
  - `uptime`
  - `disk info`
  - `disk read 0`
  - `peek 0x80000000`
  - `pagewalk 0x80000000`
  - `pte dump`
  - `halt`

- [ ] **步骤 5：运行 unit / host / guest 验证**

  运行：

  - `cd myCPU && make test-unit-monitor_commands`
  - `cd myCPU && make test-host-interactive_terminal_smoke`
  - `cd myCPU && make test-guest-interactive_os_demo`
  - `cd myCPU && make test-pipeline-guest-interactive_os_demo`

  预期：PASS。

- [ ] **步骤 6：Commit**

  示例：`git commit -m "feat(monitor): add debug and storage commands"`

### 任务 6：接 Makefile 总门禁、文档回写与全量验证

**文件：**
- 修改：`myCPU/Makefile`
- 修改：`myCPU/AGENTS.md`
- 修改：`myCPU/guest/AGENTS.md`
- 修改：`readme.md`
- 修改：`docs/status/mainline_status.md`
- 修改：`docs/status/kernel_alpha_status.md`（仅当需要补充分工边界时）
- 修改：`docs/index.md`
- 修改：`docs/design/minimal_interactive_os_design.md`
- 修改：`docs/plan/minimal_interactive_os_plan.md`

- [ ] **步骤 1：把新目标接入 Makefile 聚合门禁**

  至少接入：

  - `test-unit-uart_rx_contract`
  - `test-unit-monitor_commands`
  - `test-host-interactive_terminal_smoke`
  - `test-guest-interactive_os_demo`
  - `test-pipeline-guest-interactive_os_demo`

  并把它们纳入 `make test` / `make test-pipeline` 的合适位置。

- [ ] **步骤 2：回写文档**

  至少补齐：

  - README 中如何运行最小交互 OS 教学演示
  - `myCPU/guest/AGENTS.md` 中新的 `interactive_os` 入口边界
  - `status/mainline_status.md` 中“当前计划”与完成结果摘要
  - `design/minimal_interactive_os_design.md` 的“相关计划”回链

- [ ] **步骤 3：跑 focused verification**

  运行：

  - `cd myCPU && make test-unit-uart_rx_contract`
  - `cd myCPU && make test-unit-monitor_commands`
  - `cd myCPU && make test-host-interactive_terminal_smoke`
  - `cd myCPU && make test-guest-interactive_os_demo`
  - `cd myCPU && make test-pipeline-guest-interactive_os_demo`
  - `cd frontend && node --test`

  预期：PASS。

- [ ] **步骤 4：跑全量门禁**

  运行：

  - `cd myCPU && make test`
  - `cd myCPU && make test-pipeline`
  - `cd frontend && node --test`

  预期：PASS。

- [ ] **步骤 5：标记完成态并回写状态文档**

  要求：

  - 把本计划头部改成“已完成”。
  - 在 `status/mainline_status.md` 写入完成结果、关键历史节点和剩余风险。
  - 如仍需强调 `kernel_alpha` 冻结职责，再同步更新 `status/kernel_alpha_status.md`。

- [ ] **步骤 6：Commit**

  示例：`git commit -m "docs(plan): finalize minimal interactive os rollout"`

## 完成态回写要求

- 全部 checklist 必须勾完。
- 文件头必须改成“已完成”或等价完成态说明。
- 对应 `status` 文档必须增加：
  - 完成结果摘要
  - 关键历史节点
  - 仍然有效的剩余风险（如果有）
