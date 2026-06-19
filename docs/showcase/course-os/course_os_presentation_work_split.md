# Course OS 汇报分工与源码对应关系

**适用场景：** 课程设计答辩 PPT、讲稿分工、现场演示和答辩追问准备。

**汇报人员：**

- 梁家琦：主讲人，负责开场、主线串联、内核控制面、Linux 兼容层、演示和总结。
- 杨皓宇：同学 A，负责运行底座，包括内存管理、程序装载、文件系统和状态文件。
- 余健超：同学 B，负责用户态与交互，包括系统调用、文件描述符、用户程序、shell 和前端接入。

本文只用于展示分工和源码查阅，不改变实现归属，不作为实时状态来源。完整技术背景见
[course_os_technical_report.md](course_os_technical_report.md)。

## 总体讲述顺序

建议 15 分钟汇报按下面顺序展开：

| 时间 | 讲者 | 内容 |
|---|---|---|
| 0:00 - 0:50 | 梁家琦 | 开场、项目目标、总体架构 |
| 0:50 - 4:20 | 杨皓宇 | 程序如何被系统承载：内存、ELF、文件系统、状态文件 |
| 4:20 - 7:50 | 余健超 | 用户如何操作系统：syscall、FD、用户程序、shell、前端 |
| 7:50 - 14:20 | 梁家琦 | 进程、调度、同步、中断 / trap、Linux 兼容层、验证和现场演示 |
| 14:20 - 15:00 | 梁家琦 | 总结、成果边界和后续方向 |

这套顺序的目标是让三个人各自讲完一条完整链路：

- 杨皓宇讲“程序和数据如何被系统承载”。
- 余健超讲“用户如何通过接口使用系统”。
- 梁家琦讲“内核如何控制运行并完成演示收口”。

## 梁家琦：主线、内核控制面与 Linux 兼容层

### 汇报内容

梁家琦负责全场主线和最容易被追问的系统控制面：

- 开场：说明项目是运行在自研 RISC-V 模拟器上的课程操作系统原型。
- 总体架构：解释浏览器终端、debug server、模拟器、guest runtime、Course OS 模块之间的层次关系。
- 进程管理：讲进程创建、状态切换、退出、等待、崩溃隔离和 COW fork 与进程表的关系。
- 调度：讲 FCFS / RR / CFS-lite 的课程调度模型，以及在线 tick、抢占和调度统计。
- 同步：讲 semaphore / mutex 的教学模型，以及 `sem`、`mutex`、`concurrency_demo` 展示命令。
- 中断 / trap：讲 timer interrupt、external interrupt、异常和 syscall 统一进入内核处理入口。
- Linux 兼容层：讲为什么要和课程模块分流，只做最小兼容验证，不宣称完整 Linux。
- 验证与演示：讲默认回归、host smoke、opt-in 外部验证和现场演示命令。
- 结束：用“可运行、可交互、可观察、可扩展”收口。

### 主要源码

进程管理：

- [myCPU/guest/kernel/course_process.c](../../../myCPU/guest/kernel/course_process.c)
- [myCPU/guest/include/course_process.h](../../../myCPU/guest/include/course_process.h)

调度与在线抢占：

- [myCPU/guest/kernel/course_scheduler.c](../../../myCPU/guest/kernel/course_scheduler.c)
- [myCPU/guest/include/course_scheduler.h](../../../myCPU/guest/include/course_scheduler.h)

同步机制：

- [myCPU/guest/kernel/course_sync.c](../../../myCPU/guest/kernel/course_sync.c)
- [myCPU/guest/include/course_sync.h](../../../myCPU/guest/include/course_sync.h)

trap、中断和运行时：

- [myCPU/guest/kernel/trap.c](../../../myCPU/guest/kernel/trap.c)
- [myCPU/guest/kernel/trap_dispatch.c](../../../myCPU/guest/kernel/trap_dispatch.c)
- [myCPU/guest/include/trap.h](../../../myCPU/guest/include/trap.h)
- [myCPU/guest/kernel/timer.c](../../../myCPU/guest/kernel/timer.c)
- [myCPU/guest/include/timer.h](../../../myCPU/guest/include/timer.h)
- [myCPU/guest/kernel/kernel_runtime.c](../../../myCPU/guest/kernel/kernel_runtime.c)
- [myCPU/guest/kernel/supervisor_runtime.c](../../../myCPU/guest/kernel/supervisor_runtime.c)

Linux 兼容层：

- [myCPU/guest/kernel/linux_compat.c](../../../myCPU/guest/kernel/linux_compat.c)
- [myCPU/guest/kernel/linux_compat_exec.c](../../../myCPU/guest/kernel/linux_compat_exec.c)
- [myCPU/guest/kernel/linux_compat_loader.c](../../../myCPU/guest/kernel/linux_compat_loader.c)
- [myCPU/guest/kernel/linux_compat_process.c](../../../myCPU/guest/kernel/linux_compat_process.c)
- [myCPU/guest/kernel/linux_compat_vm.c](../../../myCPU/guest/kernel/linux_compat_vm.c)
- [myCPU/guest/kernel/linux_compat_debug.c](../../../myCPU/guest/kernel/linux_compat_debug.c)
- [myCPU/guest/kernel/linux_compat_rootfs_builtin.c](../../../myCPU/guest/kernel/linux_compat_rootfs_builtin.c)
- [myCPU/guest/include/linux_compat.h](../../../myCPU/guest/include/linux_compat.h)
- [myCPU/guest/include/linux_compat_exec.h](../../../myCPU/guest/include/linux_compat_exec.h)
- [myCPU/guest/include/linux_compat_loader.h](../../../myCPU/guest/include/linux_compat_loader.h)
- [myCPU/guest/include/linux_compat_process.h](../../../myCPU/guest/include/linux_compat_process.h)
- [myCPU/guest/include/linux_compat_vm.h](../../../myCPU/guest/include/linux_compat_vm.h)

Course OS 正向入口和汇总：

- [myCPU/guest/kernel_alpha/main.c](../../../myCPU/guest/kernel_alpha/main.c)
- [myCPU/guest/kernel/course_os_stage3.c](../../../myCPU/guest/kernel/course_os_stage3.c)
- [myCPU/guest/include/course_os_stage3.h](../../../myCPU/guest/include/course_os_stage3.h)

### 关联测试与验证入口

- [myCPU/tests/unit/course_os_stage2_process.c](../../../myCPU/tests/unit/course_os_stage2_process.c)
- [myCPU/tests/unit/course_os_stage2_cow_crash.c](../../../myCPU/tests/unit/course_os_stage2_cow_crash.c)
- [myCPU/tests/unit/course_os_stage3_sched_sync.c](../../../myCPU/tests/unit/course_os_stage3_sched_sync.c)
- [myCPU/tests/unit/course_os_preemptive_sched.c](../../../myCPU/tests/unit/course_os_preemptive_sched.c)
- [myCPU/tests/unit/trap_runtime.c](../../../myCPU/tests/unit/trap_runtime.c)
- [myCPU/tests/unit/trap_dispatch.c](../../../myCPU/tests/unit/trap_dispatch.c)
- [myCPU/tests/unit/course_os_stage5_linux_compat.c](../../../myCPU/tests/unit/course_os_stage5_linux_compat.c)
- [myCPU/tests/unit/course_os_stage8_linux_compat_loader.c](../../../myCPU/tests/unit/course_os_stage8_linux_compat_loader.c)
- [myCPU/tests/unit/course_os_stage9_linux_compat_vm.c](../../../myCPU/tests/unit/course_os_stage9_linux_compat_vm.c)
- [myCPU/tests/unit/course_os_stage9_linux_compat_exec.c](../../../myCPU/tests/unit/course_os_stage9_linux_compat_exec.c)
- [myCPU/tests/unit/course_os_stage9_linux_compat_syscall.c](../../../myCPU/tests/unit/course_os_stage9_linux_compat_syscall.c)
- [myCPU/tests/unit/course_os_stage11_linux_compat.c](../../../myCPU/tests/unit/course_os_stage11_linux_compat.c)
- [myCPU/tests/host/course_os_linux_compat_terminal_smoke.cpp](../../../myCPU/tests/host/course_os_linux_compat_terminal_smoke.cpp)
- [myCPU/tests/host/course_os_linux_compat_oscomp_help_smoke.cpp](../../../myCPU/tests/host/course_os_linux_compat_oscomp_help_smoke.cpp)
- [myCPU/tests/host/course_os_oscomp_basic_smoke.cpp](../../../myCPU/tests/host/course_os_oscomp_basic_smoke.cpp)

## 杨皓宇：运行底座

### 汇报内容

杨皓宇负责把“程序如何被系统承载起来”讲完整：

- 内存管理：讲物理页管理、地址空间、页表、缺页处理和写时复制的底层支撑。
- 程序装载：讲课程 ELF loader 如何校验 ELF、读取段、建立映射、设置入口。
- 文件系统：讲课程 FS 如何组织文件、目录、路径解析、读写、seek、mkfs 和目录索引。
- 状态文件：讲 `/proc` 为什么是只读证据面，以及 `meminfo`、`schedstat`、`fsstat` 等输出如何来自内核数据结构。

### 主要源码

内存和虚拟地址空间：

- [myCPU/guest/kernel/memory.c](../../../myCPU/guest/kernel/memory.c)
- [myCPU/guest/include/memory.h](../../../myCPU/guest/include/memory.h)
- [myCPU/guest/kernel/pmm.c](../../../myCPU/guest/kernel/pmm.c)
- [myCPU/guest/include/pmm.h](../../../myCPU/guest/include/pmm.h)
- [myCPU/guest/kernel/vm.c](../../../myCPU/guest/kernel/vm.c)
- [myCPU/guest/include/vm.h](../../../myCPU/guest/include/vm.h)
- [myCPU/guest/kernel/vm_address_space.c](../../../myCPU/guest/kernel/vm_address_space.c)
- [myCPU/guest/kernel/vm_process.c](../../../myCPU/guest/kernel/vm_process.c)
- [myCPU/guest/kernel/vm_object.c](../../../myCPU/guest/kernel/vm_object.c)
- [myCPU/guest/kernel/vm_fault.c](../../../myCPU/guest/kernel/vm_fault.c)
- [myCPU/guest/kernel/course_memory.c](../../../myCPU/guest/kernel/course_memory.c)
- [myCPU/guest/include/course_memory.h](../../../myCPU/guest/include/course_memory.h)

课程 ELF 装载：

- [myCPU/guest/kernel/course_elf_loader.c](../../../myCPU/guest/kernel/course_elf_loader.c)
- [myCPU/guest/include/course_elf_loader.h](../../../myCPU/guest/include/course_elf_loader.h)

文件系统：

- [myCPU/guest/kernel/course_fs.c](../../../myCPU/guest/kernel/course_fs.c)
- [myCPU/guest/include/course_fs.h](../../../myCPU/guest/include/course_fs.h)
- [myCPU/guest/kernel/storage.c](../../../myCPU/guest/kernel/storage.c)
- [myCPU/guest/include/storage.h](../../../myCPU/guest/include/storage.h)

`/proc` 证据面：

- [myCPU/guest/kernel/procfs.c](../../../myCPU/guest/kernel/procfs.c)
- [myCPU/guest/include/procfs.h](../../../myCPU/guest/include/procfs.h)

### 关联测试与验证入口

- [myCPU/tests/unit/vm_address_space.c](../../../myCPU/tests/unit/vm_address_space.c)
- [myCPU/tests/unit/vm_process.c](../../../myCPU/tests/unit/vm_process.c)
- [myCPU/tests/unit/vm_object.c](../../../myCPU/tests/unit/vm_object.c)
- [myCPU/tests/unit/vm_fault.c](../../../myCPU/tests/unit/vm_fault.c)
- [myCPU/tests/unit/course_os_stage1.c](../../../myCPU/tests/unit/course_os_stage1.c)
- [myCPU/tests/unit/course_os_stage2_fd_fs.c](../../../myCPU/tests/unit/course_os_stage2_fd_fs.c)
- [myCPU/tests/unit/course_os_stage3_elf.c](../../../myCPU/tests/unit/course_os_stage3_elf.c)
- [myCPU/tests/unit/course_os_stage3_vm.c](../../../myCPU/tests/unit/course_os_stage3_vm.c)
- [myCPU/tests/unit/course_os_stage3_proc.c](../../../myCPU/tests/unit/course_os_stage3_proc.c)
- [myCPU/tests/unit/course_os_stage3_fs_shell.c](../../../myCPU/tests/unit/course_os_stage3_fs_shell.c)
- [myCPU/tests/unit/kernel_alpha_storage.c](../../../myCPU/tests/unit/kernel_alpha_storage.c)

## 余健超：用户态与交互

### 汇报内容

余健超负责把“用户如何使用这个系统”讲完整：

- 系统调用：讲用户程序不能直接操作内核，需要通过 syscall 请求服务。
- 文件描述符：讲 FD 如何统一文件、终端、输入输出和状态读取。
- 用户程序：讲 `hello`、`echo`、`cat`、`forktest`、`crashdemo` 等课程程序如何证明端到端路径。
- shell：讲 `course-os> ` 如何解析命令、执行内置命令、运行用户程序、处理管道和重定向。
- 浏览器终端：讲前端 `/console` 如何通过 debug server 和 UART terminal 承载 Course OS shell。

### 主要源码

系统调用和 libc wrapper：

- [myCPU/guest/kernel/course_syscall.c](../../../myCPU/guest/kernel/course_syscall.c)
- [myCPU/guest/include/course_syscall.h](../../../myCPU/guest/include/course_syscall.h)
- [myCPU/guest/kernel/course_libc.c](../../../myCPU/guest/kernel/course_libc.c)
- [myCPU/guest/include/course_libc.h](../../../myCPU/guest/include/course_libc.h)

文件描述符：

- [myCPU/guest/kernel/course_fd.c](../../../myCPU/guest/kernel/course_fd.c)
- [myCPU/guest/include/course_fd.h](../../../myCPU/guest/include/course_fd.h)

用户程序：

- [myCPU/guest/kernel/course_user_programs.c](../../../myCPU/guest/kernel/course_user_programs.c)
- [myCPU/guest/include/course_user_programs.h](../../../myCPU/guest/include/course_user_programs.h)
- [myCPU/guest/kernel/user_program.c](../../../myCPU/guest/kernel/user_program.c)
- [myCPU/guest/include/user_program.h](../../../myCPU/guest/include/user_program.h)
- [myCPU/guest/kernel/user_task.c](../../../myCPU/guest/kernel/user_task.c)
- [myCPU/guest/kernel/user_task_bootstrap.c](../../../myCPU/guest/kernel/user_task_bootstrap.c)

shell 和命令分发：

- [myCPU/guest/kernel/course_shell.c](../../../myCPU/guest/kernel/course_shell.c)
- [myCPU/guest/include/course_shell.h](../../../myCPU/guest/include/course_shell.h)
- [myCPU/guest/kernel/course_shell_linux.c](../../../myCPU/guest/kernel/course_shell_linux.c)
- [myCPU/guest/include/course_shell_linux.h](../../../myCPU/guest/include/course_shell_linux.h)
- [myCPU/guest/course_os_shell/main.c](../../../myCPU/guest/course_os_shell/main.c)
- [myCPU/guest/kernel/console_input.c](../../../myCPU/guest/kernel/console_input.c)
- [myCPU/guest/include/console_input.h](../../../myCPU/guest/include/console_input.h)

前端和 debug server 接入：

- [frontend/app/app.js](../../../frontend/app/app.js)
- [frontend/app/components/terminal.js](../../../frontend/app/components/terminal.js)
- [frontend/app/terminal_input_pump.js](../../../frontend/app/terminal_input_pump.js)
- [frontend/server/debug_server.mjs](../../../frontend/server/debug_server.mjs)
- [frontend/server/debug_server_runtime.mjs](../../../frontend/server/debug_server_runtime.mjs)
- [frontend/server/tests_manifest.mjs](../../../frontend/server/tests_manifest.mjs)
- [frontend/shared/terminal_projection.mjs](../../../frontend/shared/terminal_projection.mjs)

### 关联测试与验证入口

- [myCPU/tests/unit/course_os_stage2_syscall.c](../../../myCPU/tests/unit/course_os_stage2_syscall.c)
- [myCPU/tests/unit/course_os_stage2_shell.c](../../../myCPU/tests/unit/course_os_stage2_shell.c)
- [myCPU/tests/unit/course_os_stage2_fd_fs.c](../../../myCPU/tests/unit/course_os_stage2_fd_fs.c)
- [myCPU/tests/unit/course_os_stage3_fs_shell.c](../../../myCPU/tests/unit/course_os_stage3_fs_shell.c)
- [myCPU/tests/unit/course_os_console_input.c](../../../myCPU/tests/unit/course_os_console_input.c)
- [myCPU/tests/host/course_os_shell_terminal_smoke.cpp](../../../myCPU/tests/host/course_os_shell_terminal_smoke.cpp)
- [myCPU/tests/host/terminal_smoke_harness.h](../../../myCPU/tests/host/terminal_smoke_harness.h)
- [frontend/tests/debug_server_runtime.test.mjs](../../../frontend/tests/debug_server_runtime.test.mjs)
- [frontend/tests/render.test.mjs](../../../frontend/tests/render.test.mjs)
- [frontend/tests/terminal_render.test.mjs](../../../frontend/tests/terminal_render.test.mjs)
- [frontend/tests/terminal_state.test.mjs](../../../frontend/tests/terminal_state.test.mjs)

## 交叉边界

| 主题 | 主要讲者 | 边界 |
|---|---|---|
| trap 与 syscall | 梁家琦讲 trap 机制；余健超讲 syscall 接口 | 梁家琦说明“如何进入内核”，余健超说明“进入后提供什么服务”。 |
| ELF 与 exec | 杨皓宇讲课程 ELF loader；余健超讲 shell `exec` 命令 | 杨皓宇讲装载过程，余健超讲用户如何触发。 |
| procfs 与快捷命令 | 杨皓宇讲 `/proc` 数据来源；余健超讲 shell 快捷命令 | 杨皓宇讲证据面，余健超讲交互入口。 |
| Linux 兼容层 | 梁家琦主讲；余健超只讲 shell 入口 | 梁家琦强调有限兼容和边界，余健超只说明 `linux ...` 如何从 shell 进入。 |
| COW | 杨皓宇讲内存页面共享与复制；梁家琦讲 fork 进程语义 | 杨皓宇负责内存机制，梁家琦负责进程生命周期效果。 |
| 浏览器终端 | 余健超主讲；梁家琦现场演示 | 余健超解释接入方式，梁家琦执行 demo。 |

## 现场演示归属

现场演示建议由梁家琦操作，余健超可在旁边补充 shell / 前端机制，杨皓宇可在演示 `fsstat`、`schedstat`、`cpuinfo` 时补充状态来源。

推荐命令：

```text
cpuinfo
schedstat
fsstat
sem
mutex
linux /bin/busybox echo course-os-demo
```

演示口径：

- `cpuinfo`：说明运行平台和 timer 证据。
- `schedstat`：说明调度统计可观察。
- `fsstat`：说明文件系统状态可观察。
- `sem` / `mutex`：说明同步状态可展示。
- `linux /bin/busybox echo course-os-demo`：说明 Linux 兼容层可以验证关键路径，但不等价于完整 Linux。

## PPT 页面对照

| 页码 | 主题 | 讲者 |
|---|---|---|
| 1 | 封面 | 梁家琦 |
| 2 | 项目目标 | 梁家琦 |
| 3 | 系统总览 | 梁家琦 |
| 4 | 程序如何被系统承载 | 杨皓宇 |
| 5 | 内存管理 | 杨皓宇 |
| 6 | 写时复制 | 杨皓宇 |
| 7 | 文件系统与状态文件 | 杨皓宇 |
| 8 | 用户如何操作系统 | 余健超 |
| 9 | 系统调用与文件描述符 | 余健超 |
| 10 | shell 与浏览器终端 | 余健超 |
| 11 | 进程、调度与同步 | 梁家琦 |
| 12 | 中断与异常处理 | 梁家琦 |
| 13 | Linux 兼容层与验证 | 梁家琦 |
| 14 | 演示与总结 | 梁家琦 |
