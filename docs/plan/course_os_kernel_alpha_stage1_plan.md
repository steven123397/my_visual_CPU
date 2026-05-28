# 课程 OS kernel_alpha 第一阶段实现计划

> **文档状态：** 执行中

## 文档定位

本文档记录 `kernel_alpha` 从 Phase 1 bring-up demo 切换为《操作系统课程设计》第一阶段主线的执行计划。

本文档只承担执行 checklist；长期边界以 [../design/course_os_kernel_alpha_stage1_design.md](../design/course_os_kernel_alpha_stage1_design.md) 为准，实时状态以 [../status/kernel_alpha_status.md](../status/kernel_alpha_status.md) 为准。

## 关联文档

- 来源设计：
  - [../design/course_os_kernel_alpha_stage1_design.md](../design/course_os_kernel_alpha_stage1_design.md)
- 目标状态：
  - [../status/kernel_alpha_status.md](../status/kernel_alpha_status.md)
  - [../status/mainline_status.md](../status/mainline_status.md)
- 已完成计划归档：
  - [history_plan.md](history_plan.md)

## 目标

- 解冻 `kernel_alpha_demo` 的旧 `KMVPETDS` 正向输出，把它切换为课程 OS 第一阶段入口。
- 第一阶段只实现 3 个模块、9 个功能点：
  - 进程模块：FCFS、RR、CFS-lite。
  - 内存模块：Demand Paging、Clock 页面置换、`kmalloc` / `kfree`。
  - 文件系统模块：文件 / 目录 CRUD、`seek`、B 树目录索引。
- 提供只读 `/proc` 指标接口：
  - `/proc/ps`
  - `/proc/meminfo`
  - `/proc/schedstat`
  - `/proc/fsstat`
- 保留旧 Phase 1 负向 demo 作为基础设施 guardrail，不再把旧 `KMVPETDS` 作为当前课程 OS 行为承诺。

## 非目标

- 不实现 MLFQ、优先级继承、Pipeline-aware scheduling。
- 不实现 SLAB、Buddy、WSClock、COW Fork。
- 不实现 COW Snapshot、LFS、自适应预读块缓存。
- 不实现 `/proc` 写控制接口。
- 不接入 AI/NPU、JIT、前端 Lab 可视化、微内核、安全隔离。
- 不把 `SimpleStorage` 扩成完整持久化文件系统或中断驱动块设备。

## 完成定义

- `kernel_alpha_demo` 正向 smoke 能展示课程 OS 第一阶段的调度、内存、文件系统和 `/proc` 证据。
- FCFS / RR / CFS-lite 可在同一 workload 下切换，并输出等待时间、周转时间、上下文切换次数。
- Demand Paging 可触发缺页装入；物理页压力下 Clock 可回收页面；`kmalloc` / `kfree` 释放后可复用。
- 文件 / 目录 CRUD、路径解析和 `seek` 可通过 guest smoke；目录查找使用 B 树索引并输出查找步数或比较次数。
- `/proc/ps`、`/proc/meminfo`、`/proc/schedstat`、`/proc/fsstat` 与内核内部统计一致。
- 旧 9 条 `kernel_alpha` 负向 demo 仍能防止 storage / interrupt / fault 基础设施回退，或已被明确替换为等价 guardrail。
- `docs/status/kernel_alpha_status.md` 已回写完成结果、剩余风险和新的验证基线。

## 任务

### 任务 1：建立课程 OS smoke 验收壳

**文件：**
- 修改：`myCPU/guest/kernel_alpha/main.c`
- 可能修改：`myCPU/guest/kernel_alpha/common.c`
- 修改：`myCPU/Makefile`
- 新增或修改：`myCPU/tests/host/*kernel_alpha*` 或现有 guest smoke harness

- [ ] **步骤 1：确认当前 `kernel_alpha_demo` 构建与测试入口**
  - 阅读 `myCPU/guest/kernel_alpha/main.c`、`myCPU/Makefile` 和现有 `test-guest-kernel_alpha_demo` 规则。
  - 记录当前正向输出检查点，明确哪些测试仍强绑 `KMVPETDS`。
- [ ] **步骤 2：新增课程 OS 正向 smoke 的最小期望**
  - 先让 smoke 只检查稳定 banner / summary 行，不一次性要求三大模块全部完成。
  - 预期输出至少包含当前模式，例如 `course-os-stage1` 或等价稳定 marker。
- [ ] **步骤 3：保留旧负向 demo 的 guardrail**
  - 确认 `fault`、`plic_not_ready`、`timer_not_ready`、storage error demos 不依赖新课程 OS 正向输出。
  - 如有共享 marker helper，被新入口替换前先拆出兼容层。
- [ ] **步骤 4：验证**
  - `cd myCPU && make test-guest-kernel_alpha_demo`
  - `cd myCPU && make test-guest-kernel_alpha_fault_demo`
  - `cd myCPU && make test-guest-kernel_alpha_plic_not_ready_demo`
  - `cd myCPU && make test-guest-kernel_alpha_timer_not_ready_demo`

### 任务 2：进程模块：FCFS / RR / CFS-lite

**文件：**
- 修改：`myCPU/guest/kernel/user_task.c`
- 修改：`myCPU/guest/kernel/user_task_bootstrap.c`
- 可能新增：`myCPU/guest/kernel/scheduler.c`
- 可能新增：`myCPU/guest/include/scheduler.h`
- 测试：`myCPU/tests/unit/*user_task*`
- 测试：`myCPU/guest/kernel_alpha/main.c`

- [ ] **步骤 1：梳理现有 user task 生命周期**
  - 阅读 `user_task.c`、`user_task_bootstrap.c`、`user_program.c` 和相关 unit tests。
  - 确认当前 task state、上下文切换和 timer 驱动边界。
- [ ] **步骤 2：抽出可切换 scheduler policy**
  - 固定 `FCFS`、`RR`、`CFS-lite` 三个 policy enum。
  - 保持默认策略显式可见，避免隐式改变旧 user task smoke。
- [ ] **步骤 3：实现 FCFS / RR 对照**
  - FCFS 按创建顺序运行。
  - RR 保留时间片计数或等价 tick 预算。
- [ ] **步骤 4：实现 CFS-lite**
  - 用 `vruntime` 和排序链表或小规模 ordered run queue 实现最小公平选择。
  - 不引入红黑树，除非现有结构已经需要。
- [ ] **步骤 5：接入统计**
  - 记录等待时间、周转时间、上下文切换次数和当前策略。
  - 确保统计能被 `/proc/ps` 和 `/proc/schedstat` 读取。
- [ ] **步骤 6：验证**
  - `cd myCPU && make test-unit-user_task`
  - `cd myCPU && make test-unit-user_task_bootstrap`
  - `cd myCPU && make test-guest-kernel_alpha_demo`

### 任务 3：内存模块：Demand Paging / Clock / kmalloc-kfree

**文件：**
- 修改：`myCPU/guest/kernel/vm_fault.c`
- 修改：`myCPU/guest/kernel/vm_address_space.c`
- 修改：`myCPU/guest/kernel/vm_process.c`
- 修改：`myCPU/guest/kernel/pmm.c`
- 修改：`myCPU/guest/kernel/memory.c`
- 可能新增：`myCPU/guest/kernel/page_reclaim.c`
- 可能新增：`myCPU/guest/include/page_reclaim.h`
- 测试：`myCPU/tests/unit/*vm*`
- 测试：`myCPU/tests/unit/*kernel_runtime*`

- [ ] **步骤 1：确认现有 fault policy**
  - 阅读 `vm_fault.c`、`vm_address_space.c`、`vm_object.c` 和 `pmm.c`。
  - 区分当前 recovery、panic 和 lazy allocation 边界。
- [ ] **步骤 2：实现 Demand Paging 最小路径**
  - 用户页首次访问时通过 fault 分配或装入页面。
  - 统计 `page_fault`，并能被 `/proc/meminfo` 读取。
- [ ] **步骤 3：实现 Clock 页面置换**
  - 先使用 PTE A 位；如果当前 guest 路径无法稳定使用 A 位，则用软件 referenced bit 明确替代。
  - 统计 `page_reclaim`。
- [ ] **步骤 4：固定 `kmalloc` / `kfree` 复用证据**
  - 保持实现为课程需求级别，不升级到 SLAB / Buddy。
  - 增加释放后复用的最窄测试或 smoke。
- [ ] **步骤 5：验证**
  - `cd myCPU && make test-unit-kernel_runtime`
  - `cd myCPU && make test-unit-vm_address_space`
  - `cd myCPU && make test-unit-vm_process`
  - `cd myCPU && make test-unit-vm_fault`
  - `cd myCPU && make test-guest-kernel_alpha_demo`

### 任务 4：文件系统模块：CRUD / seek / B 树目录索引

**文件：**
- 修改：`myCPU/guest/kernel/storage.c`
- 可能新增：`myCPU/guest/kernel/course_fs.c`
- 可能新增：`myCPU/guest/include/course_fs.h`
- 可能新增：`myCPU/guest/kernel/btree_dir.c`
- 可能新增：`myCPU/guest/include/btree_dir.h`
- 测试：`myCPU/guest/kernel_alpha/main.c`
- 测试：相关 guest smoke 或 host harness

- [ ] **步骤 1：确定第一阶段文件系统形态**
  - 优先实现课程演示所需的简化 FS，不把 `SimpleStorage` 扩成完整磁盘系统。
  - 明确文件、目录、inode / file entry、file descriptor 的最小结构。
- [ ] **步骤 2：实现文件 / 目录 CRUD 和路径解析**
  - 支持绝对路径和相对路径。
  - 支持至少一条批量创建文件的 smoke。
- [ ] **步骤 3：实现 `seek`**
  - 支持文件内偏移读写。
  - 增加跨偏移写入 / 读回的稳定输出。
- [ ] **步骤 4：实现 B 树目录索引**
  - 可采用简化 B 树、B+ 树或 2-3 树。
  - 输出查找步数或比较次数，作为 `/proc/fsstat` 证据。
- [ ] **步骤 5：验证**
  - `cd myCPU && make test-guest-kernel_alpha_demo`
  - 如新增 host/unit test，运行对应最窄 target。

### 任务 5：只读 `/proc` 指标接口

**文件：**
- 可能新增：`myCPU/guest/kernel/procfs.c`
- 可能新增：`myCPU/guest/include/procfs.h`
- 修改：`myCPU/guest/kernel_alpha/main.c`
- 修改：进程、内存、文件系统统计提供方

- [ ] **步骤 1：定义 `/proc` 只读接口**
  - `/proc/ps`
  - `/proc/meminfo`
  - `/proc/schedstat`
  - `/proc/fsstat`
- [ ] **步骤 2：接入统计源**
  - `/proc/ps` 读取 task 状态、调度策略、等待时间、周转时间。
  - `/proc/meminfo` 读取 total/free/used/page_fault/page_reclaim。
  - `/proc/schedstat` 读取上下文切换次数和 policy 统计。
  - `/proc/fsstat` 读取 CRUD、路径解析、目录索引查找和 B 树比较次数。
- [ ] **步骤 3：固定输出格式**
  - 使用稳定 key/value 或表格格式，便于 guest smoke 检查。
  - 第一阶段不支持写控制。
- [ ] **步骤 4：验证**
  - `cd myCPU && make test-guest-kernel_alpha_demo`
  - 若新增 parser / host smoke，运行对应 target。

### 任务 6：文档、门禁和分线交接

**文件：**
- 修改：`docs/status/kernel_alpha_status.md`
- 修改：`docs/status/mainline_status.md`（仅当主线优先级或 active line 变化）
- 修改：`docs/index.md`（如新增 / 删除正式文档）
- 修改：`myCPU/guest/AGENTS.md`（如 guest 边界或验证要求变化）
- 修改：`docs/plan/history_plan.md`
- 删除：`docs/plan/course_os_kernel_alpha_stage1_plan.md`（完成归档后）

- [ ] **步骤 1：回写状态**
  - `kernel_alpha_status.md` 写明课程 OS 第一阶段完成结果、保留 guardrail 和剩余风险。
- [ ] **步骤 2：同步文档索引和局部规则**
  - 如新增正式设计 / 计划 / 状态入口，更新 `docs/index.md`。
  - 如 guest runtime 边界变化，更新 `myCPU/guest/AGENTS.md`。
- [ ] **步骤 3：归档计划**
  - 在 `docs/plan/history_plan.md` 追加完成条目。
  - 删除本计划文件。
- [ ] **步骤 4：最终验证**
  - `git diff --check`
  - `cd myCPU && make test-guest-kernel_alpha_demo`
  - `cd myCPU && make test`

## 分线前协调说明

本计划可作为三条并行路线中的 `课程 OS / kernel_alpha` 路线输入。分线前应先把当前 coordinator 文档基线提交，再从同一 commit 创建工作树，避免 Linux、课程 OS 和全量代码审查三条线在 `docs/index.md`、`docs/status/mainline_status.md`、`docs/plan/history_plan.md` 上各自维护事实。

## 完成态回写要求

- 全部 checklist 必须勾完。
- [../status/kernel_alpha_status.md](../status/kernel_alpha_status.md) 必须增加：
  - 完成结果摘要。
  - 新的课程 OS 行为门禁。
  - 旧 Phase 1 guardrail 的保留或替换关系。
  - 剩余风险。
- 如主线 active line / 优先级变化，回写 [../status/mainline_status.md](../status/mainline_status.md)。
- 把“完成时间 + 完成内容 + 过程摘要”追加到 [history_plan.md](history_plan.md)。
- 归档完成后删除本计划文件，不长期保留完成态 checklist。
