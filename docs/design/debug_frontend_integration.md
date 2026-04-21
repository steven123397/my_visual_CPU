# Debug / Frontend 统一设计

## 文档定位

本文档记录当前本地调试链路与浏览器前端的正式边界，作为读者理解以下内容的统一参考资料：

- `debug_session / protocol + frontend` 的当前架构
- 浏览器端当前真正支持的观察面和交互面
- `interactive_os`、向量 demo、`kernel_alpha` 等 workload 如何接入这条链路
- 当前哪些功能明确不做

本文档只维护当前有效的模块边界，不再按“某一轮 UI 刷新”“某一轮向量可视化补丁”分散维护阶段性专题文档。

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

当前仓库的浏览器前端和本地 Node 调试服务已经正式接入主线。对当前项目而言，它们的定位并不是“正在考虑是否合入的新功能”，而是当前已落地、需要长期维护的一条教学演示与工程观察链路。

因此，当前真正需要保留的，不是“某一次前端怎么接进来”，而是下面这些当前仍然有效的模块边界：

- 这条链路如何连到 `mycpu --debug-cli`。
- 浏览器端的数据面来自哪里，哪些是只读快照、哪些是会话级状态。
- `terminal`、snapshot、workload manifest、向量视图、平台状态在体系里分别扮演什么角色。
- 当前刻意不做哪些调试器功能。

## 目标

- 给出 `debug_session / protocol + frontend` 的当前单一设计来源。
- 保持浏览器端继续服务“教学演示可用”和“最小工程调试”定位。
- 明确前端只消费当前稳定、真实、可解释的数据面，不发明第二套执行语义。

## 非目标

- 不把当前前端扩成通用调试器或 IDE。
- 不引入断点、条件暂停、任意文件加载、任意表达式求值、差分视图或 trace studio。
- 不为了 UI 便利重写 backend、guest 或执行语义。
- 不迁移到 React、Vue 或其他前端框架；当前继续保留原生 HTML / CSS / ESM。

## 当前统一设计边界

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
- `debug_server_runtime.mjs` 负责 session queue、run loop、generation guard 和 terminal 状态聚合。
- 浏览器端负责状态管理和视图呈现，不直接解释执行语义。

### 2. 当前数据面

当前前端只建立在 3 类正式数据面之上：

- backend / machine 的只读快照
- workload manifest 的只读元信息
- 前端 / Node 会话自己的运行时状态

当前稳定暴露的数据至少包括：

- 五级流水线、`stall_reason`、最小 `ROB / LSQ` 观测
- GPR diff、关键 CSR / trap 摘要
- 最近一次总线访问
- UART / CLINT / PLIC / Storage 最小状态
- 向量状态：`SEW / VL + v0..v31 raw dump`
- workload 标题、摘要、badge、固定 demo 元信息

原则是：只暴露当前仓库里真实、稳定、可解释的状态，不为了 UI 便利编造第二套推导结果。

### 3. 当前页面骨架

当前浏览器端的长期有效页面结构可以概括为 4 层：

1. `Hero / overview`
2. `Control bar`
3. `Terminal stage`
4. `Inspector`

对应职责如下：

- `Hero / overview`
  - 说明当前 workload、backend 和会话状态
- `Control bar`
  - 提供 workload 选择、backend 选择、`Load / Run / Pause / Step / Reset`
- `Terminal stage`
  - 承载 UART / `interactive_os` 这类终端主舞台
- `Inspector`
  - 承载 pipeline、寄存器、CSR、平台状态和向量观察

后续如果继续修前端，默认也只在这套骨架内收口，而不是再发明新的页面模型。

### 4. `terminal` 与 session 状态语义

当前 `terminal` 是会话级状态，不是 `DebugSnapshot` 的一个附属字符串字段。当前至少维持：

- `buffer`
- `nextOffset`
- `connected`
- `pendingInput`
- `focused`
- `loadedSession`

其中最关键的正式语义是：

- `terminal collapsed` 只改变布局和视觉主次，不改变协议状态。
- `load / reset` 会重置 terminal buffer 与 offset，避免跨 session 串味。
- 浏览器端必须如实表达“未加载 / 未连接 / 输入排队中 / 当前不可交互”等真实状态，不能用壳层文案掩盖实际会话状态。

### 5. 当前支持的 workload 与视图

当前浏览器端直接服务仓库内现有 demo / smoke 入口，包括：

- `tests/asm/*.elf`
- `guest_supervisor_demo`
- `kernel_alpha` 正负 demo
- `guest_interactive_os_demo`
- `guest_vector_demo`
- `guest_vector_cnn_demo`

当前视图面主要分成 4 类：

- 执行观察
  - 五级流水线、最近周期、`stall_reason`、最小 `ROB / LSQ`
- 架构状态
  - GPR diff、关键 CSR / trap
- 平台与 I/O
  - 总线访问、UART / CLINT / PLIC / Storage
- 向量 / NN 教学视图
  - workload 导览、向量指令高亮、`SEW / VL + v0..v31`、固定 `conv -> relu` 专题卡

### 6. 当前只读边界

当前前端的设计底线是：

- 浏览器端只消费只读快照与 manifest 元信息。
- `functional` 仍是 reference 真值来源。
- `pipeline` 只暴露自己的真实状态，不复制第二套语义解释器。
- workload 卡片、向量专题、`interactive_os` 终端都只是当前实现边界的表达，不构成新的执行语义来源。

## 当前明确不做的功能面

当前这条线明确不主动扩张到：

- 断点、条件暂停
- 多客户端 / 多用户调试
- 任意镜像上传与任意文件宿主
- profiler、性能火焰图、通用 trace 下载器
- 通用向量调试器或通用模型可视化器

如果未来要继续扩面，也应优先由真实 bug、真实观测缺口或明确 UI 需求驱动，而不是把前端本身变成新的主线。

## 验证思路

当前与这条链路直接相关的正式基线至少包括：

- `cd myCPU && make test`
- `cd myCPU && make test-pipeline`
- `cd myCPU && make test-host-debug_cli_smoke`
- `cd myCPU && make test-host-interactive_terminal_smoke`
- `cd frontend && node --test`

如果改动集中在当前向量教学视图，还应至少关注：

- `cd myCPU && make test-host-vector_vlite_smoke`
- `cd myCPU && make test-host-vector_cnn_smoke`

## 风险与取舍

- 当前继续保留原生 HTML / CSS / ESM，会让一些复杂 UI 表达不如框架方案灵活，但更符合当前仓库的小步演进方式。
- 当前把前端限制在“教学演示可用 + 最小工程调试”，会让更像产品的功能继续缺席，但这正是当前仓库刻意维持的边界。
- 当前把阶段性前端专题文档统一收口到本文件，会减少一些历史过程细节，但能显著降低旧文档和当前实现脱节后的维护成本。

## 当前有效性说明

- 当前有效：本文档作为 `debug_session / protocol + frontend` 的统一设计边界。
- 当前实时状态与后续优先级，以 [../status/mainline_status.md](../status/mainline_status.md)、[../status/project_priority_roadmap.md](../status/project_priority_roadmap.md) 与 [../../README.md](../../README.md) 为准。
