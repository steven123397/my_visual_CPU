# my_visual_CPU 演进规划

本文件整合两部分内容：

- **第一部分**：现有项目的升级与重构建议（先做认知重构，再做代码重构）。
- **第二部分**：未来发展路线规划（不对标商用产品，从项目内核长出来的发展方向）。

定位前提：项目当前已是可运行的 RISC-V 系统模拟器原型，具备 RV64IMAFDC、M/S/U + Sv39、virtio-blk、Alpine/Debian 真实 rootfs、MMIO NPU / TPU-like 加速器、浏览器 Lab Workbench、reference-first 多后端 + Spike 差分能力。本文件不再重复这些既成事实，只回答"接下来怎么走"。

## 当前执行口径（2026-05-29）

后续任务按三条线组织，但不等强度并行：

- **Linux / AI 主线**：继续作为近期产品工程主线。Linux 侧聚焦标准 Alpine / Debian 发行版平台剩余 capability、F/D / FS state 和 runtime 资产合同；AI 侧聚焦设备契约、bounded-dynamic shape、profile schema 和后续协同仿真故事。两条线都应继续以 `docs/status/mainline_status.md` 和各自专项 status 为实时事实来源。
- **OS 课程设计线**：`kernel_alpha` Stage 1 / Stage 2 已完成，当前默认进入维护、验证和展示材料整理状态。除非课程交付明确要求新阶段，否则不继续扩大 Stage 2 范围。
- **全项目升级改造线**：本文件承接战略改造方向，不直接替代 Linux / AI 的执行计划。近期第一刀先做认知和协议边界：统一 observability schema 第一版设计已落到 `docs/design/simulator_evolution_observability_schema_design.md`；AI 设备契约定调、bounded-dynamic shape 文档化、frontend-simulator protocol 版本化，以及 JIT/DBT dry-run 去留决策仍按后续独立切片推进。

`2026-05-29` 全仓库 code review remediation 已完成并归档，四条 `code-reself-*` 整改线已合入本地主线，12 条 `必须修复` 与 19 条 `建议修改` active findings 已关闭；后续只保留 `code_reself_status.md` 中的长期观察项。

---

## 第一部分：现有项目的升级与重构

项目当前最大风险不是缺功能，而是 "reference-first + observability + 协设计" 这套独特气质开始被层叠的 wave 历史、不统一的 schema、未决断的子项目稀释。先做一轮概念层重构，再让创新方向从被收紧的内核长出来。

### 1.1 概念层（影响全局，必须先决断）

**1. `kernel_alpha` 的定位已定：课程 OS 主线入口 + Phase 1 历史基线**
当 Alpine/Debian 真实 Linux 已经能跑到 shell 后，`kernel_alpha` 不再承担“证明平台能启动 Linux-like runtime”的主线职责，也不应继续在“trap / device 测试床”和“完整教学 OS”之间摇摆。

当前口径已经定为：`kernel_alpha` 接管《操作系统课程设计》A 方案主线。第一阶段已完成课程基本要求的 3 个模块、9 个功能点：

- 进程模块：FCFS、RR、CFS-lite。
- 内存模块：Demand Paging、Clock 页面置换、`kmalloc` / `kfree`。
- 文件系统模块：文件 / 目录 CRUD、`seek`、B 树目录索引。

第二阶段也已完成 A 方案核心闭环和创新线：syscall ABI、真实进程生命周期、FD / FS 统一 I/O、5 个课程用户程序、shell、单级管道、重定向、COW Fork、用户态崩溃隔离，以及 `/proc/syscalls`、`/proc/cow`、`/proc/crashlog` 可观测证据。正向 `kernel_alpha_demo` 当前固定为 `KMVPET|course-os-stage1 ...|course-os-stage2 ...` summary；旧 Phase 1 `KMVPETDS` 输出降级为 bring-up 历史基线，storage / PLIC / timer / fault 的 9 条负向 demo 继续保留为基础设施回归，但不再定义当前课程 OS 行为承诺。后续重点不是继续扩大 Stage 2 范围，而是保持 Stage 2 marker、unit targets、functional / pipeline guest demo 和旧负向 guardrail 稳定；若继续做课程 OS 后续阶段，必须另起设计和计划。

**2. AI 加速器的设备契约**
现在的接口是为 host smoke 设计的（task-spec 导入、profile 拉取）。如果路线是 "Linux-facing driver"，需要重新设计成 "DT node + ioctl + DMA descriptor + IRQ" 风格的真实设备契约。host smoke 接口可保留为底层 API，上面叠一层"真实设备视角"。越早确定方向越好。

**3. observability 数据 schema 不统一**
`AiAcceleratorProfileSummary`、`ExecutionProfile`、`shadow_cache`、各种 kernel / guest trace 各说各话。统一 observation event schema 第一版已经落到 `docs/design/simulator_evolution_observability_schema_design.md`，当前已完成 debug-probe summary、ExecutionProfile core snapshot、JIT / DBT dispatch summary、memory observation、`shadow_cache`、AI profile 的只读 wrapper，以及 frontend Evidence Drawer 的首个 read-side consumer；后续再把 Lab protocol / event view 逐步收敛到同一协议。**这件事不继续推进，时间旅行 / 因果切片 / Lab 协议化仍会受阻**。

**4. `InstructionSemantics` 的形式**
作为 ISA 真值，长期看 C++ 代码形式不够。需要演化为表驱动 / DSL / 半结构化描述，是 ISA 形式化方向的前置。短期不必做，但需要在 design 里留"未来形态"占位，避免在它上面长出更多耦合。

### 1.2 结构层

**5. Linux 第四阶段冻结点 `timerfd-one-shot-readback-ok`**
以非常窄的 syscall 行为作为整个 Linux 路径的冻结锚点本身就是问题。两个方向都比"半冻不冻"健康：
- 解冻并重新设定明确的下一阶段目标。
- 或声明 "Linux distro platform 已收口在 Alpine/Debian 真实 rootfs"，淡化或移除第四阶段冻结点。

**6. 测试矩阵需要分层**
这项已经完成第一轮落地：`myCPU/Makefile` 当前已有并已文档化
`test-fast-smoke`、`test-standard-regression`、`test-slow-guest` 和
`test-opt-in-external`。`code_reself` 整改收尾也已用这套分层门禁重新验证：
`test-fast-smoke`、`test-standard-regression`、`test-pipeline`、`make test`
和 `frontend && node --test` 均作为默认无资产收尾证据；真实 Linux console e2e、
外部发行版 rootfs、Spike 等仍保留为显式 opt-in。

后续重点不再是"有没有分层入口"，而是执行纪律：每条新计划必须明确使用哪一层门禁，外部资产门禁不能伪装成默认必跑项，慢速 guest 和 opt-in differential 需要有清晰触发条件。

**7. JIT/DBT dry-run 长期 opt-in**
host-smoke-only opt-in 状态如果再持续半年没有进展，会变成僵尸代码。必须做决断：
- 投入推到"可作为可选默认后端，差分守门"。
- 或明确收口为"方法论 demo"，停止扩张接口面。

两个方向都比"长期半成品"健康。

**8. 状态文档的边界**
专项 status 当前包括 `kernel_alpha`、`npu_tpu`、`linux_distribution` 和
`code_reself`。其中 `code_reself` 已完成四条整改线合并、统一验证和计划归档，当前只保留长期观察项；`kernel_alpha` 已进入课程 OS Stage 2 完成态；Linux / AI 仍是活跃产品线。建议设规则：专项 status 每季度自审一次，进入维护态就归档到 `mainline_status.md` 一节，不再独立维护。

### 1.3 工程层（局部但实在）

**9. Pipeline 后端的"诚实标注"**
pipeline 现在是"教学级 OoO"还是"可信微架构估算"？文档没说清。
- 若是前者：design 里明确写"pipeline 后端不作为微架构性能真值"，避免读者误解。
- 若想往后者走：规划参数化重构（见第二部分创新方向 4）。

**10. `bounded-dynamic shape` 协议提升为一等公民**
这是 AI 路线的关键抽象，目前埋在 task-spec 导入器里。建议升级为独立 design 文档，明确边界、扩展点、与未来 op 注册的关系。否则一旦支持更多 op，会被迫做不兼容的格式演进。

**11. Frontend ↔ Simulator 协议版本化**
Lab Workbench 是浏览器前端，靠 Node 调试服务和模拟器对话。这层协议是否版本化？前端发布与模拟器发布是否解耦？没看到明确契约文档。建议提一份 `frontend_simulator_protocol.md`，否则前后端会逐步耦成大泥球。

**12. Showcase 与主线脱钩**
课程结题、PPT、讲稿、HTML 预览页是项目历史基因，但与当前工程主线已不在同一时间维度。建议把 showcase 冻结为 "v1 课程结题归档"，新的对外材料另起一个面向工程 / 研究的展示线，不要让 showcase 持续侵入 status / design 的更新节奏。

### 1.4 交互性与可配置性（"展示化 → 真实工作台"）

`2026-05-31` 全盘审计发现：项目内核（myCPU CLI + 调试协议 + AI 设备 + guest runtime）实际上是高度可编程的，但**浏览器前端是一层只读展示壳**——它把 CLI 的灵活性全部封死。以下是从"展示器"到"真实工作台"需要补齐的能力。

**13. 前端打破硬编码 manifest**
当前 `POST /api/session/load` 只能按 name 在 `tests_manifest.mjs` 的约 25 个预定义条目中查找。用户不能上传或指定自己的 ELF。没有代码编辑器、没有汇编器集成、没有任何方式在浏览器里跑用户自己的程序。

提升方向：
- 最小方案：`POST /api/session/load` 增加 `elfPath`（服务器本地路径）和 `elfBase64`（base64 编码的 ELF）两种新参数，直接绕过 manifest 查找。用户把 RISC-V ELF 放到服务器目录或通过 base64 传入即可在浏览器里跑。
- 进阶方案：嵌入 Monaco/CodeMirror 代码编辑器，支持 RISC-V 汇编语法高亮，配合浏览器内 assembler 或 docker/riscv-gnu-toolchain 一键编译运行。
- 影响面：`frontend/server/debug_server.mjs` 的 `/api/session/load` 路由，`frontend/app/app.js` 的 workload 选择 UI。

**14. 调试协议补写能力**
调试 CLI 当前只读能力强（`snapshot` 完整机器状态、`peek` 读内存、`pagewalk` 遍历页表），但写能力严重缺失：只有 `set_gpr` 能写寄存器，没有 `set_memory`、没有 `set_csr`、没有断点命令。

提升方向：
- `set_memory addr value`：向指定物理/虚拟地址写入值，复用现有 `peek` 的地址解析路径。
- `set_csr csr value`：写 CSR，复用现有 CSR 读写基础设施。
- `break_at addr`：设置 PC 断点，当 `pc == addr` 时暂停执行并返回当前 snapshot。
- 影响面：`myCPU/src/debug/debug_protocol_command.h`（新增命令枚举）、`debug_protocol.cpp`（新增处理分支）、前端 debug_server 协议转发。

**15. course_os_shell 支持加载外部 ELF**
`course-os> ` 是一个真正的交互式 shell（支持管道、重定向、脚本），但 `exec` 命令只能运行 `course_user_programs.c` 里硬编码的 7 个 ELF 字节数组（hello/echo/cat/forktest/crashdemo/crash/badelf）。Stage 7 的外部 rootfs 资产链路已经能从 ext4 提取真实 ELF，但只为 Linux compat 路径服务。

提升方向：
- 让 `course-os> exec /path/to/prog` 从课程 OS 的 rootfs 或内置 FS 中加载真实 RV64 ELF，不再限制为 7 个硬编码程序。
- 或者：让 `/console` 前端支持拖入 ELF 注入到课程 OS 的文件系统，然后 `exec` 执行。
- 影响面：`myCPU/guest/kernel/course_user_programs.c`、`course_shell.c` 的 `exec` 路径。

**16. kernel_alpha 增加交互式观察面**
`kernel_alpha_demo` 跑完 Stage 1→2→3 打印一行 summary marker 就退出。9 条负向 demo 也是预编排的故障注入。和 `interactive_os` 的 monitor（有 `peek`/`regs`/`pagewalk`/`pte` 交互命令）形成鲜明对比——kernel_alpha 的能力（调度器、内存管理、FS、COW、procfs）闲置在一次性 smoke 里。

提升方向：
- 新增 `kernel_alpha_monitor` 入口：启动后不退出，进入精简 monitor，用户可通过 procfs 节点持续观察调度/内存/COW 统计（`cat /proc/schedstat`），而不是只在一轮 smoke 里被动输出。
- 或者：让 kernel_alpha_demo 在输出 summary 后 fall into 一个最小交互 prompt，允许重复查询 procfs 节点。
- 影响面：`myCPU/guest/kernel_alpha/main.c` 入口编排，`procfs.c` 已具备只读能力。

**17. AI 加速器前端自定义**
AI 加速器是一个真正可编程的硬件设备（~40 个 MMIO 寄存器、提交队列、中断闭环），Python SDK（`workloads/ai_proto/pack_graph.py`）可以构建自定义图包。但前端 AI 面板只能从 4 个服务端硬编码模板中选择。用户不能通过浏览器定义新 op、新模型、新 workload。

提升方向：
- 最小方案：新增 `POST /api/ai/custom-graph` 端点，接受 JSON 描述的自定义 op 序列和 shape，服务端调用 `pack_graph.py` 生成图包并运行，返回 profile。
- 进阶方案：前端增加简易 op 编排 UI（JSON editor 或拖拽），让用户在 bounded-dynamic shape 安全壳内定义小模型。
- 影响面：`frontend/server/ai_tiny_model_service.mjs`，`workloads/ai_proto/pack_graph.py`。

**18. Workload 系统降低门槛**
当前新增 workload 需要手动改 makefile、添加 profile 目录、写 manifest 条目。对开发者可行，对普通用户不可行。

提升方向：
- 提供 `workloads/custom/` 模板目录，用户放入 ELF + `profile.toml` 即可被自动发现。
- manifest 改为自动扫描 workload 目录生成，不再手动维护。
- 影响面：`myCPU/workloads/` 目录结构，`frontend/server/tests_manifest.mjs` 生成逻辑。

**19. 机器参数可配置**
RAM 大小通过板级 makefile 固定为 128MiB（`BOARD_RAM_SIZE := 0x08000000`）。MMU 强制 Sv39。无 `--ram-size` CLI 参数，无 CPU 数配置。

提升方向：
- 增加 `--ram-size` CLI 参数覆盖板级默认值。
- 前端增加基本的机器配置面板（RAM 大小、是否启用 MMU、设备勾选）。
- 影响面：`myCPU/src/main.cpp`，`myCPU/src/platform/machine.cpp`，`workloads/boards/`。

### 1.5 重构优先级建议

本节对应的活跃执行计划：
[P1](docs/plan/project_evolution_priority_p1_plan.md) /
[P2](docs/plan/project_evolution_priority_p2_plan.md) /
[P3](docs/plan/project_evolution_priority_p3_plan.md)。P0 已完成并归档到
[history_plan.md#project-evolution-priority-p0-plan](docs/plan/history_plan.md#project-evolution-priority-p0-plan)。

| 优先级 | 项目 | 原因 |
|---|---|---|
| P0（已完成） | kernel_alpha 课程 OS Stage 2 门禁维护（#1） | Stage 2 正向证据面和旧负向 guardrail 已重跑守住 |
| P0（已完成） | observability schema 统一（#3） | 已建立 schema，并完成 producer wrapper 与首个 frontend consumer |
| P1 | AI 加速器设备契约重设计（#2） | 决定 6 个月路线能否落地 |
| P1 | JIT dry-run 决断（#7） | 防止变成僵尸代码 |
| P1 | 测试矩阵分层执行纪律（#6） | 分层入口已落地，后续要防止默认 / slow / opt-in 边界漂移 |
| P2 | Linux 第四阶段冻结点处理（#5） | 文档可读性、路线清晰度 |
| P2 | bounded-dynamic shape 文档化（#10） | AI 路线关键抽象 |
| P2 | Frontend ↔ Simulator 协议（#11） | 长期维护成本 |
| P3 | InstructionSemantics 形式演进占位（#4） | 远期前置 |
| P3 | Pipeline 后端诚实标注（#9） | 文档清晰度 |
| P3 | 状态文档边界规则（#8） | 治理规则 |
| P3 | Showcase 冻结归档（#12） | 治理规则 |
| P0（已完成） | 前端打破硬编码 manifest（#13） | `/api/session/load` 已支持受控 `elfPath` / `elfBase64` 本地 ELF |
| P0（已完成） | 调试协议补写能力（#14） | debug CLI / debug server 已支持 `set_memory`、`set_csr` 和 `break_at` |
| P1 | course_os_shell 支持外部 ELF（#15） | 课程 OS 有了真正的用户程序加载能力，教学价值拉满；复用 Stage 7 已有资产 |
| P1 | kernel_alpha 增加交互式观察面（#16） | 让 procfs / 调度 / COW 统计变成用户可主动查询的实时数据，而不是一次性 summary |
| P1 | AI 加速器前端自定义（#17） | 设备本身已可编程，前端打开自定义图包入口即可释放硬件能力 |
| P2 | Workload 系统降低门槛（#18） | 降低用户添加自定义 workload 的工程成本，从"改 makefile"降到"放文件" |
| P2 | 机器参数可配置（#19） | RAM 大小、MMU、设备选择从硬编码升级为可配置，向 Ripes 级交互靠拢 |

---

## 第二部分：未来发展路线规划

### 2.1 项目真正的"魂"

读完所有文档的判断：项目独特价值不在 ISA 实现、不在 Linux 跑通、不在 AI 加速器本身，而在于一种气质——**"任何被执行的事情都必须留下可解释的 evidence chain"**。reference-first、多后端差分、Evidence Drawer、bounded-dynamic shape 安全壳、单一事实来源——这些看似不相关的决策本质都是同一件事：拒绝"黑盒执行"。

**健康发展的方法论级原则只有一条：把这个气质内化到每一层，而不是被"补功能"拖向通用化**。下面所有方向都从这个内核长出来。

### 2.2 内生创新方向

**1. 时间旅行 / 因果切片，而不是"trace"**
现在的 observability 是单向、事后的。下一步把模拟器做成"因果引擎"：给定任意观察点（某条指令的某个寄存器值、某次 AI op 的输出、某次 DMA 的时序），反向切片出导致它的指令链、设备访问链、AI op 链。reference-first 多后端 + Evidence Drawer 是天然底座，**这件事是项目独占的——通用模拟器永远不会做，因为它们不需要解释自己**。

**2. ISA 语义从"代码"升级为"数据"**
`InstructionSemantics` 现在是 C++ 代码形式的真值。下一步把它结构化、可读、可导出：每条指令的语义可序列化成 spec record，能 round-trip 到 SAIL / Coq / Lean / 自研 DSL。一旦做到，"差分一致性"从工程方法升级为可形式化验证的对象，且这条路没有产品化压力，永远是研究金矿。

**3. ISA × 微架构 × AI workload 三方耦合的 sandbox**
项目同时拥有：可改的 pipeline 微架构、可改的 AI 加速器调度模型、可声明的 bounded-dynamic AI workload、可改的 ISA。把这四样东西交互式连起来——
- "如果给 RV64 加一条 vmac，AI workload 端到端周期变化是多少？"
- "如果 NPU tile scheduler 改成 work-stealing，前端 Linux driver 看到的 latency 怎么变？"

这是真正稀缺的"假设性实验台"。

**4. pipeline 后端从"教学摆设"升级为参数化微架构沙盘**
现在 pipeline 跑 OoO/ROB/LSQ 但参数写死。把 pipeline 深度、issue width、ROB size、LSQ 容量、分支预测策略全部参数化，functional 做 oracle，差分守门。**一份代码就能让用户做微架构 ablation**。这条线和 ISA 数据化是孪生方向。

**5. Lab 协议化，而不只是 Lab 界面化**
Evidence Drawer 是 view，Lab Workbench 是 shell。下一步是协议：定义 `lab.json` 语义——实验声明、观察点、断点、可视化挂载、评分规则——让"实验"成为一等公民数据结构。教师 / 研究者写 lab，项目做 runtime。**把项目从"模拟器"升级为"实验描述语言 + 实验执行平台"**。

**6. observability 协议化（OpenTelemetry-style）**
现在每个模块各自产出 profile，格式不统一。升级为统一 spans / events 协议，模拟器内每个组件 emit 结构化事件，前端只是消费者。**一旦做了，时间旅行 / 因果切片 / Lab 协议化全部受益**。这与第一部分 #3 是同一件事的两面。

**7. 多 guest 横向对比作为研究 / 教学产出**
kernel_alpha vs xv6 vs Alpine vs Debian 跑同一个 workload，对比 syscall trace、TLB 行为、IO 模式、调度路径。**这是项目天生具备但没被利用的资产**，做成"对比 lab"是独有产出。

**8. AI 加速器从"设备"演化为"协设计实验对象"**
现在 AI 加速器是 MMIO 设备。下一步：让 op 集合可注册、调度策略可插拔、内存模型可参数化、DMA 行为可参数化。它不只是被调用的设备，而是"用户可二次开发的 AI 微架构原型"。bounded-dynamic shape 是这条路的安全壳，**要做强不要削弱**。

### 2.3 方法论级演化（最容易被忽略，但回报最高）

- **单一事实来源原则向代码层渗透**：文档已做到。代码里也要做——指令语义、设备 MMIO 寄存器布局、AI op spec、Linux ABI 假设，每类都要有唯一来源，禁止"两份基本一致的定义"。
- **opt-in 机制要有退场策略**：JIT dry-run、Spike 差分、shadow_cache 都是 opt-in。opt-in 不是终态，长期 opt-in 会僵化。每个 opt-in 都要有"什么时候推到默认 / 什么时候废弃"的判断。
- **Wave 编号制要冻结**：Wave 1-7 是历史路径，Post-Wave 7 已有"补丁包"味道。建议冻结 wave 编号到 7，之后用功能线（Linux Distro / AI Co-Sim / Lab Platform / ISA Formalization）组织，避免无限累积。

### 2.4 时间维度路线

#### 短期（约 3 个月）：完成认知重构 + 补完基本面

**认知重构（来自第一部分）：**
- 守住 `kernel_alpha` 课程 OS Stage 2 正向证据面和旧负向 guardrail
- 按 `docs/design/simulator_evolution_observability_schema_design.md` 推进首批 observability schema 迁移候选
- AI 加速器设备契约方向定调
- JIT dry-run 决断
- 固化测试矩阵分层执行纪律

**基本面工程：**
- F/D 浮点 ISA 收口（FMA 四件套、fsgnj、fmin/fmax、fclass、fcvt 舍入、fcsr 异常 flag、NaN box），通过 riscv-tests `rv64uf` / `rv64ud` 全集
- AI 加速器从 timed-simple 推进到 tile scheduler + DMA / compute overlap + multi-outstanding queue
- 前端 Lab Workbench 完成 Linux Distro / AI / Pipeline 三个 family 的 Evidence Drawer 标准化

#### 中期（约 6 个月）：把内生创新跑到可演示

- **AI 协处理器路线打到 Linux-facing driver**：guest Linux 通过 `/dev/aiac` 或类 ioctl 接口下发 task-spec，回读 profile
- **第二个标杆 workload**：tiny LM inference（GEMM + softmax + KV cache 写回）端到端跑在协处理器上，产出周期级 timing trace
- **pipeline 参数化**：先做 issue width / ROB size 两个参数，差分守门
- **observability 统一协议落地**：至少 AI / pipeline / shadow_cache 三个模块改成统一 schema
- **Lab 协议 v0**：定义最小 `lab.json`，至少跑通一个对外 demo lab
- 发布一篇技术文档 / 论文：主题 "Reference-first 多后端 + AI 协处理器协同仿真 + 可解释 evidence chain"

#### 远期（约 12 个月）：站住"内生定位"

- **时间旅行 / 因果切片 v0**：基于统一 observability 协议，做一个最小可用的反向切片工具
- **ISA 语义结构化**：完成 `InstructionSemantics` 表驱动 / DSL 化第一版，实现 round-trip 导出
- **AI 加速器开放二次开发**：op 注册 + 调度策略插件 + 用户 task-spec（在 bounded shape 安全壳内）
- **多核（最小 dual-core）+ SC 内存模型最小可信版本**：不追通用 SMP，只作为可观察平台
- **Lab 协议成熟**：教师 / 课程作者可写 `lab.json` 描述实验、断点、可观察量、评分点；至少与一所高校或一门课程对接落地一节
- **JIT/DBT 决断后果落地**：要么变成可选默认后端（host-smoke 守门，measurable 加速 ≥ 5x functional），要么彻底归档
- 文档 / Showcase 双语化（中 + 英），开放给国际 RISC-V / 模拟器 / 教学社区评审

### 2.5 优先级判断

按"对项目内生气质的强化程度 × 工程可行性"排序，最值得押注：

1. **observability 协议化**：所有创新方向的前置，不做就堵死下游
2. **AI 协处理器协同仿真完整故事**：现成的稀缺资产，推一步就能讲完
3. **Lab 协议化**：把"看一个模拟器内部"变成"可声明、可复现、可评分的实验"，独占空间最大
4. **pipeline 参数化**：与 ISA 数据化是孪生方向，工程量适中、长期回报高
5. **ISA 形式化前置工作**：远期金矿，现在留好接口

### 2.6 不建议的方向

- 追"通用模拟器"广度（设备矩阵、网络栈、图形栈、桌面 guest）
- 默认替换 reference 路径为 JIT/DBT
- 任意用户 ELF / 镜像 / AI 模型上传（破坏 bounded shape 安全壳和可观察性）
- 追大规模 SMP 通用一致性
- wall-clock 性能对标
- 任何会让"evidence chain 可解释性"让位的工程取舍

---

## 总结

**第一部分的核心动作**：先守住已经落地的 `kernel_alpha` 课程 OS 证据面和基础设施 guardrail，再做三件认知重构（AI 设备契约、observability schema、ISA 真值形式占位），再做四件结构层决断 / 固化（Linux 冻结点、测试分层执行纪律、JIT dry-run 出路、状态文档治理），再做四件工程清理（pipeline 标注、bounded-dynamic 文档化、frontend 协议、showcase 归档），再做七件交互性与可配置性提升（打破硬编码 manifest、调试协议写能力、course_os_shell 外部 ELF、kernel_alpha 交互式观察面、AI 前端自定义、workload 门槛降低、机器参数可配置）。其中 #13（前端任意 ELF 加载）和 #14（调试写能力+断点）是 P0，应最先落地——这两个改动让项目从"展示器"变成用户可以真正交互的"工作台"。

**第二部分的核心方向**：从"reference-first + observability + 协设计"这个内核长出八条创新方向，其中 observability 协议化、AI 协处理器协同仿真、Lab 协议化、pipeline 参数化、ISA 形式化是最值得押注的五条。

**两部分关系**：第一部分是"清地基"，第二部分是"盖楼"。地基不清就盖楼，楼会歪；地基清了不盖楼，地基会荒。两部分必须同步推进。
