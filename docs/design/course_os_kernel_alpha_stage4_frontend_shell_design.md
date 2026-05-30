# 课程 OS kernel_alpha Stage 4 前端交互 Shell 设计

## 文档定位

本文档记录《操作系统课程设计》Stage 4 在 `kernel_alpha` 分线上的当前有效设计边界。Stage 4 的目标不是继续扩大 `kernel_alpha_demo` 的一次性 marker，而是把 Stage 1 / Stage 2 / Stage 3 已完成的课程 OS 能力接入浏览器 `/console`，形成可长期运行、可输入命令、可观察 prompt settling 的课程 OS shell Lab。

本文档只说明长期有效的范围、接口边界和取舍；执行 checklist 写入对应 `plan` 文档，实时状态以对应 `status` 文档为准。

## 关联文档

- 状态文档：
  - [../status/kernel_alpha_status.md](../status/kernel_alpha_status.md)
  - [../status/mainline_status.md](../status/mainline_status.md)
- 已完成计划：
  - [../plan/history_plan.md#course-os-kernel-alpha-stage4-frontend-shell-plan](../plan/history_plan.md#course-os-kernel-alpha-stage4-frontend-shell-plan)
- 已完成设计 / 计划：
  - [course_os_kernel_alpha_stage3_design.md](course_os_kernel_alpha_stage3_design.md)
  - [course_os_kernel_alpha_stage2_design.md](course_os_kernel_alpha_stage2_design.md)
  - [course_os_kernel_alpha_stage1_design.md](course_os_kernel_alpha_stage1_design.md)
  - [../plan/history_plan.md#course-os-kernel-alpha-stage3-plan](../plan/history_plan.md#course-os-kernel-alpha-stage3-plan)
  - [../plan/history_plan.md#course-os-kernel-alpha-stage2-plan](../plan/history_plan.md#course-os-kernel-alpha-stage2-plan)
  - [../plan/history_plan.md#course-os-kernel-alpha-stage1-plan](../plan/history_plan.md#course-os-kernel-alpha-stage1-plan)
- 前端 / 调试链路：
  - [post_wave7_frontend_lab_product_design.md](post_wave7_frontend_lab_product_design.md)
  - [debug_frontend_integration.md](debug_frontend_integration.md)
  - [minimal_interactive_os_design.md](minimal_interactive_os_design.md)

## 背景与问题

Stage 1 / Stage 2 / Stage 3 已经把 `kernel_alpha_demo` 从旧 bring-up marker 推进到完整课程 OS summary：调度、Demand Paging / Clock、FS、syscall、进程、FD / FS、shell、COW、crash isolation、ELF / libc、同步、扩展 `/proc` 和 Stage 3 稳定 marker 都已经可回归。

但当前正向 `kernel_alpha_demo` 仍是一次性 smoke：输出 `KMVPET|course-os-stage1 ...|course-os-stage2 ...|course-os-stage3 ...` 后关机。它适合做门禁，却不适合在 `/console` 中展示课程 OS 的真实交互能力。现有 `/console` 已有 `guest_interactive_os_demo` 的 monitor prompt 和 `linux_proto_console` 的 Linux serial prompt；Stage 4 应沿这条 manifest / session / terminal 合同新增课程 OS shell，而不是重写前端或让 `kernel_alpha_demo` 改成常驻入口。

## 目标

- 新增独立 `guest_course_os_shell_demo`，启动后进入 `course-os> ` prompt。
- 让 `/console` 能通过现有 `Load / Run / Pause / Step / Reset / Terminate` 控制课程 OS shell session。
- 让 browser terminal 输入命令后等待当前 offset 之后的新 `course-os> ` prompt，避免旧 prompt 提前 settle。
- 复用 Stage 3 的 `course_shell`、FD / FS、procfs、ELF / libc、COW 和 crash isolation 能力。
- 增加 proc 快捷命令，让课程展示不依赖输入长路径：
  - `meminfo`
  - `schedstat`
  - `fsstat`
  - `syscalls`
  - `cow`
  - `crashlog`
  - `cpuinfo`
  - `uptime`
  - `status [pid]`
  - `fd [pid]`
  - `maps [pid]`
- 保持 `kernel_alpha_demo` 的 Stage 1 / Stage 2 / Stage 3 marker、functional / pipeline guest 回归和旧 9 条负向 demo 不变。
- 把 Stage 4 作为前端 Lab / 展示入口，不回头扩大 Stage 3 完成定义。

## 非目标

- 不把 `kernel_alpha_demo` 改成长驻 shell；它继续作为一次性 marker smoke。
- 不新增前端专用执行协议；前端继续消费 manifest、debug session、terminal 和 snapshot。
- 不新增任意镜像上传、任意命令代理、文件上传、浏览器内虚拟磁盘编辑或通用 IDE 功能。
- 不把课程 shell 声明为完整 POSIX shell、完整 bash、完整 signal / job control 或 Linux 用户态兼容。
- 不在 Stage 4 引入 AI/NPU、JIT/DBT、Pipeline-aware scheduling 或微内核化。
- 不把 `/proc` 写接口作为控制面；Stage 4 仍只展示只读证据面。

## 约束与边界

- `guest_course_os_shell_demo` 是独立 guest 入口，推荐源码路径为 `myCPU/guest/course_os_shell/main.c`，产物为 `myCPU/guest/course_os_shell.elf`。
- 新入口复用 `kernel_runtime_run_identity_superpage_bringup()`；不复制 `kernel_alpha/main.c` 的 Stage 1 / Stage 2 / Stage 3 summary 编排。
- 命令执行使用 `course_shell_run_line()` 作为唯一课程 shell 入口；proc 快捷命令应落在 guest shell / adapter 层，不由前端偷偷改写输入。
- manifest 名称固定为 `guest_course_os_shell_demo`，prompt 固定为 `course-os> `。
- 前端只根据 manifest 元数据显示课程 OS shell Lab，不根据 terminal 文本推断额外 OS 状态。
- `Reset` 必须清空 terminal projection 和 offset，并重新到达 `course-os> `。
- `Terminate` 仍由前端关闭 debug CLI session；guest 内部的 `exit` 命令只输出结果并回到 prompt，不作为浏览器 session 生命周期控制面。

## 方案

### 结构设计

Stage 4 采用“三层复用 + 一条新入口”的结构：

| 层 | 职责 | 主要改动 |
|---|---|---|
| guest shell | 课程 OS 命令、proc 快捷别名、脚本 / 外部程序复用 | 扩展 `course_shell` 或新增薄 adapter |
| guest entry | 常驻 UART REPL、prompt、line overflow、reset 后稳定启动 | 新增 `guest/course_os_shell.elf` |
| debug server manifest | 声明 image、disk、prompt、command wait budget、workload metadata | 新增 `guest_course_os_shell_demo` entry |
| frontend Lab | 新增课程 OS Shell 卡片、guide、terminal title / hint | 修改 `/console` 静态 catalog 和 render tests |

`interactive_os` 继续是 bring-up monitor；`guest_course_os_shell_demo` 是课程 OS shell；`kernel_alpha_demo` 继续是课程 OS summary smoke。三者职责不互相覆盖。

### Guest Shell 合同

第一版稳定命令面：

- 现有 shell 命令：`help`、`ls`、`cat`、`echo`、`ps`、`kill`、`cd`、`pwd`、`exit`、`exec`、`sh`。
- 现有 I/O 能力：单级 `|`、`>`、`<`、文件读写、`/demo/stage3.sh`。
- proc 快捷命令：
  - `meminfo` 等价读取 `/proc/meminfo`
  - `schedstat` 等价读取 `/proc/schedstat`
  - `fsstat` 等价读取 `/proc/fsstat`
  - `syscalls` 等价读取 `/proc/syscalls`
  - `cow` 等价读取 `/proc/cow`
  - `crashlog` 等价读取 `/proc/crashlog`
  - `cpuinfo` 等价读取 `/proc/cpuinfo`
  - `uptime` 等价读取 `/proc/uptime`
  - `status [pid]` 等价读取 `/proc/<pid>/status`，无参数时默认 shell pid
  - `fd [pid]` 等价读取 `/proc/<pid>/fd`，无参数时默认 shell pid
  - `maps [pid]` 等价读取 `/proc/<pid>/maps`，无参数时默认 shell pid

未知命令、未知 pid、坏路径或 proc 写入仍 fail-closed，输出简短错误后回到 prompt。

### Guest Entry 合同

`guest_course_os_shell_demo` 启动流程：

1. 初始化 `kernel_runtime_t`。
2. 完成最小 identity-superpage bring-up。
3. 初始化 `course_os_stage3_t`，调用新增 `course_os_stage3_prepare_shell()` 准备 Stage 3 demo 文件，但不运行 Stage 3 summary smoke。
4. 输出 banner，例如 `course-os shell ready`。
5. 输出 `course-os> `。
6. 轮询 UART 输入；一行 ready 后调用课程 shell，输出命令结果，再输出新 prompt。
7. 行过长时输出 `line too long` 并回到 prompt。

该入口默认不调用 `platform_shutdown()`，除非发生 kernel panic 或未来显式设计新的 `halt` 命令。浏览器 session 结束由 `Terminate` 负责。`course_os_stage3_prepare_shell()` 是 Stage 4 新增的 guest-side helper，只做 shell 演示文件准备，不改变 `course_os_stage3_run()` 的 marker 合同。

### Manifest / Session 合同

`frontend/server/tests_manifest.mjs` 新增 manifest entry：

- `name`: `guest_course_os_shell_demo`
- `menuLabel`: `guest_course_os_shell_demo · Course OS shell`
- `image`: `myCPU/guest/course_os_shell.elf`
- `disk`: `myCPU/tests/data/storage_basic.txt`
- `diskReady`: `true`
- `kind`: `guest`
- `backend`: `pipeline`
- `bootUntilUartText`: `course-os> `
- `terminalPrompt`: `course-os> `
- `commandUntilUartText`: `course-os> `
- `bootMaxSteps`: 使用课程 OS shell 专用 budget
- `commandMaxSteps`: 使用课程 OS shell 专用 budget
- `title`: `Course OS Shell`
- `badge`: `Course OS`
- `workload.stage`: `Course OS Stage 4`
- `workload.expectedMarker`: `course-os> `

runtime 不需要新增 endpoint。现有 `load()`、`terminalInput()`、`reset()` 已支持 boot marker、terminal prompt 和 command wait；Stage 4 只消费这些元数据。

### Frontend 展示合同

`/console` 新增 Course OS Shell Lab：

- 放入 `System Labs` 或 `Runtime Labs`，标题为 `Course OS Shell`。
- guide 说明它证明的是课程 OS Stage 1 / Stage 2 / Stage 3 能力的交互入口，而不是 Linux shell。
- `terminalPresentation()` 对 `guest_course_os_shell_demo` 返回：
  - title: `Course OS shell terminal`
  - target: `Course OS shell`
- 卡片 `proves` 至少覆盖：
  - 浏览器 terminal、debug CLI session、UART、课程 shell 形成闭环。
  - 课程 OS 的 procfs、FD / FS、pipe / redirection、ELF / libc、COW / crash evidence 可通过命令观察。
- 卡片 `boundary` 明确：这是课程级 shell，不声明完整 POSIX / Linux 兼容。

## 验证思路

最小门禁：

- `cd myCPU && make test-unit-course_os_stage2_shell`
- `cd myCPU && make test-unit-course_os_stage3_fs_shell`
- `cd myCPU && make test-unit-course_os_stage3_proc`
- `cd myCPU && make test-guest-course_os_shell_demo`
- `cd myCPU && make test-pipeline-guest-course_os_shell_demo`
- `cd frontend && node --test`
- `git diff --check`

完成门禁：

- `cd myCPU && make test-guest-kernel_alpha_demo`
- `cd myCPU && make test-pipeline-guest-kernel_alpha_demo`
- `cd myCPU && make test`
- `cd myCPU && make test-pipeline`

关键场景：

- load 后 terminal 输出 `course-os> `。
- `help` 返回课程 shell 命令和 proc 快捷命令。
- `echo file > /tmp/a` 后 `cat /tmp/a` 返回 `file`。
- `echo pipe | cat` 返回 `pipe`。
- `ps`、`meminfo`、`schedstat`、`fsstat`、`syscalls`、`cow`、`cpuinfo`、`uptime` 返回对应证据。
- `exec hello`、`exec forktest`、`exec crashdemo` 返回程序结果；`crashlog` 后仍能继续输入命令。
- reset 后旧 offset 不再读到旧输出，新 session 回到 `course-os> `。

## 风险与取舍

- 如果把 `kernel_alpha_demo` 改成长驻 shell，会破坏当前 marker 门禁。Stage 4 用独立 guest entry 隔离风险。
- 如果 proc 快捷命令在前端改写，测试会变成 UI 假象。Stage 4 要让 guest 自己理解这些命令。
- 如果 frontend 为课程 shell 新增专用 endpoint，会制造并行事实来源。Stage 4 只扩 manifest 元数据和现有 terminal 合同。
- 如果默认只跑 functional，容易漏掉 pipeline prompt settling 问题。Stage 4 必须保留 functional / pipeline 两条 guest 回归。
- 如果 shell 命令面继续膨胀，会挤占课程 OS 维护边界。第一版只锁定现有 shell 能力和 proc 快捷别名。

## 当前有效性说明

- 当前有效：本文档作为课程 OS `kernel_alpha` Stage 4 前端交互 shell 的设计口径。
- Stage 4 已完成，执行归档见 [../plan/history_plan.md#course-os-kernel-alpha-stage4-frontend-shell-plan](../plan/history_plan.md#course-os-kernel-alpha-stage4-frontend-shell-plan)。
- Stage 1 / Stage 2 / Stage 3 完成态以 [../status/kernel_alpha_status.md](../status/kernel_alpha_status.md) 和 [../plan/history_plan.md](../plan/history_plan.md) 中对应归档为准。
