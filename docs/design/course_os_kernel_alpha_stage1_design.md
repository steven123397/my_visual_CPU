# 课程 OS kernel_alpha 第一阶段设计

## 文档定位

本文档记录《操作系统课程设计》第一阶段在 `kernel_alpha` 上的设计定稿：以课程基本要求为边界，只承诺 3 个模块、9 个功能点，并保留一个只读 `/proc` 指标证据面。

本文档只说明长期有效的方案边界、取舍和验收口径，不承担实时进度更新。当前工程状态以对应 `status` 文档为准。

## 关联文档

- 状态文档：
  - [../status/kernel_alpha_status.md](../status/kernel_alpha_status.md)
  - [../status/mainline_status.md](../status/mainline_status.md)
- 相关计划：
  - [../plan/history_plan.md#course-os-kernel-alpha-stage1-plan](../plan/history_plan.md#course-os-kernel-alpha-stage1-plan)

## 背景与问题

仓库已有可运行的 RISC-V 模拟器、guest runtime、`kernel_alpha` bring-up 基线、Sv39、trap、user task、storage、monitor 等基础设施。课程 OS 方向不需要从零搭建运行环境，而应在现有 `kernel_alpha` 入口上重新定义一条更适合课程验收的内核主线。

此前 `kernel_alpha` 的 Phase 1 输出以固定 `KMVPETDS` 字符序列证明 bring-up 链路完成。该输出适合作为历史基线，但不适合作为课程 OS 第一阶段的当前行为承诺。第一阶段需要从“能启动并探测设备”转向“能展示进程、内存、文件系统三类 OS 核心机制”。

因此第一阶段采用“方案 A 作为基本目标”：只覆盖课程基本要求中的 3 个模块、9 个功能点，不追求六大模块全覆盖。更高展示价值的 AI/NPU、JIT、Pipeline 感知、前端可视化、微内核、安全隔离等内容全部推迟到 D 阶段扩展池。

## 目标

- 将 `kernel_alpha` 重新定位为课程 OS 主线入口。
- 第一阶段冻结为 3 个模块、9 个功能点：
  - 进程模块：FCFS、RR、CFS-lite。
  - 内存模块：Demand Paging、Clock 页面置换、`kmalloc` / `kfree`。
  - 文件系统模块：文件 / 目录 CRUD、`seek`、B 树目录索引。
- 提供只读 `/proc` 指标接口，作为答辩和回归的证据面。
- 复用现有 guest 基础设施：PMM、Sv39、trap、user task、storage、monitor。
- 保留旧 Phase 1 `KMVPETDS` 作为历史基线记录，但不再把它作为课程 OS 当前行为承诺。

## 非目标

- 第一阶段不覆盖课程六大模块的全量扩展。
- 第一阶段不实现 MLFQ、优先级继承、Pipeline-aware scheduling。
- 第一阶段不实现 SLAB、Buddy、WSClock、COW Fork。
- 第一阶段不实现 COW Snapshot、LFS、自适应预读块缓存。
- 第一阶段不实现完整前端面板或浏览器 Lab 可视化。
- 第一阶段不实现 `/proc` 写控制接口。
- 第一阶段不实现 `sys_ai_submit`、AI Shell、NPU 中断驱动调度。
- 第一阶段不接入默认 JIT / DBT runtime，也不改变 simulator 的 guest 可见语义。

## 约束与边界

- 第一阶段以课程基本要求为验收上限，避免为了展示效果提前拉入大功能面。
- `kernel_alpha` 可以解冻并替换旧正向输出，但旧 Phase 1 行为只保留为历史语境。
- 新行为应优先复用现有 guest runtime，而不是复制一套平行内核基础设施。
- `/proc` 作为只读证据面，不作为第一阶段控制面。
- 指标输出必须可被 guest smoke 或 host-side harness 稳定校验，避免只依赖人工观察。
- 课程 OS 主线的设计、计划、状态必须继续保持分离：
  - 本文档记录长期边界和取舍。
  - 后续分阶段落地再进入 `docs/plan/`。
  - 当前进度和风险只写入对应 `status` 文档。

## 方案

### 总体结构

第一阶段结构采用“算法三件套 + 指标证据面”：

| 模块 | 第一阶段选择 | 保留对照 / 证据 |
|---|---|---|
| 进程管理 | CFS-lite | FCFS / RR 可切换对照，输出等待时间、周转时间、上下文切换次数 |
| 内存管理 | Clock 页面置换 | Demand Paging 可触发缺页装入，`/proc/meminfo` 输出页错误和回收统计 |
| 文件系统 | B 树目录索引 | 文件 / 目录 CRUD、路径解析、`seek`，输出查找步数或比较次数 |
| 证据面 | 只读 `/proc` | `/proc/ps`、`/proc/meminfo`、`/proc/schedstat`、`/proc/fsstat` |

### 进程管理

第一阶段实现 3 种可切换调度策略：

- FCFS：课程基础对照策略，按到达顺序运行。
- RR：课程基础对照策略，保留可配置时间片。
- CFS-lite：第一阶段主算法，用 `vruntime` 表示公平性，选择 `vruntime` 最小的 runnable task。

CFS-lite 不要求直接实现 Linux 红黑树。教学级实现可先用排序链表或小规模 ordered run queue，优先固定可解释性和可测试性。

进程模块至少暴露以下统计：

- 每个任务的等待时间。
- 每个任务的周转时间。
- 全局上下文切换次数。
- 当前调度策略。

这些统计进入 `/proc/ps` 和 `/proc/schedstat`，并用于同一 workload 下 FCFS / RR / CFS-lite 的对照演示。

### 内存管理

第一阶段内存模块以 Demand Paging + Clock 页面置换 + `kmalloc` / `kfree` 为主线。

Demand Paging 的目标是让未驻留页面在访问时通过 page fault 装入，而不是在进程创建时一次性分配全部物理页。Clock 页面置换在物理页压力下触发，利用页表访问位或等价的软件引用位近似 LRU，循环扫描候选页并回收可替换页面。

`/proc/meminfo` 至少暴露：

- `total`：物理页或物理内存总量。
- `free`：可分配页数或空闲内存。
- `used`：已使用页数或内存。
- `page_fault`：缺页次数。
- `page_reclaim`：Clock 回收次数。

`kmalloc` / `kfree` 第一阶段只承担课程要求和内核对象分配需求，不升级为 SLAB 或 Buddy。

### 文件系统

第一阶段文件系统模块以课程基础文件能力和 B 树目录索引为主线。

基础能力包括：

- 文件创建、删除、读取、写入。
- 目录创建、删除、遍历。
- 绝对路径和相对路径解析。
- 文件描述符读写。
- `seek` 定位读写。

B 树目录索引用于优化目录项查找。教学级实现可采用简化 B 树、B+ 树或 2-3 树，只要接口语义固定为“按文件名 key 定位 inode / file entry”，并能输出查找步数或比较次数即可。

`/proc/fsstat` 至少暴露：

- 文件和目录操作计数。
- 路径解析次数。
- 目录索引查找次数。
- B 树查找比较次数或步数。
- 可选的线性扫描对照统计。

### `/proc` 指标接口

第一阶段 `/proc` 只读为主，提供课程 OS 的统一证据面：

| 路径 | 内容 |
|---|---|
| `/proc/ps` | 任务列表、状态、调度策略、等待时间、周转时间 |
| `/proc/meminfo` | 内存总量、空闲量、已用量、缺页次数、页回收次数 |
| `/proc/schedstat` | 调度策略、上下文切换次数、各策略运行统计 |
| `/proc/fsstat` | 文件系统操作计数、路径解析次数、目录索引查找统计 |

这些接口用于答辩演示、guest smoke 和后续前端可视化的共同数据源。第一阶段不通过 `/proc` 修改内核参数，避免控制面和证据面混在一起。

## 第一阶段不选项

以下方向保留为明确不选，避免第一阶段范围漂移：

- 调度：MLFQ、优先级继承、Pipeline-aware scheduling。
- 内存：SLAB、Buddy、WSClock、COW Fork。
- 文件系统：COW Snapshot、LFS、自适应预读块缓存。
- 展示：完整浏览器前端 OS 面板。
- AI/NPU：`sys_ai_submit`、NPU IRQ wakeup、AI Shell。
- 架构：微内核 / IPC、安全隔离、容器 / 虚拟化扩展。

## D 阶段扩展池

第一阶段完成后，可从下列方向选择 D 阶段增强：

- AI/NPU 全链路：`sys_ai_submit -> NPU IRQ wakeup -> ai-run -> 前端 NPU profile`。
- Pipeline / JIT 协同：调度器只读消费 Pipeline snapshot 或 JIT / DBT profile。
- 前端 Lab 可视化：把 `/proc` 指标接入浏览器工作台。
- 微内核 / IPC：把部分服务拆出为用户态 server 或内核内消息通道。
- 安全隔离：权限、能力或沙箱边界。
- 更完整文件系统：快照、日志结构或块缓存预读。

这些方向不阻塞第一阶段交付。进入扩展池的内容必须重新写计划和验收，不应回头扩大第一阶段承诺。

## 验证思路

第一阶段验收 smoke 应覆盖以下行为：

- 调度：同一 workload 下 FCFS / RR / CFS-lite 可切换，并输出不同调度统计。
- 内存：缺页装入可观测；物理页压力下 Clock 触发回收；`kmalloc` / `kfree` 释放后可复用。
- 文件系统：批量创建文件；目录通过 B 树索引查找；`seek` 读写结果正确。
- `/proc`：`ps`、`meminfo`、`schedstat`、`fsstat` 与内核内部统计一致。

后续实施触及 guest runtime 时，默认关注：

- `cd myCPU && make test-unit-kernel_runtime`
- `cd myCPU && make test-unit-vm_address_space`
- `cd myCPU && make test-unit-vm_process`
- `cd myCPU && make test-unit-vm_fault`
- `cd myCPU && make test-unit-user_task`
- `cd myCPU && make test-guest-kernel_alpha_demo`
- `cd myCPU && make test`
- `git diff --check`

## 风险与取舍

- CFS-lite 的真实 Linux 相似度有限。第一阶段优先保证公平性概念、统计证据和可切换对照，而不是复制完整 CFS。
- Clock 页面置换依赖页访问状态。如果硬件 A 位语义在当前 guest / simulator 路径上不完整，应明确使用软件引用位或 fault / trap 记录作为等价近似。
- B 树目录索引会增加文件系统元数据复杂度。第一阶段应优先固定目录内查找收益，不提前扩展到完整磁盘一致性、日志或快照。
- `/proc` 指标容易膨胀为控制面。第一阶段只读，后续若要写控制接口，需要独立设计。
- `kernel_alpha` 从 bring-up demo 转为课程 OS 主线，会影响旧输出门禁。旧 `KMVPETDS` 应降级为历史基线，而不是继续强绑当前正向 demo。

## 当前有效性说明

- 当前有效 / 历史语境：本文档当前有效，记录课程 OS 第一阶段设计定稿；第一阶段实现已归档到 [../plan/history_plan.md#course-os-kernel-alpha-stage1-plan](../plan/history_plan.md#course-os-kernel-alpha-stage1-plan)。
- 旧 `kernel_alpha` Phase 1 `KMVPETDS` 输出属于历史语境，当前课程 OS 行为以后续状态文档和实现门禁为准。
- 如果相关工作进入实施或完成，实时状态应回写到 [../status/kernel_alpha_status.md](../status/kernel_alpha_status.md) 或 [../status/mainline_status.md](../status/mainline_status.md)，执行 checklist 应进入 `docs/plan/` 或归档到 [../plan/history_plan.md](../plan/history_plan.md)。
