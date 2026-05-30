# kernel_alpha 状态

## 文档定位

本文档只记录 `kernel_alpha` 的当前定位、课程 OS 当前完成态、当前仍有效的限制和下一步。

它不再维护逐条执行流水账；更细的实现过程统一回写到 [../plan/history_plan.md](../plan/history_plan.md)。

## 关联文档

- 相关设计：
  - [../design/course_os_kernel_alpha_stage3_design.md](../design/course_os_kernel_alpha_stage3_design.md)
  - [../design/course_os_kernel_alpha_stage2_design.md](../design/course_os_kernel_alpha_stage2_design.md)
  - [../design/course_os_kernel_alpha_stage1_design.md](../design/course_os_kernel_alpha_stage1_design.md)
  - [../design/regression_completion_criteria.md](../design/regression_completion_criteria.md)
  - [../design/platform_mmio_contract.md](../design/platform_mmio_contract.md)
- 当前计划：
  - 无活跃 Stage 计划；Stage 3 已完成并归档。
- 相关状态：
  - [mainline_status.md](mainline_status.md)
- 已完成计划归档：
  - [../plan/history_plan.md#course-os-kernel-alpha-stage3-plan](../plan/history_plan.md#course-os-kernel-alpha-stage3-plan)
  - [../plan/history_plan.md#course-os-kernel-alpha-stage2-plan](../plan/history_plan.md#course-os-kernel-alpha-stage2-plan)
  - [../plan/history_plan.md#course-os-kernel-alpha-stage1-plan](../plan/history_plan.md#course-os-kernel-alpha-stage1-plan)
  - [../plan/history_plan.md#kernel-alpha-storage-error-contract-plan](../plan/history_plan.md#kernel-alpha-storage-error-contract-plan)

## 当前状态

`kernel_alpha` 当前已经从 Phase 1 bring-up demo 切换为《操作系统课程设计》A 方案主线入口；旧
`KMVPETDS` 只保留为历史 guardrail，不再描述当前正向能力。
第一阶段按 [../design/course_os_kernel_alpha_stage1_design.md](../design/course_os_kernel_alpha_stage1_design.md)
落地进程、内存、文件系统 3 个模块的 9 个功能点，并提供只读 `/proc` 指标证据面。

第二阶段按 [../design/course_os_kernel_alpha_stage2_design.md](../design/course_os_kernel_alpha_stage2_design.md)
完成 A 方案核心闭环和创新线：syscall ABI、真实进程生命周期、FD / FS 统一 I/O、5 个课程用户程序、
shell、单级管道、重定向、COW Fork、用户态崩溃隔离，以及 `/proc/syscalls`、`/proc/cow`、
`/proc/crashlog` 可观测证据。

Stage 2 新增 `course_os_stage2` 总编排层，不把 demo 流程堆在 `kernel_alpha/main.c`；Stage 2 完成时
`kernel_alpha_demo` 会在 `K/M/V/P/E/T` 基础 bring-up 后依次输出 Stage 1 与 Stage 2 summary。

Stage 2 基线输出曾固定为：

- `KMVPET|course-os-stage1 sched=CFS-lite ctx=9 pf=4 reclaim=1 fs_create=5 btree_steps=48 proc=ps/meminfo/schedstat/fsstat|course-os-stage2 syscall=ok shell=ok procs=ok fd=ok fs=128/64K/3 pipe=ok cow=ok crash=isolated proc=ps/meminfo/schedstat/fsstat/syscalls/cow/crashlog`

其中 `K/M/V/P/E/T` 继续证明基础 bring-up、PLIC、external interrupt 和 timer interrupt 仍可用；
`course-os-stage1 ...` 固定第一阶段调度、Demand Paging / Clock、文件系统索引和 `/proc` 证据摘要；
`course-os-stage2 ...` 固定第二阶段 syscall、进程、FD / FS、shell、管道、COW、crash isolation
和扩展 `/proc` 证据摘要。

第三阶段按 [../design/course_os_kernel_alpha_stage3_design.md](../design/course_os_kernel_alpha_stage3_design.md)
完成“课程满分基线真实化”：教学级 ELF / libc、真实用户程序、FCFS / RR 指标化、
semaphore / mutex、Sv39 fault-driven COW 证据链、`mkfs` / `seek` / `unlink` / `rmdir`、
shell 脚本模式，以及 `/proc/cpuinfo`、`/proc/uptime`、`/proc/<pid>/status`、`/proc/<pid>/fd`、
`/proc/<pid>/maps`。

当前 `guest_kernel_alpha_demo` 正向输出为 Stage 1 + Stage 2 + Stage 3 串联：

- `KMVPET|course-os-stage1 sched=CFS-lite ctx=9 pf=4 reclaim=1 fs_create=5 btree_steps=48 proc=ps/meminfo/schedstat/fsstat|course-os-stage2 syscall=ok shell=ok procs=ok fd=ok fs=128/64K/3 pipe=ok cow=ok crash=isolated proc=ps/meminfo/schedstat/fsstat/syscalls/cow/crashlog|course-os-stage3 elf=5 libc=ok sched=fcfs/rr/cfs sync=sem/mutex vm=sv39-cow fs=seek/mkfs shell=script proc=cpuinfo/uptime/pid`

Stage 3 稳定 marker 为：

- `course-os-stage3 elf=5 libc=ok sched=fcfs/rr/cfs sync=sem/mutex vm=sv39-cow fs=seek/mkfs shell=script proc=cpuinfo/uptime/pid`

旧 Phase 1 `KMVPETDS` 正向输出不再作为课程 OS 当前行为承诺；它降级为历史 bring-up 基线：

- `K`：进入独立 kernel 入口
- `M`：memory / PMM 初始化完成
- `V`：自建 Sv39 内核页表启用并稳定工作
- `P`：PLIC 最小 supervisor 初始化完成
- `E`：第一次 supervisor external interrupt 到达
- `T`：第一次 timer interrupt 到达
- `D`：storage readiness / metadata probe 完成
- `S`：`LBA 0` 读取和签名校验完成

与旧正向基线一起形成的 9 条负向 demo 继续保留为基础设施 guardrail 和历史回归：

- `guest_kernel_alpha_fault_demo = KMVX`
- `guest_kernel_alpha_plic_not_ready_demo = KMVPX`
- `guest_kernel_alpha_timer_not_ready_demo = KMVPETX`
- `guest_kernel_alpha_storage_no_media_demo = KMVNX`
- `guest_kernel_alpha_storage_not_ready_demo = KMVRX`
- `guest_kernel_alpha_storage_bad_magic_demo = KMVGX`
- `guest_kernel_alpha_storage_bad_block_count_demo = KMVBX`
- `guest_kernel_alpha_storage_lba_range_demo = KMVLX`
- `guest_kernel_alpha_storage_bad_command_demo = KMVCX`

旧 `D/S` storage readiness / signature 正向 marker 不再绑在当前 `kernel_alpha_demo` 正向 smoke 上；storage / interrupt / fault 合同由 9 条负向 demo 和 `kernel_alpha_*` 单元门禁继续守住。

## 关键历史节点

- `2026-05-30`
  - 课程 OS 第三阶段实现完成，`kernel_alpha_demo` 正向 smoke 扩展为
    `KMVPET|course-os-stage1 ...|course-os-stage2 ...|course-os-stage3 ...`。
  - 新增 `course_elf_loader`、`course_libc`、`course_sync` 和 `course_os_stage3` 编排层，补齐 ELF / libc / 真实用户程序、FCFS / RR 指标化、semaphore / mutex、Sv39 COW 证据、FS / shell 脚本和扩展 `/proc`。
  - 新增 Stage 3 六条单元门禁，并让 functional / pipeline `kernel_alpha_demo` 共同验证 Stage 3 marker。
- `2026-05-29`
  - 课程 OS 第二阶段实现完成，`kernel_alpha_demo` 正向 smoke 扩展为
    `KMVPET|course-os-stage1 ...|course-os-stage2 ...`，覆盖 syscall、进程生命周期、FD / FS、
    shell、管道 / 重定向、COW Fork、用户态崩溃隔离和 `/proc` 可观测创新线。
  - 课程 OS 第一阶段实现完成，`kernel_alpha_demo` 正向 smoke 从 `KMVPETDS` 切换为 `KMVPET|course-os-stage1 ...`。
  - 新增 `course_scheduler`、`course_memory`、`course_fs`、`procfs` 和 `course_os_stage1` 编排层，覆盖 FCFS / RR / CFS-lite、Demand Paging / Clock / `kmalloc` / `kfree`、文件 / 目录 CRUD / `seek` / 简化 B 树目录索引，以及只读 `/proc` 证据面。
  - `kernel_alpha` 课程 OS 第一阶段方案定稿，当前定位改为“课程 OS 主线入口 + Phase 1 历史基线”。
  - 第一阶段范围冻结为：FCFS / RR / CFS-lite，Demand Paging / Clock / `kmalloc` / `kfree`，文件 / 目录 CRUD / `seek` / B 树目录索引，以及只读 `/proc` 指标面。
- `2026-03-31`
  - alpha 共享 bring-up helper 继续下沉到 `kernel_runtime`、`kernel_bringup`、`storage_contract` 和 `interrupt_contract`，`kernel_alpha` 入口进一步退回到场景组合层。
- `2026-03-25` 到 `2026-03-30`
  - 从首个独立 `kernel_alpha_demo` alpha bring-up 开始，逐步扩展到当前 10 条核心 guest 基线，并把 storage / interrupt / common bring-up 合同从入口收口到共享 guest 基础设施层。

## 当前仍然有效的风险 / 限制

- 课程 OS 第三阶段仍是教学级满分基线，不声明完整 POSIX shell、完整信号语义、多级管道、真实磁盘一致性、journaling、完整 ELF 动态链接或通用 Linux 用户态兼容。
- COW Fork 当前优先覆盖课程级匿名用户页，不扩展到文件系统 snapshot 或完整文件页 COW。
- AI/NPU、JIT/DBT、Pipeline-aware scheduling、前端 Lab 面板、微内核和安全隔离不进入 Stage 3 完成范围。
- 旧 9 条负向 guest 回归仍是基础设施 guardrail；当前正向 `kernel_alpha_demo` 已不再检查旧 `D/S` marker。
- `SimpleStorage` 仍然是单块、同步、无 completion interrupt、无宿主持久化回写的最小模型。
- `/proc` 第三阶段仍保持只读证据面，不作为调度、内存或文件系统的写控制接口。

## 下一步

1. 保持 Stage 1 / Stage 2 / Stage 3 marker、Stage 3 unit targets、functional / pipeline guest demo 和旧 9 条负向 demo 稳定。
2. 如继续扩展，优先独立设计 Stage 4+ 创新线，例如全栈可观测 Lab、AI/NPU、JIT/DBT 或 Pipeline-aware 调度；不要回写扩大 Stage 3 完成范围。
3. 保留旧 Phase 1 负向 demo 作为基础设施 guardrail；除非真实 bug 或课程 OS 迁移需要，不继续扩旧 bring-up marker 面。

## 验证基线

- `cd myCPU && make test`
- `cd myCPU && make test-unit-course_os_stage1`
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
- 触及 shared guest runtime 时，额外关注：
  - `cd myCPU && make test-unit-kernel_runtime`
  - `cd myCPU && make test-unit-vm_address_space`
  - `cd myCPU && make test-unit-vm_process`
  - `cd myCPU && make test-unit-vm_fault`
  - `cd myCPU && make test-unit-user_task`
- Stage 2 当前固定门禁：
  - `cd myCPU && make test-unit-course_os_stage2_syscall`
  - `cd myCPU && make test-unit-course_os_stage2_process`
  - `cd myCPU && make test-unit-course_os_stage2_fd_fs`
  - `cd myCPU && make test-unit-course_os_stage2_shell`
  - `cd myCPU && make test-unit-course_os_stage2_cow_crash`
  - `cd myCPU && make test-unit-course_os_stage2`
  - `cd myCPU && make test-pipeline-guest-kernel_alpha_demo`
- Stage 3 当前固定门禁：
  - `cd myCPU && make test-unit-course_os_stage3_elf`
  - `cd myCPU && make test-unit-course_os_stage3_sched_sync`
  - `cd myCPU && make test-unit-course_os_stage3_vm`
  - `cd myCPU && make test-unit-course_os_stage3_fs_shell`
  - `cd myCPU && make test-unit-course_os_stage3_proc`
  - `cd myCPU && make test-unit-course_os_stage3`
