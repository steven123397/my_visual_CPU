# my_visual_CPU 演进规划

本文件整合两部分内容：

- **第一部分**：现有项目的升级与重构建议（先做认知重构，再做代码重构）。
- **第二部分**：未来发展路线规划（不对标商用产品，从项目内核长出来的发展方向）。

定位前提：项目当前已是可运行的 RISC-V 系统模拟器原型，具备 RV64IMAFDC、M/S/U + Sv39、virtio-blk、Alpine/Debian 真实 rootfs、MIO NPU/TPU-like 加速器、浏览器 Lab Workbench、reference-first 多后端 + Spike 差分能力。本文件不再重复这些既成事实，只回答"接下来怎么走"。

---

## 第一部分：现有项目的升级与重构

项目当前最大风险不是缺功能，而是 "reference-first + observability + 协设计" 这套独特气质开始被层叠的 wave 历史、不统一的 schema、未决断的子项目稀释。先做一轮概念层重构，再让创新方向从被收紧的内核长出来。

### 1.1 概念层（影响全局，必须先决断）

**1. `kernel_alpha` 的定位已定：课程 OS 主线入口 + Phase 1 历史基线**
当 Alpine/Debian 真实 Linux 已经能跑到 shell 后，`kernel_alpha` 不再承担“证明平台能启动 Linux-like runtime”的主线职责，也不应继续在“trap / device 测试床”和“完整教学 OS”之间摇摆。

当前口径已经定为：`kernel_alpha` 接管《操作系统课程设计》第一阶段主线，目标限定为课程基本要求的 3 个模块、9 个功能点：

- 进程模块：FCFS、RR、CFS-lite。
- 内存模块：Demand Paging、Clock 页面置换、`kmalloc` / `kfree`。
- 文件系统模块：文件 / 目录 CRUD、`seek`、B 树目录索引。

旧 Phase 1 `KMVPETDS` 输出降级为 bring-up 历史基线；storage / PLIC / timer / fault 的负向 demo 继续保留为基础设施回归，但不再定义当前课程 OS 行为承诺。后续重点不是再争论是否保留 `kernel_alpha`，而是同步 `kernel_alpha_status.md` 口径、解冻 `kernel_alpha_demo` 行为门禁，并围绕课程 OS 第一阶段写出可执行计划。

**2. AI 加速器的设备契约**
现在的接口是为 host smoke 设计的（task-spec 导入、profile 拉取）。如果路线是 "Linux-facing driver"，需要重新设计成 "DT node + ioctl + DMA descriptor + IRQ" 风格的真实设备契约。host smoke 接口可保留为底层 API，上面叠一层"真实设备视角"。越早确定方向越好。

**3. observability 数据 schema 不统一**
`AiAceleratorProfileSummary`、`ExecutionProfile`、`shadow_cache`、各种 kernel / guest trace 各说各话。建议提一份独立 design 文档定义 observation event schema，再把各模块的 profile 收敛到同一协议。**这件事不做，时间旅行 / 因果切片 / Lab 协议化全部受阻**。

**4. `InstructionSemantics` 的形式**
作为 ISA 真值，长期看 C++ 代码形式不够。需要演化为表驱动 / DSL / 半结构化描述，是 ISA 形式化方向的前置。短期不必做，但需要在 design 里留"未来形态"占位，避免在它上面长出更多耦合。

### 1.2 结构层

**5. Linux 第四阶段冻结点 `timerfd-one-shot-readback-ok`**
以非常窄的 syscall 行为作为整个 Linux 路径的冻结锚点本身就是问题。两个方向都比"半冻不冻"健康：
- 解冻并重新设定明确的下一阶段目标。
- 或声明 "Linux distro platform 已收口在 Alpine/Debian 真实 rootfs"，淡化或移除第四阶段冻结点。

**6. 测试矩阵需要分层**
AGENTS.md 列出的 test-unit / test-guest 已接近 30 个，缺乏分层。建议显式分为：
- fast smoke（每次 commit）
- standard regression（PR 默认）
- slow guest（夜间）
- opt-in differential（Spike 差分、JIT 差分）

现在所有测试"看起来差不多重要"，长期会侵蚀开发节奏。

**7. JIT/DBT dry-run 长期 opt-in**
host-smoke-only opt-in 状态如果再持续半年没有进展，会变成僵尸代码。必须做决断：
- 投入推到"可作为可选默认后端，差分守门"。
- 或明确收口为"方法论 demo"，停止扩张接口面。

两个方向都比"长期半成品"健康。

**8. 状态文档的边界**
专项 status 已经四份（kernel_alpha / npu_tpu / linux_distribution / code_reself），每多一份都在挑战"单一事实来源"。建议设规则：专项 status 每季度自审一次，进入维护态就归档到 `mainline_status.md` 一节，不再独立维护。

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

### 1.4 重构优先级建议

| 优先级 | 项目 | 原因 |
|---|---|---|
| P0 | kernel_alpha 课程 OS 口径同步与计划化（#1） | 决策已落定，需同步 status、测试门禁和第一阶段执行计划 |
| P0 | observability schema 统一（#3） | 所有"内核创新方向"的前置 |
| P1 | AI 加速器设备契约重设计（#2） | 决定 6 个月路线能否落地 |
| P1 | JIT dry-run 决断（#7） | 防止变成僵尸代码 |
| P1 | 测试矩阵分层（#6） | 直接影响开发节奏 |
| P2 | Linux 第四阶段冻结点处理（#5） | 文档可读性、路线清晰度 |
| P2 | bounded-dynamic shape 文档化（#10） | AI 路线关键抽象 |
| P2 | Frontend ↔ Simulator 协议（#11） | 长期维护成本 |
| P3 | InstructionSemantics 形式演进占位（#4） | 远期前置 |
| P3 | Pipeline 后端诚实标注（#9） | 文档清晰度 |
| P3 | 状态文档边界规则（#8） | 治理规则 |
| P3 | Showcase 冻结归档（#12） | 治理规则 |

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
现在 AI 加速器是 MIO 设备。下一步：让 op 集合可注册、调度策略可插拔、内存模型可参数化、DMA 行为可参数化。它不只是被调用的设备，而是"用户可二次开发的 AI 微架构原型"。bounded-dynamic shape 是这条路的安全壳，**要做强不要削弱**。

### 2.3 方法论级演化（最容易被忽略，但回报最高）

- **单一事实来源原则向代码层渗透**：文档已做到。代码里也要做——指令语义、设备 MIO 寄存器布局、AI op spec、Linux ABI 假设，每类都要有唯一来源，禁止"两份基本一致的定义"。
- **opt-in 机制要有退场策略**：JIT dry-run、Spike 差分、shadow_cache 都是 opt-in。opt-in 不是终态，长期 opt-in 会僵化。每个 opt-in 都要有"什么时候推到默认 / 什么时候废弃"的判断。
- **Wave 编号制要冻结**：Wave 1-7 是历史路径，Post-Wave 7 已有"补丁包"味道。建议冻结 wave 编号到 7，之后用功能线（Linux Distro / AI Co-Sim / Lab Platform / ISA Formalization）组织，避免无限累积。

### 2.4 时间维度路线

#### 短期（约 3 个月）：完成认知重构 + 补完基本面

**认知重构（来自第一部分）：**
- 同步 `kernel_alpha` 课程 OS 口径，并冻结第一阶段执行计划
- 提出统一 observability schema 的 design 文档
- AI 加速器设备契约方向定调
- JIT dry-run 决断
- 测试矩阵分层

**基本面工程：**
- F/D 浮点 ISA 收口（FMA 四件套、fsgnj、fmin/fmax、fclass、fcvt 舍入、fcsr 异常 flag、NaN box），通过 riscv-tests `rv64uf` / `rv64ud` 全集
- AI 加速器从 timed-simple 推进到 tile scheduler + DMA / compute overlap + multi-outstanding que
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

**第一部分的核心动作**：先把已经落定的 `kernel_alpha` 课程 OS 基线同步到 status / 计划 / 测试门禁，再做三件认知重构（AI 设备契约、observability schema、ISA 真值形式占位），再做四件结构层决断（Linux 冻结点、测试分层、JIT dry-run 出路、状态文档治理），再做四件工程清理（pipeline 标注、bounded-dynamic 文档化、frontend 协议、showcase 归档）。

**第二部分的核心方向**：从"reference-first + observability + 协设计"这个内核长出八条创新方向，其中 observability 协议化、AI 协处理器协同仿真、Lab 协议化、pipeline 参数化、ISA 形式化是最值得押注的五条。

**两部分关系**：第一部分是"清地基"，第二部分是"盖楼"。地基不清就盖楼，楼会歪；地基清了不盖楼，地基会荒。两部分必须同步推进。
