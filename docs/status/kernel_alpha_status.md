# kernel_alpha 状态

## 文档定位

本文档只记录 `kernel_alpha` 的当前定位、课程 OS 当前完成态、当前仍有效的限制和下一步。

它不再维护逐条执行流水账；更细的实现过程统一回写到 [../plan/history_plan.md](../plan/history_plan.md)。

## 关联文档

- 相关设计：
  - [../design/course_os_kernel_alpha_linux_compat_plus_design.md](../design/course_os_kernel_alpha_linux_compat_plus_design.md)
  - [../design/course_os_kernel_alpha_course_os_baseline_design.md](../design/course_os_kernel_alpha_course_os_baseline_design.md)
  - [../design/regression_completion_criteria.md](../design/regression_completion_criteria.md)
  - [../design/platform_mmio_contract.md](../design/platform_mmio_contract.md)
- 当前计划：
  - [../plan/course_os_kernel_alpha_stage11_writable_rootfs_process_file_plan.md](../plan/course_os_kernel_alpha_stage11_writable_rootfs_process_file_plan.md)
- 相关状态：
  - [mainline_status.md](mainline_status.md)
- 已完成计划归档：
  - [../plan/history_plan.md#course-os-kernel-alpha-stage10-oscomp-help-run-plan](../plan/history_plan.md#course-os-kernel-alpha-stage10-oscomp-help-run-plan)
  - [../plan/history_plan.md#course-os-kernel-alpha-stage9-linux-compat-real-exec-plan](../plan/history_plan.md#course-os-kernel-alpha-stage9-linux-compat-real-exec-plan)
  - [../plan/history_plan.md#course-os-kernel-alpha-stage8-linux-compat-loader-trace-plan](../plan/history_plan.md#course-os-kernel-alpha-stage8-linux-compat-loader-trace-plan)
  - [../plan/history_plan.md#course-os-kernel-alpha-stage7-linux-compat-external-rootfs-plan](../plan/history_plan.md#course-os-kernel-alpha-stage7-linux-compat-external-rootfs-plan)
  - [../plan/history_plan.md#course-os-kernel-alpha-stage6-linux-compat-rootfs-syscall-plan](../plan/history_plan.md#course-os-kernel-alpha-stage6-linux-compat-rootfs-syscall-plan)
  - [../plan/history_plan.md#course-os-kernel-alpha-stage5-linux-compat-plus-plan](../plan/history_plan.md#course-os-kernel-alpha-stage5-linux-compat-plus-plan)
  - [../plan/history_plan.md#course-os-kernel-alpha-stage4-frontend-shell-plan](../plan/history_plan.md#course-os-kernel-alpha-stage4-frontend-shell-plan)
  - [../plan/history_plan.md#course-os-kernel-alpha-stage3-plan](../plan/history_plan.md#course-os-kernel-alpha-stage3-plan)
  - [../plan/history_plan.md#course-os-kernel-alpha-stage2-plan](../plan/history_plan.md#course-os-kernel-alpha-stage2-plan)
  - [../plan/history_plan.md#course-os-kernel-alpha-stage1-plan](../plan/history_plan.md#course-os-kernel-alpha-stage1-plan)
  - [../plan/history_plan.md#kernel-alpha-storage-error-contract-plan](../plan/history_plan.md#kernel-alpha-storage-error-contract-plan)

## 当前状态

`kernel_alpha` 当前已经从 Phase 1 bring-up demo 切换为《操作系统课程设计》A 方案主线入口；旧
`KMVPETDS` 只保留为历史 guardrail，不再描述当前正向能力。
第一阶段按 [../design/course_os_kernel_alpha_course_os_baseline_design.md](../design/course_os_kernel_alpha_course_os_baseline_design.md)
落地进程、内存、文件系统 3 个模块的 9 个功能点，并提供只读 `/proc` 指标证据面。

第二阶段按 [../design/course_os_kernel_alpha_course_os_baseline_design.md](../design/course_os_kernel_alpha_course_os_baseline_design.md)
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

第三阶段按 [../design/course_os_kernel_alpha_course_os_baseline_design.md](../design/course_os_kernel_alpha_course_os_baseline_design.md)
完成“课程满分基线真实化”：教学级 ELF / libc、真实用户程序、FCFS / RR 指标化、
semaphore / mutex、Sv39 fault-driven COW 证据链、`mkfs` / `seek` / `unlink` / `rmdir`、
shell 脚本模式，以及 `/proc/cpuinfo`、`/proc/uptime`、`/proc/<pid>/status`、`/proc/<pid>/fd`、
`/proc/<pid>/maps`。

当前 `guest_kernel_alpha_demo` 正向输出为 Stage 1 + Stage 2 + Stage 3 串联：

- `KMVPET|course-os-stage1 sched=CFS-lite ctx=9 pf=4 reclaim=1 fs_create=5 btree_steps=48 proc=ps/meminfo/schedstat/fsstat|course-os-stage2 syscall=ok shell=ok procs=ok fd=ok fs=128/64K/3 pipe=ok cow=ok crash=isolated proc=ps/meminfo/schedstat/fsstat/syscalls/cow/crashlog|course-os-stage3 elf=5 libc=ok sched=fcfs/rr/cfs sync=sem/mutex vm=sv39-cow fs=seek/mkfs shell=script proc=cpuinfo/uptime/pid`

Stage 3 稳定 marker 为：

- `course-os-stage3 elf=5 libc=ok sched=fcfs/rr/cfs sync=sem/mutex vm=sv39-cow fs=seek/mkfs shell=script proc=cpuinfo/uptime/pid`

Stage 4 已完成前端交互 shell 接入。该阶段没有修改 `kernel_alpha_demo` 的一次性
Stage 1 / Stage 2 / Stage 3 summary，而是新增独立 `guest_course_os_shell_demo`，
启动后进入 `course-os> ` prompt，并通过 `/console` 的 manifest / terminal / Lab workbench
展示 Stage 3 已完成的课程 shell、FD / FS、procfs、ELF / libc、COW 和 crash isolation 能力。
前端继续复用现有 debug session 和 UART terminal 合同，没有新增并行执行协议。

Stage 4 之后的 plus 方向已补充为独立设计边界：在本 myCPU 模拟器上继续扩展自写
`kernel_alpha` 内核，尝试运行 testsuits-for-oskernel README 中涉及的 Linux 用户态程序。
这条 plus 线必须旁路新增 Linux ABI / ELF / FS / syscall 模块，不把课程级 `course_*`
实现直接膨胀成通用 Linux 兼容层，也不改变 Stage 1 / Stage 2 / Stage 3 marker 和
Stage 4 shell prompt。

Stage 5 v0 已完成 Linux compat Plus 第一刀。`course-os> ` 现在新增显式
`linux <path-or-command> [args...]` launcher；`linux /bin/busybox --help` 和
`linux /usr/bin/git -h` 会进入旁路 `linux_compat_*` 后端，经过最小 rootfs catalog、
RV64 little-endian ELF header / program-header inspection、进程 ABI 标记和
fail-closed syscall 诊断后回到同一个 `course-os> ` prompt。直接 `git -h` 自动 fallback
在 Stage 5 当时尚未启用，课程命令和 Stage 3 catalog 仍优先保持原语义。

Stage 6 已完成 Linux compat Plus 的 rootfs / syscall 第一层语义。`linux_compat_*`
现在提供最小 rootfs metadata、`stat`、FD 表、`openat/read/lseek/close`、`getdents64`、
`brk`、`mmap`、`write`、`clock_gettime`、`exit_group` 和 unsupported syscall fail-closed
合同；`linux /bin/busybox --help` 与 `linux /usr/bin/git -h` 不再停在 unsupported syscall，
而是通过这组最小 syscall 路径输出稳定 help 文本后回到同一个 `course-os> ` prompt。
该阶段仍使用内置最小 rootfs catalog 和模拟 help 路径，不声明外部真实 rootfs、动态链接器
或通用 Linux 用户态兼容已经完成。

Stage 7 已完成 Linux compat 外部 rootfs 资产链路。显式 opt-in target 现在可以从外部目录
或 ext4 rootfs 提取 `/bin/busybox`、`/usr/bin/git`，生成可编译 C provider，并让
`course-os> linux ...` 使用真实 RV64 ELF bytes 和 metadata 进入现有 Linux compat lookup /
stat / read / ELF inspect / help 路径。默认 `make test` 仍使用 builtin provider，不依赖外部
rootfs、`debugfs` 或本机特定镜像。

Stage 8 已完成 Linux compat loader / trace 收口。`linux_compat_loader` v0 现在能为 RV64
`ET_EXEC` / `ET_DYN`、`PT_LOAD` 和 `PT_INTERP` 生成只读 load plan，并把显式
`course-os> linux ...` run path 的输出扩展为 `loader=static|dynamic`、`interp=<path|none>`、
`segments=<n>`、`stack=argv/envp/auxv` 和 `trace=...` 诊断。外部 rootfs generator 新增
optional interpreter asset 记录，manifest 能区分 required / optional path 的 present / missing；
`linux_compat_runtime` 也新增固定上限 syscall trace record buffer，为后续真实 trace-driven
syscall 扩展提供稳定观察面。该阶段仍不声明真实动态链接器运行、真实 ELF 执行、完整 Linux
syscall 面、rootfs 写语义或自动 `git -h` fallback 已完成。

Stage 9 已完成 Linux compat 真实 ELF 执行第一刀。显式
`course-os> linux ...` launcher 现在结束 Stage 5-8 的硬编码 help 字符串和模拟 syscall
序列；静态 RV64 ELF 会经过真实 PT_LOAD 段映射、argv / envp / auxv 用户栈、U-mode
入口、真实 ecall dispatch、UART `write` 和 `exit_group` 闭环后回到同一个
`course-os> ` prompt。端到端验收覆盖 hand-crafted 最小 ELF、`linux /bin/busybox --help`、
`linux /bin/busybox echo hello` 和 `linux /usr/bin/git -h`，输出中包含 `exec=real` 与真实
syscall trace。Stage 9 当时仍不启用直接命令 fallback，不声明动态链接器运行、完整 Linux
syscall 面、完整 signal / futex、rootfs 写语义或通用 Linux 用户态兼容。

Stage 10 已完成 OSComp Linux 用户态 help-run 基线。`course-os> git -h` 和
`course-os> git help` 现在会在课程内置命令与 Stage 3 课程用户程序均未命中后，通过受限
Linux PATH fallback 解析到 `/usr/bin/git`，进入真实 Linux compat exec 路径，并在输出
`usage: git`、`exec=real` 和真实 syscall trace 后回到同一个 `course-os> ` prompt。
`course-os> vim -h`、`course-os> gcc --h`、`course-os> rustc -h` 在 builtin provider
缺少对应资产时不再落回课程 shell 裸 `error`，而是进入 Linux compat fail-closed 诊断，
输出 resolved path、`errno=2`、`path: no such file` 和 prompt 回归。Stage 10 同时补齐
OSComp help-run 所需的 provider manifest、direct fallback、dynamic-loader v0 元数据和
最小只读 / 低副作用 syscall 面；默认回归仍不依赖外部 rootfs。

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

- `2026-06-03`
  - Stage 11 已完成第二个可验证切片：`linux_compat_runtime_t` 现在带 session-local writable
    rootfs overlay v0，支持 `openat(O_CREAT|O_TRUNC|O_WRONLY)`、`write` / `pwrite64`、
    `ftruncate`、`fsync` / `fdatasync` / `sync`、`mkdirat`、`unlinkat`、`renameat` /
    `renameat2`、`newfstatat` / `fstat` / `statx`、目录枚举和 bad path / bad fd / lower
    readonly guardrail，并同步补齐真实 U-mode ecall 到 Linux compat syscall request 的参数映射；
    新增 `test-unit-course_os_stage11_linux_compat` 并扩展 `test-unit-trap_dispatch` 固定该合同。
  - Stage 11 已完成首个可验证切片：external asset preflight 现在能区分 Stage 11 required
    tools、gcc toolchain 子资产、optional `rustc`、optional interpreter 和 optional shared
    assets；external rootfs smoke 的 direct `git -h` fallback 已对齐 Stage 10 当前合同；
    新增 `test-host-course_os_linux_compat_external_workflow_smoke` 作为 external-only 工作流红灯。
  - 课程 OS Stage 10 OSComp help-run 基线完成，新增 `git -h` / `git help` 直接命令
    fallback real-exec host smoke，并把 `vim -h`、`gcc --h`、`rustc -h` 在 builtin provider
    缺资产时的 fail-closed Linux compat 诊断固定为回归合同。
  - Stage 10 同步补齐 external provider Stage 10 required path / shared asset manifest、
    `PT_INTERP` dynamic-loader v0、`AT_BASE` 等 auxv 诊断，以及 help-run trace 证明需要的
    最小 syscall 面；课程命令优先级和 Stage 1 / Stage 2 / Stage 3 marker 不变。
  - 课程 OS Stage 9 Linux compat 真实 ELF 执行第一刀完成，`linux_compat_run()` 不再对
    `/bin/busybox` / `/usr/bin/git` 使用硬编码 help 文本或模拟 syscall 序列；显式 launcher
    的静态 ELF 路径统一进入 `linux_compat_exec_load()`、`linux_compat_exec_build_stack()` 和
    `linux_compat_exec_enter()`。
  - 新增/收紧 host smoke 只匹配命令后的 UART 增量，验证 `minimal-elf`、busybox `--help`、
    busybox `echo hello` 和 `git -h` 都出现 `exec=real`，通过真实 `write` / `exit_group`
    回到 `course-os> ` prompt；直接 `git -h` fallback 继续关闭。
- `2026-05-31`
  - 课程 OS Stage 8 Linux compat loader / trace 完成，新增只读 load-plan v0、optional
    interpreter asset manifest、run path loader 诊断和 syscall trace record v0。
  - Stage 8 保持显式 `linux ...` launcher、Stage 1 / Stage 2 / Stage 3 marker、Stage 4
    `course-os> ` prompt、Stage 5 / Stage 6 / Stage 7 Linux compat guardrail 和直接
    `git -h` fallback 关闭状态不变。
- `2026-05-30`
  - 课程 OS Stage 4 前端交互 shell 完成，新增独立 `guest_course_os_shell_demo`、
    `course-os> ` prompt、proc 快捷命令、functional / pipeline guest 回归、
    `/console` manifest、Course OS Shell Lab 卡片和 terminal 文案。
  - Stage 4 保持 `kernel_alpha_demo` 的 Stage 1 / Stage 2 / Stage 3 marker 不变，并继续保留旧 9 条负向 demo 作为基础设施 guardrail。
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
- AI/NPU、JIT/DBT、Pipeline-aware scheduling、微内核和安全隔离不进入 Stage 3 / Stage 4 完成范围。
- Stage 4 `guest_course_os_shell_demo` 是课程级 shell 展示入口，不声明完整 POSIX shell、完整 Linux shell、job control 或通用用户态兼容。
- 旧 9 条负向 guest 回归仍是基础设施 guardrail；当前正向 `kernel_alpha_demo` 已不再检查旧 `D/S` marker。
- `SimpleStorage` 仍然是单块、同步、无 completion interrupt、无宿主持久化回写的最小模型。
- `/proc` 第三阶段仍保持只读证据面，不作为调度、内存或文件系统的写控制接口。
- Stage 10 只证明 OSComp help-run 基线、`git -h` / `git help` 直接 fallback real-exec、
  builtin provider 缺 `vim` / `gcc` / `rustc` 资产时的 fail-closed 诊断，以及 dynamic-loader
  v0 / 最小 syscall 的受限门禁。Stage 11 当前仅新增 session-local writable overlay v0，
  尚未把 cwd、relative path、shell workflow、process / wait / exec 和 TTY 串成端到端工作流；
  当前仍不支持完整 testsuits-for-oskernel、网络
  `git clone/push/pull`、`git init/add/commit/log`、交互式 `vim hello.c`、`gcc hello.c &&
  ./a.out`、`rustc helloworld.rs && ./helloworld`、完整 `execve` / `wait4` / `futex` /
  signal 或完整 TTY。
- 课程级 ELF catalog、课程 syscall ABI、RAMFS、固定小进程表、教学 COW 和课程 shell 仍不能直接
  声明为 Linux ABI 兼容层；Linux ABI 扩展必须继续走旁路 `linux_compat_*` 模块和进程 ABI 分流。

## 下一步

1. 保持 Stage 1 / Stage 2 / Stage 3 marker、Stage 4 shell prompt、functional / pipeline `kernel_alpha_demo` 和旧 9 条负向 demo 稳定。
2. 继续执行 [../plan/course_os_kernel_alpha_stage11_writable_rootfs_process_file_plan.md](../plan/course_os_kernel_alpha_stage11_writable_rootfs_process_file_plan.md)，下一刀从任务 3 cwd、relative path 和 `course-os> ` workflow shell 开始，最终目标仍是收口 `git init/add/commit/log`、`vim hello.c`、`gcc hello.c && ./a.out`。
3. Stage 12 再推进 virtio-net、socket、DNS、SSH / TLS 或最小 git remote path，目标放到 `git clone/push/pull`，不混入 Stage 11。
4. Stage 13 再处理 `rustc` 大内存 / 重工具链闭环和稳定性，不把 Rust 编译成功作为 Stage 11 完成条件。
5. 后续新增 Linux 语义继续放在旁路 `linux_compat_*`，按真实 trace 补能力，不直接改大 `course_*` 教学模块；AI/NPU、JIT/DBT 或 Pipeline-aware 调度继续作为独立后续方向。
6. 保留旧 Phase 1 负向 demo 作为基础设施 guardrail；除非真实 bug 或课程 OS 迁移需要，不继续扩旧 bring-up marker 面。

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
- Stage 4 当前固定门禁：
  - `cd myCPU && make test-unit-course_os_stage2_shell`
  - `cd myCPU && make test-unit-course_os_stage3_fs_shell`
  - `cd myCPU && make test-unit-course_os_stage3_proc`
  - `cd myCPU && make test-host-course_os_shell_terminal_smoke`
  - `cd myCPU && make test-guest-course_os_shell_demo`
  - `cd myCPU && make test-pipeline-guest-course_os_shell_demo`
  - `cd frontend && node --test`
- Stage 5 / Stage 6 / Stage 7 / Stage 8 / Stage 9 / Stage 10 Linux compat 当前固定门禁：
  - `cd myCPU && make test-unit-course_os_stage5_linux_compat`
  - `cd myCPU && make test-unit-course_os_stage6_linux_compat`
  - `cd myCPU && make test-unit-course_os_stage8_linux_compat_loader`
  - `cd myCPU && make test-unit-course_os_stage9_linux_compat_vm`
  - `cd myCPU && make test-unit-course_os_stage9_linux_compat_exec`
  - `cd myCPU && make test-unit-course_os_stage9_linux_compat_syscall`
  - `cd myCPU && make test-unit-course_os_stage10_linux_compat`
  - `cd myCPU && make test-unit-course_os_stage3_fs_shell`
  - `cd myCPU && make test-host-course_os_linux_compat_terminal_smoke`
  - `cd myCPU && make test-host-course_os_linux_compat_minimal_elf_smoke`
  - `cd myCPU && make test-host-course_os_linux_compat_oscomp_help_smoke`
  - `cd myCPU && make test-guest-course_os_linux_compat_shell_demo`
  - `cd myCPU && make test-pipeline-guest-course_os_linux_compat_shell_demo`
