# 课程 OS UART 中断驱动输入设计

## 文档定位

本文档定义课程 OS shell 的 UART 中断驱动输入边界，对应已归档的
[课程 OS 架构后续增强计划](../plan/history_plan.md#course-os-arch-followup-plan)
任务 2。

本文档只约束 `guest_course_os_shell_demo` / Stage 4 terminal 的输入路径，不改变
`kernel_alpha_demo` 的一次性 Stage 1 / Stage 2 / Stage 3 marker，也不替代
[platform_mmio_contract.md](platform_mmio_contract.md) 中的平台 MMIO 合同。

## 关联文档

- 状态文档：[../status/kernel_alpha_status.md](../status/kernel_alpha_status.md)
- 相关计划归档：[../plan/history_plan.md#course-os-arch-followup-plan](../plan/history_plan.md#course-os-arch-followup-plan)
- 课程 OS 基线设计：[course_os_kernel_alpha_course_os_baseline_design.md](course_os_kernel_alpha_course_os_baseline_design.md)
- 边界设计：[course_os_gap_closure_boundary_design.md](course_os_gap_closure_boundary_design.md)
- 平台 MMIO 合同：[platform_mmio_contract.md](platform_mmio_contract.md)
- 前端 Lab 设计：[post_wave7_frontend_lab_product_design.md](post_wave7_frontend_lab_product_design.md)

## 背景与问题

当前 `guest_course_os_shell_demo` 的输入主循环直接调用 `console_input_poll()`。
该路径通过 `platform_uart_rx_ready()` / `platform_uart_getc()` 轮询 UART RX 字节，
再在 `console_input_poll()` 内完成回显、退格、不可见字符过滤、行完成和溢出判断。
这个实现已经满足 Stage 4 `course-os> ` terminal 展示，但不能证明课程 OS 输入路径已经
接入 PLIC external interrupt。

模拟器侧 UART 设备已经有 RX interrupt 合同：当 `UART_IER_RDI` 打开且 RX FIFO
有数据时，同一个 UART PLIC source 会被拉起。guest 侧目前只有
`platform_uart_enable_thre_irq()` 这条 TX empty interrupt helper，不应复用这个名字来
表达 RX 输入语义。

## 目标

- 保留现有轮询输入 fallback，未显式启用中断模式时 `console_input_poll()` 行为不变。
- 新增中断输入 buffer，让 supervisor external post handler 能把 UART RX 字节放入
  `console_input_state_t`，再由 shell 主循环统一消费。
- 用现有 trap / supervisor runtime 的 external post handler opt-in 机制接入，不把课程
  input 逻辑硬编码进 `trap_dispatch.c` 主流程。
- 保持前端 `/console` terminal 合同不变：前端仍只注入 UART 字节并等待 `course-os> `
  prompt，不新增浏览器 API。
- 通过 host unit 证明 interrupt buffer 能唤醒 shell 输入路径，通过 guest smoke 证明
  Stage 4 prompt 仍可用。

## 非目标

- 不实现完整 TTY、canonical mode、signal、futex、阻塞 sleep / wakeup 或多进程终端会话。
- 不重构 Linux compat stdin / pselect / TTY 语义。
- 不修改 C++ UART / PLIC 设备合同；RX interrupt 设备行为由既有 `uart_rx_contract`
  继续证明。
- 不改变 `kernel_alpha_demo` Stage marker、Stage 4 `course-os> ` prompt 或 Linux
  compat Plus 旁路边界。
- 不让默认 supervisor external interrupt handler 无条件绑定课程 shell 输入状态。

## 约束与边界

- 中断输入是 `course_os_shell` 显式 opt-in 能力；`interactive_os` monitor 和其他 guest
  入口默认继续使用现有轮询路径。
- `PLIC_SOURCE_UART_THRE` 是历史命名的 UART source id。设计上它代表当前平台的 UART
  PLIC source，RX / TX interrupt 通过 UART IIR / LSR / IER 区分，不新增第二个 PLIC source。
- external post handler 只负责 drain UART RX 并写入 raw input FIFO，不解析 shell 命令、
  不调用 `course_shell_run_line()`，也不直接输出命令结果。
- 行编辑、回显、退格、不可见字符过滤、行过长判断仍由 `console_input_poll()` 统一处理。
  中断路径和轮询路径必须进入同一套字节处理函数，避免两份行规程。
- PLIC claim / complete 顺序继续由 `default_supervisor_external_handler()` 管理。课程 post
  handler 必须在 complete 前 drain RX，避免 RX ready level 没被清除导致同一 source
  反复触发。
- 标准 user runtime / Linux compat 进入 U-mode 时，如果未显式传入新的 supervisor timer /
  external post handler，应保留 shell 已安装的 post hook；否则连续运行 `linux ...` 命令后
  UART RX 中断会被 claim / complete 但不再 drain 输入 FIFO。
- 后续实现必须显式安装 supervisor external policy、初始化 PLIC supervisor context、打开
  `UART_IER_RDI`，并确认或补齐 supervisor external interrupt 的 CSR enable helper。

## 方案

### 结构设计

`console_input_state_t` 继续拥有当前行缓冲，同时新增一个固定大小 raw RX FIFO。建议命名为
`CONSOLE_INPUT_RX_BUFFER_SIZE`，默认 256 字节，大于 `CONSOLE_INPUT_MAX_LINE`，足够容纳
一行输入和少量提前键入字符。

`console_input_poll()` 的取字节顺序固定为：

1. 优先从 raw RX FIFO 取一个字节。
2. FIFO 为空时 fallback 到 `platform_uart_rx_ready()` / `platform_uart_getc()`。
3. 所有字节都进入现有行编辑状态机。

`course_os_shell/main.c` 保持 shell 主循环形态不变，只在 bring-up 后执行一次输入中断
安装。建议把安装逻辑封装为本地 helper 或小型 runtime helper，失败时 `panic_shutdown()`，
不要在主循环里散落 PLIC / trap / UART IER 细节。

### 接口 / 数据 / 契约

后续实现的最小接口方向：

- `platform_uart_enable_rx_irq()`：写 `UART_IER_RDI`，表达 RX data available interrupt。
- `console_input_push_interrupt_byte(console_input_state_t* state, uint8_t ch)`：
  把 external post handler 读到的原始字节写入 FIFO；不做过滤、不回显。
- `console_input_drain_uart_rx_interrupt(console_input_state_t* state)`：
  在 UART source external post handler 中循环读取 RX ready 字节并 push 到 FIFO。
- `console_input_supervisor_external_post_handler(uint32_t source_id, void* context)`：
  只接受 UART source，context 必须是 `console_input_state_t*`。
- 如果复用 `supervisor_runtime_interrupt_state_bind_self_handlers()`，shell 入口必须提供
  shell-local adapter：该 runtime 的 self handler context 是 interrupt state，不是
  `console_input_state_t*`，adapter 负责显式转发到 `g_input`。

FIFO 溢出合同：

- FIFO 满时丢弃新输入字节，并设置 raw overflow 标志。
- `console_input_poll()` 观察到 raw overflow 后返回 `CONSOLE_INPUT_OVERFLOW`，由现有 shell
  路径输出 `line too long` 并重置当前行。
- `console_input_init()` 清空当前行和 FIFO；`console_input_reset()` 清空当前行和 overflow
  状态，但不丢弃 FIFO 中已经排队的后续字节。

### 接入顺序

`guest_course_os_shell_demo` 的中断输入安装顺序固定为：

1. `kernel_runtime_run_identity_superpage_bringup()` 完成 K/M/V 基线。
2. `course_os_stage3_prepare_shell()` 完成课程 shell 状态准备。
3. `console_input_init(&g_input)` 清空行状态和 raw FIFO。
4. 安装 supervisor external policy，并绑定 shell-local external post adapter；adapter
   显式把 UART source 转发到 `g_input`。
5. 初始化 PLIC supervisor context，并启用 UART source。
6. 打开 `platform_uart_enable_rx_irq()`。
7. 输出 `course-os shell ready` 和 `course-os> ` prompt，进入现有主循环。

如果步骤 4 到步骤 6 任一步失败，shell demo fail-closed 进入 `panic_shutdown()`；不静默降级为
“看似启用中断但实际仍轮询”。未调用这组安装逻辑的入口仍保留轮询 fallback。

### 验证思路

- 新增 host unit 覆盖 interrupt FIFO：push `echo hi\n` 后，`console_input_poll()` 返回
  `CONSOLE_INPUT_READY`，`state.data` 为 `echo hi`。
- 覆盖 fallback：不 push FIFO、只让 `platform_uart_rx_ready()` 返回数据时，现有轮询路径
  仍能得到同样的行。
- 覆盖共享行规程：interrupt 字节中的退格、不可见字符和超长行与轮询路径结果一致。
- 覆盖 external handler source 过滤：非 UART source 不写 FIFO，UART source 会 drain
  所有 ready RX 字节。
- 覆盖 trap policy 交互：标准 user runtime policy 以 `NULL` post handler 重新安装时，
  不清除 shell 已安装的 supervisor external post hook；host terminal smoke 连续执行
  多条 `linux ...` 命令后仍回到 prompt。
- guest smoke 运行 `cd myCPU && make test-guest-course_os_shell_demo`，证明 `course-os> `
  prompt 和 terminal 闭环保持。
- 如果实现阶段触及 trap / PLIC 共享路径，再运行 `cd myCPU && make test-guest-kernel_alpha_demo`
  和对应 pipeline guest smoke。

## 风险与取舍

- 当前设计选择 raw FIFO 而不是在 interrupt handler 中直接完成行编辑。这样可以避免中断路径
  与轮询路径各维护一份输入规则，但会引入固定容量 FIFO 和溢出合同。
- RX / TX 共享历史命名的 `PLIC_SOURCE_UART_THRE`，文档明确其实际含义为 UART source，避免
  后续实现误以为需要新增 PLIC source。
- 保留轮询 fallback 会让手动展示在中断安装失败前仍有清晰行为，但正式启用中断的 shell demo
  必须 fail-closed，避免虚假声明“中断驱动输入已接入”。

## 当前有效性说明

- 当前有效。
- 本文档对应的当前状态以 [../status/kernel_alpha_status.md](../status/kernel_alpha_status.md) 为准。
