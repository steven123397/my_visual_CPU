# Debug / Frontend 统一设计

## 文档定位

本文档是当前仓库关于 `debug_session / protocol + frontend` 的统一设计来源，用于吸收此前分散维护的：

- `debug/frontend` 正式接入边界
- 浏览器壳层 UI 收口
- 向量 / 神经网络教学可视化

它重点回答：

- 当前本地调试链路的正式边界是什么
- 浏览器端的页面结构、状态语义和交互边界是什么
- `vector / NN` 如何在前端中以教学方式只读展示
- 这条线当前明确不做哪些功能

本文档不承担实时进度更新。当前实现状态、验证入口和后续优先级，以对应 `status` 文档与 README 为准。

## 关联文档

- 状态文档：
  - [../status/mainline_status.md](../status/mainline_status.md)
  - [../status/project_priority_roadmap.md](../status/project_priority_roadmap.md)
- 相关设计：
  - [minimal_interactive_os_design.md](minimal_interactive_os_design.md)
  - [vector_ml_workload_direction_design.md](vector_ml_workload_direction_design.md)
  - [platform_mmio_contract.md](platform_mmio_contract.md)
- 已完成计划归档：
  - [../plan/history_plan.md#p1-debug-frontend-boundary-refinement-plan](../plan/history_plan.md#p1-debug-frontend-boundary-refinement-plan)
  - [../plan/history_plan.md#vector-frontend-visualization-plan](../plan/history_plan.md#vector-frontend-visualization-plan)

## 背景与问题

当前仓库已经是一个已可运行的模拟器原型。`pipeline core`、`debug_session/protocol`、本地 Node 调试服务和浏览器前端都已经正式接入主线；浏览器端不再只是旧分支遗留资产，而是当前主线维护的一部分。

与此同时，前端的目标也已经很明确：它服务于本地教学演示、实验观察和最小调试，而不是新的通用调试器产品线。此前这条线被拆成“正式接入”“UI 刷新”“向量 / CNN 可视化”三份设计文档，在阶段推进时有价值；但当前这些能力都已经落地，继续拆开维护会让长期边界重复。

因此，当前更健康的做法，是把 `debug/frontend` 的长期有效设计收口到一份统一文档中：既保留当前架构链路，也把 UI 壳层和向量 / NN 教学视图纳入同一事实来源。

## 目标

- 把 `debug_session / protocol + frontend` 的长期有效边界收口成一份统一设计文档。
- 保持浏览器端继续服务“教学演示可用”的定位，而不是扩成通用调试器。
- 明确当前前端的统一页面骨架、关键状态语义和 `terminal collapsed` 交互边界。
- 明确当前可视化范围，包括 `pipeline` 摘要、平台状态和 `vector / NN` 教学视图。
- 保持前端所有新增展示都建立在现有只读快照与 manifest 元信息之上，不额外发明第二套执行语义。

## 非目标

- 不引入断点、条件暂停、差分视图、任意文件上传或任意表达式求值。
- 不修改 `functional / pipeline` 的执行语义。
- 不为了浏览器 UI 去重写 `Machine`、backend 或 guest 合同。
- 不把当前前端扩成 profiler、trace studio、waveform viewer 或通用模型可视化器。
- 不迁移到 React、Vue 或其他前端框架；继续保留原生 HTML / CSS / ESM。

## 统一设计边界

### 1. 调试链路架构

当前正式调试链路如下：

```text
browser
  -> frontend/app/*
  -> frontend/server/debug_server.mjs
      -> debug_server_runtime.mjs
           -> debug_cli_session.mjs
                -> mycpu --debug-cli
                     -> DebugSession
                     -> Machine
                     -> ExecutionBackend::debug_snapshot()
```

其中：

- `DebugSession` 负责 `load / snapshot / step / reset` 等最小调试控制。
- `debug_protocol` 继续保持单行 JSON 的 `--debug-cli` 协议。
- `frontend/server/debug_server.mjs` 负责静态文件服务、HTTP API 与 WebSocket 广播。
- `debug_server_runtime.mjs` 负责 session queue、run loop、generation guard 和 terminal 状态聚合。
- `debug_cli_session.mjs` 负责 `mycpu --debug-cli` 子进程生命周期与 fail-closed timeout 合同。
- 浏览器端只负责状态管理和视图呈现，不直接解释执行语义。

### 2. 最小调试数据面

当前前端建立在统一只读快照之上，主要包括：

- backend 侧：
  - `name()`
  - `debug_snapshot()`
- `Machine` 侧：
  - `cpu()`、`bus()`、`uart()`、`clint()`、`plic()`、`storage()`、`backend()`
  - `loaded()`、`reset_loaded_image()`、局部调试用 reset helper
- `Bus / 设备` introspection：
  - 最近一次总线访问
  - UART / CLINT / PLIC / Storage 的最小只读状态
- `DebugSnapshot`：
  - 五级流水线、`stall_reason`、最小 `ROB / LSQ` 观测
  - 标量寄存器、关键 CSR / trap 摘要
  - 平台与 I/O 状态
  - 当前已落地的向量状态：`SEW / VL + v0..v31 raw dump`

这组数据面的原则是：只暴露当前仓库里真实、稳定、可解释的状态，不为了 UI 便利发明第二套推导数据。

### 3. 前端页面骨架

当前浏览器壳层收口为 4 层结构：

1. **Hero 总览层**
   - 展示页面定位、当前后端与运行摘要
2. **控制带层**
   - 承载 workload 选择、backend 选择、`Load / Run / Pause / Step / Reset`
   - 提供 `terminal` 展开 / 收起入口
3. **主舞台层**
   - 默认保持“深色终端主舞台 + 轻量观察区”结构
   - 适配依赖 UART / terminal 的演示路径
4. **Inspector 深度观察层**
   - 聚合 `执行观察 / 架构状态 / 平台与 I/O`
   - 在 `terminal` 收起时上升为主视区

这套骨架已经是当前前端的正式布局边界；后续继续修 UI，也应只在这套骨架内做收口，而不是再发明新的页面模型。

### 4. `terminal collapsed` 状态语义

`terminal` 收起当前只影响布局与视觉主次，不改变协议语义：

- 不销毁 buffer
- 不重置 `nextOffset`
- 不改变 `connected / pendingInput / focused` 的真实含义
- 不隐式触发重连、会话切换或 terminal 清空

也就是说，`terminal collapsed` 是视图层状态，不是调试协议状态。页面必须继续如实表达“已连接 / 未连接 / 正在发送输入 / 当前不可交互”的真实情况。

### 5. 当前支持的 workload 与演示入口

前端测试清单当前直接暴露仓库内现有演示入口，包括：

- `tests/asm/*.elf`
- `guest_supervisor_demo`
- `guest_vector_demo`
- `guest_vector_cnn_demo`
- `kernel_alpha_demo` 及其负向 demo
- `guest_interactive_os_demo`

其中 manifest 会提供只读元信息，例如：

- `title`
- `summary`
- `badge`
- `workload`
- 对固定 `vector_cnn_demo` 额外提供 `conv_input / conv_kernel / expected outputs`

这些字段只服务浏览器展示，不参与执行协议。

### 6. 当前统一可视化范围

浏览器端当前统一支持以下几类视图：

- **执行观察**
  - 五级流水线
  - 最近周期时间线
  - `stall_reason`、`lsq_load_state`
  - 最小 `ROB / LSQ` 摘要
- **架构状态**
  - GPR diff
  - 关键 CSR / trap 信息
- **平台与 I/O**
  - 最近一次总线访问
  - UART / CLINT / PLIC / Storage 状态
- **向量 / NN 教学视图**
  - `guest_vector_demo` 与 `guest_vector_cnn_demo` 的 workload 导览
  - 向量指令 `config / memory / ALU` 分类高亮
  - `Vector State`：`SEW / VL + v0..v31`
  - 固定 `conv -> relu` 专题卡
  - 当前 `vector_state_busy` / serializing guard 的边界提示

这里的定位仍然是“当前已落地能力的教学式表达”，不是完整通用调试器。

### 7. 当前 `vector / NN` 教学可视化边界

当前向量 / 神经网络展示严格遵守以下边界：

- 前端只消费只读快照与固定 demo 元信息。
- `functional` 仍是参考语义真值来源；`pipeline` 只暴露自己的真实状态，不复制第二套向量解释器。
- `guest_vector_cnn_demo` 的专题卡只解释固定 `conv -> relu` workload，不扩展到 `Pool / FC`、模型文件加载或 memory hierarchy 图。
- 当前 `pipeline` 的向量边界必须如实表达：
  - non-memory vector ALU 已经脱离统一 serializing fallback
  - `vsetcfg / vle.v / vse.v` 仍然保守 serializing
  - 这不是 lane / latency / vector rename 级模型

## 验证思路

当前这条线至少应持续守住：

- `cd myCPU && make test`
- `cd myCPU && make test-pipeline`
- `cd myCPU && make test-host-debug_cli_smoke`
- `cd frontend && node --test`

如果改动集中在 terminal / runtime / interaction shell，至少额外关注：

- `cd myCPU && make test-host-interactive_terminal_smoke`

如果改动集中在当前向量教学视图，至少额外关注：

- `cd myCPU && make test-host-vector_vlite_smoke`
- `cd myCPU && make test-host-vector_cnn_smoke`

## 风险与取舍

- 保持原生 HTML / CSS / ESM 架构，会让某些布局表达不如框架方案灵活，但更符合当前仓库的小步演进边界。
- 把 UI、integration 和 vector 可视化统一收口到一份设计文档，会让单文档信息量更大，但能减少长期边界重复和链接分散。
- 把前端限制在教学演示和工程调试视角，会让“更像产品”的功能继续缺席，但这正是当前仓库刻意维持的边界。

## 当前有效性说明

- 当前有效 / 历史语境：当前有效，作为 `debug/frontend` 的统一设计边界。
- 当前实现进度、当前优先级和后续是否继续扩浏览器功能面，以 [../status/mainline_status.md](../status/mainline_status.md)、[../status/project_priority_roadmap.md](../status/project_priority_roadmap.md) 与 [../../README.md](../../README.md) 为准。
