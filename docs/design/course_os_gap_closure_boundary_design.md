# 课程 OS 缺口收口边界设计

## 文档定位

本文档定义 Stage 1-4 课程 OS 基线与《操作系统课程设计》A 方案要求之间的缺口收口边界。

它不替代 [course_os_kernel_alpha_course_os_baseline_design.md](course_os_kernel_alpha_course_os_baseline_design.md)
的长期基线，也不替代
[course_os_kernel_alpha_linux_compat_plus_design.md](course_os_kernel_alpha_linux_compat_plus_design.md)
的 Linux compat Plus 边界；它只负责说明哪些内容属于展示前课程证据补洞，哪些内容必须拆到架构后续或外部验证。

## 关联文档

- 课程要求：[../background/操作系统课程设计-A方案-OS内核实现.md](../background/操作系统课程设计-A方案-OS内核实现.md)
- 课程 OS 基线设计：[course_os_kernel_alpha_course_os_baseline_design.md](course_os_kernel_alpha_course_os_baseline_design.md)
- Linux compat Plus 设计：[course_os_kernel_alpha_linux_compat_plus_design.md](course_os_kernel_alpha_linux_compat_plus_design.md)
- 当前状态：[../status/kernel_alpha_status.md](../status/kernel_alpha_status.md)
- 展示前计划归档：[../plan/history_plan.md#course-os-display-gap-closure-plan](../plan/history_plan.md#course-os-display-gap-closure-plan)
- 架构后续计划归档：[../plan/history_plan.md#course-os-arch-followup-plan](../plan/history_plan.md#course-os-arch-followup-plan)
- Plus / 外部验证计划归档：[../plan/history_plan.md#course-os-plus-external-validation-plan](../plan/history_plan.md#course-os-plus-external-validation-plan)

## 分层原则

原单体缺口收口计划把展示前补洞、架构增强和 OSComp / 外部资产验证放在同一张清单中，导致优先级和验证口径混在一起。后续按三层管理：

1. **展示前课程证据补洞**
   - 目标是让 Stage 1-4 已有课程 OS 能力在 shell、procfs、guest marker 和前端展示中更直接可见。
   - 允许补充窄接口和 shell 命令，例如目录枚举、真实 `ls`、真实 `kill`、同步机制演示、`mkfs` 命令、`timer_hz` 证据。
   - 不引入新的执行模型，不改变 Stage 1/2/3 marker 和 Stage 4 `course-os> ` prompt。

2. **架构后续增强**
   - 目标是处理会改变 trap / timer / scheduler / console / ELF 执行链路的能力。
   - 在线抢占调度、UART 中断驱动输入、真实课程 ELF 执行、多级管道和 context switch cost 都属于这一层。
   - 这类工作需要先落设计或局部合同，再做实现；不能作为展示前 P0 被混入简单补洞。

3. **Plus / 外部验证**
   - 目标是把 Linux compat Plus 与 testsuits-for-oskernel / OSComp 资产对接成可复验的 opt-in 证据。
   - 该层不属于 Stage 1-4 课程基线完成定义，也不默认依赖本机外部 rootfs、OSComp 仓库或网络资产。
   - 缺资产时必须跳过或 fail-closed，不能让默认回归变红。

## 展示前接口合同

展示前补洞可以扩展课程 OS 自有 API，但合同必须小而明确。

### `course_fs_listdir`

- 输入路径必须解析为已有目录；不存在路径、普通文件、空指针或输出缓冲区为 0 都返回 `false`。
- 输出只列出直接子节点名，不递归。
- 子节点按 `course_fs` 目录索引顺序输出；当前索引按文件名升序维护。
- 多个名字之间用单个空格分隔，末尾追加一个换行符，再追加 `NUL`。
- 空目录输出为 `"\n"`。
- 只有完整输出含末尾 `NUL` 能放入 `out` 时才返回 `true`；缓冲区不足时返回 `false`，不得静默截断为成功。

### shell 展示命令

- `ls` 应调用 `course_fs_listdir`，默认列出 cwd，带参数时列出目标路径。
- `kill` 应走 `course_process` 的真实进程状态转换，而不是固定字符串。
- `sem` / `mutex` / `concurrency_demo` 只展示课程同步机制，不要求引入真实多核或完整 POSIX signal。
- `mkfs` 是课程 shell 工具证据，允许重置课程 FS 实例；不等同于外部磁盘镜像制作工具。
- `timer_hz` 是证据字段，优先通过 `/proc/cpuinfo` 或等价 procfs 输出固定合同；不借此重写平台计时器。

## 非目标

- 不把 Stage 1-4 改造成完整 POSIX / Linux 内核。
- 不把 Linux compat Plus 的 syscall / ELF / rootfs 语义倒灌到课程 `course_*` 模块。
- 不把 OSComp、外部 rootfs、交叉编译器或网络访问设为默认门禁。
- 不把“QEMU 中 < 1ms”作为当前默认可验证指标；若要验证 context switch cost，应先定义模拟器 cycle 到时间的换算合同。

## 验证分层

- 默认门禁只使用仓库内资产：相关 `test-unit-course_os_*`、`test-guest-kernel_alpha_demo`、`test-guest-course_os_shell_demo` 和必要的 `git diff --check`。
- Slow guest 门禁只在改动 guest demo、pipeline guest 或 Stage marker 时启用，并在计划中写明具体命令。
- Opt-in external 门禁只出现在 Plus / 外部验证计划中，并显式声明环境变量、资产路径和缺资产行为。
