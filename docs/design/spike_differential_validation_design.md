# Spike 外部差分验证设计

## 文档定位

本文档只说明 Spike 外部差分验证这条子线的长期有效设计边界：

- 为什么当前只做离线 `myCPU vs Spike` final-state differential
- 当前实现的结构拆分、输入输出契约和用户入口
- 哪些限制是 V1 的有意识收窄

本文档不承担实时进度更新。当前是否已经落地、跑通哪些门禁，统一以 [../status/mainline_status.md](../status/mainline_status.md) 为准。

## 关联文档

- 状态文档：
  - [../status/mainline_status.md](../status/mainline_status.md)
- 已完成计划归档：
  - [../plan/history_plan.md#spike-external-differential-validation-plan](../plan/history_plan.md#spike-external-differential-validation-plan)
- 用户入口：
  - [../../README.md](../../README.md)
- 相关设计：
  - [regression_completion_criteria.md](regression_completion_criteria.md)

## 背景

仓库当前已经有一条内部差分基线：`myCPU/tests/host/backend_differential_smoke.cpp` 会比较 `functional` 与 `pipeline` 两条执行后端的一致性。这条链路能守住 myCPU 内部后端一致性，但仍然缺少外部 oracle，无法防止“两边一起错”。

对当前主线而言，最值得补上的外部真值不是更重的设备模型，也不是更大的统一验证框架，而是一条独立、可重复、不会污染默认门禁的 Spike 差分链路。它最适合帮助我们在 privilege、CSR、trap、以及后续 `Sv39` 等高风险语义面上做外部交叉验证。

## 设计目标

- 为 host 微场景提供独立的 `myCPU vs Spike` 外部 oracle。
- 当前以 final state 为比较核心；默认统一比较 `pc`、`halted/timed_out`、privilege、GPR、tracked CSR、watched memory 和最小 trap summary。
- 对执行 `mret / sret` 的 returning trap handler，额外比较“第一次入 trap”的最小 checkpoint summary，而不是把整段中间态扩成 trace differential。
- 尽量复用现有 host differential 的场景资产与状态口径，而不是另起一套测试 DSL。
- 把 Spike 的镜像生成、进程调用、输出解析全部收口在 adapter / runner 层，不把外部工具细节扩散到 compare 逻辑。
- 默认不把 Spike 变成 `make test` 或 `make test-pipeline` 的必需依赖。

## 明确非目标

- 当前不把 Spike 差分并入默认主门禁。
- 当前不直接扩成统一 multi-oracle framework。
- 当前不覆盖 guest workload、设备 side effect、MMIO 行为或 pipeline 内部微架构状态。
- 当前不做逐提交 trace differential。

## 当前结构

### 1. 共享规格层

共享规格位于 `myCPU/tests/host/spike_differential/`，核心是：

- `shared_spec.h`
  - 定义 `Scenario`、`MemoryWatch`、`MemoryInit`、`kTrackedCsrs`
  - 复用 host 微场景常量、入口地址和 trap vector 约定
- `final_state.h`
  - 定义 `FinalState`、`TrapSummary`、`DiffReport`
  - `FinalState` 当前同时保留最终 `trap_summary` 与 returning trap handler 使用的 `first_trap_summary`
- `state_compare.h`
  - 负责 final-state compare 和必要归一化

当前 `Scenario` 已经能表达：

- `program`
- `trap_program`
- `watches`
- `max_steps`
- `initial_gprs`
- `initial_csrs`
- `initial_memory`
- `initial_privilege`
- `configure`
- `fixture`

这里故意把规格定义得略宽于 Spike V1 当前支持的子集。原因是 myCPU 侧 runner 和未来扩展面都需要这些字段，但 Spike adapter 会在入口处显式拒绝当前还不支持的 setup，而不是静默忽略。

### 2. myCPU Runner

`myCPU/tests/host/spike_differential/mycpu_runner.h` 负责在 myCPU 上执行同一份 `Scenario`，并导出统一 `FinalState`。

当前 contract 是：

- 使用 `functional` backend 作为 reference path
- 捕获 `pc`、`halted`、`instret`、privilege、GPR、tracked CSR 和 watched memory
- 在执行过程中尝试抓取“第一次稳定可识别”的 machine/supervisor trap summary
- 若命中受控退出点，则归一化为 `halted=true`
- 若步数预算耗尽，则归一化为 `timed_out=true`

### 3. Spike Runner

`myCPU/tests/host/spike_differential/spike_runner.h` 是整个外部 oracle 的核心 adapter。当前设计要点如下：

- 先把 `Scenario` 物化成 Spike 可执行输入，而不是直接对文本脚本拼补丁。
- 生成的是最小但合法的 ELF，而不是裸 RAM dump。
  - 包含 program header、section header、`.symtab/.strtab/.shstrtab`
  - 同时补齐 `tohost/fromhost`，避免 FESVR warning 污染解析
- 通过一段 bootstrap 负责把场景初始化为 Spike 可理解的起始态。
  - 支持最小 `initial_gprs / initial_csrs / initial_memory`
  - 支持非 `M-mode` 起始态
  - 支持 `trap_program`
  - non-`M-mode` 起始通过真实 `mret` 进入，而不是在 Spike 外部伪造最终 privilege
- Spike 最终态通过 `-d --debug-cmd=...` 采集，当前固定读取：
  - `pc 0`
  - `priv 0`
  - `reg 0 instret`
  - `reg 0 <0..31>`
  - tracked CSR 集合
  - `mem <watch-addr>`
- 对执行 `mret / sret` 的场景，Spike runner 当前会在同一个 Spike 进程里分两段采集同构 snapshot：
  - 第一段在首个 trap handler 入口的 `mtvec/stvec` 处抓 first-trap checkpoint
  - 第二段在受控退出点或步数预算结束后抓最终 snapshot
- first-trap trap summary 的推断会按 Spike bootstrap 的“有效初始 CSR 值”做比较，避免把 non-`M-mode` 入口预写的 `mepc` 噪音误判成 machine trap。
- 解析是严格 fail-closed：
  - `privilege` 行必须且只能出现一次
  - 数值字段数量必须精确匹配
  - 未识别的非空输出行直接视为 parse failure
  - 只忽略 debug prompt 和命令回显这类受控噪音

Spike runner 当前会显式拒绝以下 setup：

- `configure` hook
- `Scenario::PlatformFixture` 非 `None`
- `initial misa` 写入
- 不受支持的 watch 形状
- `kEntry` 以下的 `initial_memory`

这不是实现疏漏，而是 V1 有意识保留的边界。当前目标是先建立稳定的 final-state oracle，而不是把所有 host 测试入口一次性镜像给 Spike。

### 4. Compare / Report 层

`myCPU/tests/host/spike_differential/state_compare.h` 统一承接 final-state compare。

当前比较项包括：

- `halted`
- `timed_out`
- `pc`
- privilege
- `gpr[x0..x31]`
- tracked CSR
- watched memory
- trap summary
- returning trap handler 场景下的 `first_trap_summary`

当前 compare 还做了两类必要归一化：

- 对 `sstatus / mstatus` 只比较 myCPU 当前已建模的位，避免把 Spike 更完整实现的一些非目标位混进结果。
- 对 non-`M-mode` bootstrap 必然带来的少量 `mstatus.MPIE / mepc` 噪音做场景级剥离，避免把初始化机制差异误报成 privilege 语义错误。
- 对 returning trap handler，只比较 first-trap 的最小 trap summary，不比较完整 checkpoint 架构态。

## 用户入口

当前用户入口分成两层：

- `cd myCPU && make test-host-spike_differential_smoke`
  - 只跑 helper / parser / error-classification 自测
  - 不要求本机安装 Spike
- `cd myCPU && make test-host-spike_differential`
  - 真实执行 Spike
  - 跑当前接入的正向差分场景

运行真实差分时，Spike 路径解析顺序如下：

1. 若设置了 `SPIKE_PATH`，优先使用它
2. 否则若设置了 `SPIKE_BIN`，使用它
3. 否则默认执行 `spike`

当前还提供一个调试开关：

- `SPIKE_DIFF_KEEP_TEMPS=1`
  - 保留临时 ELF 和 debug script，便于本地排障

## 当前实现边界

当前这条链路是“离线 final-state differential”，不是更大的通用 trace framework。具体边界如下：

- 当前已接入并适合作为正向样例的，是 host 微场景，不是 guest workload。
- 当前重点覆盖 ALU、memory、control flow、CSR、machine trap、delegated privilege、returning trap handler first-trap checkpoint，以及第一批 device-free `Sv39/page fault` final-state 子集这类架构可见语义面。
- 当前不比较设备 side effect、平台状态或 pipeline 内部状态。
- 当前不要求 `make test` / `make test-pipeline` 依赖 Spike。

## V1 的有意识收窄

以下限制是当前设计选择，不应误读为“临时坏掉但还没修”：

- 只比较 final state，不比较逐提交轨迹。
- 当前不比较 `instret`，因为 Spike bootstrap 会引入额外初始化指令。
- 对执行 `mret / sret` 的 returning trap handler，当前只比较 first-trap 的最小 trap summary，不比较完整中间态寄存器轨迹。
- Spike 侧 first-trap checkpoint 当前仍限制为“单次运行里抓第一段 trap 入口 + 最终态”这一个最小形态，不扩成多 checkpoint / nested trap trace。
- 当前虽已覆盖第一批 device-free `Sv39/page fault` final-state 子集和 returning trap handler checkpoint，但 `configure hook`、platform fixture、设备 side effect、更广 `Sv39` 语义面以及逐提交 trace 仍不在当前覆盖面内。

## 扩展边界

当前 Spike oracle 适合继续停留在 final-state / checkpoint 级验证。更广的
`Sv39 / page fault` 子集、更细 privilege / CSR 合同、多 checkpoint / nested trap 变体可以在
该模型内扩展，但仍不改变默认非依赖、非 trace-framework 的定位。

只有当 final-state / checkpoint 级能力不足以定位问题时，才应重新评估 commit-level 或
trace-level differential。

## 当前有效性说明

- 当前有效 / 历史语境：当前有效。
- 实时状态和已接入场景，请看 [../status/mainline_status.md](../status/mainline_status.md)。
- 相关完成态计划已归档到 [../plan/history_plan.md#spike-external-differential-validation-plan](../plan/history_plan.md#spike-external-differential-validation-plan)。
