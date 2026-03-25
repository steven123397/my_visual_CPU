# 实时流水线可视化调试前端设计

## 1. 文档状态

- 日期：2026-03-25
- 状态：已完成头脑风暴与分节确认，待进入实现计划
- 目标读者：项目维护者、后续实现者

## 2. 背景

当前仓库已经具备可运行的 RISC-V 模拟器原型，并且已经有：

- `functional` 参考后端
- 增量完成的 `pipeline` 后端
- `Machine + Bus + Ram + UART + CLINT + PLIC + SimpleStorage` 平台骨架
- 现成的汇编回归测试集与 `guest/supervisor_demo.elf`

但当前仓库还没有以下能力：

- 前端栈
- 面向浏览器的调试协议
- 可单步驱动的调试会话层
- 可直接提供给 UI 的流水线/设备状态快照

因此，这个需求的关键不只是“画一个界面”，而是把当前模拟器补成一个可以被前端稳定控制和观测的本地调试系统。

## 3. 目标

本设计的目标是新增一个本地实时调试前端，使用户可以：

- 选择项目中的现有测试程序并加载运行
- 实时观察 5 级流水线在运行过程中的状态变化
- 进行 `Run`、`Pause`、`Reset`、`Step Cycle`、`Step Commit`
- 查看关键架构态：
  - `PC`
  - `cycle`
  - `instret`
  - `GPR`
  - 关键 `CSR`
  - 当前特权级
- 查看关键微结构与平台状态：
  - IF/ID/EX/MEM/WB 各级占用
  - `stall` / `flush` / `redirect`
  - 最近一次访存或 MMIO 访问
  - `UART` / `CLINT` / `PLIC` / `SimpleStorage` 状态
  - `trap` / `interrupt` / `commit` / hazard 事件

## 4. 非目标

首版不包含以下内容：

- 断点、watchpoint、条件暂停
- 超长历史波形录制
- 多会话并发调试
- `functional` 与 `pipeline` 并排差分可视化
- 任意 ELF/BIN 文件浏览器与工作区管理
- 桌面壳应用（Tauri / Electron）
- 为了前端而改写现有 ISA 语义或破坏参考执行路径

## 5. 方案选择

已确认采用的方向是：

- 浏览器前端 + 本地 Debug Server + Machine Debug Session

不采用的方向：

- 单纯离线 trace 回放
- 围绕现有 CLI 做脆弱的文本输出抓取
- 首版直接做桌面壳应用

选择该方案的原因：

1. 用户明确希望优先得到“实时调试”能力，而不是离线回放。
2. 当前模拟器已有明确的模块边界，适合继续补“控制”和“观测”接口。
3. 该方案后续可以自然扩展断点、录制、后端切换，而不需要推倒重来。

## 6. 总体架构

推荐架构如下：

1. 浏览器前端负责展示与交互，不直接持有模拟器内部状态。
2. 本地 Debug Server 负责：
   - 创建和管理调试会话
   - 接收前端控制命令
   - 将内部快照序列化为统一 JSON
   - 通过 `HTTP + WebSocket` 向前端暴露接口
3. `Machine Debug Session` 负责：
   - 加载测试镜像
   - 驱动 `Machine`
   - 提供 `step_cycle` / `step_commit` / `run` / `pause` / `reset`
   - 组装统一快照
4. 底层仍然是现有模拟器模块：
   - `Machine`
   - `CPU`
   - `ExecutionBackend`
   - `PipelineBackend`
   - `Bus`
   - `UART` / `CLINT` / `PLIC` / `SimpleStorage`

核心原则：

- 前端不参与模拟逻辑
- 调试层不改变 ISA 语义
- 保持 `functional` 参考路径作为架构真值来源
- 首版完整服务 `pipeline` 可视化，架构上预留未来接入 `functional`

## 7. 模块设计

### 7.1 模拟器侧新增边界

建议新增 `myCPU/src/debug/` 模块，至少包含：

- `debug_snapshot.h`
  - 统一定义前端消费的快照结构
- `debug_events.h`
  - 定义 `commit`、`stall`、`flush`、`redirect`、`trap`、`interrupt` 等事件
- `debug_session.h/.cpp`
  - 持有单个调试会话状态
  - 提供控制命令与快照导出
- `debug_server.*`
  - 负责本地服务与协议适配

对现有模块的建议修改：

- `Machine`
  - 从“只能 `run()` 跑到底”扩展为“可由调试层单步驱动”
  - 暴露有限的只读状态访问接口
- `ExecutionBackend`
  - 维持 `step()` 基础语义
  - 为调试层提供可选的状态快照接口
- `PipelineBackend`
  - 暴露 IF/ID/EX/MEM/WB 的稳定只读快照
  - 暴露本周期 `stall` / `flush` / `redirect` / forwarding 相关标记
- 设备对象
  - 增加轻量级状态导出接口，不改变 MMIO 行为

### 7.2 前端模块

建议在仓库根目录新增 `frontend/`，独立放置浏览器前端工程。

前端建议包含：

- `src/app/`
  - 全局应用状态、WebSocket 连接、命令发送
- `src/panels/`
  - `ControlPanel`
  - `PipelinePanel`
  - `TimelinePanel`
  - `RegistersPanel`
  - `CsrTrapPanel`
  - `BusPanel`
  - `DevicesPanel`
  - `EventsPanel`
- `src/types/`
  - 调试协议和快照类型定义

前端职责：

- 维护当前会话与当前快照
- 发出控制命令
- 渲染和高亮变化
- 控制时间线窗口与展示密度

前端不负责：

- 推断流水线语义
- 解释 Trap 行为
- 决定单步逻辑

## 8. 运行与数据流

### 8.1 单次 `Step Cycle`

`Step Cycle` 的执行流为：

1. 前端发送 `stepCycle`
2. Debug Server 将命令转给 `Machine Debug Session`
3. Session 驱动后端执行 1 个 cycle
4. Session 收集：
   - 架构态
   - 流水线阶段寄存器
   - 本周期事件
   - 最近访存/MMIO
   - 设备状态
5. Server 输出统一快照并推送给前端

### 8.2 单次 `Step Commit`

`Step Commit` 的语义定义为：

- 从当前状态开始连续执行若干个 cycle
- 直到满足以下任一条件即停止：
  - `instret` 增加
  - CPU `halted`
  - 调试会话进入异常停止条件

这样处理的原因是：

- 对 `pipeline` 来说，1 次架构提交未必等于 1 个 cycle
- 对 `functional` 来说，1 次 `step()` 天然就是 1 次提交

因此，`step_commit` 可以在调试层统一定义，而不需要把两种后端硬捏成同一套内部实现。

### 8.3 连续运行

`Run` 不应该对浏览器逐 cycle 无限推送。

首版推荐：

- Debug Session 在后台批量推进若干个 cycle
- 按固定刷新频率向前端推送最新快照，例如 10 到 30 Hz
- 前端保留最近 N 个 cycle 的时间线窗口

这样可以避免：

- WebSocket 数据量失控
- UI 重绘过重
- 连续运行时页面明显卡顿

### 8.4 协议草案

首版协议建议拆成两类：

- `HTTP`
  - 用于低频控制命令和会话初始化
- `WebSocket`
  - 用于状态推送和运行期事件流

建议的首版接口：

- `GET /api/tests`
  - 返回测试清单
- `POST /api/session/load`
  - 输入测试名、后端类型、附加参数
- `POST /api/session/run`
  - 开始连续运行
- `POST /api/session/pause`
  - 暂停
- `POST /api/session/reset`
  - 重置当前会话
- `POST /api/session/step-cycle`
  - 执行 1 个 cycle
- `POST /api/session/step-commit`
  - 执行到下一个提交边界
- `GET /api/session/snapshot`
  - 拉取当前快照（用于初始同步或异常恢复）
- `WS /ws`
  - 推送最新快照、运行状态变更、错误事件

协议原则：

- 控制命令是显式动作
- 浏览器不依赖模拟器标准输出
- WebSocket 推送的是结构化状态，而不是格式化文本

## 9. 测试加载模型

首版调试器不做任意文件选择器，直接复用仓库内已有测试资产。

推荐可选来源：

- `myCPU/tests/asm/*.elf`
- `myCPU/guest/supervisor_demo.elf`

加载模型建议使用显式测试清单，而不是解析 `Makefile`：

- 每个测试项明确给出：
  - 名称
  - 镜像路径
  - 是否需要磁盘镜像
  - 默认附带参数

原因：

- `Makefile` 适合构建与回归验证，不适合作为前端运行时协议来源
- 显式清单更稳定，也更便于未来增加 demo、过滤不适合实时展示的测试

## 10. 快照数据模型

首版快照建议至少包含以下字段：

```json
{
  "session": {
    "test_name": "timer_interrupt",
    "backend": "pipeline",
    "run_state": "paused"
  },
  "summary": {
    "cycle": 482,
    "instret": 117,
    "pc": "0x8000007c",
    "privilege": "S",
    "halted": false
  },
  "pipeline": {
    "if": {},
    "id": {},
    "ex": {},
    "mem": {},
    "wb": {},
    "flags": {
      "stalled": true,
      "flushed": false,
      "redirected": false
    }
  },
  "registers": {
    "gpr": []
  },
  "csrs": {},
  "bus": {
    "last_access": {}
  },
  "devices": {
    "uart": {},
    "clint": {},
    "plic": {},
    "storage": {}
  },
  "events": []
}
```

其中字段含义如下：

- `session`
  - 当前测试、后端、运行状态
- `summary`
  - 顶部摘要栏最关心的架构态
- `pipeline`
  - 5 级阶段占用和控制标记
- `registers.gpr`
  - 32 个通用寄存器值，前端负责高亮本次变化
- `csrs`
  - 首版聚焦关键 CSR：
    - `mstatus`
    - `sstatus`
    - `mepc`
    - `sepc`
    - `mcause`
    - `scause`
    - `mie`
    - `mip`
    - `sie`
    - `sip`
    - `satp`
- `bus.last_access`
  - 最近一次访存或 MMIO 访问摘要
- `devices`
  - 平台设备当前快照
- `events`
  - 当前 cycle 或最近一小段时间内的重要事件

## 11. 设备状态边界

首版建议展示如下设备状态：

### 11.1 UART

- `ier`
- 中断线当前是否拉高
- 最近一次输出字符（如果有）

### 11.2 CLINT

- `mtime`
- `mtimecmp`
- 定时器是否触发

### 11.3 PLIC

- `pending`
- `claimed`
- machine/supervisor context 的 `enable` 与 `threshold`

### 11.4 SimpleStorage

- 是否已附加镜像
- `status`
- `lba`
- `block_count`
- `error`

设备状态导出接口必须保持“只读观察”，不能和设备 MMIO 语义耦合在一起。

## 12. 前端信息架构

已确认的主工作台布局如下：

### 12.1 顶部控制区

- 测试选择
- 后端选择
- `Load`
- `Run`
- `Pause`
- `Step Cycle`
- `Step Commit`
- `Reset`

### 12.2 中部主区域

左侧为核心工作区：

- 当前周期 5 级流水线概览
- 最近 N 个 cycle 的流水线时间线

右侧为辅助工作区：

- 事件流
- 设备状态
- 运行摘要

### 12.3 底部状态区

- GPR 面板
- CSR / Trap 面板
- 访存 / 总线窗口

信息主次关系固定为：

1. 当前发生了什么
2. 最近几拍发生了什么
3. 架构态和设备态为什么会变成这样

## 13. 第一期开工范围

已确认的首版范围如下：

- 完整支持 `pipeline` 后端可视化
- 同时提供 `Step Cycle` 和 `Step Commit`
- 默认主视角围绕 `Step Cycle`
- 直接复用现有测试集
- 展示流水线、寄存器、关键 CSR、Trap/Interrupt、设备状态、最近一次总线访问

首版不把以下内容纳入交付门槛：

- `functional` 完整 UI 切换
- 差分对比界面
- 断点
- 任意镜像文件浏览器

但架构需要预留：

- 后续接入 `functional`
- 后续增加录制与差分
- 后续加入更强的调试控制能力

## 14. 实现约束与工程注意点

### 14.1 不破坏现有参考路径

任何调试能力新增都不能让：

- `functional` 参考后端失去可读性
- `pipeline` 后端混入大量 UI 专用逻辑
- 现有回归测试失效

### 14.2 状态导出优先走只读快照

不要让前端直接读取内部可变对象，更不要让 UI 依赖内部实现细节。

正确方向是：

- 由调试层组装稳定快照
- 前端只消费稳定协议

### 14.3 数据量控制

实时模式下，最大风险不是“能不能显示”，而是“会不会刷爆”。

因此首版必须内建：

- 运行时批量推进
- 限频推送
- 时间线窗口裁剪
- 事件窗口裁剪

### 14.4 `.superpowers/` 不应进入版本库

头脑风暴视觉原型输出目录位于 `.superpowers/`。

该目录属于本地协作产物，不应纳入最终功能提交；必要时应加入 `.gitignore`。

## 15. 验证策略

首版至少需要以下验证：

### 15.1 模拟器侧

- Debug Session 的加载、重置、单步、运行、暂停行为
- `step_commit` 在 `pipeline` 下的停止语义
- 快照字段和底层状态的一致性
- 设备状态导出与真实设备寄存器状态一致

### 15.2 前端侧

- WebSocket 断开/重连
- 空会话、已暂停、已停止、已结束状态切换
- 高亮寄存器变化和阶段变化
- 高频刷新下界面可读性

### 15.3 回归保障

新增调试能力后，现有：

- `make test`
- `make test-pipeline`

仍然必须保持可通过。

## 16. 风险与后续扩展

### 16.1 主要风险

- 当前仓库没有现成前端和调试协议，需要从零建立
- `PipelineBackend` 当前没有对外快照边界，需要额外补接口
- 如果连续运行按每拍推送，浏览器和协议都会被数据量拖垮

### 16.2 后续合理扩展

当首版稳定后，可以继续追加：

- `functional` / `pipeline` 切换
- 差分对比视图
- 断点与条件暂停
- 离线 trace 录制与回放
- 更丰富的内存窗口与地址观察

## 17. 结论

该需求的正确落地方向不是“先画一个页面”，而是：

1. 先给现有模拟器补出调试会话层和稳定快照边界
2. 再通过本地 Debug Server 将状态暴露给浏览器前端
3. 首版只做真正高价值、可稳定使用的实时调试能力

按本设计落地后，项目将获得一个面向 `pipeline` 后端的实时可视化调试入口。这既能帮助验证当前五级流水线的行为，也能显著提升后续 OS bring-up、Trap/Interrupt 调试、设备平台观察和教学展示的可读性。
