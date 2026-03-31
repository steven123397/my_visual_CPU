# 最小可交互 Monitor OS 设计

## 文档定位

本文档用于说明如何在当前已可运行的模拟器原型上，沿现有 guest runtime、平台 MMIO 契约和 `debug/frontend` 链路，落地一条“前端桌面壳 + guest 串口 monitor 内核”的最小可交互 OS 路线。

本文档重点定义：

- 当前要解决的问题
- 目标与非目标
- host / frontend / guest 三层的职责边界
- 输入输出链路、最小交互合同和验证口径
- 与 `kernel_alpha` 基线及 `Phase 3-A` 预测增强路线的分工边界

本文档不承担实时进度更新。当前进展请写入对应 `status` 文档。

## 关联文档

- 状态文档：
  - [status/mainline_status.md](/home/liangjiaqi/projects/my_visual_CPU/docs/status/mainline_status.md)
  - [status/code_self_review_status.md](/home/liangjiaqi/projects/my_visual_CPU/docs/status/code_self_review_status.md)
  - [status/kernel_alpha_status.md](/home/liangjiaqi/projects/my_visual_CPU/docs/status/kernel_alpha_status.md)
- 相关计划：
  - 无。该方向的执行细节已经回写到相关 `status` 文档。

## 当前有效性说明

- 当前有效 / 历史语境：当前有效，作为当前主线对"最小可交互 OS"目标的结构边界说明。
- 当前正式进展以 [status/mainline_status.md](/home/liangjiaqi/projects/my_visual_CPU/docs/status/mainline_status.md) 为准；当前 terminal 壳层剩余稳定化问题统一写入 [status/code_self_review_status.md](/home/liangjiaqi/projects/my_visual_CPU/docs/status/code_self_review_status.md)。

## 背景与问题

当前仓库已经是一个已可运行的模拟器原型，而不是纯设计稿。`phase1-stable`（`283aee6`）对应的 Phase 1 核心 bring-up 基线已经形成，独立 `kernel_alpha` 正向与九条负向回归、`functional` / `pipeline` 两个 backend，以及本地 `debug_session/protocol + frontend` 教学演示链路都已经接入主线。

这意味着“把一个最小内核跑起来”已经不是当前主问题。当前真正缺的，是一条面向使用者的最小交互闭环：guest 侧虽然已经有稳定的 UART 输出、trap、VM、timer、external interrupt 和 storage bring-up 基础，但还没有一条正式的 guest 可见输入路径；现有前端本质上仍是只读调试器，而不是一个承载 guest 交互面的终端式桌面壳。

如果直接把目标抬到“guest 自己驱动的图形桌面”，就会立刻引入 framebuffer / 鼠标设备 / 指针事件 / compositing / widget hit-test 等新虚拟硬件与 guest 图形子系统。这已经超出“最小可交互 OS”范围，也会把当前主线从 reference path hardening 和结构收口拉回大范围扩功能。因此，这份设计明确收口到更小的切片：前端负责桌面壳和窗口交互，guest 只负责一个 `S-mode` 串口 monitor 内核。

## 目标

- 在现有 guest 基础设施之上，新增一条独立的 `S-mode` 串口 monitor 路径，而不是继续膨胀 `kernel_alpha`。
- 让浏览器前端把 guest 的交互面包装成一个“桌面里的终端窗口”，支持鼠标点击聚焦、运行控制和窗口级操作。
- 打通一条从键盘到 guest 的正式输入链路：`browser -> frontend server -> simulator -> UART RX -> guest console`。
- 让 guest 提供一个激进型串口 monitor：包含 prompt、回显、最小行编辑，以及面向 bring-up / 调试的内建命令。
- 继续复用现有 `kernel_bringup`、`kernel_runtime`、`trap`、`vm`、`console`、`timer` 等共享基础设施，不为这个目标重新起一套 runtime。
- 保持当前 `functional` reference path、共享 ISA 语义层和 `pipeline` backend 的边界不变；输入输出能力应是平台层能力，而不是某个 backend 的私有行为。
- 把 `kernel_alpha` 保持为 Phase 1 bring-up / hardening 冻结基线，而不是把它演化成最终可交互 OS 入口。

## 非目标

- 不实现 guest 自己驱动的图形桌面，不新增 framebuffer、tile buffer 或其他显示硬件。
- 不实现 guest 可见的鼠标设备；鼠标点击只服务前端桌面壳，不作为 guest 的输入事件来源。
- 不引入用户态程序加载、进程切换、应用模型或更高层软件生态。
- 不在首版中引入文件系统、持久化写盘、网络栈、窗口管理器或图形组件系统。
- 不把现有前端扩成通用调试器 / IDE / 任意文件终端宿主。
- 不为了这条交互路径重做 `Machine`、`ExecutionBackend`、`kernel_alpha` 或既有 MMIO 契约。
- 不把 monitor 命令集当成长期稳定 ABI；首版优先服务 bring-up、调试和教学演示。

## 约束与边界

- `functional + shared InstructionSemantics` 继续是唯一 ISA 语义真值来源；交互路径不能演化出第二套执行语义。
- 当前主线对 guest 的输入语义只收口到“串口文本输入流”，而不是通用事件总线。
- 鼠标点击只影响前端桌面壳的按钮、窗口焦点和布局状态，不透传给 guest。
- 首版交互只保证可见 ASCII、`Enter`、`Backspace` 这组最小键集；不承诺方向键、历史、补全、ANSI 终端控制序列或 IME。
- 首版 guest 输入路径优先采用 polling 式 UART RX 消费，不把“先上接收中断”当成前置条件。
- 新的交互式 OS 应作为新的 guest demo / 入口存在，`kernel_alpha` 继续保持“第一次真正的小 kernel alpha bring-up 基线 + hardening 回归”的职责，不转型成长期 monitor OS。
- `DebugSnapshot` 继续保持稳定、有限的快照语义；终端滚动缓冲不应通过把整段文本无限塞进 snapshot 来实现。
- monitor 命令可以偏激进，但默认视为开发期调试接口；允许后续因实现和验证需要调整命令名、输出和参数格式。
- 这条设计线默认拥有 `frontend` 的终端 / 桌面壳层；与 `Phase 3-A` 并行时，`Phase 3-A` 的可观察性优先收口在 snapshot / 协议，不直接主导前端 UI。

## 方案

### 结构设计

整体结构收口为 3 层：

```text
browser
  -> frontend desktop shell
  -> frontend/server
  -> mycpu --debug-cli
       -> DebugSession / debug_protocol
       -> Machine
            -> Uart16550 RX/TX
            -> functional / pipeline backend
       -> guest interactive monitor OS
            -> kernel_bringup / kernel_runtime
            -> console / monitor loop
```

第一层是 host / simulator 平台层。这里不新增图形设备，只在现有 `Uart16550` 上补最小可接收输入的 RX 路径，并为 host 提供显式的“注入输入字节流”入口。这个入口属于平台设备能力，不属于 `functional` 或 `pipeline` 私有逻辑。

第二层是 `debug_session/protocol + frontend` 交互层。这里不把当前调试快照改造成无界终端日志，而是补一组最小终端 I/O 命令，由 Node 服务负责把浏览器键盘输入转成 UART RX 注入，把 guest 的 UART 输出增量同步到浏览器终端窗口。前端桌面壳负责窗口呈现、焦点和按钮行为；终端内容仍然是 guest 串口文本。

第三层是 guest 侧最小交互 monitor 内核。它复用现有 `kernel_bringup` / `kernel_runtime` / `console` / `trap` / `vm` 基础设施，新建独立的 `interactive_os` 入口和最小 monitor 循环。目标是形成一条“能启动、能提示、能读键盘、能执行调试命令、能继续输出”的稳定 demo，而不是把当前 `kernel_alpha` 入口越改越大。

### 接口 / 数据 / 契约

#### 1. UART 输入合同

当前 UART 已有稳定的发送路径和 THRE 相关中断语义。最小可交互 OS 所需的新能力，是补一条与之对称、但仍然很小的接收路径：

- guest 可从 UART 读取下一个输入字节；
- 当没有待消费输入时，guest 可观察到“当前无数据”；
- host 可向 UART 的接收队列注入一串文本字节；
- 这条路径首版只保证文本输入，不引入鼠标、窗口或高层 UI 事件。

首版建议继续保持 polling 合同，而不是先上 RX interrupt。原因是：这条路线的目标是“先接通交互闭环”，不是“先做更像真实 UART 的完整中断模型”。后续若需要更真实的串口接收中断，应单独作为平台设备增强设计，而不是并入本切片。

#### 2. Debug CLI / Node 服务合同

当前 `snapshot` 已暴露 UART `output_size` 和最近输出尾部，但这只适合调试面板，不适合作为终端滚动缓冲的正式来源。最小交互终端应额外补两类显式命令：

- 输入命令：向当前会话的 UART RX 队列注入文本
- 输出命令：按 offset 读取 UART 输出增量

推荐保持命令语义尽量窄，例如：

- `uart_input(text)`
- `uart_output(offset) -> { next_offset, text }`

这样做有两个好处：

- 不把 `DebugSnapshot` 膨胀成无界文本载体；
- 让终端滚动缓冲成为前端 / Node 会话状态，而不是 simulator 快照结构的一部分。

在这个模型下：

- 浏览器到 Node：通过一个专门的 terminal input API 发送键盘文本；
- Node 到 simulator：复用现有 `--debug-cli` 子进程，串行发送终端 I/O 和 step/run 相关命令；
- Node 到浏览器：继续通过 WebSocket 广播 snapshot，同时单独广播 UART 输出增量或把它并入 session-level terminal state。

#### 3. 前端桌面壳合同

“桌面”在这里是前端呈现层概念，不是 guest 图形系统。当前实现与后续维护都应继续收口到以下 3 个区域：

- `session bar`
  展示当前测试、backend、运行状态和 `Load / Run / Pause / Reset / Step` 控件。
- `terminal stage`
  作为主舞台，负责 banner、prompt、命令回显和滚动缓冲。
- `debug inspector`
  作为辅助观察区，继续承接 snapshot、pipeline、events、registers、CSR、bus 与设备状态，但不反向定义 terminal 协议。

terminal 相关前端状态至少包括：

- `terminal.buffer`
- `terminal.nextOffset`
- `terminal.focused`
- `terminal.connected`
- `terminal.pendingInput`

这组状态当前必须继续遵守以下规则：

- terminal buffer 只通过独立 terminal API / terminal WebSocket 增量推进，不并入 `DebugSnapshot`。
- `load` / `reset` 必须清空 terminal buffer 和 offset，避免跨 session 串味。
- 键盘输入继续只收口到可见 ASCII、`Enter`、`Backspace` 这组最小键集。
- 鼠标只影响前端壳层焦点与控件行为，不向 guest 暴露更高层点击语义。
- 调试信息继续保留为辅助观察区，不与 guest monitor 的命令语义耦合。

当前这组合同已经实现完成；剩余工作重点是 session 串行化、terminal 性能和协议稳健性，统一见 [status/code_self_review_status.md](/home/liangjiaqi/projects/my_visual_CPU/docs/status/code_self_review_status.md)。

#### 4. Guest monitor 合同

guest 侧收口为一个最小串口 monitor：

- 启动后打印 boot 信息和 prompt；
- 支持逐字符回显；
- 至少支持 `Enter` 提交一行、`Backspace` 删除一个字符；
- 使用固定长度的行缓冲，超长输入按明确策略处理；
- 提供一组偏激进的内建命令，优先覆盖：
  - 基础交互：`help`、`echo`、`time`、`uptime`、`halt`
  - 内核观察：`regs`、`peek`、`pagewalk`、`pte dump`
  - storage 只读探测：`disk info`、`disk read`
- 首版命令集以开发期调试效率为目标，不承诺输出格式长期稳定；测试应更多约束关键语义片段，而不是把完整终端输出当作稳定 ABI。

这条 monitor 路径应理解为“内核自带的交互入口”，不是应用层。它不依赖用户态、可执行文件加载或文件系统。

### 验证思路

验证应覆盖 4 层，而不是只看人工演示：

1. simulator / device 层
   - 覆盖 UART RX 队列、空队列读取、输入注入后的可见性，以及与既有发送路径不互相污染的合同。

2. debug / host 层
   - 为 `debug_protocol` 和 `DebugSession` 新增 smoke，验证：
   - 文本输入能够进入当前会话；
   - UART 输出能够按 offset 增量读取；
   - 这条路径在 `functional` 与 `pipeline` 下都不依赖 backend 私有行为。

3. guest demo 层
   - 新增独立 `interactive_os` guest demo 的 host-driven smoke：
   - 加载镜像；
   - 注入一组固定命令；
   - 运行若干 step / cycle；
   - 断言 prompt、回显和关键命令语义稳定可见。

4. frontend 层
   - `node --test` 至少守住：
   - terminal input API；
   - WebSocket / session terminal output 同步；
   - 点击聚焦与键盘捕获的纯状态逻辑。

架构相关改动的验证基线仍应守住：

- `cd myCPU && make test`
- `cd myCPU && make test-pipeline`
- `cd frontend && node --test`

实现阶段还应新增一组与本设计直接对应的目标，例如：

- `cd myCPU && make test-guest-interactive_os_demo`
- `cd myCPU && make test-host-interactive_terminal_smoke`

## 风险与取舍

- 选择“前端桌面壳 + 串口 monitor 内核”，意味着首版的“桌面感”来自前端呈现，而不是 guest 自己驱动图形硬件。这是有意取舍，用来换取更小范围和更快收口。
- 选择 polling 式 UART RX，而不是首版就做 RX interrupt，意味着设备模型不够“完整”，但可以显著降低 guest trap / PLIC / 时序耦合复杂度。
- 选择显式 `uart_input / uart_output` 命令，而不是把整段终端文本塞进 snapshot，意味着协议面会多一层，但能保持 `DebugSnapshot` 结构稳定、边界清晰。
- 把交互式 OS 做成新的 guest 入口，而不是继续扩 `kernel_alpha`，会增加一个 demo 路径，但能避免 bring-up 基线和交互 monitor 目标互相污染。
- 把命令集定位为开发期调试接口，而不是长期稳定 ABI，能显著降低前期设计压力，但代价是后续若要把它转成正式用户界面，需要再做一次合同收口。
- 由于 `frontend` ownership 默认归这条设计线，后续若与 `Phase 3-A` 并行推进，预测器的最小可观察性应优先通过 snapshot / 协议落地；否则会在终端桌面壳与 pipeline 可视化之间制造高冲突区。
- 如果后续要走真正的 guest 图形桌面路线，应新开一份设计文档，单独定义显示设备、指针输入和 guest 图形层，不应在本设计上直接加码。

## 当前有效性说明

- 当前有效 / 历史语境：当前有效，作为当前主线对“最小可交互 OS”目标的结构边界说明。
- 当前正式进展以 [status/mainline_status.md](/home/liangjiaqi/projects/my_visual_CPU/docs/status/mainline_status.md) 为准；若后续为该方向建立专门 `status` 文档，再以对应文档承载实时进度。
