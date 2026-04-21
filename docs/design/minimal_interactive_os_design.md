# `interactive_os` 最小可交互 Monitor 设计

## 文档定位

本文档记录当前 `interactive_os` 这条 guest 交互 demo 的正式边界，作为下列内容的统一参考资料：

- `interactive_os` 在仓库里的定位
- browser terminal、Node 调试服务、UART 与 guest monitor 之间的输入输出链路
- 当前 monitor 的命令面、输入模型和验证口径

本文档只保留当前仍然有效的设计边界，不再按“首次接通交互链路时怎么切”维护过程性设计。

## 关联文档

- 状态文档：
  - [../status/mainline_status.md](../status/mainline_status.md)
  - [../status/project_priority_roadmap.md](../status/project_priority_roadmap.md)
  - [../status/kernel_alpha_status.md](../status/kernel_alpha_status.md)
- 相关设计：
  - [debug_frontend_integration.md](debug_frontend_integration.md)
  - [platform_mmio_contract.md](platform_mmio_contract.md)

## 背景与问题

当前仓库已经具备完整的 guest bring-up、`debug/frontend` 教学演示链路，以及可双向工作的 terminal 输入输出路径。对当前项目而言，`interactive_os` 的价值不在于“做一个新的产品级 OS”，而在于提供一条正式、稳定、可回归的交互 demo：

- guest 侧通过最小 monitor 提供 UART 文本交互面
- host / frontend 侧把这条串口交互包装成浏览器终端体验
- 整条链路可以和 pipeline、平台状态、寄存器视图一起观察

## 目标

- 保留一条稳定的、最小可交互的 guest monitor 路径。
- 明确 host / frontend / guest 三层的职责边界。
- 把当前命令面和输入模型固定为参考资料，便于后续维护和回归。

## 非目标

- 不把 `interactive_os` 扩成图形桌面、窗口系统或应用平台。
- 不把它当成 `kernel_alpha` 的替代主线。
- 不引入文件系统、网络栈、用户态应用模型或长期 ABI 承诺。
- 不为了这条 demo 重写当前 `debug/frontend` 或平台 MMIO 契约。

## 当前统一设计边界

### 1. 整体分层

当前 `interactive_os` 的正式分层如下：

```text
browser terminal
  -> frontend/server
  -> mycpu --debug-cli
  -> DebugSession / Machine
  -> Uart16550 RX/TX
  -> guest interactive_os
       -> kernel_runtime / console / monitor
```

其中：

- 浏览器端负责 terminal 呈现、焦点和输入发送。
- Node 服务负责 session 与 UART 输入输出桥接。
- simulator 负责 UART 设备与 `debug-cli` 协议。
- guest monitor 负责 prompt、回显、命令解析和最小只读调试能力。

### 2. guest 侧定位

当前 `interactive_os` 是一条独立 demo 路径，而不是新的“大而全 runtime 框架”：

- 复用 `kernel_runtime`、`console`、`monitor`、`vm_debug` 等已有基础设施。
- 只承担“最小 monitor 内核”的职责。
- 不承载 `kernel_alpha` 的 bring-up 合同，也不取代 `kernel_alpha` 的冻结基线角色。

### 3. 当前输入输出合同

当前交互模型继续收口到“串口文本流”：

- guest 输入来自 UART RX
- guest 输出走 UART TX
- host 侧通过专门 terminal API / session 状态按 offset 拉取输出增量
- 首版保证可见 ASCII、`Enter`、`Backspace` 这组最小键集

当前这条链路默认仍是 polling 风格的 monitor 交互，不把“更像真实 UART 的接收中断模型”当作当前前置条件。

### 4. 当前 monitor 命令面

当前 monitor 至少稳定支持以下命令：

- `help`
- `echo`
- `time`
- `uptime`
- `disk info`
- `disk read <lba>`
- `regs`
- `peek <addr>`
- `pagewalk <addr>`
- `pte dump <addr>`
- `halt`

这些命令的定位仍然是 bring-up / 调试 / 教学接口，而不是长期稳定 ABI。后续若要调整输出格式，应优先由回归锁住关键语义，而不是把完整终端文本当作不可变协议。

### 5. 与前端的关系

当前 `interactive_os` 与 `debug/frontend` 的边界如下：

- `interactive_os` 提供 guest 可见交互面。
- `debug/frontend` 提供 terminal 壳层、会话状态和外围观察面。
- terminal 是主舞台之一，但并不替代 pipeline / 寄存器 / 平台状态观察。
- `terminal collapsed` 只改变壳层布局，不改变 guest / session 的真实协议状态。

## 验证思路

当前与这条设计直接相关的正式基线至少包括：

- `cd myCPU && make test`
- `cd myCPU && make test-pipeline`
- `cd myCPU && make test-guest-interactive_os_demo`
- `cd myCPU && make test-host-interactive_terminal_smoke`
- `cd frontend && node --test`

## 风险与取舍

- 当前选择“浏览器 terminal 壳 + guest monitor”而不是 guest 自己驱动图形桌面，是有意收窄范围，以换取更低的虚拟硬件复杂度和更稳定的回归口径。
- 当前 monitor 命令集偏向 bring-up / 调试接口，会牺牲一部分“更像产品”的使用体验，但更符合这条 demo 的目的。
- 当前仍把输入模型保持在最小 ASCII / Enter / Backspace 范围，会限制交互花样，但能显著降低 guest / frontend / protocol 三层一起膨胀的风险。

## 当前有效性说明

- 当前有效：本文档作为 `interactive_os` 当前设计边界的参考资料。
- 当前实时状态与后续是否继续扩交互面，以 [../status/mainline_status.md](../status/mainline_status.md) 与 [../status/project_priority_roadmap.md](../status/project_priority_roadmap.md) 为准。
