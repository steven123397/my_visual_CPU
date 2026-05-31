# kernel_alpha Linux 用户态兼容 Plus 设计

## 文档定位

本文档记录《操作系统课程设计》`kernel_alpha` 在 Stage 4 完成后的 plus 方向设计边界。
该 plus 的目标是在本项目 myCPU 模拟器上继续扩展自写 `kernel_alpha` 内核，让 Stage 4
已经接入浏览器 `/console` 的 `course-os> ` 交互 shell 能作为统一入口，尝试启动
testsuits-for-oskernel README 中涉及的 Linux 用户态程序。

这条线不是 QEMU、LoongArch64 或真实开发板适配线，也不是把当前课程级 `course_*`
模块直接声明成 Linux ABI。它是在保留课程 OS 既有 marker 和 shell 体验的前提下，
新增旁路 Linux 用户态兼容后端，让 `course-os> ` 成为课程命令和 Linux 兼容程序的
统一 launcher。

本文档只说明长期有效的边界、现有实现审计结论和启动 plus 前必须守住的结构要求。
具体执行 checklist 应另写 `docs/plan/`，实时状态以
[../status/kernel_alpha_status.md](../status/kernel_alpha_status.md) 为准。

## 关联文档

- 状态文档：
  - [../status/kernel_alpha_status.md](../status/kernel_alpha_status.md)
- 已完成计划：
  - [../plan/history_plan.md#course-os-kernel-alpha-stage5-linux-compat-plus-plan](../plan/history_plan.md#course-os-kernel-alpha-stage5-linux-compat-plus-plan)
- 已完成设计：
  - [course_os_kernel_alpha_stage4_frontend_shell_design.md](course_os_kernel_alpha_stage4_frontend_shell_design.md)
  - [course_os_kernel_alpha_stage3_design.md](course_os_kernel_alpha_stage3_design.md)
  - [course_os_kernel_alpha_stage2_design.md](course_os_kernel_alpha_stage2_design.md)
  - [course_os_kernel_alpha_stage1_design.md](course_os_kernel_alpha_stage1_design.md)
- 背景文档：
  - [../background/操作系统课程设计-A方案-OS内核实现.md](../background/操作系统课程设计-A方案-OS内核实现.md)

## 已完成基线

当前 `kernel_alpha` 已经完成到 Stage 4：

- Stage 1：进程、内存、文件系统 3 模块 9 功能点和只读 `/proc` 指标面。
- Stage 2：syscall ABI、进程生命周期、FD / FS、shell、管道 / 重定向、COW Fork、
  用户态崩溃隔离和扩展 `/proc`。
- Stage 3：教学级 ELF / libc、5 个课程用户程序、FCFS / RR / CFS-lite、semaphore /
  mutex、Sv39 COW 证据、`mkfs` / `seek` / `unlink` / `rmdir`、shell 脚本和
  `/proc/<pid>` 证据面。
- Stage 4：独立 `guest_course_os_shell_demo`、`course-os> ` prompt、浏览器 `/console`
  manifest / terminal / Lab workbench 接入。

Plus 启动必须保持 `kernel_alpha_demo` 的 Stage 1 / Stage 2 / Stage 3 marker、Stage 4
shell prompt、functional / pipeline guest 回归和旧 9 条负向 demo 稳定。

## 背景与问题

Stage 4 已经把课程 OS shell 接到 `/console`，用户能在 `course-os> ` 里观察课程 shell、
FD / FS、procfs、ELF / libc、COW 和 crash isolation 能力。后续 plus 的展示价值不在于
再开一个互不相干的 `linux-compat> ` prompt，而在于让同一个课程 OS shell 看起来像一个
更完整的系统入口：课程命令继续可用，同时可以输入 `busybox --help`、`git -h`、`vim -h`
或后续更复杂的 Linux 用户态命令。

但实现上必须避免把两套 ABI 混成一团。课程程序仍使用 `COURSE_SYSCALL_*` 和课程级 FS /
进程语义；Linux 兼容程序必须使用 Linux syscall number、Linux errno、真实 ELF/rootfs
加载和独立的地址空间 / 用户栈 / 文件描述符合同。`course-os> ` 只是统一前端和 launcher，
不是 Linux shell 完整实现。

## 目标

- 在 plus 入口中保留 `course-os> ` 作为主要人机入口，而不是把 `linux-compat> ` 作为最终用户主入口。
- 允许在 `course-os> ` 输入 Linux 用户态命令，例如 `linux /bin/busybox --help`、
  `linux /usr/bin/git -h`，稳定后再支持直接输入 `git -h`、`vim -h`、`gcc -h`。
- 通过进程级 ABI 标记区分课程程序和 Linux 兼容程序，让同一 trap / user runtime 能按当前进程 ABI 分发 syscall。
- 从外部 rootfs / 文件资产加载真实 RISC-V64 Linux 用户态 ELF，逐步支持静态 ELF 和最小动态 ELF。
- 对未支持 syscall、非法 ELF、坏路径和用户态崩溃 fail-closed，输出 syscall number、PC、errno、path
  等可诊断字段。
- 继续守住 Stage 1 / Stage 2 / Stage 3 marker、Stage 4 shell prompt、functional / pipeline guest
  回归和旧 9 条负向 demo。

## 非目标

- 不把 `course_syscall`、`course_shell`、`course_user_programs` 直接改造成 Linux ABI 实现。
- 不声明完整 POSIX shell、job control、完整 signal/futex、完整动态链接器、完整 termios 或通用发行版兼容。
- 不把 `linux-compat> ` 作为最终用户主入口；如保留独立 prompt 或独立 guest target，它只服务 bring-up
  smoke、诊断和测试隔离。
- 第一刀不绑定网络相关 `git clone/push/pull`、完整 `vim` 终端编辑、`rustc` 编译或完整 `gcc` 工具链闭环。
- 第一刀不把课程 RAMFS 伪装成真实 Linux FS，也不使用固定 `ls` 文本替代真实目录遍历。

## 现有实现审计结论

现有实现适合作为 plus 的启动基座，但不能把课程级 `course_*` 模块直接扩成 Linux ABI。
以下简化是为了课程设计目标做出的合理取舍，也是 plus 第一阶段必须隔离处理的地方：

- ELF / 程序来源仍是课程 catalog：`course_user_programs.c` 里内嵌固定小 ELF 和 5 个
  课程程序名，不支持从文件系统加载真实 `/bin/git`、`/usr/bin/gcc` 或动态 loader。
- ELF loader 只接受 RV64 little-endian `ET_EXEC` 和 `PT_LOAD`，没有 `PT_INTERP`、
  PIE / `ET_DYN`、auxv、TLS、真实 argv/envp 栈布局或动态链接器协作。
- syscall 表是课程 ABI，不是 Linux syscall ABI；当前只有 `read/write/open/close/seek`
  和少量进程接口，缺少 `openat`、`newfstatat`、`getdents64`、`mmap`、`munmap`、
  `mprotect`、`brk`、`ioctl`、`fcntl`、`poll`、`clock_gettime`、`getrandom`、
  `futex`、`rt_sig*`、`execve` 等真实用户态常用接口。
- 进程模型是固定小表和教学生命周期，当前上限、argv 长度、用户页数量和 COW 页数量都很小；
  `exec` 依赖内置程序查找，不是路径解析 + 文件系统 ELF 加载。
- 文件系统是教学 RAMFS / 简化 FS，固定节点数、文件名长度和单文件容量，不具备真实 ext4
  元数据、权限、时间戳、link / symlink、rename 覆盖、fsync / sync、目录遍历语义。
- FD 层只有普通文件、stdio 和只读 procfs，缺少 Linux 工具常用的 `O_*` flag 语义、
  `dup`、`pipe2`、`fcntl`、`stat` family、`isatty` / termios 等。
- shell 是课程 parser，只支持固定参数数量、单级 pipe、简单重定向和脚本逐行执行；
  Stage 4 明确不声明 POSIX shell、job control 或 Linux shell。
- VM 基础设施可复用 Sv39、address space、object、fault handler，但课程进程里的用户页 /
  COW 证据仍主要是教学模型；plus 不能继续只用统计型 COW，必须让 Linux ELF 映射真实进入
  address-space / page-fault 路径。
- trap frame 当前保存通用寄存器，未形成 Linux 用户态需要的完整 signal frame、sigreturn、
  futex sleep / wake 或 FPU/vector lazy context 管理。
- `SimpleStorage` 支持 host-backed block 读写，但课程 FS 没有接真实块设备；plus 若复用
  testsuits rootfs，应新增只读或最小读写 rootfs 访问层，而不是继续扩大 RAMFS 假象。

这些不是当前 Stage 4 的缺陷；它们是 plus 必须显式剥离出来的新边界。

## 约束与边界

- Plus 采用“统一 shell 前端、旁路 Linux 后端、进程 ABI 分流、不改课程 marker”的方式启动。
- `course-os> ` 是主要用户入口；课程内置命令、课程用户程序和 procfs 快捷命令优先保持原语义。
- Linux 兼容层应新增 `linux_compat_*` 或 `course_plus_linux_*` 模块，放在
  `myCPU/guest/kernel/` 和 `myCPU/guest/include/`，由 shell launcher 调用，不反向污染
  `course_syscall`、`course_shell` 或 `course_user_programs`。
- 可新增独立 guest smoke target，例如 `guest_kernel_alpha_linux_compat_demo` 或
  `guest_course_os_linux_compat_shell_demo`；它可以启动同一个 `course-os> ` shell 并开启 plus
  launcher，也可以暴露调试用 `linux-compat>`，但 `kernel_alpha_demo` 和已有
  `guest_course_os_shell_demo` 的完成态语义不得被 plus 需求改写。
- Linux 用户态从 `a7` 读取 Linux syscall number，返回 Linux errno 风格结果；课程程序继续走
  `COURSE_SYSCALL_*`。
- 先从外部 rootfs / 文件资产中加载真实 ELF，建立 path -> inode/file -> ELF loader 的链路；
  内置 catalog 只保留给课程 Stage 3。
- 文件系统第一阶段可以只读或最小读写，但必须返回 Linux 工具可接受的 `stat`、目录项、
  权限、时间戳和 seek 语义；不能用固定文本冒充目录遍历。

## 方案

### 统一入口体验

Plus 最终面向用户的体验应以 `course-os> ` 为准：

```text
course-os> linux /bin/busybox --help
BusyBox v...
course-os> linux /usr/bin/git -h
usage: git ...
course-os> git -h
usage: git ...
```

第一阶段优先支持显式 `linux <path-or-command> [args...]`，这样课程命令解析和 Linux 兼容路径
不会相互误判。显式路径稳定后，再增加 fallback：当命令不是课程内置命令、不是课程用户程序、
也不是课程 shell 关键字时，shell launcher 才查询 Linux rootfs 的 `$PATH` 映射并尝试以
Linux 兼容进程启动。

### 命令解析与分流

`course-os> ` 的解析顺序固定为：

1. 课程 shell 内置命令，例如 proc 快捷命令、脚本控制和课程 FS 操作。
2. 课程 Stage 3 用户程序 catalog。
3. 显式 `linux ...` launcher。
4. 稳定后可选的 Linux rootfs fallback，例如 `git -h`、`busybox --help`、`vim -h`。

该顺序保证课程 OS 既有展示不被 Linux rootfs 中同名命令覆盖。若发生歧义，第一阶段应要求
用户使用 `linux /path/to/program ...` 显式指定 Linux 兼容路径。

### 进程 ABI 标签

新增进程级 ABI 标记，例如：

```text
COURSE_ABI
LINUX_COMPAT_ABI
```

课程 shell 自身、课程内置程序和 Stage 3 catalog 程序继续使用 `COURSE_ABI`。通过
`linux ...` launcher 或 Linux rootfs fallback 启动的进程使用 `LINUX_COMPAT_ABI`。
用户态 `ecall` 回到 trap 层后，dispatcher 按当前进程 ABI 分发：

```text
COURSE_ABI        -> course_syscall_dispatch()
LINUX_COMPAT_ABI -> linux_compat_syscall_dispatch()
```

ABI 标记必须跟随进程生命周期、`fork` / `exec` / `exit` / `wait`、FD 表和 crash 诊断流转。
Linux 兼容进程退出后，控制权回到同一个 `course-os> ` prompt。

### Linux ELF 与用户栈合同

Linux 兼容 loader 不复用课程 catalog 语义。它应新增第二套合同，逐步支持：

- path -> file -> ELF header / program-header 读取。
- RV64 little-endian 静态 ELF。
- `ET_DYN`、`PT_INTERP` 和最小动态 loader 协作。
- argv/envp 用户栈布局、auxv、TLS、随机数来源的最小策略或 fail-closed 策略。
- `brk` 初值、`mmap` 区域管理和真实 address-space / page-fault 路径。
- loader 诊断：ELF class、machine、type、interp、entry、load segments、拒绝原因。

### Linux syscall 最小子集

第一刀只实现帮助输出和低风险读路径需要的最小 syscall trace：

- `write`
- `exit` / `exit_group`
- `brk`
- `mmap`
- `munmap`
- `openat`
- `read`
- `close`
- `newfstatat`
- `getdents64`
- `lseek`
- `clock_gettime`

后续进入 `git init/add/commit/log` 和 `gcc hello.c` 前，再按真实 trace 补 `fcntl`、`ioctl`、
`getrandom`、`rt_sig*`、`futex`、`execve`、`wait4`、`renameat2`、`unlinkat`、`mkdirat`、
`fsync` / `sync` 等接口。每个新增 syscall 都应先有 trace / regression 再实现最小语义。

### 文件系统与终端桥接

Linux 兼容层需要一条独立的 rootfs 访问层。第一阶段可以是只读或最小读写，但必须提供真实
目录项和 metadata：

- 普通文件、目录、权限、时间戳、文件大小和 seek。
- `stat` family 与 `getdents64` 可被 Linux 工具接受。
- stdout / stderr 接回当前 `course-os> ` UART terminal。
- stdin 第一阶段可只支持简单输入或显式拒绝交互式需求。
- 未支持 termios / tty ioctl 时输出明确诊断，避免完整 `vim` 交互被误判为已支持。

## 推荐第一刀

第一刀目标应小而真实：

1. 新增 plus plan 和 plus-enabled shell smoke target，启动后仍显示 `course-os> `。
2. 在 `course-os> ` 增加显式 `linux <path-or-command> [args...]` launcher，不启用自动 fallback。
3. 接入外部 rootfs 资产读取，能打开并读取一个真实 RISC-V64 用户态 ELF 文件。
4. 支持静态或最小动态 ELF 的 header / program-header 解析，生成真实用户栈摘要。
5. 实现最小 Linux syscall trace：`write`、`exit` / `exit_group`、`brk`、`mmap`、`munmap`、
   `openat`、`read`、`close`、`newfstatat`、`getdents64`、`lseek`、`clock_gettime`。
6. 先验收 `linux /bin/busybox --help` 或 `linux /usr/bin/git -h` / `linux /usr/bin/vim -h`
   这类低风险帮助输出。
7. 显式 launcher 稳定后，再考虑 `course-os> git -h` 形式的自动 Linux rootfs fallback。

网络相关 `git clone/push/pull`、完整 `vim` 终端编辑、`rustc` 编译和完整 signal / futex
应单独作为后续阶段，不绑定 plus 第一刀。

## 验证要求

Plus 每一刀都必须同时守住：

- `cd myCPU && make test-guest-kernel_alpha_demo`
- `cd myCPU && make test-guest-course_os_shell_demo`
- `cd myCPU && make test-pipeline-guest-kernel_alpha_demo`
- `cd myCPU && make test-pipeline-guest-course_os_shell_demo`
- 对应新增 plus unit / guest target
- `git diff --check`

新增 plus smoke 至少应覆盖：

- `course-os> linux /bin/busybox --help` 或等价真实 Linux ELF 帮助输出。
- 未支持 syscall fail-closed 诊断。
- 坏 ELF / 坏路径 / 坏用户指针诊断。
- Linux 兼容进程退出后回到 `course-os> ` prompt。
- 课程内置命令仍优先于 Linux rootfs fallback。

触及 VM、trap、syscall、loader、FS 或 storage 时，还应按根 `AGENTS.md` 和
`myCPU/AGENTS.md` 补跑相关单元门禁。

## 风险与取舍

- 如果过早把 `git`、`vim`、`gcc` 做成直接命令 fallback，会让课程 shell 命令解析和 Linux
  rootfs PATH 解析互相污染；因此第一刀先用显式 `linux ...` launcher。
- 如果把 Linux syscall 塞进 `course_syscall`，后续 errno、FD、signal、mmap 和 exec 语义会和
  课程 ABI 混淆；因此必须使用进程 ABI 标签分流。
- 如果只支持固定文本输出，会失去 testsuits-for-oskernel 的真实兼容价值；因此目录遍历、
  `stat` 和 ELF 读取必须来自真实 rootfs / 文件资产。
- 如果直接追求完整 `vim`、`rustc` 或网络 `git`，会把 tty、signal、futex、工具链、网络和
  文件系统一致性问题同时引入；因此第一刀只证明帮助输出和最小 Linux syscall trace。
- 如果 plus 修改了 Stage 1-4 完成态语义，就会破坏课程 OS 证据链；因此已有 demo 和 marker
  必须继续作为回归门禁。

## 当前有效性说明

- 当前有效：本文档作为 Stage 4 完成后开启 Linux 用户态兼容 plus 的设计边界。
- 当前不是完成声明：`kernel_alpha` 仍未声明 Linux 用户态兼容，plus 尚未实现。
- 当前可复用基础：bring-up、Sv39、trap、user runtime、course shell、FD / FS、procfs、
  storage guardrail 和 frontend terminal 合同。
