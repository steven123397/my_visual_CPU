# 项目方向评审（2026-07-26）：发展建议与 Rust 重写评估

## 文档定位

- 本文档是 2026-07-26 一次外部视角的项目评审快照，性质是建议输入，不是计划或状态文档。
- 它不替代 [PROJECT_EVOLUTION_PLAN.md](PROJECT_EVOLUTION_PLAN.md)、`docs/design`、`docs/plan` 或 `docs/status` 中的任何单一事实来源；其中任何被采纳的条目，落点都应是更新对应正式文档，而不是持续维护本文档。
- 评审输入：仓库代码结构、`AGENTS.md` 体系、[docs/status/mainline_status.md](docs/status/mainline_status.md)、[PROJECT_EVOLUTION_PLAN.md](PROJECT_EVOLUTION_PLAN.md) 与 git 历史。

## 结论

1. **不建议把模拟器重写成 Rust。** 项目的核心资产都不在语言层，重写会把它们从"已证明"变回"待重证"。
2. **发展方向上 `PROJECT_EVOLUTION_PLAN.md` 的判断基本正确，当前真正的约束不是方向而是承载力。** 建议：收敛押注、治理维护面、给下一阶段设一个"结题级"外部锚点。

## 评审时点的项目画像

- 模拟器主体约 2.9 万行 C++17（`myCPU/src` + `include`），guest 侧约 2.9 万行 C（Course OS / kernel_alpha / xv6 harness），测试约 6.7 万行，前端约 1.6 万行 Node/JS。
- 架构：共享 `InstructionSemantics` + functional backend 为 ISA 真值来源；`pipeline` 后端具备 rename / ROB / LSQ / 最小 OoO；JIT/DBT 已按 P1 决断归档为 method-demo / opt-in research asset。
- 系统面：M / S / U、Sv39、TLB、UART / CLINT / PLIC、`virtio-blk`、MMIO AI accelerator；Course OS 展示主线已收口，OSComp basic external validation 已接入。
- Post-Wave 7 两条主线：标准 Linux 发行版平台（Alpine / Debian curated smoke 已到长线阶段 4）；用户 AI 任务 / NPU 性能模型。
- 验证体系：`test-fast-smoke` → `test-standard-regression` → `test-slow-guest` → `test-opt-in-external` 四层门禁 + Spike 差分 smoke；仓库当前没有 CI（无 `.github/`）。

## Rust 重写评估

### 结论：不重写

### 理由 1：核心资产与语言无关

项目真正的资产有四样：

1. 已被约 6.7 万行测试、Spike 差分和 xv6 / Linux / Course OS 实跑验证过的指令语义真值（`InstructionSemantics` + functional backend）。
2. 分层回归矩阵与几十个窄合同门禁。
3. observability 契约（debug snapshot schema、probe 输出、前端消费者）。
4. design / plan / status 文档流程体系。

重写成 Rust 的那一刻，前两样从"已证明"退回"待重证"：[docs/plan/history_plan.md](docs/plan/history_plan.md) 里的几十个 hardening 切片（CSR 边角、trap delegation、Sv39 page walk、virtio 生命周期）都要重走一遍。经典重写陷阱正是：80% 对齐很快，最后 20% 的长尾就是当初花了几个月才收口的那些边界。

### 理由 2：Rust 的两个招牌收益在本项目都不成立

- **内存安全**：模拟器单线程、输入面受控（ELF loader 有专门单元门禁，公网边界在 Node 层）。C++ UB 的典型后果是"模拟结果错"，而这正是 reference-first 差分体系专门兜住的失效模式。
- **并发安全**：multicore / SMP 明确未启动，且 `PROJECT_EVOLUTION_PLAN.md` 2.6 节已排除大规模 SMP；JIT 已归档。最需要 Rust 的两条线恰好是项目已经决定不走的两条线。

另外 `AGENTS.md` 自身规则"不做没有结构收益的纯 cosmetic 重写或纯语言迁移"已经覆盖本问题：今天的整仓 Rust 重写就是这一条的教科书案例。

### 如果对 Rust 的兴趣是真实的：先做 ISA 数据化

更好的路径已在规划内（`PROJECT_EVOLUTION_PLAN.md` 2.2 节第 2 条"ISA 语义从代码升级为数据"）：

- 指令语义表驱动 / DSL 化并可导出后，任何语言的执行引擎都可以从 spec 生成，再用现成差分门禁对着 C++ reference 验证。
- 语言从"重写决策"降级为"后端细节"；届时一个 Rust 解释器是周末级实验而不是数月迁移，还顺带产出形式化验证接口。
- 结论：**先做 ISA 数据化，Rust 问题会自行消解。**

若在此之前就想做 Rust 实验，应采用窄合同 opt-in 形态（例如 RV64I 子集第二解释器接入差分 harness），并按 2.3 节"opt-in 退场策略"预先定义毕业或废弃条件。

## 发展建议

### 1. 收敛押注：五条选两条

`PROJECT_EVOLUTION_PLAN.md` 2.5 节的五大方向对单人维护太宽。建议排序：

1. **observability 协议化** — 唯一的前置依赖（时间旅行、Lab 协议、因果切片全靠它），先做。
2. **AI 协处理器打通 Linux-facing driver 端到端**（guest 用户态 → `/dev` 节点 → MMIO / DMA / 中断 → profile 回读）— 最稀缺的展示资产。"完整栈协同仿真"这个故事纯性能模型和纯模拟器都讲不了，P1 的 host-facade 合同已经把地基打好。

pipeline 参数化适合作为下一个课程季项目（模块边界清晰、差分守门现成）；Lab 协议化等 observability 协议落地后再开；ISA 形式化以"数据化第一版"为切入点。

### 2. 治理维护面（当前最大隐性风险）

- 每个"第一刀"都在给回归矩阵永久加门禁：仅已归档的 DBT 研究资产就有约 20 个 `dbt_*` 模块和配套 host smoke 要永远编译、永远跑。
- 2.3 节"opt-in 要有退场策略"尚无执行动作，建议真的执行一轮：已归档研究资产收进独立 build target（如 `OPT_IN_RESEARCH=1`），给默认 `make test` 时长设预算并跟踪。
- [docs/status/mainline_status.md](docs/status/mainline_status.md) 已约 1100 行，同一份 DBT 切片清单重复出现三次以上；作为"唯一事实来源"已难以人读，值得压缩（已完成项只留一行指向 `history_plan.md`）。

### 3. 明确 Linux 发行版线的封顶

- Post-Wave 7 Line A"像 QEMU 那样跑通标准发行版"的表述与 2.6 节"不追通用模拟器广度"存在张力：发行版真要"可用"必然滑向网络栈 → 包管理 → SMP → 性能 → 重启 JIT 的 QEMU 跑步机；解释器 + JIT 已归档的现状也决定了这条线性能天花板不高。
- 建议显式定格为 **curated evidence matrix**（当前事实走法），并把"多 guest 横向对比"（2.2 节第 7 条：kernel_alpha vs xv6 vs Alpine vs Debian 同 workload 对比）作为这条线的产出形态——这是 QEMU 不会做、本项目独占的东西。

### 4. 补两个基础设施空白

- **CI**：加最小 GitHub Actions 跑 `make test-fast-smoke` + `cd frontend && node --test` 保护 `main`。测试矩阵是本项目的护城河，值得机器守门。
- **riscv-tests `rv64uf` / `rv64ud` 全集收口**值得提前——它与 Spike 差分一起构成对外可引用的正确性凭据。

### 5. 给下一阶段找一个外部锚点

从提交历史看，项目在"结题 / 展示 deadline"驱动下收口质量最高（计组结题、OS 课程、OSComp basic validation 均如此）。课程周期已收割完，建议主动选定下一个：

- OSComp 正式参赛；或
- 把中期规划里那篇"reference-first 多后端 + 协同仿真"技术报告写出并对外发布；或
- 找一门课程试点 `lab.json`。

方向规划不缺，缺的是让 wave 收口的外部时钟。

## 采纳方式

按仓库单一事实来源规则：任何被采纳的条目，落点是更新 `PROJECT_EVOLUTION_PLAN.md` / 对应 design / plan / status 文档；本文档保持评审快照原样，不随执行进度更新。
