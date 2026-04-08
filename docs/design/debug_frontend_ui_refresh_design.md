# Debug / Frontend UI 美化设计

## 文档定位

本文档记录 `frontend` 当前这一轮 UI 美化的目标、边界和结构方案。

它承接现有 [debug_frontend_integration.md](debug_frontend_integration.md) 已落地的 `debug_session/protocol + frontend` 能力，但本轮不扩功能面，重点解决浏览器端的视觉层级、模块布局和交互顺手度问题。

本文档不承担实时进度更新。当前实现进展应回写到对应 `status` 文档。

## 关联文档

- 状态文档：
  - [status/mainline_status.md](../status/mainline_status.md)
  - [status/project_priority_roadmap.md](../status/project_priority_roadmap.md)
- 相关计划：
  - 当前无活跃计划。
- 相关设计：
  - [debug_frontend_integration.md](debug_frontend_integration.md)
  - [minimal_interactive_os_design.md](minimal_interactive_os_design.md)

## 背景与问题

当前前端已经具备本地调试服务接入、终端增量同步、运行控制、流水线 / 架构状态 / 平台状态展示等能力，功能边界已经足够支撑本地教学演示和调试使用。

当前主要问题不在功能缺口，而在呈现方式：`terminal`、控制区和 `inspector` 的视觉主次还不够稳定；页面虽然已经具备“终端优先”的倾向，但整体布局仍更像功能拼装页，而不是一张可长期使用的“调试实验台”。此外，当前页面缺少对“两类使用场景”的直接支持：一类是依赖串口交互的终端调试，另一类是不依赖终端、只关注运行状态和快照观察的状态检查。

因此，这一轮设计的目标不是继续扩张调试器能力，而是在不改动现有 host / protocol / guest 合同的前提下，把前端收口成一套更清晰、更耐看、也更顺手的 UI 壳层。

## 目标

- 明确 `terminal`、控制区和 `inspector` 的主次层级，让页面一眼可读。
- 把整体视觉收口为“浅色实验台 + 深色终端主舞台”的统一语言。
- 支持 `terminal` 可展开 / 可收起，以覆盖“交互式终端调试”和“纯状态观察”两类场景。
- 重新组织 `inspector` 的模块分区，使执行观察、架构状态和平台 / I/O 更符合调试心智。
- 在保持原生 HTML / CSS / ESM 架构不变的前提下，提升细节质感和交互一致性。

## 非目标

- 不新增断点、条件暂停、差分对比、任意文件加载等更大调试功能面。
- 不修改 `debug_session/protocol`、`DebugSnapshot` 或 guest `interactive_os` 的现有合同。
- 不迁移到 React、Vue 或其他前端框架。
- 不把当前前端扩展成通用调试器产品。
- 不为了视觉改造去重做不相关的 Node server、CLI 或 backend 数据结构。

## 约束与边界

- 现有 `frontend/app` 的原生静态页面结构继续保留，不引入框架级重写。
- `terminal.buffer`、`terminal.nextOffset`、`terminal.focused`、`terminal.connected`、`terminal.pendingInput` 等状态语义保持不变；本轮只调整呈现层和交互组织。
- `load` / `reset` 后的 `terminal` 清空与 offset 重置语义保持不变。
- `functional` / `pipeline` 的现有选择、控制按钮和快照展示合同保持不变。
- 终端收起只影响布局与可见性，不丢弃当前会话 buffer，也不改变输入同步协议。
- 页面仍应服务“本地教学演示 / 调试”这一定位，不为了当前使用场景预建更重的浏览器功能面。

## 方案

### 结构设计

本轮采用用户确认的 **方案 B：页面骨架重排 + 样式系统一起收口**。

整体页面收口为 4 层：

1. **Hero 总览层**
   - 左侧展示项目标题、当前页面定位和简短说明。
   - 右侧展示高价值状态卡，如 `run state`、`backend`、关键执行摘要。
   - 目标是把顶部区域从“说明文字”提升为“实验台总览抬头”。

2. **控制带层**
   - 继续承载测试选择、backend 选择、`Load / Run / Pause / Step / Reset`。
   - 控制区按职责重排为“会话选择”“主操作”“精细控制 / 视图控制”三组。
   - 加入明确的 `terminal` 展开 / 收起入口，并为主次按钮建立更明显的视觉层级。

3. **主舞台层**
   - 默认维持“终端优先”：深色终端作为最大视觉锚点。
   - 在终端右侧保留一列轻量级观察区，用于展示运行摘要、输入状态和少量快捷观测信息。
   - 当 `terminal` 收起时，该层不保留空白，而是把可用空间让渡给下方 `inspector`。

4. **Inspector 深度观察层**
   - 改为更稳定的分组视图，而不是松散的 slot 堆叠。
   - 核心分为：
     - **执行观察**：`summary`、`pipeline`、`timeline`、`predictor / OoO`
     - **架构状态**：`registers`、`CSR / trap`
     - **平台与 I/O**：`devices`、`bus`、`events`
   - `terminal` 展开时它是第二层；`terminal` 收起时它自动上升为主视区，并重排为更高利用率的网格。

### 视觉语言

整体视觉采用 **浅色实验台** 方向，核心记忆点是：

> 像一张浅色实验台桌面，中间放着一块深色终端主屏，下面铺开 CPU 观察面板。

具体语言如下：

- **基调颜色：** 暖米白、浅沙色、低饱和灰，避免冷白后台的通用管理台质感。
- **强调色：** 延续现有铜色与青色，但收口为少量高价值强调，用于操作热点、状态提示和局部点亮。
- **终端对比：** `terminal` 保持深色、高对比、带舱体感的容器，以强化主舞台地位。
- **卡片质感：** 使用轻透明浅色面板、细描边、柔和阴影和更稳定的圆角体系，不走厚重玻璃态。
- **排版：** 标题和状态数字更像仪表盘；正文更克制，减弱页面装饰性文案的存在感。

### 交互与状态契约

- 页面增加显式的 `terminal` 展开 / 收起控制。
- `terminal` 收起时：
  - 不销毁 buffer。
  - 不改变 `terminal.connected`、`terminal.pendingInput` 等会话状态。
  - 仅切换 UI 可见层级与网格布局。
- `terminal` 展开时继续保持焦点态、输入提示和状态栏语义。
- 若会话断开、重连失败或本地 debug 服务不可达，页面仍沿用现有 notice / 状态提示合同：
  - 不因为 UI 改造而伪装成“终端已重置”或“guest 已停止”。
  - 终端容器允许保持可见，但要明确呈现当前不可输入、未连接或等待重连的状态。
  - 收起 / 展开 `terminal` 不能隐式触发重连、副作用清空或额外会话切换。
- 对长输出场景，本轮只允许做呈现层优化，不改变增量同步协议：
  - 继续基于现有 `buffer + nextOffset` 语义渲染。
  - 终端收起时不额外复制或重建 buffer，避免因为布局切换放大浏览器端重排成本。
  - 页面不引入更重的虚拟滚动或日志管理系统；若后续出现真实性能问题，再单开专项处理。
- `inspector` 的折叠组保留现有行为，但视觉反馈要与新的分组层级统一。
- 控制按钮建立主次关系：`Load / Run / Pause` 优先级更高，`Step` 与视图控制次一级，避免整排按钮权重相同。

### 响应式与布局约束

- 桌面宽屏仍以“终端主舞台 + 右侧轻量观察区 + 下方 Inspector”作为默认展开形态。
- 在中等宽度下，主舞台层允许退化为上下堆叠，但仍应保持控制区、notice 和主操作按钮始终先于深度面板出现。
- 在窄屏或较小浏览器窗口下：
  - 不追求把所有观察面板同时常显在首屏。
  - `Inspector` 应优先退化为单列或更疏的网格，避免卡片宽度被压缩到不可读。
  - `terminal` 收起入口、运行控制和核心状态摘要必须保持可见且可点击，不被次要面板挤出首屏。
- 本轮只要求做到“窄屏可用、层级不乱”，不额外扩展为移动端产品级适配。

### 模块边界

本轮仍沿现有文件边界收口，不做无关抽象扩张：

- `frontend/app/index.html`
  - 负责页面骨架重排，体现 Hero / 控制带 / 主舞台 / Inspector 的结构层级。
- `frontend/app/styles.css`
  - 负责颜色、间距、面板体系、按钮层级、响应式网格和 `terminal collapsed` 视图切换样式。
- `frontend/app/render.js`
  - 继续承担总装配职责，同时根据布局状态切换 `terminal` 与 `inspector` 的主次排布。
- `frontend/app/components/terminal.js`
  - 保持 `terminal` 容器渲染，但增强其标题栏、状态栏和折叠态挂载语义。
- `frontend/app/components/panels.js`
  - 强化 `inspector` 的分组表达，使视觉分区和调试分区保持一致。
- `frontend/app/state.js`
  - 仅在必要时增加最小 UI 布局状态，如 `terminalCollapsed`；不改变既有调试协议状态。

### 验证思路

本轮主要是浏览器前端的结构与样式重排，验证应聚焦以下几层：

1. **静态结构验证**
   - 页面骨架是否符合“Hero / 控制带 / 主舞台 / Inspector”四层结构。
   - `terminal` 展开 / 收起时是否能正确切换主视区。

2. **状态与交互验证**
   - `terminal` 收起后重新展开，buffer 和会话状态保持连续。
   - 焦点、提示文案、按钮禁用态和通知区仍与当前状态一致。
   - `functional / pipeline` 切换、`Load / Run / Pause / Step / Reset` 行为不受 UI 重排影响。

3. **前端测试基线**
   - `cd frontend && node --test`
   - 如涉及前端运行逻辑或 debug server 集成，再额外关注：
     - `cd myCPU && make test-host-debug_cli_smoke`
     - `cd myCPU && make test-host-interactive_terminal_smoke`

4. **人工体验验证**
   - 至少对“依赖终端输入的场景”和“只看状态快照的场景”各做一轮实际浏览。
   - 重点确认页面主次是否稳定、信息是否更易扫读，以及 `terminal` 收起后 `inspector` 是否真正变得更好用。

## 风险与取舍

- 选择“结构重排 + 样式系统收口”，可以一次解决层级、质感和交互问题，但改动面会大于纯 CSS 美化。
- 保持原生 HTML / CSS / ESM 架构，有利于沿现有仓库边界小步推进，但也意味着布局表达需要更克制，避免为了 UI 改造引入过度抽象。
- `terminal` 可收起会显著提升“非交互测试”场景体验，但如果控制入口和状态反馈做得不够明确，可能让用户误以为终端会话被重置，因此交互提示必须清晰。
- `inspector` 重新分组后，信息路径会更清晰，但必须避免为分组而牺牲现有关键快照的可见性。
- 当前项目对 `debug/frontend` 的定位仍是“教学演示可用”，因此本轮设计应以可读、可维护、可验证为主，不追求额外的产品化功能面。

## 当前有效性说明

- 当前有效 / 历史语境：当前有效，记录这一轮 `debug/frontend` UI 美化的设计边界与用户确认方向。
- 当前实现进展与后续执行结果以 [status/mainline_status.md](../status/mainline_status.md) 和 [status/project_priority_roadmap.md](../status/project_priority_roadmap.md) 为准。