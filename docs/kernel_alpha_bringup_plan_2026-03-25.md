# Kernel Alpha Bring-up 实现计划

> **面向 AI 代理的工作者：** 推荐使用 `superpowers:executing-plans` 或 `superpowers:subagent-driven-development` 按任务推进。步骤使用复选框（`- [ ]`）语法跟踪。

**目标：** 新增一条独立于 `guest_supervisor_demo` 的 `kernel_alpha_demo` bring-up 路径，完成第一次真正的小 kernel alpha 启动，并把它纳入可回归测试。

**架构：** 保持现有 `supervisor_demo_smoke` 不继续膨胀，在 `guest/` 下新增独立入口 ELF，复用共享的 guest kernel 基础设施。第一次 alpha 里程碑使用独立的 S-mode `kernel_main`、自建 Sv39 内核页表、RAM identity map 加 MMIO fault-range lazy map，验证最小 timer interrupt 路径。

**技术栈：** C11、RISC-V assembly、GNU Make、现有 guest runtime / VM / trap 基础设施。

---

## 里程碑定义

第一次真正的小 kernel bring-up 先限定为一个可回归的 alpha 基线，而不是一次性做完整 OS：

- 独立的 kernel ELF / entry，不再借 `guest_supervisor_demo` 的 smoke runner 承载。
- 进入 S-mode 后能输出 boot 标记。
- 完成 early allocator / PMM 初始化。
- 创建并启用自己的 Sv39 内核页表。
- 在开启 VM 后仍能通过 fault-range lazy map 访问 UART / CLINT。
- 至少观察到一次 supervisor timer interrupt。
- 失败时保持可观察，默认仍走现有 `panic_shutdown()` 路径。
- 能被 `make test` 稳定回归。

本轮首个成功标记定义为串行输出 `KMVT`：

- `K`：进入独立 kernel 入口。
- `M`：memory / PMM 初始化完成。
- `V`：自建页表启用后，UART 在 VM 下可继续输出。
- `T`：第一次 timer interrupt 已经到达。

## 文件范围

预计涉及：

- 创建：`docs/kernel_alpha_bringup_plan_2026-03-25.md`
- 创建：`myCPU/guest/kernel_alpha/main.c`
- 修改：`myCPU/Makefile`
- 修改：`AGENTS.md`
- 修改：`myCPU/AGENTS.md`
- 修改：`myCPU/guest/AGENTS.md`
- 修改：`docs/AGENTS.md`
- 修改：`readme.md`
- 视实现需要修改：`docs/code_self_review_2026-03-24.md`

## 任务 1：补计划与回归入口

**文件：**

- 创建：`docs/kernel_alpha_bringup_plan_2026-03-25.md`
- 修改：`myCPU/Makefile`

- [x] **步骤 1：把 alpha bring-up 的目标和验收标准写进 `docs/`**

要求：

- 明确它是独立 kernel alpha 路径，不是继续往 smoke runner 塞逻辑。
- 明确第一次 bring-up 的最小成功标准。
- 明确首个回归输出标记。

- [x] **步骤 2：先在 Makefile 中定义失败中的新 guest 测试目标**

运行：`cd myCPU && make test-guest-kernel_alpha_demo`

预期：

- 当前应失败，因为独立 `kernel_alpha_demo` 入口尚未实现。
- 失败应来自缺失的源文件或目标，而不是已有路径回归变红。

## 任务 2：实现独立 kernel alpha demo

**文件：**

- 创建：`myCPU/guest/kernel_alpha/main.c`
- 修改：`myCPU/Makefile`

- [x] **步骤 1：实现最小独立 `kernel_main`**

要求：

- 不复用 `supervisor_demo_smoke_run()`。
- 只依赖现有 `memory`、`pmm`、`vm`、`trap`、`timer`、`console`、`platform` 基础设施。
- 把状态机尽量收在单文件最小范围，不引入过早抽象。

- [x] **步骤 2：建立 alpha 版页表和 MMIO fault-range**

要求：

- RAM 先使用 coarse identity map，优先换来稳定 bring-up。
- UART / CLINT 通过 `vm_address_space_register_fault_range()` 做 lazy map。
- 启用 VM 后立刻输出 `V`，用可观察行为证明 fault path 生效。

- [x] **步骤 3：接通 timer interrupt 观察**

要求：

- 安装 supervisor timer policy。
- 调度一次 timer。
- 在 post-handler 中留下 `T` 标记。
- 主循环必须在 timer 未到达时走失败路径，而不是无限卡死。

- [x] **步骤 4：回归验证绿灯**

运行：`cd myCPU && make test-guest-kernel_alpha_demo`

预期：

- 输出严格等于 `KMVT`。

## 任务 3：同步文档

**文件：**

- 修改：`AGENTS.md`
- 修改：`myCPU/AGENTS.md`
- 修改：`myCPU/guest/AGENTS.md`
- 修改：`docs/AGENTS.md`
- 修改：`readme.md`
- 视需要修改：`docs/code_self_review_2026-03-24.md`

- [x] **步骤 1：更新仓库总览和 guest 当前状态**

要求：

- 把 `kernel_alpha_demo` 写成新的独立 bring-up 基线。
- 明确它和 `guest_supervisor_demo` 的分工差异。

- [x] **步骤 2：同步 README**

要求：

- 只保留读者需要知道的高层能力和回归入口。
- 不把内部实现流水账堆进 README。

## 任务 4：最终验证

**文件：**

- 修改：无新增代码文件

- [x] **步骤 1：运行独立 guest 回归**

运行：`cd myCPU && make test-guest-kernel_alpha_demo`

预期：

- 输出 `KMVT`

- [x] **步骤 2：运行全量基线**

运行：`cd myCPU && make test`

预期：

- 现有 asm / unit / `guest_supervisor_demo` / `kernel_alpha_demo` 全部通过。
