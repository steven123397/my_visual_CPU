# 系统软件模拟器取证报告

## 范围与现场

本报告承接 [系统软件取证计划](../plans/2026-10-02-1038-system-software-evidence-plan.md)，作为回传入口。U1 固定版本、环境及资产；[U2](system-software-evidence-c253dda-u2.md) 补局部一致性与权限/fence、真实 Spike 证据；[U3](system-software-evidence-c253dda-u3.md) 补观察配对、事件/时间口径及两项产品缺陷；[U4](system-software-evidence-c253dda-u4.md) 已验证本次 functional 普通 fork、父子两页隔离和 exit/wait/局部回收。U5 回传整理尚未交付，不能据局部测试宣称全 ISA 正确。以下 U1 内容保留其当时范围与未验证边界。

- 日期：2026-10-02；取证分支：`codex/system-software-evidence-plan`（用户指定沿用本分支）。
- 实际 checkout：`c253ddad1862a8e222c74d633f9269390e418624`；规划基点：`fa5dfeac584863b0a0c98ac2747b99f1aa1ae90c`。
- 两基点之间仅修改 `docs/current.md` 并添加 Plan，没有产品源码变化。
- 交付提交由 Git 中主题带 `(U1)` 的提交标识；该提交只交付报告、文本证据及本单元 current 更新。后续单元在此报告追加证据，不将 U1 环境清单冒充最终报告。
- 开工已有外来修改：`docs/current.md`、`docs/reviews/2026-10-01-pr14-17-pre-merge.md`，以及未跟踪的 `docs/reviews/2026-10-01-remote-pr-pre-merge.md`。它们的开工哈希见 E-U1-001；已跟踪脏文件的完整补丁及哈希亦已保存。原审查结果不由本单元重新验收或提交。
- 产品源码、协议、CI、Wiki 均未修改；没有新增客体夹具、故障注入或产品修复。

## 证据索引

| E-ID | 分类与执行状态 | 内容与入口 |
| --- | --- | --- |
| E-U1-001 | 已证实环境事实；清单采集完成 | [环境、开工补丁、源码及旧资产指纹](system-software-evidence-c253dda/u1-inventory.json) |
| E-U1-002 | 静态核查，未运行产品测试 | 下文的目标、配置、问题覆盖映射；入口以本次 checkout 核对 |
| E-U1-003 | 已证实构建完成；未运行产品测试 | [强制重建命令、完整文本输出及新资产指纹](system-software-evidence-c253dda/u1-build.json) |
| E-U1-004 | 历史证据适用性核查 | E-U1-001 中与 `f69ea75` 的产品路径差异，以及下文的复用限制 |
| E-U1-005 | 文档与指纹验证通过 | [验证命令及结果](system-software-evidence-c253dda/u1-validation.json)：10 个内部相对链接、651 个产品文件哈希、15 项重建资产哈希；不检查远端链接 |

E-U1-001 为可直接取得的 Git 内文本证据。每条命令包含 argv（避免 shell 转义歧义）、工作目录、退出码、stdout/stderr、耗时及 30 秒上限；没有运行 guest，后端、guest 输入、停止条件和客体步数不适用于这些环境查询。取证采集通过 Node 标准库 `spawnSync` 顺序执行记录的 argv；SHA-256 由 `node:crypto` 计算。可按各记录的 cwd/argv 重跑。命令时间上限和失败均保留原状。

清单含所有已跟踪 `myCPU/`、`frontend/`、`.github/` 文件的工作树 SHA-256；总指纹算法为按路径排序的 `{path,sha256}` 数组经 `JSON.stringify` 后计算 SHA-256。源码总指纹为 `df561a7560a7c661002784dee668cd1e2b248bb3e6c36b6610dd8709724247f2`。开工已跟踪脏树补丁 SHA-256 为 `da2d72b5c7402b637f46614644c8766e329ae263cbf67a717f3843f38bc7f8da`。采集输出文件因重定向先创建，也出现在自身的 status 记录中；它不是外来修改。

## 环境与资产

| 项目 | 本次实际值 | 可支持的结论 |
| --- | --- | --- |
| 宿主 | WSL2 x86-64，内核 `6.18.40.1-microsoft-standard-WSL2` | 只代表本机环境 |
| Make / GCC / G++ | Make 4.3；GCC/G++ 11.4.0 | 版本查询完成；构建另列 |
| RISC-V 工具链 | GCC 10.2.0；objcopy 2.35.1 | `make -C myCPU check-riscv-toolchain` 退出 0，但该目标只检查工具存在 |
| Python / Node / npm | 3.10.12 / 24.16.0 / 11.13.0 | 不等于前端或 Python 测试通过 |
| CodeGraph | 1.6.1；仓库存在索引，已优先使用 explore/node | 用于代码入口定位；结论仍对照当前源码 |
| Spike | PATH 查找退出 1；`SPIKE_PATH`、`SPIKE_BIN` 均未设置 | 当前默认真实参考不可用；只阻断真实差分，不断言全机不存在其他安装 |
| 扩展名探测 | 显式 `rv64im_zicsr_zifencei` 预处理退出 1，报不支持 `z` 子集 | GCC 10.2 的能力分支；不是模拟器测试失败 |

当前 Makefile 根据上述探测选择空 `RV_Z_EXTENSIONS`；默认 guest 为 `-march=rv64im -mabi=lp64`，xv6 board 为 `-march=rv64ima -mabi=lp64`。host 默认 `-O2 -Wall -Wextra -g -std=c++17`（C 为 `-std=c11`）。xv6 还使用 vendored Makefile 的 `-O -ggdb -gdwarf-2 -mcmodel=medany -ffreestanding` 等参数；完整实际编译命令以构建证据为准，不把这些摘要当作全部参数。

board 配置来自 [mycpu_virt.mk](../../myCPU/workloads/boards/mycpu_virt.mk)：1 hart，RAM 基址 `0x80000000`、大小 `0x08000000`（128 MiB）；UART `0x10000000`、CLINT `0x02000000`、PLIC `0x0c000000`，virtio-blk `0x10001000`、IRQ 1。它是 workload 配置，不代表所有独立 host 夹具采用相同内存大小。U2/U3 必须为每个夹具另记实际配置。

xv6 profile 声明上游版本 `5474d4bf72fd95a6e5c735c2d7f208f58990ceab`，实际版本真值仍为本仓库 vendored 文件；没有重新下载或替换上游。开工已有 simulator、kernel ELF、kernel.sym、fs.img 和部分 host 二进制，哈希见清单。旧资产没有构建出处证明，不能直接用于行为结论；缺失的 host 二进制也仅是尚未构建。后续使用重建后的 E-U1-003 指纹，并在镜像可能变化的每次运行前重新取哈希。

## 构建结果与来源对应

E-U1-003 记录两次串行强制构建。host 使用 `make -C myCPU -B -j2 mycpu` 加清单中的 11 个 host 二进制目标，退出 0；该步由主会话监控，未设置显式 timeout，也未记录精确总墙钟耗时，证据中如实记为 null，不能据此作耗时比较。xv6 使用 `timeout 120s make -C myCPU -B WORKLOAD_NAME=xv6 build-workload`，退出 0、耗时 16.233 秒，未超时。所有实际编译和链接参数均在同一 JSON 的原始输出中。

构建后再次核对 E-U1-001 的全部产品源码哈希，没有变化；目标强制编译/链接完成并记录新的二进制、kernel、符号和镜像 SHA-256。至此建立本次源码到重建资产的对应关系；它不是第三方可复现构建证明，也不涵盖未指定的其他测试工具。host 二进制的若干哈希与开工不同，说明直接使用旧文件的确不能作为版本证据。

U1 未运行 host 测试或 guest；上述退出 0 只代表构建成功。后续 debug CLI 目标还需按依赖构建其两个专用 guest，不能因本单元 host 构建完成就跳过目标依赖。

## 目标与执行预算

以下目标在 `myCPU/` 下运行；本单元核对了命令定义，没有执行这些测试。

| 入口 | 当前 Makefile 行为 | 后续取证限制 |
| --- | --- | --- |
| `test-host-backend_differential_smoke` 等普通 host 目标 | 通用 `test-host-%`；`TEST_TIMEOUT=3s` | U2 先检查实际耗时；超时记超时，有限调整须记录 |
| `test-host-spike_differential_smoke` | 通用入口，不附带参数 | 执行 runner 自测，包含 fake Spike；不能记为真实差分 |
| `test-host-spike_differential` | 传 `--run-differential`；外层 3 秒 | runner 每场景内层 5000 ms；完整运行前需协调有限外层预算；当前环境阻断 |
| `test-host-debug_cli_smoke` | 构建 simulator、两个 guest 和测试；外层 30 秒 | 不等于完整观察副作用配对实验 |
| `test-host-debug_protocol_command_smoke` / `test-host-execution_profile_smoke` | 通用入口，外层 3 秒 | 验证协议和 profile 的既有断言；不能外推 Q3 |
| `test-host-run_debug_cli_probe` | 构建 simulator 与 xv6，然后运行 Python 文件；该 recipe 无外层 timeout | U3 应另外设置有限墙钟预算；外部 skip 单列 |
| `test-host-xv6_boot_smoke` | 构建 simulator、xv6 和测试；外层 3 秒 | functional 的 5000 步启动 checkpoint，依赖精确符号/profile 基线；不是到 shell 的完整启动 |
| `test-host-xv6_shell_smoke` | 构建 simulator、xv6 和测试；外层 1200 秒 | 源码明确选择 functional；boot 最大 5 亿步，forktest 输出/提示预算分别为 5000 万/2000 万步；不覆盖 pipeline |

共享构建与 guest 测试串行，避免 fs.img 被并发覆盖。既有 [xv6 工具链经验](../solutions/build-errors/xv6-ci-toolchain-baselines-and-budget.md) 的精确布局与有限预算规则适用；不从当前被测输出生成期望、不因超时放宽语义断言。

## Q1–Q6 覆盖清单

这里的“待验证假设”表示尚未得到行为结论；入口存在和源码字段存在不等于能力已验证。

| 问题 | 当前分类 | 源码与现有测试入口 | 后续必须补的证据 |
| --- | --- | --- | --- |
| Q1 后端一致性 | 待验证假设；真实 Spike 为环境阻断 | `isa/instruction_semantics.cpp`、`exec/functional_backend.cpp`、`exec/pipeline_backend_cycle.cpp`、`exec/pipeline_commit_boundary.cpp`；`backend_differential_smoke.cpp`、`pipeline_commit_trace_smoke.cpp`、`spike_differential_smoke.cpp` | 选定规范/子集、逐提交检查点及比较字段；核查异步注入、归一化和排除项；执行后才可判定一致 |
| Q2 隔离基础 | 待验证假设 | `mem/address_space.cpp`、`mem/bus.cpp`、`isa/execution_context.cpp`、`exec/memory_ops.cpp`、`exec/system_ops.cpp`；`address_space_faults_smoke.cpp`、`atomic_semantics_smoke.cpp`、`pipeline_speculation_contracts_smoke.cpp` | 正常/权限拒绝、异常委托/sret、映射变更/sfence.vma、store/MMIO 副作用；逐项核查现有断言不足之处 |
| Q3 观察副作用 | 待验证假设 | `debug/debug_session.cpp`、`debug/debug_protocol_command.cpp`、`mem/address_space.cpp`、`mem/bus.cpp`、`devices/uart16550.cpp`、`devices/plic.cpp`；debug CLI/protocol/profile smoke 与 `run_debug_cli_probe_test.py` | 无观察/开启观察/重复查询配对；用独立观察确认 UART、PLIC、A/D、TLB、统计是否变化 |
| Q4 事件关联 | 待验证假设 | `debug/debug_snapshot.h`、`exec/execution_profile.*`、`exec/pipeline_sequence.*`；`execution_profile_smoke.cpp`、`pipeline_commit_trace_smoke.cpp`；xv6 shell | 一次真实访存样例及 VA/PA/地址空间/PC/结果/提交属性；记录源、关联、窗口与丢失信号；fork 时刻的父子映射 |
| Q5 职责边界 | 待验证假设 | `platform/machine.cpp`、debug 协议；`frontend/server/debug_cli_session.mjs`、`frontend/server/debug_server_runtime.mjs`；xv6 `kernel/proc.c`、`kernel/vm.c` | 真实跨层调用链、OS 结构/符号依赖、前端消费路径；不能仅按文件大小或存在源码判定耦合 |
| Q6 时间口径 | 待验证假设 | `arch/core_state.cpp`、`exec/functional_backend.cpp`、`exec/pipeline_backend_cycle.cpp`、`devices/clint.cpp`、profile 与 debug 快照 | 单位/推进点/所有者/客体可见性/后端差异/查询影响；设备 tick 与 step 的实际关系 |

上表的 CPU 源码路径均相对 `myCPU/src/`；host 测试相对 `myCPU/tests/host/`；xv6 源码相对 `myCPU/external/xv6-riscv/`。只是取证路线，不是声明所有列出的测试已充分覆盖问题。F-ID 暂为空：本单元未复现产品缺陷，也没有把未运行的实验命名为缺陷。

## 历史 CI 可复用范围

current 和 solution 记录 `f69ea75` 的日常 `36797856401/1` 与完整 `36797868411/1`，后者含 shell/forktest。U1 没有回读远端 run 或 artifact；这些是项目已有记录，不是本轮新验证。E-U1-004 的实际 Git 差异显示，之后改动了 UART、virtio MMIO、pipeline cycle/hazards/LSQ、AI 设备及前端和 Python helper。

因此，旧完整回归只用于寻找入口、工具链基线及预算；不能直接给当前 Q1–Q6 或普通 fork 场景判通过。即使 xv6 vendored 源码和 CI 配置未变，模拟器执行路径已变也必须核对影响。current 所述最新 main 日常 CI `36884322845/1` 同样未在本单元重新核验，不能替代观察配对和父子映射证据。

## 下一步与未验证边界

U1 提交前常规审查见 [审查记录](system-software-evidence-u1-review.md)，correctness、project-standards、learnings 三项均无待处理发现。纯报告和生成证据没有可精简产品代码；未派发精简视角，也未因零发现派发 validator。

下一单元为 U2：固定所选规范版本与子集，在已重建工具上核查比较器及针对性正常/负向路径。真实 Spike 当前不可用，不安装或替换工具链；其他实验继续。U3/U4 再补观察配对、时间表和普通 fork 时间线；U5 汇总最终分类、职责图和建议。

本轮尚无一致性通过结论、观察无副作用结论、完整访存事件或 fork 内存隔离证据；不据此提出产品修复或重构。构建成功也不代表上述测试通过。原始二进制/镜像不进 Git；Git 内保留构建文本证据和哈希，后续按命令重建，在需要复核旧二进制时另做可取得的归档。
