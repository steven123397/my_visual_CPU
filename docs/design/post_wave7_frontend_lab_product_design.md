# Post-Wave 7 前端 Lab 产品设计

## 文档定位

本文档记录 `Wave 7` 首轮产品化站点完成之后，前端继续向 `Lab product` 方向重构时的当前有效设计边界。

它回答：

- 为什么现有 `/console` 还不足以承载 `Post-Wave 7` 的 Linux 发行版和 AI 主线。
- 浏览器前端应该以什么产品心智组织底层能力。
- `/console` 应如何从 `demo workspace v1` 演进为统一的 `Lab workbench`。
- Linux / AI / Machine / Runtime 几类场景如何在同一工作台内共存。

本文档不记录执行 checklist。具体实施步骤写入 `docs/plan/`，当前状态以
[../status/mainline_status.md](../status/mainline_status.md) 为准。

## 关联文档

- 状态文档：
  - [../status/mainline_status.md](../status/mainline_status.md)
- 相关计划：
  - [../plan/history_plan.md#post-wave7-frontend-lab-product-plan](../plan/history_plan.md#post-wave7-frontend-lab-product-plan)
- 相关设计：
  - [wave7_productization_and_showcase_design.md](wave7_productization_and_showcase_design.md)
  - [post_wave7_linux_distribution_platform_design.md](post_wave7_linux_distribution_platform_design.md)
  - [debug_frontend_integration.md](debug_frontend_integration.md)

## 背景与问题

`Wave 7` 首轮产品化已经把站点壳层拆成 `/`、`/console`、`/docs` 三个入口，并把
`/console` 从纯调试页整理成 `demo workspace v1`。这一步已经足够支撑已有 demo 的公开访问，
但它仍然沿用“按 demo 卡片选工作负载，再把已有 inspector 拼出来”的组织方式。

这个结构在 `interactive_os`、`xv6`、AI demo、vector demo 这些单点体验上还能工作，
但面对 `Post-Wave 7` 两条新主线时已经出现结构性不足：

- 对 Linux 发行版平台来说，单个 demo 卡片不足以承载 `boot -> shell -> process ->
  filesystem -> ISA/platform` 的证据阶梯。
- 对 AI 用户任务 / NPU 性能模型来说，单个 demo 卡片也不足以同时承载 `task entry ->
  profile -> runtime counters -> boundary explanation`。
- 对整个平台来说，当前 `/console` 更像“demo 目录 + 状态面板”，而不是“用户进入一个 lab，
  启动实验、操作它、观察它、理解它”。

因此，`Post-Wave 7` 的前端重心不应继续扩首页叙事，而应把 `/console` 正式重构成
`Lab workbench`。

## 目标

- 把 `/console` 的产品心智从 `demo workspace` 提升为 `Lab workbench`。
- 用统一的信息架构承载 `System / Linux / AI / Machine / Runtime` 五类实验主题。
- 让每个场景都同时提供四类内容：`Run`、`Control`、`Inspect`、`Explain`。
- 把 Linux 发行版和 AI 用户任务两条主线做成可持续扩展的 `lab family`，而不是一次性卡片。
- 保持现有 `/console` 路由、Node debug server、terminal session 和 snapshot 合同不变，
  前端只重构组织方式和展示层。
- 当前切片优先补强 `Session Bar / Inspector Stack / Evidence / Boundary Drawer`，
  先把工作台本身做完整，再单独展开 Linux 发行版专题入口。

## 非目标

- 不重做 `/` 首页的完整叙事结构；首页在本轮只保持轻量入口角色。
- 不在本轮改动 simulator 执行语义、debug CLI 协议或 workload manifest 的基础格式。
- 不在本轮引入新的前端框架、SSR、状态库或设计系统。
- 不把 Linux lab 夸大为“完整云主机终端”，也不把 AI lab 夸大为“任意模型上传平台”。
- 不把 `/docs` 变成实时状态来源；工程真相仍由 `design / plan / status` 体系承接。

## 约束与边界

- `/console` 仍必须兼容现有 `Load / Run / Pause / Reset / Terminate` 会话流转。
- terminal、snapshot、profile、diagnostics 都必须来自真实后端响应，浏览器不能伪造执行结果。
- Linux 发行版相关页面必须显式展示当前边界，不得把 `gated route`、外部资产依赖和
  未完成 capability 隐去。
- AI lab 相关页面必须坚持白名单模板 / 受限 task spec / simulated cycles 口径，不得暗示
  任意模型导入；AI 专题扩写需等待独立 AI 主线继续推进，不在当前切片展开。
- 风格上延续现有 `工程纸面 + 精密控制台` 方向，但 `/console` 的布局和模块层级允许重构。

## 方案

### 结构设计

前端产品的最小单位不再是“组件”或“单个 demo 卡片”，而是一个 `Lab Scenario`。

一个 `Lab Scenario` 必须同时回答四个问题：

1. 这是什么实验？
2. 用户现在能操作什么？
3. 用户应该关注什么现象？
4. 这说明了什么能力边界？

基于这个心智，`/console` 固定为四区工作台：

```text
Lab Workbench
  -> Lab Navigator
  -> Session Bar
  -> Primary Stage
  -> Inspector Stack
  -> Evidence / Boundary Drawer
```

#### 1. Lab Navigator

左侧导航不再只展示一组 demo 卡片，而是按实验主题分组：

- `System Labs`
  - interactive_os
  - supervisor / xv6
  - Linux serial
- `Linux Distro Labs`
  - Alpine shell
  - Debian shell
  - capability / filesystem / process / ISA
- `Machine Labs`
  - pipeline
  - registers / CSR
  - memory / bus / devices
- `AI Labs`
  - AI accelerator demo
  - parameterized tiny model
  - task-spec-backed workloads
- `Runtime Labs`
  - vector CNN
  - L1D / shadow cache
  - JIT / DBT runtime stats

导航卡片本身要包含：

- lab family
- scenario 名称
- 这条场景想证明什么
- 当前 ready / gated / planned 状态

#### 2. Session Bar

顶部会话条统一承载当前实验的运行上下文：

- 当前 lab / scenario
- backend
- asset readiness
- session run state
- 主操作按钮

它负责把“选场景”和“开会话”收成一条产品主路径，而不是把选择器散落在页面中部。

#### 3. Primary Stage

中央主舞台只展示当前场景最重要的 live 内容：

- Linux / OS 场景：terminal
- Machine 场景：pipeline timeline / registers focus
- AI 场景：profile result + runtime output
- Runtime 场景：vector / JIT / cache 观察面

一个时刻只强调一个主舞台，避免把所有 inspector 平铺成噪音。

#### 4. Inspector Stack

右侧 inspector 改成“按场景裁剪”的观察栈，而不是固定全开面板。

每类场景有默认关注面：

- `System / Linux`
  - summary
  - workload contract
  - platform / devices
- `Machine`
  - registers / CSR
  - events / bus
  - pipeline side notes
- `AI`
  - accelerator counters
  - output / expected
  - shape / profile summary
- `Runtime`
  - vector registers
  - cache counters
  - JIT stats

#### 5. Evidence / Boundary Drawer

底部证据区专门负责“解释层”，统一容纳：

- expected marker
- what this proves
- current boundary
- required assets
- next observation hints

这部分是 Linux 和 AI 两条主线最需要补强的产品层：不能再要求用户自己去猜 demo 说明。

### 场景族设计

#### Linux Distro Labs

Linux 不再只是一个 `Linux Serial Console` 卡片，而是一组渐进式场景：

- `Linux Serial`
  - 最快进入 live terminal 的入口
- `Distro Shell`
  - Alpine / Debian 的外部资产场景
- `Process & Filesystem`
  - process control、filesystem persistence、FS state roundtrip
- `Capability & ISA`
  - `riscv,isa`、`hwcap`、FP / FCSR / capability 收口

这些场景共享同一个工作台骨架，但各自有不同的主舞台和证据说明。

#### AI Labs

AI 线同样拆成统一的实验族：

- `AI Accelerator Demo`
  - guest MMIO 闭环
- `Parameterized Tiny Model`
  - 白名单模板、runtime shape、profile 输出
- `Task Spec Workloads`
  - 受限 importer、bounded dynamic GEMM / CNN
- `Performance Model`
  - DMA / compute / stall / utilization 观察面

AI lab 的重点不是做模型市场，而是让用户能把“任务入口、执行结果、性能解释、边界限制”放在同一视图中理解。
但当前这一轮前端重构不会继续扩 AI Labs 的新专题内容，只保留现有已落地展示入口。

### 接口 / 数据 / 契约

- 现有 `tests manifest` 继续作为场景发现来源，但前端需要在渲染层补充
  `lab family / scenario brief / inspector focus / evidence copy` 的 curated metadata。
- `loadedSession`、`loadProgress`、terminal buffer、snapshot history 和 diagnostics 继续复用现有
  state 合同，不新增并行事实来源。
- AI tiny model 继续依赖服务器端模板与结果返回；前端只重构显示方式和解释层。
- Linux 相关 gated diagnostics 继续使用 `diagnostics.linuxConsole`，但要进入更明确的
  `asset readiness` 展示位。
- 当前阶段允许在浏览器端维护一份 `scenario catalog`，但它只能补充展示元数据，不得替代
  manifest / snapshot / diagnostics 的真实来源。

### 验证思路

文档与前端渲染层改动至少运行：

- `git diff --check`
- `cd frontend && node --test`

如果本轮涉及 debug server / session / manifest 语义变化，还需继续守住：

- `cd myCPU && make test-host-debug_cli_smoke`
- `cd myCPU && make test-host-interactive_terminal_smoke`

如果在新 worktree 中运行 `frontend` 测试遇到真实 e2e `ENOENT`，需要区分：

- 渲染 / 单测回归
- worktree 内缺少 `myCPU/mycpu` 可执行文件导致的环境性失败

不能把后者误判为前端逻辑回归。

## 风险与取舍

- 如果只改视觉、不改信息架构，Linux / AI 主线仍会被压扁成若干卡片，无法形成完整产品体验。
- 如果一次性重写所有 panel，风险会过大；因此应先重构工作台骨架，再逐步替换各场景内容。
- 继续保留现有 manifest 与 snapshot 合同会限制前端自由度，但这是避免前端自造事实来源的必要取舍。
- Linux 和 AI 的解释层若写得太重，会让工作台退化成文档页；因此 evidence 区必须短、硬、可扫描。

## 当前有效性说明

- 当前有效：本文档作为 `Post-Wave 7` 前端从 `demo workspace v1` 走向 `Lab workbench`
  的设计入口。
- 当前状态以 [../status/mainline_status.md](../status/mainline_status.md) 为准。
