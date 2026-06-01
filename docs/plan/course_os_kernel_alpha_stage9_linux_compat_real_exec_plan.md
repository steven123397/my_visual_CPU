# 课程 OS kernel_alpha Stage 9 Linux compat 真实 ELF 执行计划

> **文档状态：** 执行中

> **2026-06-01 阶段性 checkpoint：** 已完成并验证 Slice A（`linux_compat_vm` 真实
> `brk` / `mmap` / `munmap`）、Slice B（ELF PT_LOAD 映射 + 用户栈构建单元面）和
> Slice D（`fcntl` / `getrandom` / `clock_gettime` / `ioctl` 单元面）。当前已通过：
> `make test-unit-course_os_stage9_linux_compat_vm`、
> `make test-unit-course_os_stage9_linux_compat_exec`、
> `make test-unit-course_os_stage9_linux_compat_syscall`、
> `make test-host-course_os_linux_compat_terminal_smoke` 和 `git diff --check`。
> 仍未完成：`test-host-course_os_linux_compat_minimal_elf_smoke` 当前在
> `trace=write/exit_group` 等待点超出 step budget；真实 U-mode `exit_group` 闭环与静态
> busybox `--help` / `echo hello` 端到端验收不能标完成。

## 文档定位

本文档安排 Stage 8 之后的 `kernel_alpha` Linux compat Plus 下一阶段工作。Stage 9 定位为
**"真实 ELF 执行"**：结束 Stage 5-8 的硬编码 help 字符串 + 模拟 syscall 序列路线，
让静态链接的 RV64 ELF 通过真实的段映射 → 用户栈构建 → U-mode 执行 → ecall dispatch →
写 UART → exit_group 闭环，输出真实帮助文本后回到 `course-os> ` prompt。

Stage 9 仍不启用直接 `git -h` / `vim -h` / `gcc -h` fallback，不声明动态链接器运行、
完整 Linux syscall 面、完整 signal / futex 或 rootfs 写语义。

## 关联文档

- 来源设计：
  - [../design/course_os_kernel_alpha_linux_compat_plus_design.md](../design/course_os_kernel_alpha_linux_compat_plus_design.md)
- 目标状态：
  - [../status/kernel_alpha_status.md](../status/kernel_alpha_status.md)
- 前置完成态：
  - [history_plan.md#course-os-kernel-alpha-stage8-linux-compat-loader-trace-plan](history_plan.md#course-os-kernel-alpha-stage8-linux-compat-loader-trace-plan)
  - [history_plan.md#course-os-kernel-alpha-stage7-linux-compat-external-rootfs-plan](history_plan.md#course-os-kernel-alpha-stage7-linux-compat-external-rootfs-plan)
  - [history_plan.md#course-os-kernel-alpha-stage6-linux-compat-rootfs-syscall-plan](history_plan.md#course-os-kernel-alpha-stage6-linux-compat-rootfs-syscall-plan)

## 当前状态诊断

Stage 5-8 已完成 rootfs 查找、ELF header 检查、load-plan 构建（ET_EXEC / ET_DYN / PT_LOAD / PT_INTERP）、
ABI 分流（COURSE_ABI / LINUX_COMPAT_ABI）、syscall dispatch 骨架、trace record buffer 和外部 rootfs
资产链路。但 `linux_compat_run()` 的核心逻辑仍是**硬编码 help 字符串 + 模拟 syscall 序列**——从未执行过
真实的 ELF 指令。关键缺口：

| 缺口 | 现状 | 导致的问题 |
|------|------|-----------|
| `brk` | 总是返回 `program_break` 桩值，不分配物理页 | 任何真实 ELF 启动第一步就卡死 |
| `mmap` | 总是返回 `next_mmap` 并递增，不创建页表映射 | libc 初始化失败 |
| `munmap` | 总是返回 0，不做真正释放 | 内存泄漏，无回收路径 |
| 用户栈 | load-plan 只记录计数，不构建真实 argv/envp/auxv 栈帧 | ELF 的 `_start` 无法访问 argc/argv |
| ELF 段映射 | load-plan 只记录 vaddr/offset 元数据，不映射到页表 | 代码段和数据段不在内存 |
| U-mode 入口 | 不存在 | CPU 从未进入用户态执行 Linux ELF |
| ecall 分发 | dispatch 函数存在，但只在模拟路径中调用 | 真实 ecall 没有分发路径 |

## 目标

- 让 `linux_compat_run()` 从"模拟 syscall 序列"切换为"加载 ELF → 通过现有 trap runtime 进入 U-mode → ecall / page-fault trap 返回"。
- 实现真实 `brk` / `mmap` / `munmap`，接入 guest 已有的 `vm_address_space` / `vm_process` / `vm_object` 基础设施。
- 构建符合 Linux RISC-V ABI 的用户栈（argc / argv / envp / auxv），让 ELF 的 `_start` 能正常访问。
- 将 PT_LOAD 段映射到进程地址空间，设置 entry PC 和 user SP，通过现有 `trap_user_runtime_enter()` 进入 U-mode。
- 在 trap dispatch 中为 LINUX_COMPAT_ABI 进程增加 ecall → `linux_compat_syscall_dispatch()` 的真实分发路径。
- 实现 `fcntl` / `getrandom` / `clock_gettime` / `ioctl` 最小真实语义（busybox 启动链上大概率触发的 4 个 syscall）。
- 用一个静态链接的 RV64 busybox 做端到端验证：`linux /bin/busybox --help` 和 `linux /bin/busybox echo hello` 输出真实结果。
- 继续保持课程 OS Stage 1 / Stage 2 / Stage 3 marker、Stage 4 shell prompt、Stage 5 / Stage 6 / Stage 7 / Stage 8 guardrail 和旧 9 条负向 demo 稳定。

## 非目标

- 不启用直接 `git -h` / `vim -h` / `gcc -h` fallback。
- 不支持动态链接（ET_DYN + PT_INTERP 进入 U-mode 执行），load-plan 中 `requires_interp` 的 ELF 只输出诊断。
- 不声明完整 Linux syscall 面、完整 signal/futex、完整 TTY termios 或 rootfs 写语义。
- 不把 `course_syscall`、`course_shell`、`course_user_programs` 改造成 Linux ABI 实现。
- 不绑定网络 `git clone/push/pull`、完整 `vim` 终端编辑、`rustc` / `gcc` 编译。

## 方案概要

不复用现有的 `user_task_bootstrap`（它只支持 5 个固定单页 region，不做 ELF 加载），
而是新增 `linux_compat_exec` 和 `linux_compat_vm` 两个模块，直接操作 guest 已有的底层 VM 原语。
核心执行流从：

```
（Stage 8）lookup → inspect → build_load_plan → 匹配 path → 硬编码 help 字符串
```

变为：

```
（Stage 9）lookup → inspect → build_load_plan
            → linux_compat_exec_load（映射 segment）
            → linux_compat_exec_build_stack（构建用户栈）
            → trap_user_runtime_prepare_standard / activate / enter（进入 U-mode）
            → [trap: ecall → linux_compat_syscall_dispatch / page fault → vm_handle_page_fault]
            → exit_group → 清理 → 返回 course-os> prompt
```

## 完成定义

- 新增文件：`linux_compat_vm.h/c`、`linux_compat_exec.h/c`、3 条 unit test、1 条 host smoke。
- `brk` 和 `mmap` 通过现有 VM object / region API 建立真实用户映射；`munmap` 解除映射并释放 backing object。
- ELF PT_LOAD 段通过匿名或物理 `vm_object_t` + `vm_process_map_object_region_at()` 映射到页表；默认不假设 `pmm_alloc_page()` 返回连续页。
- 用户栈包含正确的 argc/argv/envp/auxv 布局，`sp` 16 字节对齐。
- LINUX_COMPAT_ABI 进程的 ecall 从 trap dispatch 真实路由到 `linux_compat_syscall_dispatch()`。
- `exit_group` 触发地址空间销毁、物理页释放，控制权回到 shell。
- `fcntl`（F_GETFD/F_SETFD/F_GETFL/F_SETFL）、`getrandom`（LCG）、`clock_gettime`（CLOCK_REALTIME/MONOTONIC）、`ioctl`（TIOCGWINSZ stub）正向可用且有 fail-closed 合同。
- `make test-unit-course_os_stage9_linux_compat_vm` / `_exec` / `_syscall` 绿灯。
- `linux /bin/busybox --help` 输出真实 BusyBox 帮助文本（非硬编码），`linux /bin/busybox echo hello` 输出 `hello`，两次执行后均恢复 `course-os> ` prompt。
- `make test-guest-course_os_linux_compat_shell_demo`（functional + pipeline）继续绿灯。
- `make test-guest-kernel_alpha_demo` 和旧 9 条负向 demo 无回归。
- 完成后回写 `docs/status/kernel_alpha_status.md`，归档到 `docs/plan/history_plan.md`，删除本计划文件。

## 任务

### 任务 1：Slice A — 真实 brk / mmap / munmap（新增 linux_compat_vm 模块）

**文件：**
- 创建：`myCPU/guest/include/linux_compat_vm.h`
- 创建：`myCPU/guest/kernel/linux_compat_vm.c`
- 修改：`myCPU/guest/kernel/linux_compat.c`
- 修改：`myCPU/guest/include/linux_compat.h`
- 创建：`myCPU/tests/unit/course_os_stage9_linux_compat_vm.c`
- 修改：`myCPU/Makefile`

- [x] **步骤 1：定义 `linux_compat_vm_t` 和接口**
  - `linux_compat_vm_t` 持有 `vm_address_space_t*` 和 `vm_process_t*` 指针，类型来自现有 `vm.h`。
  - 维护固定上限的 heap / mmap 区域表，每个区域绑定一组 `vm_user_region_t`、`vm_object_t` 和 vaddr / length / prot / flags 元数据。
  - 区域预算必须显式受 `VM_PROCESS_MAX_USER_REGIONS` 约束；Stage 9 先采用“PT_LOAD 段 + stack + brk arena + mmap arena”的合并策略，只有实际 busybox trace 证明不够时才提升该上限并补回归。
  - `linux_compat_vm_init(vm, address_space, process)` — 绑定到已有地址空间。
  - `linux_compat_vm_brk(vm, new_break)` — 通过 `vm_object_init_anon()` + `vm_process_map_object_region_at()` 建立 page-backed heap arena；收缩时解除完整页映射并释放 backing object。
  - `linux_compat_vm_mmap(vm, addr, length, prot, flags)` — 通过匿名 `vm_object_t` 和 `vm_process_map_object_region_at()` 建立真实用户映射，记录到区域表。
  - `linux_compat_vm_munmap(vm, addr, length)` — 遍历区域表，调用 `vm_process_remove_user_region()` 解除映射，再 `vm_object_reset()` 释放 backing pages。
  - `linux_compat_vm_destroy(vm)` — 遍历所有区域，按 remove-region + object-reset 路径释放映射和 backing pages。
- [x] **步骤 2：重写 `linux_compat.c` 中的 brk/mmap/munmap dispatch**
  - `linux_compat_runtime_t` 增加 `linux_compat_vm_t*` 字段。
  - `linux_compat_syscall_dispatch()` 中 brk/mmap/munmap 改为调用 `linux_compat_vm_*` 接口。
  - brk 的初始值从 `linux_compat_runtime_t.program_break` 改为由 `vm` 管理。
  - mmap 返回地址继续走递增策略，但现在映射是真实的。
- [x] **步骤 3：新增 Stage 9 VM 单元测试**
  - 在 `Makefile` 的 `UNIT_TEST_NAMES` 中加入 `course_os_stage9_linux_compat_vm`。
  - 测试 1：brk 扩展 → 建立 backing pages / page-table 映射 → 返回新 break 地址 → brk 再次扩展 → 地址递增。
  - 测试 2：mmap 分配 → 能通过现有 VM debug / page-table walk helper 确认页表项存在且权限正确。
  - 测试 3：munmap 释放 → 页表项清除 → backing pages 被释放。
  - 测试 4：brk 收缩 → 完整页映射和 backing pages 被释放。
  - 测试 5：`linux_compat_vm_destroy()` 清理所有映射和物理页。
- [x] **步骤 4：确认绿灯**
  - 运行：`cd myCPU && make test-unit-course_os_stage9_linux_compat_vm`
- 预期：全部通过。
  - `2026-06-01` checkpoint：已通过。

### 任务 2：Slice B — ELF 段加载 + 用户栈构建（新增 linux_compat_exec 模块）

**文件：**
- 创建：`myCPU/guest/include/linux_compat_exec.h`
- 创建：`myCPU/guest/kernel/linux_compat_exec.c`
- 修改：`myCPU/guest/include/vm.h`
- 修改：`myCPU/guest/kernel/vm_object.c`
- 修改：`myCPU/guest/kernel/linux_compat_loader.c`
- 创建：`myCPU/tests/unit/course_os_stage9_linux_compat_exec.c`
- 修改：`myCPU/Makefile`

- [x] **步骤 1：实现 `linux_compat_exec_load()`**
  - 输入：`load_plan`（来自 Stage 8 的 `build_load_plan()`）、ELF image 指针、`linux_compat_vm_t*`。
  - 先在 `vm.h` / `vm_object.c` 暴露一个窄 helper，例如 `vm_object_resolve_page_for_write()`，只允许已初始化对象按页解析 backing page，供 ELF loader 和用户栈填充使用。
  - 遍历 `plan->segments[]`，对每个 `PT_LOAD` 段：
    - 计算 page-aligned vaddr、object offset、filesz / memsz 覆盖范围。
    - 用 `vm_object_init_anon()` 创建 segment backing object，再用 `vm_object_resolve_page_for_write()` 获取页地址并填充 ELF 文件中 `filesz` 字节。
    - 剩余 `memsz - filesz` 字节（bss）保持零填充。
    - `vm_process_map_object_region_at()` 映射到用户地址空间，权限从 `p_flags` 转换：
      - PF_R → `VM_PAGE_READ | VM_PAGE_USER`
      - PF_W → `VM_PAGE_WRITE | VM_PAGE_USER`
      - PF_X → `VM_PAGE_EXEC | VM_PAGE_USER`
  - 只有在能证明 ELF segment 位于连续物理 span 时，才允许使用 `vm_object_init_physical()`；默认不要假设连续 `pmm_alloc_page()`。
  - 如果 `plan->requires_interp`：不加载段，返回 `LINUX_COMPAT_ERR_UNSUPPORTED_ELF`，诊断写 `dynamic linker not executed`。
  - 返回 `*entry_pc = plan->entry`。
- [x] **步骤 2：实现 `linux_compat_exec_build_stack()`**
  - 按 Linux RISC-V ABI 布局在栈顶向下构建：
    ```
    [高位地址]
      auxv 数组:   AT_PHDR, AT_PHENT, AT_PHNUM, AT_PAGESZ=4096, AT_ENTRY, AT_NULL
      envp 数组:   NULL（第一阶段传空环境变量）
      argv 数组:   指向各参数字符串的指针数组，NULL 终止
      argc:        64-bit 参数个数
      [16 字节对齐 padding]
      argv 字符串:  "busybox\0", "--help\0", ...
    [低位地址 — sp 指向 argc]
    ```
  - auxv 的 AT_PHDR/AT_PHENT/AT_PHNUM 从 load-plan 的 segment 信息推算。
  - auxv 的 AT_RANDOM 指向 16 字节确定性随机序列（用 LCG 种子生成）。
  - 栈页通过匿名 `vm_object_t` 分配并映射到 `LINUX_COMPAT_STACK_TOP - PAGE_SIZE`，栈内容同样通过 `vm_object_resolve_page_for_write()` 写入。
  - 返回 `*user_sp`，确保 16 字节对齐。
- [x] **步骤 3：新增 Stage 9 exec 单元测试**
  - 在 `Makefile` 的 `UNIT_TEST_NAMES` 中加入 `course_os_stage9_linux_compat_exec`。
  - 测试 1：构造最小 ET_EXEC ELF → `exec_load()` → 确认 segment 映射到页表 → 确认 entry_pc 正确。
  - 测试 2：构造带 bss 段的 ELF → `exec_load()` → bss 区域全零。
  - 测试 3：`exec_build_stack()` → 确认 sp 指向的 argc 正确 → argv[0] 可读 → auxv AT_ENTRY 匹配 entry_pc。
  - 测试 4：ET_DYN + PT_INTERP → 返回 `UNSUPPORTED_ELF`，诊断含 `dynamic linker not executed`。
  - 测试 5：坏 ELF（bad magic / unsupported machine） → fail-closed 不掉段。
- [x] **步骤 4：确认绿灯**
  - 运行：`cd myCPU && make test-unit-course_os_stage9_linux_compat_exec`
- 预期：全部通过。
  - `2026-06-01` checkpoint：已通过。

### 任务 3：Slice C — U-mode 入口 + ecall dispatch + exit 闭环

**文件：**
- 修改：`myCPU/guest/kernel/linux_compat_exec.c`
- 修改：`myCPU/guest/include/linux_compat_exec.h`
- 修改：`myCPU/guest/kernel/linux_compat.c`
- 修改：`myCPU/guest/kernel/trap_dispatch.c`
- 修改：`myCPU/guest/kernel/trap.c`
- 修改：`myCPU/guest/include/trap.h`
- 修改：`myCPU/guest/include/linux_compat.h`
- 创建：`myCPU/tests/host/course_os_linux_compat_minimal_elf_smoke.cpp`
- 修改：`myCPU/Makefile`

- [ ] **步骤 1：实现 `linux_compat_exec_enter()`**
  - 输入：`linux_compat_vm_t*`、`entry_pc`、`user_sp`、`linux_compat_runtime_t*`。
  - 复用现有 `trap_user_runtime_prepare_standard()` / `trap_user_runtime_activate()` / `trap_user_runtime_enter()`，不要新增第二套内联 `sret` 入口。
  - 在 `trap_user_runtime_prepare_standard()` 中设置 entry PC、user SP 和独立 supervisor trap stack。
  - 在 `trap_user_runtime_enter()` 返回后检查 `runtime->exited`、exit code 和 trace summary，再 `trap_user_runtime_deactivate()`。
  - 如果 U-mode trap 返回但 `runtime->exited == false`，输出 fail-closed 诊断（例如 unexpected trap return），不要用 `wfi` 循环等待。
  - `2026-06-01` checkpoint：已有初稿，但尚未通过
    `test-host-course_os_linux_compat_minimal_elf_smoke`，不能标完成。
- [ ] **步骤 2：在 `trap_dispatch.c` 中增加 LINUX_COMPAT_ABI ecall 分发**
  - 找到现有 ecall-from-U-mode 处理路径。
  - 扩展 `trap_user_ecall_policy_t`，新增 Linux compat runtime 指针或 generic ecall callback；避免只靠文件级全局变量绕过 trap context。
  - 分流顺序固定为：
    - 现有 `course_syscall_t* syscalls` 非空 → 走 `course_syscall_dispatch()`，保持课程 ABI 不变。
    - Linux compat policy 非空 → 从 `a7` 读 syscall number，从 `a0`-`a5` 读参数，构造 `linux_compat_syscall_request_t`，调用 `linux_compat_syscall_dispatch()`。
    - 返回值写入 `a0`，`sepc += 4`。
  - 对于 `exit` / `exit_group`：
    - dispatch 内部设置 `runtime->exited = true`。
    - trap dispatch 检测到 `exited` 后将 `sepc` 设置为 `trap_user_runtime_arch_resume` 对应 resume path，使 `trap_user_runtime_enter()` 返回 S-mode 收口。
  - page fault 路径：LINUX_COMPAT_ABI 进程复用现有 `vm_handle_page_fault()`，无需改动。
  - `2026-06-01` checkpoint：已有分发初稿和 trap 单元覆盖，但真实最小 ELF
    `write -> exit_group` host smoke 仍未通过，待继续排查。
- [x] **步骤 3：让 write syscall 真正写 UART**
  - `linux_compat_write()` 当前只写 `stdout_buffer`。
  - 改为同时调用现有 `console_putc()` 逐字符输出到 UART。
  - `stdout_buffer` 保留，用于 trace summary。
  - `2026-06-01` checkpoint：已通过 Stage 9 syscall unit 覆盖；真实 U-mode 路径仍由
    Slice C smoke 收口。
- [x] **步骤 4：新增最小 ELF host smoke**
  - 在 `Makefile` 中加入 `test-host-course_os_linux_compat_minimal_elf_smoke`。
  - 构造一个最小 hand-crafted 静态 RV64 ELF（只用汇编手写的两条 ecall）：
    - `write(1, "hello", 5)` → 写 UART。
    - `exit_group(0)` → 退出。
  - smoke 验证：加载 → 执行 → UART 输出 "hello" → runtime 标记 `exited` → trace record 包含 write 和 exit_group。
  - `2026-06-01` checkpoint：target 和 hand-crafted ELF 已加入，但测试当前失败，见步骤 5。
- [ ] **步骤 5：确认绿灯**
  - 运行：`cd myCPU && make test-host-course_os_linux_compat_minimal_elf_smoke`
  - 运行：`cd myCPU && make test-unit-course_os_stage9_linux_compat_exec`
- 预期：全部通过。
  - `2026-06-01` checkpoint：`make test-unit-course_os_stage9_linux_compat_exec`
    已通过；`make test-host-course_os_linux_compat_minimal_elf_smoke` 失败：
    `run_until_uart_contains exceeded step budget`，等待点为 `trace=write/exit_group`。

### 任务 4：Slice D — 额外真实 syscall（fcntl + getrandom + clock_gettime + ioctl）

**文件：**
- 修改：`myCPU/guest/kernel/linux_compat.c`
- 修改：`myCPU/guest/include/linux_compat.h`
- 创建：`myCPU/tests/unit/course_os_stage9_linux_compat_syscall.c`
- 修改：`myCPU/Makefile`

- [x] **步骤 1：实现 `fcntl` 最小 flag 合同**
  - `F_GETFD`：返回 close-on-exec flag 状态（默认 0）。
  - `F_SETFD`：只接受 `FD_CLOEXEC`（值 1），其他 flag 返回 `-EINVAL`。
  - `F_GETFL`：返回 `O_RDONLY` 和当前 nonblock 状态。
  - `F_SETFL`：只允许切换 `O_NONBLOCK`，其他写 flag 返回 `-EINVAL`。
  - `F_DUPFD`：从指定 arg 开始找最小可用 slot，复制 FD。
  - 不支持 `F_DUPFD_CLOEXEC`、`F_GETLK`/`F_SETLK`、`F_GETOWN`/`F_SETOWN`。
  - 给 `linux_compat_fd_t` 增加 `flags` 字段。
- [x] **步骤 2：实现 `ioctl` TTY 最小合同**
  - fd 0（stdin）、fd 1（stdout）、fd 2（stderr）视为最小 TTY。
  - `TIOCGWINSZ`：返回固定 `ws_row=24, ws_col=80, ws_xpixel=0, ws_ypixel=0`。
  - `TCGETS`：返回固定最小 termios 结构（`c_lflag` 含 `ECHO|ICANON`）。
  - `TCSETS`/`TCSETSW`/`TCSETSF`：接受但不改变任何 termios 状态（返回 0）。
  - `FIONBIO`：在 fd 上设置/清除 nonblock 标记。
  - 非 TTY fd（普通 rootfs 文件 fd）对 TTY ioctl 返回 `-ENOTTY`。
  - 未知 ioctl request 返回 `-ENOTTY` 并写入 trace message。
- [x] **步骤 3：实现 `getrandom` 确定性实现**
  - 用简单 LCG + timer tick 计数做种子。
  - 每次调用填充最多 256 字节。
  - 不读取宿主 `/dev/urandom`，不声明密码学安全。
- [x] **步骤 4：实现 `clock_gettime` 真实语义**
  - `CLOCK_REALTIME`：从 guest boot 以来的 tick 数计算秒 + 纳秒。
  - `CLOCK_MONOTONIC`：同上（当前与 CLOCK_REALTIME 相同，无 NTP 调整）。
  - 其他 clock_id：返回 `-EINVAL`。
- [x] **步骤 5：新增 Stage 9 syscall 单元测试**
  - 在 `Makefile` 的 `UNIT_TEST_NAMES` 中加入 `course_os_stage9_linux_compat_syscall`。
  - 测试 `fcntl`：F_GETFD/F_SETFD/F_GETFL/F_SETFL/F_DUPFD 正向和 fail-closed。
  - 测试 `ioctl`：stdio fd 的 TIOCGWINSZ 返回 80×24；rootfs fd 返回 ENOTTY；未知 request 返回 ENOTTY。
  - 测试 `getrandom`：返回确定非零字节，长度正确。
  - 测试 `clock_gettime`：CLOCK_REALTIME 和 CLOCK_MONOTONIC 返回合理值。
- [x] **步骤 6：确认绿灯**
  - 运行：`cd myCPU && make test-unit-course_os_stage9_linux_compat_syscall`
- 预期：全部通过。
  - `2026-06-01` checkpoint：已通过。

### 任务 5：Slice E — 静态 busybox 端到端验证

**文件：**
- 修改：`myCPU/guest/kernel/linux_compat.c`
- 修改：`myCPU/guest/kernel/course_shell.c`
- 修改：`myCPU/guest/kernel/linux_compat_rootfs_builtin.c`
- 修改：`myCPU/tools/linux_compat_rootfs_asset.py`
- 修改：`myCPU/tests/host/course_os_linux_compat_terminal_smoke.cpp`
- 修改：`myCPU/Makefile`

- [ ] **步骤 1：接入静态 busybox binary**
  - 构建或获取一个静态链接的 RV64 busybox（`riscv64-linux-gnu-gcc -static` 编译或从 Alpine riscv64 rootfs 提取）。
  - 通过 `linux_compat_rootfs_asset.py --required-path /bin/busybox` 生成 C provider。
  - 将 busybox ELF bytes 编译进 guest。
- [ ] **步骤 2：重写 `linux_compat_run()` 的核心逻辑**
  - 移除 `k_busybox_help` / `k_git_help` 硬编码字符串。
  - 移除模拟 syscall 序列（brk → mmap → fstat → open → read → close → write → exit_group 的手动编排）。
  - 新的 `linux_compat_run()` 流程：
    1. `lookup(path)` → 找到 rootfs entry → ELF bytes。
    2. `build_load_plan(image, image_size, argc, 0, &plan, &trace)` → 沿用 Stage 8。
    3. 若 `plan.requires_interp`：输出诊断 `dynamic linker not executed`，返回 `UNSUPPORTED_ELF`。
    4. `linux_compat_vm_init(&vm, address_space, process)` → 初始化 VM 上下文。
    5. `linux_compat_exec_load(&vm, image, image_size, &plan, &entry_pc)` → 映射段。
    6. `linux_compat_exec_build_stack(&vm, &plan, argc, argv, &user_sp)` → 构建栈。
    7. 输出诊断行（rootfs source、path、argc、loader summary）。
    8. `linux_compat_exec_enter(&vm, entry_pc, user_sp, runtime)` → 进入 U-mode。
    9. `trap_user_runtime_enter()` 返回后：输出 `trace=...` 摘要 + stdout 内容。
    10. `linux_compat_vm_destroy(&vm)` → 清理。
  - 保留 `course_shell.c` 中 `run_linux_command()` 的 fork + set_abi + exit + waitpid 流程。
  - 在 `run_linux_command()` 中创建子进程的 `vm_address_space` 和 `vm_process`，传给 `linux_compat_run()`。
  - `2026-06-01` checkpoint：已加入面向 `/bin/minimal-elf` 的 real-exec 分流初稿；
    `/bin/busybox` 和 `/usr/bin/git` 仍保留旧帮助文本 / 模拟 syscall 兼容路径，尚未切换为真实 ELF
    执行，不能标完成。
- [ ] **步骤 3：修复 busybox 实际 trace 暴露的缺口**
  - 用 `linux /bin/busybox --help` 实际跑一次。
  - 通过 trace record 观察触发了哪些 syscall。
  - 按需最小实现缺失的 syscall（不追求完整度，只修崩掉的）。
  - 常见的潜在缺口：`mprotect`、`set_tid_address`、`set_robust_list`、`prlimit64`、`uname`。
  - 如果缺口太大（如 musl libc 初始化调用 `futex` 做锁），考虑先用 `--help` 这类低风险命令验证基本路径。
- [ ] **步骤 4：扩展 terminal smoke**
  - shell smoke 中增加：
    - `linux /bin/busybox --help` → 输出中包含 `BusyBox` 关键字且不是硬编码文本 → prompt 恢复。
    - `linux /bin/busybox echo hello` → 输出 `hello` → prompt 恢复。
  - 确保直接 `git -h`、`vim -h`、`gcc -h` 仍报课程 shell 错误（fallback 关闭）。
  - `2026-06-01` checkpoint：既有 `test-host-course_os_linux_compat_terminal_smoke`
    已通过；它仍覆盖旧显式 launcher / help / fallback guardrail，不代表真实 busybox
    end-to-end 完成。
- [ ] **步骤 5：确认绿灯**
  - 运行：`cd myCPU && make test-host-course_os_linux_compat_terminal_smoke`
  - 运行：`cd myCPU && make test-guest-course_os_linux_compat_shell_demo`
  - 运行：`cd myCPU && make test-pipeline-guest-course_os_linux_compat_shell_demo`
  - 预期：全部通过，UART 输出真实 busybox help 文本。

### 任务 6：Slice F — 收口

**文件：**
- 修改：`docs/status/kernel_alpha_status.md`
- 修改：`docs/design/course_os_kernel_alpha_linux_compat_plus_design.md`
- 修改：`docs/plan/history_plan.md`
- 修改：`docs/index.md`

- [ ] **步骤 1：回写 `kernel_alpha_status.md`**
  - 当前状态增加 Stage 9 完成摘要：
    - 真实 brk/mmap/munmap 接入 guest VM 基础设施。
    - ELF 段映射 + 用户栈构建 + U-mode 入口打通。
    - ecall → syscall dispatch 真实分发，exit_group 闭环。
    - 静态 busybox `--help` 和 `echo` 端到端验证通过。
    - 新增 fcntl/getrandom/clock_gettime/ioctl 真实语义。
  - 关键历史节点追加 Stage 9 完成条目，日期按归档当天填写。
  - 剩余风险更新：
    - 仍不支持动态链接（ET_DYN + PT_INTERP 不执行）。
    - `execve` / `wait4` / `futex` / `rt_sig*` 未实现，多进程 Linux 兼容不可用。
    - 不声明完整 Linux 用户态兼容。
  - 下一步更新为 Stage 10+ 方向（如动态链接器、execve、更多 syscall）。
  - 验证基线增加 Stage 9 unit/host smoke target。
- [ ] **步骤 2：更新设计文档**
  - `linux_compat_plus_design.md` 的"当前有效性说明"段更新：
    - Stage 9 已完成真实 ELF 执行路径和静态 busybox 验证。
    - 仍不声明完整 Linux 用户态兼容。
  - 增加 Stage 9 相关文件的链接。
- [ ] **步骤 3：归档到 `history_plan.md`**
  - 追加 `course-os-kernel-alpha-stage9-linux-compat-real-exec-plan` 条目。
  - 记录完成内容、实现过程摘要、剩余风险和验证摘要。
- [ ] **步骤 4：同步 `index.md`**
  - 更新活跃计划列表（移除 Stage 9 计划）。

### 任务 7：全面门禁验证

**文件：**
- 按任务 1-6 实际改动收口

- [ ] **步骤 1：Stage 9 窄门禁**
  - [x] 运行：`cd myCPU && make test-unit-course_os_stage9_linux_compat_vm`
    - `2026-06-01` checkpoint：通过。
  - [x] 运行：`cd myCPU && make test-unit-course_os_stage9_linux_compat_exec`
    - `2026-06-01` checkpoint：通过。
  - [x] 运行：`cd myCPU && make test-unit-course_os_stage9_linux_compat_syscall`
    - `2026-06-01` checkpoint：通过。
  - [ ] 运行：`cd myCPU && make test-host-course_os_linux_compat_minimal_elf_smoke`
    - `2026-06-01` checkpoint：失败，`run_until_uart_contains exceeded step budget`，
      当前卡在等待 `trace=write/exit_group`。
- [ ] **步骤 2：Stage 6-8 无回归**
  - 运行：`cd myCPU && make test-unit-course_os_stage5_linux_compat`
  - 运行：`cd myCPU && make test-unit-course_os_stage6_linux_compat`
  - 运行：`cd myCPU && make test-unit-course_os_stage8_linux_compat_loader`
- [ ] **步骤 3：shell / external smoke**
  - [x] 运行：`cd myCPU && make test-host-course_os_linux_compat_terminal_smoke`
    - `2026-06-01` checkpoint：通过。
  - 如本机具备临时 rootfs：运行带临时 rootfs 的 `make test-host-course_os_linux_compat_external_rootfs_smoke`
- [ ] **步骤 4：课程 OS guardrail 无回归**
  - 运行：`cd myCPU && make test-guest-kernel_alpha_demo`
  - 运行：`cd myCPU && make test-pipeline-guest-kernel_alpha_demo`
  - 运行：`cd myCPU && make test-guest-course_os_shell_demo`
  - 运行：`cd myCPU && make test-pipeline-guest-course_os_shell_demo`
  - 运行：`cd myCPU && make test-guest-course_os_linux_compat_shell_demo`
  - 运行：`cd myCPU && make test-pipeline-guest-course_os_linux_compat_shell_demo`
- [ ] **步骤 5：全量门禁**
  - 运行：`cd myCPU && make test`
  - 运行：`cd myCPU && make test-pipeline`
  - 运行：`git diff --check`
- [ ] **步骤 6：旧负向 guardrail 无回归**
  - 运行：`cd myCPU && make test-guest-kernel_alpha_fault_demo`
  - 运行：`cd myCPU && make test-guest-kernel_alpha_storage_no_media_demo`
  - 运行：`cd myCPU && make test-guest-kernel_alpha_storage_not_ready_demo`
  - 运行：`cd myCPU && make test-guest-kernel_alpha_storage_bad_magic_demo`
  - 运行：`cd myCPU && make test-guest-kernel_alpha_storage_bad_block_count_demo`
  - 运行：`cd myCPU && make test-guest-kernel_alpha_storage_lba_range_demo`
  - 运行：`cd myCPU && make test-guest-kernel_alpha_storage_bad_command_demo`
  - 运行：`cd myCPU && make test-guest-kernel_alpha_plic_not_ready_demo`
  - 运行：`cd myCPU && make test-guest-kernel_alpha_timer_not_ready_demo`

## 风险与缓解

| 风险 | 等级 | 缓解 |
|------|------|------|
| U-mode 入口 + trap 循环首次打通时，trap dispatch 修改可能误伤现有课程 OS ecall 路径 | high | 用 ABI 标记严格分流；先跑全部已有课程 OS unit/guest test 确保零回归 |
| 静态 busybox 的 libc 初始化链可能触发远超预期的 syscall（futex/mprotect/set_tid_address 等） | med | 先构造最小 hand-crafted ELF 验证核心路径；busybox 按需防御性补 syscall |
| `vm_process` 现有接口是为固定 5-region bootstrap 设计的，映射多段 ELF 可能触发限制 | med | `linux_compat_exec` 使用底层 `vm_process_map_object_region_at` 逐个映射，不依赖 bootstrap 的 bulk bind；同时用 brk / mmap arena 合并策略控制 `VM_PROCESS_MAX_USER_REGIONS` 预算 |
| 物理页管理（pmm）与 linux_compat_vm 的生命周期不一致导致泄漏 | med | `linux_compat_vm_destroy()` 统一清理，unit test 固定泄漏检测 |
| Stage 9 必须不引入对 `course_*` 教学模块的依赖 | low | 所有新代码在 `linux_compat_*` 模块内，不 import `course_*` 头文件 |

## 完成态回写要求

- 全部 checklist 勾完后，在 `docs/status/kernel_alpha_status.md` 增加 Stage 9 完成摘要、关键历史节点和剩余风险。
- 在 `docs/design/course_os_kernel_alpha_linux_compat_plus_design.md` 的"当前有效性说明"段更新完成声明。
- 在 `docs/plan/history_plan.md` 追加 `course-os-kernel-alpha-stage9-linux-compat-real-exec-plan` 归档条目。
- 删除 `docs/plan/course_os_kernel_alpha_stage9_linux_compat_real_exec_plan.md`。
- 最终汇报必须明确：Stage 9 完成后仍不声明动态链接器运行、完整 Linux syscall 面、rootfs 写语义或自动 fallback。
