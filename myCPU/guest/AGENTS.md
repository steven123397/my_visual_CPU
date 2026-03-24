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

当前已经能完成：

- S-mode 最小 runtime bring-up
- U-mode enter / return
- delegated user page-fault recovery
- delegated user `ecall`
- delegated timer / external interrupt return
- 单用户生命周期和清理 smoke
- 独立 kernel alpha 的正向 bring-up 与两条负向回归

## 分层边界

当前 guest 侧应理解为两层：

### 基础设施层：`guest/kernel/`

这里放通用能力，而不是某个 demo 的专属逻辑：

- `memory.c`：早期内存布局、early allocator、linker symbol 边界
- `pmm.c`：物理页管理
- `vm.c`：address space / process / region / object / fault policy
- `trap.c`：trap dispatch、trap context、user runtime 生命周期
- `runtime_context.c`：当前活跃 process / address_space / trap_context 记录
- `console.c` / `timer.c` / `storage.c`：最小平台驱动封装
- `user_task.c` / `user_task_bootstrap.c` / `user_program.c`：标准用户生命周期装配

### 入口与编排层：`supervisor_demo` / `kernel_alpha`

- [supervisor_demo/main.c](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/supervisor_demo/main.c)
  只负责基础初始化和调用 `supervisor_demo_smoke_run()`。
- [kernel_alpha/main.c](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/kernel_alpha/main.c)
  独立 `kernel_alpha_demo` 正向入口。
- [kernel_alpha/fault_main.c](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/kernel_alpha/fault_main.c)
  独立 `kernel_alpha_fault_demo` 负向入口。
- [kernel_alpha/storage_no_media_main.c](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/kernel_alpha/storage_no_media_main.c)
  独立 `kernel_alpha_storage_no_media_demo` 负向入口。

## 当前三条 `kernel_alpha` 路径

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

## 局部规则

- 不要重新把 demo 逻辑堆回 `guest/supervisor_demo/main.c`。
- 不要把独立 kernel alpha bring-up 再塞回 `supervisor_demo_smoke`。
- 新的标准生命周期逻辑优先放到 `user_program`、`user_task_bootstrap`、`trap`、`vm`，而不是直接塞进 smoke/demo。
- smoke 的 public surface 应尽量小，细碎 orchestration 尽量内部化、`static` 化。
- `kernel_alpha/*` 入口不应退化成新的“大而全 runtime 框架”。

## 当前仍需关注的问题

- [kernel/vm.c](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/kernel/vm.c)
  仍同时承担 address space、process、region / object 与 fault policy。
- [kernel/trap.c](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/kernel/trap.c)
  仍同时承担 dispatch、policy 与 user runtime lifecycle。
- 当前实现还有一批阶段性固定上限：
  - `VM_MAX_ADDRESS_SPACES`
  - `VM_MAX_USER_REGIONS`
  - `VM_PROCESS_MAX_USER_REGIONS`
  - `VM_MAX_FAULT_ACTIONS`
  - `TRAP_MAX_INTERRUPT_CAUSE`
  - `TRAP_MAX_EXCEPTION_CAUSE`
- `kernel_alpha` 仍是 alpha 形态，还没有真正的内核对象、调度或设备探测流程。

## 本子树下一步工作

1. 保持 `guest_supervisor_demo` 和 `kernel_alpha` 分工清晰，不要把两条路径重新揉成一个入口。
2. 在 `kernel_alpha_demo` / `kernel_alpha_fault_demo` / `kernel_alpha_storage_no_media_demo` 基线上继续补更多 fault / panic / device readiness。
3. 继续推进 process / runtime refinement。
4. 继续补更多 user interrupt / trap coverage。
5. 把 [kernel/vm.c](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/kernel/vm.c) 和 [kernel/trap.c](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/kernel/trap.c) 继续拆小。

## 验证要求

只要触及 guest runtime / demo / smoke 路径，默认至少关注：

- `cd myCPU && make test-guest-supervisor_demo`
- `cd myCPU && make test-guest-kernel_alpha_demo`
- `cd myCPU && make test-guest-kernel_alpha_fault_demo`
- `cd myCPU && make test-guest-kernel_alpha_storage_no_media_demo`

通常仍应回归：

- `cd myCPU && make test`
