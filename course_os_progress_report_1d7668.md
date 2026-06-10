# 课程 OS（A 方案·OS 内核实现）进度汇报

> 基准版本：`1d7668c` — feat(kernel-alpha): 完成课程 OS Stage 4 前端 shell
>
> 汇报日期：2026-05-30
>
> 项目仓库：myCPU — 已可运行的 RISC-V 模拟器原型，本分支在此基础上实现自写 `kernel_alpha` 课程 OS 内核。

---

## 一、整体框架

### 1.1 项目定位

本课程设计选择 **A 方案「OS 内核实现」**（难度系数 100%），在 myCPU RISC-V64 模拟器上从零实现一个教学级操作系统内核 `kernel_alpha`。课程 OS 使用"基础设施复用 + 阶段编排层 + 独立展示入口"的架构，所有新代码放在旁路 `course_*` 模块中，不污染模拟器主体和 guest runtime 既有基础设施。

### 1.2 架构总览

```
┌─────────────────────────────────────────────────────┐
│  kernel_alpha_demo          guest_course_os_shell   │
│  (一次性正向 smoke)          _demo (常驻交互 shell)  │
├─────────────────────────────────────────────────────┤
│  Stage 编排层                                        │
│  course_os_stage1 / stage2 / stage3                 │
├─────────────────────────────────────────────────────┤
│  课程 OS 模块                                       │
│  scheduler │ memory │ fs │ process │ syscall │ fd    │
│  shell │ sync │ elf_loader │ libc │ user_programs   │
├─────────────────────────────────────────────────────┤
│  procfs（只读证据面）                                │
├─────────────────────────────────────────────────────┤
│  Guest Runtime 基础设施（PMM / Sv39 / trap / VM …） │
├─────────────────────────────────────────────────────┤
│  myCPU 模拟器核心（ISA / pipeline / cache / DBT …） │
└─────────────────────────────────────────────────────┘
```

### 1.3 已完成总览（Stage 1–4）

截至版本 `1d7668c`，课程 OS 已按四阶段推进完成，覆盖课程基本要求中的全部 6 个模块，并额外补全教学创新线和前端交互 shell：

| 阶段 | 日期 | 内容 | 状态 |
|------|------|------|:----:|
| Stage 1 | 2026-05-29 | 三模块九功能点 + `/proc` 证据面 | ✅ |
| Stage 2 | 2026-05-29 | A 方案核心闭环（syscall / 进程 / FS / shell）+ COW / crash 创新 | ✅ |
| Stage 3 | 2026-05-30 | 课程满分基线真实化（ELF / libc / 调度指标 / 同步 / COW 证据） | ✅ |
| Stage 4 | 2026-05-30 | 前端交互 shell 与浏览器 `/console` Lab 接入 | ✅ |

#### 正向 smoke 证据

`kernel_alpha_demo` 一次性正向输出（Stage 1–3 串联）：

```
KMVPET|
course-os-stage1 sched=CFS-lite ctx=9 pf=4 reclaim=1 fs_create=5 btree_steps=48 proc=ps/meminfo/schedstat/fsstat|
course-os-stage2 syscall=ok shell=ok procs=ok fd=ok fs=128/64K/3 pipe=ok cow=ok crash=isolated proc=ps/meminfo/schedstat/fsstat/syscalls/cow/crashlog|
course-os-stage3 elf=5 libc=ok sched=fcfs/rr/cfs sync=sem/mutex vm=sv39-cow fs=seek/mkfs shell=script proc=cpuinfo/uptime/pid
```

`guest_course_os_shell_demo` 常驻入口：启动后进入 `course-os> ` 交互 prompt，接入浏览器 `/console` Lab。

### 1.4 代码规模

课程 OS 模块（course_* + procfs）在 commit `1d7668c` 的代码量：

| 类别 | 文件数 | 说明 |
|------|:------:|------|
| 核心实现 `.c` | 14 | scheduler, memory, fs, process, syscall, fd, shell, sync, elf_loader, libc, user_programs, procfs + 3 个 stage 编排 |
| 头文件 `.h` | 14 | 对应模块的公开接口 |
| 单元测试 `.c` | 12 | Stage 1–4 各模块独立门禁 |
| Host smoke `.cpp` | 1 | course_os_shell_terminal_smoke |
| 前端变更 | 13 files | debug server / manifest / terminal / render / tests |

### 1.5 验证矩阵

版本 `1d7668c` 已完成全量验证通过：

- `cd myCPU && make test`（全量单元 + guest 回归）
- `cd myCPU && make test-pipeline`（pipeline 路径全量回归）
- `cd frontend && node --test`（前端全量测试）
- `git diff --check`（空白检查）

---

## 二、三人分工与进度

> 以下分工为基于课程 OS Stage 1–4 实际工作内容的合理划分。三个角色各自承担独立的模块领域，交叉覆盖验证面。

---

### 2.1 成员 A：进程管理 + 调度 + 同步 + 中断/trap

**负责模块：**

| 模块 | 源文件 | 核心能力 |
|------|--------|----------|
| `course_scheduler` | `course_scheduler.c` (425 行) | FCFS / RR / CFS-lite 三种调度算法，时间片可配置，上下文切换计数 |
| `course_process` | `course_process.c` (674 行) | PCB、父子进程树、ready/running/blocked/zombie/dead 状态机、`fork`/`exec`/`exit`/`waitpid`、COW Fork |
| `course_sync` | `course_sync.c` (164 行) | semaphore / mutex 阻塞、唤醒、owner 追踪、misuse guard |
| `trap_dispatch` | `trap_dispatch.c`（增量 114 行） | ecall 按进程 ABI 分发（`COURSE_ABI` → `course_syscall_dispatch`），用户态崩溃隔离 |

**已完成功能点：**

1. 三种调度算法（FCFS / RR / CFS-lite），指标化输出等待时间、周转时间、上下文切换次数、当前调度策略
2. 真实进程生命周期：`fork` 写时复制、`exec` 替换地址空间、`exit` 状态流转、`waitpid` 回收
3. COW Fork 完整证据链：refcount、saved pages、fault count、copy count、leak summary
4. semaphore / mutex：阻塞唤醒、owner 追踪、misuse 检测（重复释放、非持有者释放）
5. 用户态崩溃隔离：非法指针、非法 syscall、坏参数一律 fail-closed，不升级为 kernel panic；crashlog 可诊断
6. trap 层 ABI 分流：`COURSE_ABI` ↔ `LINUX_COMPAT_ABI` 预留

**进度：Stage 1–3 全部完成，Stage 4 继承复用，当前 100%。**

对应测试门禁：
- `test-unit-course_os_stage2_process`
- `test-unit-course_os_stage2_cow_crash`
- `test-unit-course_os_stage3_sched_sync`
- `test-unit-course_os_stage3_vm`

---

### 2.2 成员 B：内存管理 + 文件系统 + ELF 加载 + procfs

**负责模块：**

| 模块 | 源文件 | 核心能力 |
|------|--------|----------|
| `course_memory` | `course_memory.c` (189 行) | Demand Paging、Clock 页面置换、`kmalloc`/`kfree`、物理页统计 |
| `course_fs` | `course_fs.c` (567 行) | 文件/目录 CRUD、`seek`、`unlink`/`rmdir`、简化 B 树目录索引、`mkfs`、3 层目录、128 文件/64KB 单文件 |
| `course_elf_loader` | `course_elf_loader.c` (228 行) | RV64 little-endian 静态 ELF 解析、`PT_LOAD` 装载、entry pc、用户栈 `argc/argv/envp` |
| `procfs` | `procfs.c` (772 行) | 只读 `/proc` 证据面：`ps/meminfo/schedstat/fsstat/syscalls/cow/crashlog/cpuinfo/uptime/<pid>/status/fd/maps` |

**已完成功能点：**

1. Demand Paging + Clock 页面置换：缺页次数、回收次数、释放后复用统计
2. `kmalloc`/`kfree`：物理页分配器，无泄漏验证
3. 文件系统：文件/目录 CRUD、`seek`、`unlink`/`rmdir`、至少 3 层目录、128 文件、单文件 64KB
4. 简化 B 树目录索引：每目录维护索引视图，暴露内部节点/叶节点/查找步数证据
5. ELF 加载：支持 5 个课程用户程序（hello / echo / cat / forktest / crashdemo）
6. 只读 `/proc`：12 个证据节点，覆盖进程、内存、调度、文件系统、COW、crashlog、per-pid 三大面

**进度：Stage 1–3 全部完成，Stage 4 继承复用，当前 100%。**

对应测试门禁：
- `test-unit-course_os_stage1`（调度 + 内存 + FS + procfs 综合）
- `test-unit-course_os_stage2_fd_fs`
- `test-unit-course_os_stage3_elf`
- `test-unit-course_os_stage3_vm`
- `test-unit-course_os_stage3_fs_shell`
- `test-unit-course_os_stage3_proc`

---

### 2.3 成员 C：syscall + FD + shell + 用户程序 + 前端接入

**负责模块：**

| 模块 | 源文件 | 核心能力 |
|------|--------|----------|
| `course_syscall` | `course_syscall.c` (481 行) | 课程 syscall ABI：`read/write/open/close/seek/exit/fork/exec/wait/waitpid/getpid/ps/kill`，用户指针校验，fail-closed |
| `course_fd` | `course_fd.c` (294 行) | 进程级 FD 表：0/1/2 标准流、普通文件、管道端点、procfs 只读节点统一通过 FD 读写 |
| `course_shell` | `course_shell.c` (817 行) | `help/ls/cat/echo/ps/kill/cd/pwd/exit` + 外部程序 + 单级管道 + 基础重定向 + 脚本模式 + proc 快捷命令 |
| `course_libc` | `course_libc.c` (123 行) | 简化 libc syscall wrapper（`read/write/open/close/exit/fork/exec/wait/getpid`） |
| `course_user_programs` | `course_user_programs.c` (101 行) | 5 个课程用户程序 catalog：hello / echo / cat / forktest / crashdemo |
| Stage 4 前端接入 | `frontend/` (13 files) | debug server manifest、Course OS Shell Lab 卡片、terminal 文案、e2e smoke |

**已完成功能点：**

1. 课程 syscall ABI 完整闭环：12 个核心 syscall，用户指针校验，坏 fd/坏路径/非法 syscall 一律 fail-closed
2. FD 表：统一 I/O 面（普通文件 + 管道 + procfs），0/1/2 标准流
3. 课程 shell：10 个内置命令、外部程序执行、参数传递、单级管道、基础重定向（`>`/`<`）、shell 脚本（`sh /demo.sh`）
4. 简化 libc：封装课程 syscall，支撑 5 个用户程序
5. 前端接入：`/console` 通过 manifest 加载 `guest_course_os_shell_demo`，browser terminal 输入命令 → 等待新 `course-os> ` prompt → 渲染输出；新增 Course OS Shell Lab 专题卡和完整 terminal 文案
6. Stage 1/2/3 总编排层：`course_os_stage1/2/3.c` 负责正向 summary 输出，`kernel_alpha/main.c` 只串接

**进度：Stage 1–4 全部完成，当前 100%。**

对应测试门禁：
- `test-unit-course_os_stage2_syscall`
- `test-unit-course_os_stage2_shell`
- `test-unit-course_os_stage2`（总编排）
- `test-unit-course_os_stage3_fs_shell`
- `test-unit-course_os_stage3`（总编排）
- `test-host-course_os_shell_terminal_smoke`
- `test-guest-course_os_shell_demo`
- `test-pipeline-guest-course_os_shell_demo`
- `cd frontend && node --test`

---

## 三、验证状态汇总

| 验证层级 | 命令 | 结果 |
|----------|------|:----:|
| 全量单元 + guest 回归 | `cd myCPU && make test` | ✅ |
| Pipeline 路径全量 | `cd myCPU && make test-pipeline` | ✅ |
| 前端全量测试 | `cd frontend && node --test` | ✅ |
| 空白检查 | `git diff --check` | ✅ |
| 旧 9 条负向 demo guardrail | `make test-guest-kernel_alpha_*_demo` | ✅ |

---

## 四、风险与下一步

### 当前风险

- 课程 OS 第三阶段仍是教学级满分基线，不声明完整 POSIX shell、完整信号语义、多级管道、真实磁盘一致性、完整 ELF 动态链接或通用 Linux 用户态兼容。
- COW Fork 当前优先覆盖课程级匿名用户页，不扩展到文件系统 snapshot 或完整文件页 COW。
- `/proc` 保持只读证据面，不作为调度、内存或文件系统的写控制接口。

### 下一步方向

1. 保持 Stage 1–4 正向 marker 和旧 9 条负向 demo 稳定。
2. Linux 用户态程序兼容 plus（Stage 5+）：在本 myCPU 模拟器上继续扩展 `kernel_alpha`，通过旁路 `linux_compat_*` 模块尝试加载运行 `testsuits-for-oskernel` 中的 RISC-V64 Linux 用户态程序（`git`、`vim`、`gcc`、`rustc` 等），从 help-run 基线起步，不污染课程 OS 既有 marker。
3. AI/NPU、JIT/DBT、Pipeline-aware 调度作为独立后续方向，不回写扩大课程 OS 基线完成范围。

---

> *本报告基于 `course-os` 分支 commit `1d7668c` 版本编写，反映截至 2026-05-30 的课程 OS Stage 1–4 完整完成态。*
