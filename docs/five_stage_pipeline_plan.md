# myCPU 五级指令流水线引入方案

## 1. 目标

本文给出一套适配当前 `myCPU` 仓库状态的五级指令流水线方案。目标不是“把现有参考核改写成流水线版单实现”，而是：

1. 保留当前简单、正确、可调试的功能参考路径。
2. 在不复制 ISA 语义的前提下，新引入一个经典 5-stage pipeline backend。
3. 为后续多周期、流水线、甚至更复杂微架构提供统一的语义源和提交边界。

这与仓库既有路线一致：

- Phase 1 先完成可靠的功能模拟器和 OS bring-up 地基。
- Phase 2 再引入 alternate CPU execution models。
- 所有 backend 共享一套架构语义源，而不是各自重复实现。

## 1.1 当前落地状态

在当前仓库状态下，这份方案里最关键的结构步骤已经基本落地：

| 阶段 | 当前状态 |
|---|---|
| 阶段 A：backend 骨架 | 已完成。`ExecutionBackend`、`FunctionalBackend`、`PipelineBackend` 已接入 `Machine` / CLI。 |
| 阶段 B：共享语义层 | 已完成。`InstructionSemantics + InsnEffects + ExecutionContext` 已成为后端共享语义源。 |
| 阶段 C：`AddressSpace` fault-result | 已完成。后端可用 `fetch32_result/load_result/store_result` 控制 trap 提交时机。 |
| 阶段 D：第一版流水线骨架 | 已完成。`IF/ID/EX/MEM/WB`、forwarding、load-use interlock、redirect/flush 已落地。 |
| 阶段 E：系统指令与特权行为 | 当前仓库已覆盖当前实现子集中的 `CSR/system`、`ecall`、illegal instruction、`mret/sret`、fetch/load/store fault、commit-boundary interrupt。 |
| 阶段 F：pipeline 专用验证 | 当前仓库已具备 `make test-pipeline`、`pipeline_backend_smoke` 和 `backend_differential_smoke`。 |

因此，本文剩余最主要的价值已经从“如何起第一版骨架”转变为：

- 解释当前架构为什么这样组织。
- 给后续继续扩 privileged / platform / 更复杂 backend 时提供边界约束。
- 作为后续增量 patch 的设计依据，而不是表示当前流水线还完全未落地。

## 2. 当前仓库现状与约束

当前仓库已经具备以下基础：

- `Machine + Bus + Ram + Device` 平台骨架已经成型。
- CPU 状态已经有 `CoreState + CsrFile + TrapController + AddressSpace` 边界。
- 指令语义已经按 `integer / control-flow / memory / system` 族拆到 `exec/`。
- `AddressSpace` 已支持 bare-mode 与 Sv39。
- trap / interrupt 路由已经有独立 `TrapController`。

但当前实现依然是单步参考路径：

- `cpu_step()` 在一个函数里完成取指、译码、执行、退休与 cycle 计数。
- `exec/*` 里的语义函数会直接写寄存器、访问内存、调用 trap。
- `AddressSpace` 当前会直接触发 page fault / access fault。
- `TrapController` 会立即修改 CSR、特权级和 `pc`。

这些特点对功能参考核是合理的，但对流水线不合适。五级流水要求“按阶段推进”和“按提交点更新架构状态”，而不是“译码后立刻把所有架构效果做完”。

## 3. 总体设计原则

### 3.1 保留功能参考核

现有功能参考核应继续存在，作为：

- 架构正确性的黄金基线
- trap / CSR / MMU / privileged 行为的对照实现
- 后续流水线与更复杂 backend 的差分验证对象

不要用流水线版本替换当前参考核。

### 3.2 先拆语义，再做调度

五级流水不应直接在当前 `cpu_step()` 上堆叠 `IF/ID/EX/MEM/WB` 变量。  
正确顺序是：

1. 把当前语义提炼为“架构效果描述”。
2. 把当前功能路径包装成 `FunctionalBackend`。
3. 再新增 `PipelineBackend`。

### 3.3 共享一套架构语义源

功能后端与流水线后端都必须共享同一套 ISA 语义。  
backend 的区别只体现在：

- 何时取指
- 何时译码
- 何时访问 memory
- 何时写回/提交
- 何时插入 stall/flush/forwarding

而不能体现在“这条指令到底做什么”。

### 3.4 精确异常必须优先于性能

本项目当前优先级是正确、可调试、可验证。  
因此第一版五级流水必须保证：

- 按程序顺序提交
- 精确异常
- younger 指令不会在 older fault 之前污染架构状态

## 4. 推荐演进后的执行模型

引入 backend 抽象后，推荐结构如下：

```text
main.cpp
  -> Machine
      -> ExecutionBackend
           -> FunctionalBackend
           -> PipelineBackend
      -> CoreState
      -> CsrFile
      -> TrapController
      -> AddressSpace
      -> Bus
          -> Ram / Uart16550 / Clint / Plic / SimpleStorage
```

其中：

- `Machine` 持有平台级对象和 backend
- `FunctionalBackend` 复用现有参考行为
- `PipelineBackend` 新实现五级流水控制
- `InstructionSemantics` 成为唯一语义源

## 5. 第一步必须完成的结构重构

在开始真正实现五级流水前，建议先完成以下基础改造。

### 5.1 引入 `ExecutionBackend`

新增一个最小 backend 接口：

```cpp
class ExecutionBackend {
public:
    virtual ~ExecutionBackend() = default;
    virtual void step() = 0;
};
```

建议新增文件：

- `myCPU/src/exec/backend.h`
- `myCPU/src/exec/functional_backend.h`
- `myCPU/src/exec/functional_backend.cpp`
- `myCPU/src/exec/pipeline_backend.h`
- `myCPU/src/exec/pipeline_backend.cpp`

### 5.2 先包装当前功能路径为 `FunctionalBackend`

当前 `cpu_step()` 逻辑可以先几乎原封不动迁入 `FunctionalBackend`。  
这样做的作用是：

- 不改行为
- 先把 `Machine -> backend_->step()` 路径打通
- 为新增 `PipelineBackend` 腾出接口位置

### 5.3 引入统一语义层 `InstructionSemantics`

当前 `exec/*` 的函数直接修改 CPU 状态。要支持流水线，需要把它们改成产出“架构效果”。

推荐新增：

- `myCPU/src/isa/instruction_semantics.h`
- `myCPU/src/isa/instruction_semantics.cpp`
- `myCPU/src/isa/step_result.h`
- `myCPU/src/isa/execution_context.h`

`DecodedInsn` 仍然可以沿用当前 `Insn` 结构，后续再决定是否做更强类型化。

### 5.4 `AddressSpace` 不再直接触发 trap

当前 `AddressSpace` 直接调用 `TrapController` 进入异常。  
这会破坏流水线的精确异常模型。

建议改为返回结果结构：

```cpp
struct MemoryAccessResult {
    bool ok{false};
    bool fault{false};
    uint64_t value{0};
    uint64_t cause{0};
    uint64_t tval{0};
};
```

取指、load、store、page walk fault、access fault 都先返回结果，由 backend 在提交点决定是否真正进入 trap。

### 5.5 `TrapController` 保持为唯一 trap 进入/返回实现

不需要删除或弱化 `TrapController`。  
它应该继续作为：

- trap 进入唯一入口
- `mret` / `sret` 返回唯一实现
- interrupt 路由唯一实现

区别只是：

- 在 `FunctionalBackend` 中，trap 仍然几乎“立即”发生
- 在 `PipelineBackend` 中，trap 只在提交边界被调用

## 6. 架构语义层设计建议

### 6.1 语义层输出对象

建议定义一个统一的“指令架构效果”结构：

```cpp
struct TrapRequest {
    bool valid{false};
    uint64_t cause{0};
    uint64_t tval{0};
};

struct RegWrite {
    bool enable{false};
    uint8_t rd{0};
    uint64_t value{0};
};

struct CsrWrite {
    bool enable{false};
    uint32_t addr{0};
    uint64_t value{0};
};

struct MemoryRequest {
    enum class Kind : uint8_t { None, Load, Store };
    Kind kind{Kind::None};
    uint64_t addr{0};
    uint64_t store_value{0};
    int size{0};
    bool sign_extend{false};
};

struct ControlEffect {
    bool redirect_pc{false};
    uint64_t target_pc{0};
    bool halt{false};
};

struct InsnEffects {
    RegWrite rd_write{};
    CsrWrite csr_write{};
    MemoryRequest mem{};
    TrapRequest trap{};
    ControlEffect control{};
};
```

这个结构不必一开始完美，但必须满足一个关键要求：

**一条指令的语义先被“描述”，再由 backend 决定何时提交。**

### 6.2 语义层与 backend 的职责划分

语义层负责：

- 算 ALU 结果
- 判断 branch 是否成立
- 生成 jump target
- 生成 load/store 请求
- 生成 CSR 写请求
- 生成 trap 请求

backend 负责：

- 何时取指
- 何时推进阶段寄存器
- 何时发 memory request
- 何时写寄存器
- 何时更新 `pc`
- 何时提交 trap
- 何时 `stall/flush/forward`

## 7. 五级流水结构设计

### 7.1 五级划分

建议采用经典 5-stage：

- `IF`: instruction fetch
- `ID`: decode + register read
- `EX`: ALU / branch / address generation
- `MEM`: memory access
- `WB`: write-back / commit

### 7.2 流水寄存器

第一版建议使用如下 4 组阶段寄存器：

```cpp
struct IfIdReg {
    bool valid{false};
    uint64_t pc{0};
    uint32_t raw{0};
};

struct IdExReg {
    bool valid{false};
    uint64_t pc{0};
    Insn insn{};
    uint64_t rs1_value{0};
    uint64_t rs2_value{0};
};

struct ExMemReg {
    bool valid{false};
    uint64_t pc{0};
    Insn insn{};
    InsnEffects effects{};
    uint64_t alu_result{0};
    uint64_t forwarded_rs2{0};
};

struct MemWbReg {
    bool valid{false};
    uint64_t pc{0};
    Insn insn{};
    InsnEffects effects{};
    uint64_t wb_value{0};
};
```

### 7.3 每周期更新顺序

在软件模拟器里，建议每个 cycle 按以下顺序计算“下一拍状态”：

1. `WB`
2. `MEM`
3. `EX`
4. `ID`
5. `IF`

然后统一提交新的阶段寄存器。

这样有两个好处：

- forwarding 更容易写
- 同周期写回对 younger 阶段的可见性更容易控制

## 8. 各流水级职责

### 8.1 IF

职责：

- 从当前 fetch PC 取指
- 接收 `AddressSpace` 返回的取指结果
- 遇到取指 fault 时不立即 trap，而是把 fault 信息放入流水寄存器
- 默认 `next fetch pc = pc + 4`

第一版建议：

- 不做 branch prediction
- 默认静态 not-taken
- `jal/jalr/branch taken` 在 EX 级发现后 flush

### 8.2 ID

职责：

- 调用当前 `decode.c`
- 读取寄存器堆
- 检测 hazard
- 为 EX 级准备操作数

ID 级需要至少实现：

- source/destination 寄存器识别
- load-use 检测
- branch/jump 是否需要使用 forwarding 后的值

### 8.3 EX

职责：

- 计算整数 ALU
- 计算 branch 比较结果
- 生成 jump target
- 生成 effective address
- 生成语义层输出 `InsnEffects`

建议把现有 `integer_ops` 和 `control_flow_ops` 的组合逻辑逐步迁到这里，但不要直接写 `CoreState`。

### 8.4 MEM

职责：

- 对 load/store 请求访问 `AddressSpace`
- 接收 load 数据或 fault
- 对 store 的 fault 做提交前保留

第一版建议：

- 所有 memory access 都视为单周期完成
- 不做 cache
- 不做 variable-latency memory

### 8.5 WB / Commit

职责：

- 写回寄存器
- 写 CSR
- 更新架构 `pc`
- 计 `instret`
- 处理 halt
- 处理 trap

这是流水线里最重要的一层。  
在当前项目里，建议把 WB 视为 **唯一架构提交点**。

## 9. Hazard 处理方案

### 9.1 数据冒险

第一版至少支持以下 forwarding：

- `EX/MEM -> ID/EX`
- `MEM/WB -> ID/EX`

覆盖对象：

- 算术指令写回结果
- `jal/jalr` 写回返回地址
- CSR 指令写回结果

### 9.2 load-use 冒险

对于 `load` 之后下一条立刻使用其结果的情况，第一版建议：

- 插入 1 个 bubble
- 冻结 `IF/ID`
- 让 `ID/EX` 注入空操作

这是最直接且可验证的实现。

### 9.3 控制冒险

第一版建议采用：

- 静态 not-taken
- EX 级决定 branch/jump
- 若重定向，则 flush `IF/ID` 和 `ID/EX`

对于 `jal/jalr/branch taken`：

- EX 级给出 redirect target
- younger 指令全部作废

## 10. Trap、异常与中断

### 10.1 精确异常目标

流水线版必须实现精确异常：

- 出错指令之前的 older 指令都已提交
- 出错指令本身触发 trap
- 出错指令之后的 younger 指令全部被丢弃

### 10.2 访存 fault / page fault

由于当前 `AddressSpace` 直接触发 trap，必须先改成“返回 fault 结果”。  
推荐做法：

- IF fault 在 IF 阶段记录，但在 commit 点真正触发 trap
- MEM fault 在 MEM 阶段记录，但在 commit 点真正触发 trap

### 10.3 `ecall` / `ebreak` / illegal instruction

这些在语义层里表现为 `TrapRequest`，但直到该指令到达提交点才调用 `TrapController::enter_exception()`。

### 10.4 `mret` / `sret`

`mret/sret` 属于控制类系统指令。  
建议在提交点处理：

- 调 `TrapController::return_from_mret()` / `return_from_sret()`
- flush younger pipeline state

### 10.5 外部中断与定时器中断

每个 cycle 仍然保留 `bus.tick()` 与 `PlatformEvents` 汇总。  
但建议中断采样策略改为：

- 先把 pending 状态同步到 CSR 视图
- 只在提交边界检查“是否应进入 interrupt”
- 若进入 interrupt，则清空 younger stages

这样更符合顺序提交模型，也更利于后续 precise interrupt 验证。

## 11. CSR 与 privileged 行为处理建议

当前 `CsrFile` 和 `TrapController` 已具备基本边界，应继续复用。  
流水线版中：

- CSR 读值可以在 ID/EX 获取，但必须考虑 forwarding 或顺序约束
- CSR 写应在提交点完成
- `mstatus/mie/mip/mideleg/medeleg` 等影响 trap 路由的 CSR 也应按提交点更新

第一版建议不要做 CSR 投机更新。

## 12. 对当前代码的最小改动路径

### 阶段 A：铺 backend 骨架

目标：

- 不改变现有行为
- 引入 `ExecutionBackend`
- 增加 `FunctionalBackend`
- 让 `Machine` 使用 backend 驱动

建议文件改动：

- `myCPU/src/platform/machine.h`
- `myCPU/src/platform/machine.cpp`
- 新增 `myCPU/src/exec/backend.h`
- 新增 `myCPU/src/exec/functional_backend.*`

### 阶段 B：抽离语义层

目标：

- `exec/*` 不再直接写架构状态
- 形成统一 `InsnEffects`

建议文件改动：

- 新增 `myCPU/src/isa/execution_context.h`
- 新增 `myCPU/src/isa/step_result.h`
- 新增 `myCPU/src/isa/instruction_semantics.*`
- 逐步迁移 `exec/integer_ops.*`
- 逐步迁移 `exec/control_flow_ops.*`
- 逐步迁移 `exec/memory_ops.*`
- 逐步迁移 `exec/system_ops.*`

### 阶段 C：改 AddressSpace fault 模型

目标：

- 访问失败返回结果，不直接进入 trap

建议文件改动：

- `myCPU/src/mem/address_space.h`
- `myCPU/src/mem/address_space.cpp`

### 阶段 D：落第一版 `PipelineBackend`

目标：

- 支持 RV64I 基础整数、branch、jump、load/store
- 支持 forwarding、load-use stall、branch flush
- 支持顺序提交

建议文件：

- 新增 `myCPU/src/exec/pipeline_backend.h`
- 新增 `myCPU/src/exec/pipeline_backend.cpp`

### 阶段 E：接系统指令与特权行为

目标：

- 接入 CSR 指令
- 接入 `ecall/ebreak/mret/sret`
- 接入 timer / external interrupt
- 接入 Sv39 fault

### 阶段 F：增加 pipeline 专用验证

目标：

- 功能后端与流水后端共用测试集
- 增加提交级 trace 对比

## 13. 建议的第一版功能范围

为了控制风险，第一版五级流水建议只覆盖：

- RV64I 基础整数
- branch / `jal` / `jalr`
- load / store
- `ecall` / illegal instruction
- 最基础 CSR 路径

暂时推迟：

- 复杂 privileged corner cases
- 完整 supervisor 运行时 bring-up
- 更复杂的中断嵌套场景
- variable-latency memory
- cache / branch prediction / multi-issue

## 14. 文件级新增建议

推荐新增如下文件：

```text
myCPU/src/exec/backend.h
myCPU/src/exec/functional_backend.h
myCPU/src/exec/functional_backend.cpp
myCPU/src/exec/pipeline_backend.h
myCPU/src/exec/pipeline_backend.cpp
myCPU/src/isa/execution_context.h
myCPU/src/isa/step_result.h
myCPU/src/isa/instruction_semantics.h
myCPU/src/isa/instruction_semantics.cpp
```

如果想把阶段寄存器单独收口，也可以增加：

```text
myCPU/src/exec/pipeline_types.h
```

## 15. 测试与验证方案

### 15.1 保持现有回归不退化

新增 backend 后，现有 `make test` 应继续对功能后端保持全绿。

### 15.2 增加 backend 选择

建议命令行增加：

```bash
./mycpu --backend functional <image>
./mycpu --backend pipeline <image>
```

### 15.3 共用现有 asm 回归

第一阶段至少对以下子集做双后端验证：

- `hello`
- `sum`
- `control_flow`
- `loads_signed_unsigned`
- `alu_word`
- `branches_signed_unsigned`
- `muldiv`
- `csr_trap`
- `exception_traps`

### 15.4 增加提交级 trace 对比

建议新增可选提交 trace：

```text
cycle, pc, insn, rd_write, rd_value, trap, privilege
```

这样可以对比：

- `FunctionalBackend`
- `PipelineBackend`

是否在同一条退休指令序列上得到一致的架构结果。

### 15.5 增加流水线专用测试

新增最小流水线测试集，重点覆盖：

- EX forwarding
- MEM forwarding
- load-use stall
- taken branch flush
- `jal/jalr` redirect
- precise trap on illegal instruction
- precise load/store fault

## 16. 风险与控制

### 风险 1：语义重复实现

如果直接在 `PipelineBackend` 里重写一套 ALU / branch / CSR 逻辑，未来功能后端和流水后端一定会漂移。

控制：

- 先落 `InstructionSemantics`
- backend 中禁止重新编码 ISA 行为

### 风险 2：异常不精确

如果 `AddressSpace` 或 `exec/*` 继续直接改状态或直接 trap，流水线很容易出现 younger 指令先污染状态的问题。

控制：

- fault 先记录
- trap 只在 commit 生效

### 风险 3：一次改太多，失去可运行状态

控制：

- 先引入 `FunctionalBackend`
- 每阶段保持仓库可编译、可运行
- 小步提交，不做大爆炸式重写

### 风险 4：在 Phase 1 未稳定前过早把精力转向复杂微架构

控制：

- 第一版 pipeline 范围严格受限
- 先保证不破坏当前参考核与 OS bring-up 方向

## 17. 推荐实施顺序

按最小风险顺序，建议实际动手顺序如下：

1. 引入 `ExecutionBackend`
2. 把当前参考路径迁成 `FunctionalBackend`
3. 为 `Machine` 增加 backend 选择
4. 抽离 `InstructionSemantics` / `InsnEffects`
5. 改 `AddressSpace` fault 返回模型
6. 落 `PipelineBackend` 骨架与 5-stage 寄存器
7. 先接整数、branch、jump
8. 加 forwarding / stall / flush
9. 再接 memory / CSR / trap / interrupt
10. 用现有 asm 回归和提交 trace 做差分验证

## 18. 第一版验收标准

第一版五级流水方案落地后，建议以以下条件作为完成标准：

- 现有功能后端行为不变
- 新增 `PipelineBackend`
- `PipelineBackend` 能正确运行基础整数、分支、访存路径
- 至少支持基本 forwarding、load-use stall、branch flush
- trap / interrupt 保持顺序提交与精确异常
- 能通过一批现有 asm 回归，并能与功能后端做提交级对比

## 19. 结论

在当前 `myCPU` 架构下，引入五级流水线的正确方式不是把现有 `cpu_step()` 直接“拆成五段”，而是：

- 保留当前参考执行器
- 先抽出统一语义层
- 再把五级流水作为独立 backend 加入

这样做的收益是：

- 当前功能参考核继续作为黄金基线
- 五级流水实现不会和功能语义耦死
- 后续多周期、缓存、甚至更复杂后端也有清晰演进路径

如果未来要继续做 Phase 3 的高级微架构，这一步也是必须的前置基础。
