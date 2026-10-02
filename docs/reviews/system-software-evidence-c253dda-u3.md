# U3：观察链与时间口径

## 范围和证据入口

实际 checkout 为 `1b54928fc0f1d2c7e39d2c9c781be4e9a43babc7`，产品源码沿用 [U1 基线](system-software-evidence-c253dda.md)。本单元新增 [独立 host 探针](../../myCPU/tests/host/system_software_observation_evidence.cpp)，没有修改产品、公共协议、Makefile、CI 或 Wiki。主会话编译并运行最终探针；本文核对其 JSON 原始结果，并运行下表的既有 host 工具。

配置为单 hart、默认 128 MiB RAM、functional/pipeline、L1D 关闭，无宿主随机注入；配对 guest 处于 M/Bare 模式。每个短程序最大 1000 后端 step，暂停点重复 snapshot 100 次，planner 候选先运行固定循环 100 step。guest 指令字在最终 JSON 的 `guest_image.words` 中保存；源文件、可执行文件 SHA-256 和完整编译参数在采集文件中，可重新构造临时二进制。AddressSpace 子实验独立使用 Sv39、S 模式、VA `0x2000`、物理页 A/B，没有 OS。

| E-ID | 对象 | 实际结果 | 边界 |
| --- | --- | --- | --- |
| E-U3-001 / 002 | 首次探针构建 / 执行 | 构建成功；执行退出 1，pipeline 配对超过 1000 step | 原夹具依赖 load 后 `set_gpr`；保留失败，不能记为配对通过 |
| E-U3-003 / 004 | guest 自初始化后构建 / 执行 | 构建成功；执行退出 1，`TLB refresh failed` | 暴露 A/D 写回恢复旧映射；后续细化诊断输出，没有修产品 |
| E-U3-005 / 006 | 最终构建 / 执行 | 均退出 0；执行 0.010 秒 | 30 秒运行上限；`completed` 表示采集完成，不代表所有产品契约通过 |
| E-U3-011 | `timeout 15s ./tests/host/debug_cli_smoke` | 超时，退出 124，15.008 秒，无输出 | 工具包含 pipeline AI guest 的 12000000 step 内部预算；未定位超时时正在执行的场景，不定性为挂死或产品缺陷 |
| E-U3-012 | `timeout 15s ./tests/host/debug_protocol_command_smoke` | 通过，退出 0，0.009 秒 | 命令解析/预算契约自测 |
| E-U3-013 | `timeout 15s ./tests/host/execution_profile_smoke` | 通过，退出 0，0.011 秒 | 既有 profile 聚合/后端/序列断言 |
| E-U3-014 | 默认 `python3 tests/host/run_debug_cli_probe_test.py` | 未运行 | 源码第 1630、1676 行直接使用共享 xv6 kernel/fs.img；该阶段由 U4 占用，不并发碰共享镜像 |

原始文件：[首次失败](system-software-evidence-c253dda/u3-probe-runs.json)、[第二次失败](system-software-evidence-c253dda/u3-probe-retry-runs.json)、[最终采集](system-software-evidence-c253dda/u3-probe-final-runs.json)、[guest 指令字与哈希](system-software-evidence-c253dda/u3-guest-images.json)、[既有工具运行](system-software-evidence-c253dda/u3-existing-runs.json)。每个记录有 argv、cwd、预算、退出码、耗时、stdout/stderr 和哈希；既有工具采集源码也内嵌保存。临时 guest 按指令字逐项写入 little-endian 32 位字即可重建，不依赖已删除的 `/tmp` 二进制。未运行的 Python 不能因其他工具通过而记为通过。

E-U3-001–004 的旧探针全文没有另存，当前源码已修订；旧 source_sha256 仅标识当时内容，不能从哈希恢复源码。这四条保留历史输出与采集器，只用于解释诊断过程，不能宣称可精确重建其运行。最终 E-U3-005/006 的探针源码及 guest 指令字已保存，可独立复现两项产品分歧；重要结论以最终记录为依据。历史源码无法恢复是本轮证据缺口。

`debug_cli_smoke.cpp:969` 的 `run_until_halt(12000000)` 说明该二进制包含较大 pipeline 工作负载，15 秒外层预算可能先于内部预算耗尽。本次没有逐场景进度输出，无法断言停在该 AI 场景，也没有以超时推断死循环；没有扩大预算重跑。

## Q3：观察副作用配对

模式 0 用 `debug_step_raw()`，模式 1 用 `step_cycle()`（逐步采集前后状态并生成事件），模式 2 用 raw step，并在第 4 个 step 后的同一暂停点重复 snapshot 100 次。客体自己建立地址寄存器，写 `0x80001000` 的 8 字节签名，再写 UART `A`，最后用项目 M 模式 `a7=93; ecall` 退出约定。三个模式的内存采样均在执行结束后使用同一个物理读取；UART 取完整输出字符串。结果并非只由被检查的 snapshot 自证。

| 后端 | 模式 | 独立 RAM / UART 结果 | cycle / instret / mtime | profile 退休 / 访存 |
| --- | --- | --- | --- | --- |
| functional | 0 / 1 / 2 | 三者均为 65 / `A` | 三者均为 13 / 13 / 13 | 三者均为 13 / 2 |
| pipeline | 0 / 1 / 2 | 三者均为 65 / `A` | 三者均为 19 / 13 / 19 | 三者均为 13 / 2 |

探针还比较 GPR、PC、SATP、profile trap 数、shadow-cache hits/misses，以及 L1 loads/stores；最终完整快照可回读其他字段。L1D 本轮关闭，不能扩成启用 L1D 时所有状态无影响的证明。mode 1 的 DebugEvent 增加是规定的观察记录差异；mode 0/2 无逐步事件。仅对这段固定程序及这些字段证实重复 snapshot/逐步观察没有改变客体结果或定义统计，不证明任意 guest、任意查询均无副作用。

源码路径：`debug_session.cpp:470` snapshot → `collect_snapshot():514`，只读 core、CSR、backend snapshot、L1 stats、设备 getter 和 Bus 最新记录。`csr_file.cpp:162` 的 read 为 const；TIME 从 CLINT getter 获取。UART getter 在 `uart16550.cpp:159`，PLIC getter 在 `plic.cpp:112`，不执行设备 load。`uart_output():406` 读取输出字符串，不读 UART 寄存器。

其他入口的边界由 E-U3-006 单独实证：

| 入口 | 真实结果 | 分类及限制 |
| --- | --- | --- |
| `debug_try_load_guest_memory(UART_RBR)` | 两后端 helper 读出 `A`（65），下一真实 guest RBR 读到 `B`（66）并写 RAM | 已证实有副作用访问；这是普通 Bus load helper，不能包装成无副作用查询 |
| `debug_try_load_guest_memory(PLIC_CLAIM)` | helper 返回源 10，pending 清除、claimed 置位；下一真实 guest claim 写 RAM 为 0 | 已证实 claim 消费；UART RDI/PLIC 优先级与使能由 guest 指令设置 |
| `translate_result(..., false)` | 立即查询不写 A/D，却记录 page-walk Bus access，并影响后续缓存翻译 | 已证实 false 只抑制 A/D 更新，不表示纯查询；另见 F-U3-002 |
| `translation_plan()` / `jit_dispatch()` | 两后端都有真实重复候选；Bus source 从 debug 变成 fetch，GPR/cycle/instret 未变 | 已证实污染最新 Bus 观察槽；裸模式未实证代码 PTE A/D，不能记为该项通过 |
| `set_gpr/set_memory/set_csr/uart_input/reset/step*` | 按 API 目的改变状态 | 本来就是调试修改/执行命令；应与纯查询区分，其中 set_gpr 另有 F-U3-001 |

helper 链为 `debug_session.cpp:428` → `bus.cpp:146/154` → `Device::load()`。UART RBR 在 `uart16550.cpp:15` 弹出 RX；IIR 在 `:33` 清 TX pending（本次没有单独动态复现 IIR）。PLIC load `plic.cpp:35` → `claim():207` 修改 pending/claimed。Bus 没有通用 debug peek API。CLI `debug_protocol.cpp:107` 有 snapshot、UART 输出与 planner，没有独立 memory-read、VA translation 或 page-table 查询命令；不能把 helper 的副作用宣称成前端已暴露的错误查询行为。

planner 链：`debug_session.cpp:435/460` → `dbt_block_plan.cpp:521/424` → `fetch_decode():218` → AddressSpace fetch16/fetch32 → `access_result():398`，真实取指使用 update_access_bits=true；有候选时才到这条链。`append_code_physical_spans():382` 再用 false 翻译。JIT dry-run 的“没有执行 guest code”不保证没有规划取指副作用。本轮对代码 A-bit 影响保留为源码路径推断，实际 bare-mode 只证实 Bus 覆盖。

## 两项已复现产品缺陷

### F-U3-001：pipeline 的调试 GPR 写入没有同步执行视图

触发：空闲、刚加载的 session 调 `set_gpr("x5", 41)`；快照显示 41，guest 首条执行 `addi x6,x5,1`，再用 guest 指令建立 RAM 地址并写 x6。functional 写入签名 42，pipeline 写入 1。预期来自公开 setter 的写寄存器行为及独立 guest 加法，不由被测输出生成。E-U3-006 的 `debug_set_gpr.contract_matches=false` 保存了原始分歧。

首个执行分歧在 guest 对 x5 的读取；归属为平台/调试与 pipeline 执行状态的同步，置信度高。`Machine::set_gpr` (`machine.cpp:450`) 只写 core；pipeline 构造 (`pipeline_backend.cpp:113`) 已复制初态到物理寄存器，`pipeline_core_state.cpp:154` 的 rebuild 才同步执行源。该 setter 路径未触发 rebuild。初版观察夹具因依赖 setter 而无法正常运行，不能仅归类为 guest 程序错误；修订夹具改用 guest 自初始化，以免掩盖本发现。

影响是 pipeline 调试注入/通过 setter 初始化的客体可能读取旧值；并非已证明正常 xv6 guest 自己写 GPR 也有问题。没有修产品。后续最小调整应定义暂停点 setter 与 in-flight 指令的同步契约，并回归空闲及运行后暂停的场景，而非默认重写后端。

### F-U3-002：缓存旧 PTE 的 A/D 写回恢复已经改过的 PPN

规则依据为官方 [Privileged 20240411](https://docs.riscv.org/reference/isa/v20240411/_attachments/riscv-privileged.pdf) §10.3.2 step 7：A/D 更新须以当前内存 PTE 做原子检查，不匹配时重走 step 2；该步骤不能使用缓存 PTE。Sv39 沿用这一翻译过程。允许未执行适用 SFENCE 时使用旧翻译，不等于允许缓存 A/D 更新覆盖新内存 PTE。

触发：S/Sv39 页 A 的叶 PTE A/D=0，先 false 翻译把它填入 TLB，再直接把 RAM 叶 PTE 改为页 B，尚未 fence，执行真实 load。E-U3-006 的三组输出如下：

| 控制组 | remap 后 PTE / PPN | load 结果 | load 后 PTE / PPN | 再 flush 后 load |
| --- | --- | --- | --- | --- |
| 无 query | 536877063 / 524294（B） | 34（B） | 536877127 / 524294（B，A 置位） | 34 |
| false query，无 fence | 536877063 / 524294（B） | 17（A） | 536876103 / 524293（A，A 置位） | 17 |
| false query，load 前 flush | 536877063 / 524294（B） | 34（B） | 536877127 / 524294（B，A 置位） | 34 |

旧翻译读取 17 本身是允许差异；确定的问题是 load 后 RAM 的 PPN 从 B 恢复为 A，之后 flush 仍读 A。源码 `address_space.cpp:383` 的 TLB hit 使用缓存 `entry.pte`，`update_pte_access_bits():337` 将缓存值 OR A/D 后直接 `bus.try_store_observed`，没有重读比较当前 RAM PTE。归属为 MMU A/D 写回，已复现产品缺陷，置信度高。

覆盖只有单 hart direct AddressSpace API、RAM 页表及上述 remap 条件，未证明 xv6 正常 fork 已触发；guest 正确 fence 的控制组没有复现恢复旧 PPN。本轮不修 MMU，不将它误说成 SFENCE 本身失效；后续最小修复应遵循当前 PTE 的原子比较/重试契约，并补旧 TLB 项、PTE 换 PPN/权限与 fence 控制回归。

## Q4：一次访存的关联与窗口

真实事件样例来自 E-U3-006 的 pipeline mode 1：

```json
{"cycle":15,"kind":"store","detail":"ram write 0x80001000"}
{"cycle":17,"kind":"store","detail":"uart write 0x10000000"}
```

同份末态退休记录有 sequence 10、PC `0x80000024`、raw `0x543023`（RAM store），以及 sequence 11、PC `0x80000028`、raw `0x548023`（UART store）。末态 `guest_bus` 为 `source=guest-data,kind=mmio-commit,addr=0x10000000,value=0x41,size=1,success=true`。这支持固定 guest 下人工把指令、RAM 签名和 UART 写入联系起来；事件自身没有 sequence/PC，不能据此声称已有自动完整因果链。末态 `bus` 已被同等结果采样的 RAM read 覆盖成 internal/bus-load，`guest_bus` 仍保留 UART 写。

执行链是 guest instruction semantics 产生 MemoryRequest → AddressSpace 翻译 → Bus 实际物理访问 → 提交边界 → profile/序列 → DebugSnapshot → JSONL → Node session/runtime → 浏览器 snapshot。需要的 PC、VA、PA、SATP/privilege、翻译路径和提交属性分散在临时执行状态及不同槽中，没有统一访存关联 ID。

| 层 | 现有字段或行为 | 能力缺口 |
| --- | --- | --- |
| 记录源：Bus (`debug_snapshot.h:85`,`bus.cpp:341`) | PA/value/size/source/kind/device/write/mmio/success/detail；latest 与 latest guest 各 1 条 | 无 PC/VA/SATP/privilege/sequence/cycle；同一步多访问只剩最后条目 |
| 记录源：memory profile (`execution_profile.h:35`) | PC/raw、PA-valid/PA、region/write/fault/bytes | 无 VA、地址空间、翻译路径、关联 sequence；逐条对象直接聚合而非导出 |
| 关联：DebugEvent (`debug_session.cpp:612`) | 比较前后最新 guest bus，尚无 guest 访问时 fallback 到 latest bus | 两次相同访问可不生成第二事件；早期 fetch 被显示为 load；同 cause 的重复 trap 可遗漏 |
| 保留：DebugEvent (`debug_session.cpp:606`) | 最近 64 条；`run_until_*` raw step 不逐步调用 record_step_events | 静默删除，无 overflow/drop 数；batch run 不能还原过程 |
| 保留：pipeline sequence (`pipeline_sequence.h:38`) | 最近 16 条退休记录；sequence 单调分配，包含被清除指令的空隙 | 无明确覆盖计数；sequence 间隙不能单独证明丢失；functional 无此 trace |
| 保留：profile (`execution_profile.h:309`) | 内部累计 map，snapshot 各类只取 top 8 | 无原始事件；输出项数不是完整唯一 PC/分支总数，无省略项计数 |
| 协议 (`debug_protocol_response.cpp:155`) | RetireTrace 只导出 sequence/PC/raw/trap/redirect | C++ 的 cycle/target 被省略，不能精确加入事件时间线 |
| 协议 observation_event (`:174`) | `execution-profile/snapshot-summary`；event_id 是 backend/cycle/PC | 这是聚合快照摘要，不是一次访存/异常 transaction ID |
| 前端 (`state.js:1/415/533`) | 最近 32 个 snapshot；timeline 用 cycle/stages/flags | 静默 slice；不提供持久采集、恢复、翻译或 PID 关联 |

Node `debug_cli_session.mjs:106/208` 按 FIFO 发送 JSONL、解析响应；runtime `debug_server_runtime.mjs:486/499/512` 请求或广播快照，浏览器 state 接收。它们没有补充 MMU 或 OS 事实，frontend 不是当前 PID/fork 的裁判。CLI 没有页表查询入口，也不能因为 RAM 中读到 PTE 就称那是 CPU 此时实际使用的 TLB 项。

窗口大小、导出省略和 dedup/batch 路径由源码确定；本轮短配对程序没有刻意跑满 64/16/32 以动态验证溢出。窗口外无法从现有输出恢复属于能力缺口；没有把未经运行的超窗恢复测试记为通过。暂停/重复 snapshot 不延长历史，继续后 latest 槽与 bounded trace 会更新。

## Q6：时间口径

| 指标 | 单位与所有者 | 推进/读取位置 | 后端差异和结论边界 |
| --- | --- | --- | --- |
| cycle | core 模型 cycle | `core_state.cpp:44`；functional `:187/197/247`；pipeline cycle `:139/180` | 每 backend step 加 1，pipeline stall/replay 也加；本 guest 13 对 19，不等于宿主秒或硬件频率 |
| instret | 成功退休指令数 | `pipeline_commit_boundary.cpp:145`；CSR read `csr_file.cpp:170` | 本 guest 两后端都是 13；不能当作周期或任意 trap 步数 |
| CLINT mtime / CSR TIME | CLINT tick 数 | backend step 调 Bus tick，`clint.cpp:75` 加 1；CSR TIME `csr_file.cpp:167` | functional 在 `functional_backend.cpp:163`，pipeline 在 `pipeline_backend_cycle.cpp:133`；相同退休数也会有不同设备时间 |
| UART drain / 平台事件 | 设备 tick 与状态 | `bus.cpp:317` tick 所有设备；UART `uart16550.cpp:142`；PLIC tick 只 peek | 设备依 backend step 推进；不同后端合法调度差异须独立解释 |
| profile PC cycles | 两退休之间的模型 cycle 差 | `execution_profile.cpp:467`，首条为 cycle+1 | 间隔归到当前 PC，含 stall/异常间隔；不是单条独立 latency |
| 宿主墙钟秒 | 采集器 Date.now/外部 timeout | 各证据 argv 和 wall_seconds；UI load progress 用 Date.now | 包含宿主实现、I/O、观察/序列化成本；不能宣称客体性能或真实频率 |

TIME 没有绑定 CLINT 时退回 core cycle；Machine 的正常 platform 绑定 CLINT。`Machine::step():719` 在 halted 后不再调 backend，所以停机后的 snapshot 不推进设备。CSR counter getter 不 tick；mode 2 对照也实证本暂停点的 100 次 snapshot 没有推进 cycle/instret/mtime。可写 MCYCLE/MINSTRET 和调试 set_csr 的行为不属于纯读取时间口径，本轮没有测计数禁止位或任意 counter 写后性能结论。

## 单元结论和下一轮范围

Q3：固定程序的 snapshot/逐步观察配对成立；普通 memory helper、false 翻译、planner 必须分别说明副作用。Q4：已有局部状态和统计，能够人工关联短程序，但缺地址空间/VA/翻译路径、统一 ID、完整保留与丢失信号，不能自动解释 xv6 的父子页归属。Q6：时间代表简化 backend/设备推进，能支持功能调试和模型内对照，不能支持真实客体性能声明。

最小后续范围依次为：独立修复 MMU 当前 PTE 检查/A-D 更新、明确并修复 pipeline setter 同步；为目标场景分离无副作用查询与正常设备访问，并补窄范围访存关联/采集；OS 解释交 U4 的固定版本 xv6 适配证据。没有实现这些建议，没有创建 Issue。debug_cli_smoke 超时及默认 Python 未运行仍是验证缺口，不由最终新探针退出 0 自动解除。
