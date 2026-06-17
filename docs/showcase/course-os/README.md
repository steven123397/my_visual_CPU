# Course OS 展示材料

本目录用于存放本轮操作系统课程展示内容。这里是 Course OS 最终总结入口，后续新增 PPT、
讲稿、截图、演示脚本或 HTML 预览页时，优先放在本目录下。

## 展示口径

- `kernel_alpha` Stage 1-4 是课程 OS 基线展示口径。
- Linux compat Plus、OSComp / testsuits-for-oskernel basic smoke 只作为 opt-in 外部验证证据，
  不作为 Stage 1-4 完成条件。
- 不新增浏览器 external rootfs 运行入口；涉及真实 rootfs、OSComp assets 或 testsuits
  checkout 的内容只保留 host-only / 文档说明口径。

## 最终总结层级

1. 课程设计要求对照
   - 进程管理：课程进程生命周期、syscall ABI、shell、管道、重定向、COW fork、crash
     isolation。
   - 内存管理：Demand Paging、Clock 页面置换、`kmalloc / kfree` 复用证据、Sv39
     fault-driven COW。
   - 文件系统：文件 / 目录 CRUD、`seek`、`mkfs`、`ls`、`unlink`、`rmdir`、简化 B 树目录索引。
   - 证据面：`/proc/ps`、`/proc/meminfo`、`/proc/schedstat`、`/proc/fsstat`、
     `/proc/syscalls`、`/proc/cow`、`/proc/crashlog`、`/proc/cpuinfo`、
     `/proc/uptime`、`/proc/<pid>/status`、`/proc/<pid>/fd`、`/proc/<pid>/maps`。

2. 交互展示能力
   - `guest_course_os_shell_demo` 提供 `course-os> ` prompt。
   - 前端 `/console` 继续走 UART terminal / debug session 合同。
   - 展示命令包括 `ls`、`kill`、`mkfs`、`sem`、`mutex`、`concurrency_demo`、`exec` 和
     proc 快捷读。

3. 架构增强
   - 在线抢占调度第一刀：FCFS / RR / CFS-lite online tick、preempt、cycle-only switch cost。
   - UART 中断输入：RX external post handler drain 到 shell-local raw FIFO，并保留轮询 fallback。
   - 真实课程 ELF 来源统一：内置课程程序和课程 FS 文件 ELF 复用同一课程 ELF loader /
     process image 路径。
   - context switch cost 只声明 scheduler-local cycle 证据，不换算 wall-clock 延迟。

4. Linux compat Plus
   - 显式 `linux ...` launcher 和 Linux PATH fallback 走旁路 `linux_compat_*`，不膨胀
     `course_*` 教学模块。
   - 已覆盖 BusyBox / git help-run、真实静态 ELF 执行、external rootfs opt-in、
     Stage 11 本地 writable workflow v0。
   - Stage 11 v0 可展示 `git init/add/commit/log`、`vim hello.c`、`gcc hello.c && ./a.out`
     的 host-only workflow 证据，但不声明完整 toolchain 子进程链。

5. OSComp / 外部验证
   - `test-host-course_os_oscomp_basic_smoke` 是 host-only opt-in 证据。
   - 缺 `MYCPU_COURSE_OS_LINUX_COMPAT_ROOTFS` 时清晰 `SKIP`，不污染默认 `make test`。
   - 有 rootfs 时覆盖 `/bin/busybox`、`/usr/bin/git`、缺失 guest path、loader / trace /
     exit / errno 诊断。

6. 明确不声明的边界
   - 不声明完整 Linux 用户态兼容、完整 `testsuits-for-oskernel`、网络 git、真实包管理器、
     完整 `cc1/as/ld` toolchain、完整 signal / futex / pthread、完整 TTY / job control、
     `rustc` 或浏览器 external rootfs route。

## 建议展示证据

- `cd myCPU && make test-guest-kernel_alpha_demo`
- `cd myCPU && make test-guest-course_os_shell_demo`
- `cd myCPU && make test-host-course_os_shell_terminal_smoke`
- `cd myCPU && make test-host-course_os_linux_compat_oscomp_help_smoke`
- `cd myCPU && make test-host-course_os_oscomp_basic_smoke`
- 有外部 rootfs 时：
  `cd myCPU && MYCPU_COURSE_OS_LINUX_COMPAT_ROOTFS=/path/to/rootfs make test-host-course_os_oscomp_basic_smoke`

## 关联文档

- [kernel_alpha_status.md](../../status/kernel_alpha_status.md)
- [course_os_kernel_alpha_course_os_baseline_design.md](../../design/course_os_kernel_alpha_course_os_baseline_design.md)
- [course_os_gap_closure_boundary_design.md](../../design/course_os_gap_closure_boundary_design.md)
- [course_os_kernel_alpha_linux_compat_plus_design.md](../../design/course_os_kernel_alpha_linux_compat_plus_design.md)
- [course_os_oscomp_external_validation_design.md](../../design/course_os_oscomp_external_validation_design.md)
- [course_os_preemptive_scheduler_design.md](../../design/course_os_preemptive_scheduler_design.md)
- [course_os_uart_interrupt_input_design.md](../../design/course_os_uart_interrupt_input_design.md)
- [course_os_real_user_elf_design.md](../../design/course_os_real_user_elf_design.md)
- [course_os_scheduler_timing_contract.md](../../design/course_os_scheduler_timing_contract.md)
- [history_plan.md#course-os-plus-external-validation-plan](../../plan/history_plan.md#course-os-plus-external-validation-plan)
- [history_plan.md#course-os-arch-followup-plan](../../plan/history_plan.md#course-os-arch-followup-plan)
- [history_plan.md#course-os-display-gap-closure-plan](../../plan/history_plan.md#course-os-display-gap-closure-plan)
