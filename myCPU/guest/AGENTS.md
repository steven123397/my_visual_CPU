# AGENTS.md

## 适用范围

本文件适用于 [myCPU/guest](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest) 子树下的 guest supervisor runtime、VM、trap、runtime、user task/program，以及 demo / smoke orchestration 与独立 kernel bring-up 入口。

## 当前实现基线

guest 侧当前已经不是单纯 demo 代码，而是一条已接通的最小 bring-up 路径，包含：

- linker-backed early allocator
- bitmap PMM
- guest-side Sv39 page-table builder
- `vm_address_space_t` / `vm_process_t`
- `trap_context_t` / `trap_user_runtime_t`
- `user_task_t` / `user_task_bootstrap_t`
- `user_program_t` / `user_program_smoke_t`
- `supervisor_demo_smoke`
- 独立 `kernel_alpha` bring-up / negative demos
- 独立 `interactive_os` 串口 monitor demo

当前已经能完成：

- S-mode 最小 runtime bring-up
- U-mode enter / return
- delegated user page-fault recovery
- delegated user `ecall`
- delegated timer / external interrupt return
- 单用户生命周期和清理 smoke
- 独立 kernel alpha 的正向 bring-up 与九条负向回归
- 独立 `interactive_os` 的 browser/front-end 终端壳闭环与 monitor 命令集
- `guest_supervisor_demo` 与 `kernel_alpha` 十条 demo 当前共同构成 Phase 1 核心 guest 门禁，回归收口口径见 [docs/design/regression_completion_criteria.md](/home/liangjiaqi/projects/my_visual_CPU/docs/design/regression_completion_criteria.md)
- 当前冻结稳定基线 tag 为 `phase1-stable`（`283aee6`），后续 guest runtime 调整默认按 post-Phase1 hardening 理解。

## 分层边界

当前 guest 侧应理解为两层：

### 基础设施层：`guest/kernel/`

这里放通用能力，而不是某个 demo 的专属逻辑：

- `memory.c`：早期内存布局、early allocator、linker symbol 边界
- `pmm.c`：物理页管理
- `vm.c`：当前主要承载低层 page-table / map / unmap / TLB primitive
- `vm_address_space.c`：`vm_address_space_*` 生命周期、kernel range / fault-range 注册与 Sv39 address-space 编排
- `vm_process.c`：`vm_process_*` 生命周期与 region binding 编排
- `vm_object.c`：`vm_object_*` 与 `vm_user_region_*object*` 生命周期
- `vm_fault.c`：page-fault policy、fault action 与 `vm_handle_page_fault`
- `trap.c`：当前主要承载 active runtime 与 user runtime lifecycle
- `trap_dispatch.c`：trap dispatch、default policy 与 handler 安装
- `runtime_context.c`：当前活跃 process / address_space / trap_context 记录
- `console.c` / `timer.c` / `storage.c`：最小平台驱动封装
- `kernel_bringup.c`：共享的早期 `K/M/V` bring-up 骨架，负责 memory / PMM / trap / VM 的最小启动编排
- `kernel_runtime.c`：最小 kernel runtime 对象，承接 `trap_context` / `address_space` / `interrupt_state`，并负责 common bring-up options 的 runtime/self-context 装配，避免 bring-up 入口继续裸拼三件套
- `supervisor_runtime.c`：`kernel_alpha` 与 `supervisor_demo_smoke` 共享的 supervisor bring-up interrupt state、self-bound contract、policy adapter、delivery / deadline wait 最小编排
- `user_task.c` / `user_task_bootstrap.c` / `user_program.c`：标准用户生命周期装配

### 入口与编排层：`supervisor_demo` / `kernel_alpha` / `interactive_os`

- [supervisor_demo/main.c](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/supervisor_demo/main.c)
  只负责基础初始化和调用 `supervisor_demo_smoke_run()`。
- [kernel_alpha/main.c](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/kernel_alpha/main.c)
  独立 `kernel_alpha_demo` 正向入口。
- [kernel_alpha/fault_main.c](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/kernel_alpha/fault_main.c)
  独立 `kernel_alpha_fault_demo` 负向入口。
- [kernel_alpha/storage_no_media_main.c](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/kernel_alpha/storage_no_media_main.c)
  独立 `kernel_alpha_storage_no_media_demo` 负向入口。
- [kernel_alpha/storage_not_ready_main.c](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/kernel_alpha/storage_not_ready_main.c)
  独立 `kernel_alpha_storage_not_ready_demo` 负向入口。
- [kernel_alpha/storage_bad_magic_main.c](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/kernel_alpha/storage_bad_magic_main.c)
  独立 `kernel_alpha_storage_bad_magic_demo` 负向入口。
- [kernel_alpha/storage_bad_block_count_main.c](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/kernel_alpha/storage_bad_block_count_main.c)
  独立 `kernel_alpha_storage_bad_block_count_demo` 负向入口。
- [kernel_alpha/storage_lba_range_main.c](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/kernel_alpha/storage_lba_range_main.c)
  独立 `kernel_alpha_storage_lba_range_demo` 负向入口。
- [kernel_alpha/storage_bad_command_main.c](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/kernel_alpha/storage_bad_command_main.c)
  独立 `kernel_alpha_storage_bad_command_demo` 负向入口。
- [kernel_alpha/plic_not_ready_main.c](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/kernel_alpha/plic_not_ready_main.c)
  独立 `kernel_alpha_plic_not_ready_demo` 负向入口。
- [kernel_alpha/timer_not_ready_main.c](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/kernel_alpha/timer_not_ready_main.c)
  独立 `kernel_alpha_timer_not_ready_demo` 负向入口。
- [kernel_alpha/common.c](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/kernel_alpha/common.c)
  `kernel_alpha` 各入口共享的 alpha bring-up phase helper：当前承接 PLIC phase、first external / timer delivery wait，以及 storage probe / signature check，不再承载通用 `K/M/V` 骨架。
- [kernel_alpha/interrupt_contract.c](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/kernel_alpha/interrupt_contract.c)
  `kernel_alpha` non-storage 负向回归共享的 interrupt / fault helper：当前收口 interrupt bring-up、platform interrupt readiness、PLIC not-ready、timer not-ready 与标准 post-handler 合同。
- [kernel_alpha/storage_contract.c](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/kernel_alpha/storage_contract.c)
  `kernel_alpha` storage 负向回归共享的合同 helper：当前收口 no-media / not-ready / bad-magic / bad-block-count / lba-range / bad-command 六条独立路径的公共协议检查。
- [interactive_os/main.c](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/interactive_os/main.c)
  独立 `interactive_os` 入口。当前只负责最小 bring-up、进入串口 monitor 主循环，不承载 `kernel_alpha` 的 bring-up 合同。
- [kernel/console_input.c](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/kernel/console_input.c)
  轮询式 UART 输入与最小行编辑。
- [kernel/monitor.c](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/kernel/monitor.c)
  banner / prompt / monitor 主循环。
- [kernel/monitor_commands.c](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/kernel/monitor_commands.c)
  monitor 命令解析与执行，当前至少包含 `help`、`echo`、`time`、`uptime`、`disk info`、`disk read`、`regs`、`peek`、`pagewalk`、`pte dump` 与 `halt`。
- [kernel/vm_debug.c](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/kernel/vm_debug.c)
  monitor 专用的只读页表 walk / PTE dump helper，不把这组调试输出混回 `kernel_alpha`。

## 当前十条 `kernel_alpha` 路径

### `kernel_alpha_demo`

当前只承载第一次真正的小 kernel alpha 基线：

- boot marker
- PMM 初始化
- 自建 Sv39 内核页表
- 内核镜像 / early heap / managed RAM 显式映射
- UART / CLINT / PLIC / storage 的 MMIO lazy map
- 一次 supervisor external interrupt
- 一次 supervisor timer interrupt
- 一次 storage readiness probe
- 一次 block `LBA 0` 读取与签名校验

当前正向回归输出：

- `KMVPETDS`

### `kernel_alpha_fault_demo`

当前用于验证：

- VM 已开启
- UART 仍可输出
- CLINT 未映射时的内核 MMIO 访问会进入 fault / panic 路径

当前负向回归输出：

- `KMVX`

### `kernel_alpha_storage_no_media_demo`

当前用于验证：

- VM 已开启
- UART / storage MMIO lazy map 仍可工作
- storage metadata 可读，但 `ATTACHED` 缺失、`capacity_blocks == 0`
- 首次 block read 返回 `STORAGE_ERR_NO_MEDIA`

当前负向回归输出：

- `KMVNX`

### `kernel_alpha_storage_not_ready_demo`

当前用于验证：

- VM 已开启
- storage MMIO 可达且 metadata / `ATTACHED` / capacity 可观察
- `READY` 缺失时 `storage_probe()` 不会误判 bring-up readiness 已满足
- 首次 block read 返回 `STORAGE_ERR_NOT_READY`
- `COMMAND = NONE` clear-error 后不会把设备误恢复成 ready

当前负向回归输出：

- `KMVRX`

### `kernel_alpha_storage_bad_magic_demo`

当前用于验证：

- VM 已开启
- storage MMIO 可达且 attached / ready / capacity 成功态仍可观察
- `MAGIC` 元数据损坏时，`storage_read_info()` 与 `storage_probe()` 不会误判 probe 已成功
- 即使 probe 失败，基础 block read 数据路径仍可工作，证明失败点在元数据合同而不是读命令本身

当前负向回归输出：

- `KMVGX`

### `kernel_alpha_storage_bad_block_count_demo`

当前用于验证：

- VM 已开启
- storage MMIO 可达且 attached / ready 成功态可观察
- `BLOCK_COUNT != 1` 的 read 命令会返回 `STORAGE_ERR_BAD_BLOCK_COUNT`
- `COMMAND = NONE` 可清除粘滞 error 状态

当前负向回归输出：

- `KMVBX`

### `kernel_alpha_storage_lba_range_demo`

当前用于验证：

- VM 已开启
- storage MMIO 可达且 attached / ready 成功态可观察
- `LBA == capacity_blocks` 的 block read 返回 `STORAGE_ERR_LBA_RANGE`
- `COMMAND = NONE` 可清除粘滞 error 状态

当前负向回归输出：

- `KMVLX`

### `kernel_alpha_storage_bad_command_demo`

当前用于验证：

- VM 已开启
- storage MMIO 可达且 attached / ready 成功态可观察
- 非法 `COMMAND` 值会返回 `STORAGE_ERR_BAD_COMMAND`
- `COMMAND = NONE` 可清除粘滞 error 状态

当前负向回归输出：

- `KMVCX`

### `kernel_alpha_plic_not_ready_demo`

当前用于验证：

- VM 已开启
- UART / CLINT / PLIC MMIO lazy map 仍可工作
- PLIC 已映射但未初始化时，UART THRE 不会到达 supervisor external interrupt
- deadline 超时后会进入 panic 路径

当前负向回归输出：

- `KMVPX`

### `kernel_alpha_timer_not_ready_demo`

当前用于验证：

- VM 已开启
- UART / CLINT / PLIC MMIO lazy map 仍可工作
- PLIC supervisor 初始化与第一次 external interrupt 已成功到达
- 未安排第一次 timer delivery 时，bring-up 会在 deadline 超时后进入 panic 路径

当前负向回归输出：

- `KMVPETX`

## 局部规则

- 不要重新把 demo 逻辑堆回 `guest/supervisor_demo/main.c`。
- 不要把独立 kernel alpha bring-up 再塞回 `supervisor_demo_smoke`。
- 不要把 `interactive_os` 扩成图形桌面、窗口管理器或 `kernel_alpha` 的替代入口。
- 新的标准生命周期逻辑优先放到 `user_program`、`user_task_bootstrap`、`trap`、`vm`，而不是直接塞进 smoke/demo。
- smoke 的 public surface 应尽量小，细碎 orchestration 尽量内部化、`static` 化。
- `kernel_alpha/*` 入口不应退化成新的“大而全 runtime 框架”。

## 当前仍需关注的问题

- [kernel/vm.c](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/kernel/vm.c)
  已收口为低层 page-table / mapping core。
- [kernel/vm_address_space.c](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/kernel/vm_address_space.c)
  当前承载 address space 生命周期、kernel mapping 与 fault-range 注册。
- [kernel/vm_object.c](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/kernel/vm_object.c)
  当前承载 object / region object 生命周期。
- [kernel/vm_fault.c](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/kernel/vm_fault.c)
  当前承载 fault policy / action 与 page-fault dispatch。
- [kernel/trap.c](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/kernel/trap.c)
  已拆出 `trap_dispatch.c`，但后续仍要继续守住两侧边界不要重新耦合。
- 当前实现还有一批阶段性固定上限：
  - `VM_MAX_ADDRESS_SPACES`
  - `VM_MAX_USER_REGIONS`
  - `VM_PROCESS_MAX_USER_REGIONS`
  - `VM_MAX_FAULT_ACTIONS`
  - `TRAP_MAX_INTERRUPT_CAUSE`
  - `TRAP_MAX_EXCEPTION_CAUSE`
- `kernel_alpha` 仍是 alpha 形态，还没有真正的内核对象、调度或设备探测流程。
- [kernel/kernel_runtime.c](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/kernel/kernel_runtime.c)
  当前已继续收口 `kernel_alpha` 入口的基础 runtime 三件套与 common bring-up options 装配，但后续仍要继续往真正的小内核对象组织推进。
- [kernel/kernel_bringup.c](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/kernel/kernel_bringup.c)
  通用 `K/M/V` bring-up 已从 `kernel_alpha` 子树下沉到基础设施层，避免 `supervisor_demo` 再被 alpha 私有骨架反向耦合。

## 本子树下一步工作

1. 保持 `guest_supervisor_demo` 和 `kernel_alpha` 分工清晰，不要把两条路径重新揉成一个入口。
2. 继续把 `kernel_alpha_demo`、`kernel_alpha_fault_demo`、六条 storage 负向 demo、`kernel_alpha_plic_not_ready_demo` 和 `kernel_alpha_timer_not_ready_demo` 这十条回归守在稳定输出上；它们当前就是 Phase 1 核心 guest 门禁的一部分。公共 bring-up 编排继续收敛在 `kernel_alpha/common.c` 和 `kernel/supervisor_runtime.c`，不要让重复骨架重新散回各入口。
3. 守住 [kernel/vm.c](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/kernel/vm.c) / [kernel/vm_address_space.c](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/kernel/vm_address_space.c) / [kernel/vm_process.c](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/kernel/vm_process.c) / [kernel/vm_object.c](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/kernel/vm_object.c) / [kernel/vm_fault.c](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/kernel/vm_fault.c) 的边界，不要重新耦合。
4. 在 [kernel/trap.c](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/kernel/trap.c) 与 [kernel/trap_dispatch.c](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/kernel/trap_dispatch.c) 的边界上继续保持 lifecycle / dispatch 分离，不要回退。
5. 继续沿着 process / runtime refinement 与大文件拆分的方向收口 `kernel_runtime`、`kernel_bringup` 和相关基础设施，而不是再把逻辑重新堆回 demo 入口。
6. 继续补更多 user interrupt / trap coverage，但不要把这条线和 Phase 2 backend 稳定化混在一起。

## 验证要求

只要触及 guest runtime / demo / smoke 路径，默认至少关注：

- `cd myCPU && make test-unit-supervisor_runtime`
- `cd myCPU && make test-unit-kernel_runtime`
- `cd myCPU && make test-unit-kernel_alpha_common`
- `cd myCPU && make test-unit-kernel_alpha_interrupt`
- `cd myCPU && make test-unit-kernel_alpha_storage`
- `cd myCPU && make test-guest-supervisor_demo`
- `cd myCPU && make test-guest-kernel_alpha_demo`
- `cd myCPU && make test-guest-kernel_alpha_fault_demo`
- `cd myCPU && make test-guest-kernel_alpha_storage_no_media_demo`
- `cd myCPU && make test-guest-kernel_alpha_storage_not_ready_demo`
- `cd myCPU && make test-guest-kernel_alpha_storage_bad_magic_demo`
- `cd myCPU && make test-guest-kernel_alpha_storage_bad_block_count_demo`
- `cd myCPU && make test-guest-kernel_alpha_storage_lba_range_demo`
- `cd myCPU && make test-guest-kernel_alpha_storage_bad_command_demo`
- `cd myCPU && make test-guest-kernel_alpha_plic_not_ready_demo`
- `cd myCPU && make test-guest-kernel_alpha_timer_not_ready_demo`

通常仍应回归：

- `cd myCPU && make test`
