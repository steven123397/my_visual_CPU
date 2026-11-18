# myCPU v2 现代化重构蓝图与确认意图（Intent Statement）

> **当前状态**：备考初试冻结期（2026-11-18 至 **2026-12-20**）。
> **恢复入口**：2026-12-20 初试结束后，新会话可直接以本文件作为顶层意图输入，沿用 `spec-driven-development` → `planning-and-task-breakdown` 启动全仓重构，目标在 **2027 年 3 月末研究生复试前** 高质量收敛交付。

---

## 1. 已确认的最终意图陈述（Confirmed Intent Statement）

- **Outcome**：将 `myCPU` 彻底重塑为工业级代码质量与清晰分层架构的「基于声明式 ISA-DSL 代码生成、统一语义驱动三后端（解释器 / 真乱序流水线 / 真 x86-64 DBT JIT）与零污染系统可观测验证的 RISC-V 编译与系统平台」。
- **User**：即将参加 2027 年 3 月末研究生复试的开发者本人（面向「低代码编程与代码自动生成技术」「编程语言与编译工具链」「分布式/软件系统工程」三个方向的复试导师深度审阅与源码级追问）。
- **Why now**：项目已脱离本科多阶段跨课程设计展示阶段，需在 2027 年 3 月末复试前彻底消除“功能繁杂堆砌”“假乱序/半成品 JIT”以及“既有代码库架构与实现质量不足”的隐患，打造经得起源码级审视的硬核代表作。
- **Success**：
  1. 砍掉所有偏离主线的课设支线（约 45% 代码），采用现代 CMake 与高内聚低耦合的现代 C++ 架构对全代码库（含 ISA、内存、设备、调试协议与前端）进行无禁区深度重构；
  2. 实现声明式 ISA-DSL 代码生成管线，自动生成解码器、反汇编器、语义接口、DBT IR 降级规则与差分测试；
  3. 补齐 `IssueQueue`（Wakeup / Select）实现真乱序发射后端，补齐 DBT 访存 / 控制流 / 块链接实现真正可独立运行系统负载的 x86-64 JIT 后端；
  4. 全量通过单元测试、Spike 真实差分测试与 `xv6` 全系统零污染可观测性验证。
- **Constraint**：
  - 必须在 2027 年 3 月末复试前完成；
  - **全仓无禁区重构**：哪怕当前相对完备、正确的部分也在重构范围内，不迁就任何既有低质量代码或半成品设计，所有保留模块必须真刀真枪、架构自洽、可直接应对源码级追问；
  - 严格守住“单一 ISA 真值（`InstructionSemantics + functional`）”与“零污染系统观测契约”。
- **Out of scope**：
  - 彻底移除并不再维护自定义 MMIO AI 加速器（`myCPU/src/devices/ai_*`、`tensor_golden_ops.*` 及前端 `frontend/server/ai_tiny_model_service.mjs`）；
  - 彻底移除 `V-lite` 向量扩展（`vector_state.*`、`vector_ops.*` 及前端向量面板）；
  - 彻底移除 `myCPU/guest/` 下冗余的阶段性教学 demo 内核（`kernel_alpha_*`、`supervisor_demo`、`vector_demo`、`vector_cnn_demo`、`ai_accel_demo` 等），故障测试用例统一收拢进 `tests/`，系统负载收敛为 `xv6` 与精简测试集；
  - 彻底移除前端与上述支线绑定的课设展示面板。

---

## 2. 研究生复试核心叙事与问答锚点

当面试官追问“这与普通本科体系结构课设有什么区别 / 有什么创新点 / 能证明什么能力”时，用“解决两大系统工程矛盾”替代“罗列指令与外设数量”：

### 2.1 解决的两大核心工程矛盾（创新点）

1. **多执行后端的“语义分裂（Semantic Divergence）”矛盾**
   - **痛点**：传统模拟器中，功能解释器、周期级乱序微架构（OoO）与动态二进制翻译（DBT JIT）往往各写一套指令执行逻辑，极易产生隐蔽的语义不一致。
   - **解法**：建立**无副作用的声明式指令语义契约（`InstructionSemantics` + `InsnEffects`）与 ISA-DSL 代码生成器**。从单一语义源出发，自动生成解码/反汇编框架与映射桩代码，并同时驱动 **Functional（参考真值）**、**Pipeline（真乱序微架构）** 和 **DBT（自定义 IR → 优化 → x86-64 JIT 机器码生成）** 三个执行后端。
2. **操作系统内核调试中的“观测者污染（Heisenbug）”矛盾**
   - **痛点**：调试 `xv6` 页表切换、`fork()` 写时复制或异常路径时，若调试探针复用 CPU 自身的地址翻译路径，会意外触发 TLB 填充或修改页表项 A/D 位，导致“一开调试器 Bug 就消失或变样”。
   - **解法**：实现**架构状态演进与观测探针严格解耦的零污染取证机制**（离线纯函数式 Sv39 页表解释 + 有界指令访存事务捕获 `memory_capture`），配合真实 Spike 差分验证，实现对 OS 内核行为的确定性取证。

### 2.2 面向三个复试志愿方向的能力证明映射

| 复试志愿方向 | 重点展开的架构切片 | 证明的核心能力 |
|---|---|---|
| **低代码编程与代码自动生成技术** | `ISA-DSL` 声明式描述语言与代码生成管线 | 元模型与领域特定语言（DSL）设计能力；用编译器手段自动生成高度结构化 C++ 解码器、语义框架、IR 转换规则与差分测试向量，替代人工堆砌 |
| **编程语言与编译工具链** | `dbt_*` 动态编译器后端与 `isa/` 语义建模 | 形式化 ISA 语义与中间表示（IR）抽象、IR Lowering 与优化 Pass、x86-64 机器码发射（Emitter）、访存 Helper 桥接、Basic Block 链接与 `SFENCE.VMA` / SMC 缓存失效管理 |
| **分布式 / 软件系统工程** | 全仓 C++/CMake 分层架构、零污染观测协议与差分验证闭环 | 高内聚低耦合的系统软件架构能力、契约式可观测性设计、跨组件（Spike 差分 + `xv6` 全系统）自动化测试与 CI 质量门禁 |

---

## 3. 现有代码缺陷核查记录（重构必改清单）

在 2026-11-18 深度访谈与源码核查中，明确识别出以下绝不能带入复试现场的“糊弄点 / 工程债务”：

1. **`pipeline` 后端假乱序问题**（必补真乱序）
   - **源码事实**：当前 `PipelineCoreState`（[pipeline_core_state.h](../../myCPU/src/exec/pipeline_core_state.h)）仅拥有单槽位级间寄存器（`if_id`、`id_ex`、`ex_mem`、`mem_wb`），不存在保留站或发射队列；且 `PipelineBackend::step_id()`（[pipeline_backend_frontend.cpp](../../myCPU/src/exec/pipeline_backend_frontend.cpp)）在 `!sources_ready(decoded_slot)` 时直接原地 Stall，阻止寄存器重命名与 ROB 分配。现有 ROB/LSQ 仅起到“顺序发射 + 投机分支/异常回滚 + Store-to-Load 转发”作用，后续独立指令无法越过前序阻塞指令乱序执行。
   - **重构要求**：引入真正的 **`IssueQueue`（保留站）与唤醒/选择（Wakeup & Select）逻辑**、支持多发射/多执行通道，让“源操作数未就绪的指令照常完成重命名并入 ROB 与 IssueQueue，后续独立指令可真正超前乱序执行”。
2. **`dbt` 后端 Dry-Run 壳子问题**（必做真 JIT）
   - **源码事实**：当前 `DbtJitEngineDryRun`（[dbt_jit_engine.h](../../myCPU/src/exec/dbt_jit_engine.h)）与 `DbtIrOpcode`（[dbt_ir.h](../../myCPU/src/exec/dbt_ir.h)）仅能对连续整数寄存器算术指令进行 Dry-Run 翻译，遇到 Load/Store、控制流分支、CSR 等全部 Reject 并退回参考执行，未作为独立 `ExecutionBackend` 接入运行主线。
   - **重构要求**：重构并做实 DBT IR、Register Allocation / Lowering、x86-64 Emitter、Guest 访存 Helper 桥接、块内/跨块分支控制流（Block Chaining）与页表/TLB/`SFENCE.VMA` 失效处理，使其成为可独立跑通系统负载的正式 `ExecutionBackend`。
3. **全仓工程架构与构建系统老化问题**（无禁区重构）
   - **源码事实**：单文件 `myCPU/Makefile` 膨胀至 1,757 行；早期 C 遗留模块（`myCPU/src/decode.c`、`myCPU/src/memory.c`）与后期 C++17 模块混杂；前端 `frontend/app/render.js`（1,435 行）偏向直接拼接大量状态展示卡片。
   - **重构要求**：全面迁移至模块化 **CMake**；将全仓源码按清晰领域分层彻底重整为现代 C++（建议 C++20/17 统一规范与清晰命名空间边界）；前端同步精简重构为聚焦“源码/汇编/IR 对照 + 页表/访存时间线 + 流水线真实状态”的高质量调试工作台。

---

## 4. 2026-12-20 后恢复开发的分阶段推进路线

| 阶段 | 核心目标 | 关键交付与验证门禁 |
|---|---|---|
| **Phase 0（契约立项）** | 基于本文件运行 `spec-driven-development` 与 `planning-and-task-breakdown` | 产出新架构目录规范、CMake 工程规范、ISA-DSL 语法与三后端接口 Spec |
| **Phase 1（大减法与现代工程底座）** | 彻底剥离 AI 加速器、V-lite、冗余 demo 内核及绑定面板；废除旧 Makefile 迁移 CMake；全仓模块接口与代码质量重构 | CMake 构建绿色；精简后的系统测试集、Spike 差分与 `xv6` 回归在 `functional` 后端全绿 |
| **Phase 2（ISA-DSL 与代码自动生成）** | 设计声明式 RISC-V ISA-DSL，编写代码生成器自动生成解码器、反汇编器、语义桩与 IR Lowering 规则 | 生成代码完全替代手写 `decode.c` 等重复逻辑；Spike 差分全绿 |
| **Phase 3（真 DBT JIT 与真乱序 Pipeline）** | 1) 将 DBT 升级为可独立跑程序/系统负载的真 x86-64 JIT `ExecutionBackend`；<br>2) 为 Pipeline 补全 `IssueQueue` Wakeup/Select 实现真乱序发射与调度 | DBT 后端与真乱序 Pipeline 后端分别通过核心测试、Spike 差分及 `xv6` 验证；提供可量化的乱序超前调度与 JIT 加速实测证据 |
| **Phase 4（可观测调试台与复试封板）** | 重构精简前端调试工作台（汇编/IR 对照、页表树与访存时间线、真实流水线状态）；全仓文档、Wiki 与演示资产对齐 | 全量 unit / host / frontend / Spike 差分 / `xv6` 回归通过；产出面向三方向导师的架构导览 |
