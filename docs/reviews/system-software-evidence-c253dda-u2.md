# U2：行为一致性与副作用边界

## 范围与依据

实际 checkout 为 `1b54928fc0f1d2c7e39d2c9c781be4e9a43babc7`；与 U1 的产品源码总指纹相同。环境和工具链继承 [U1](system-software-evidence-c253dda.md)，实际运行的命令、工作目录、有限墙钟预算、退出码、输出和资产哈希见下方 JSON。没有改产品、原有测试、协议、CI 或 Wiki；仅新增两项独立 host 证据夹具。

本轮选定规则依据为官方归档 [Unprivileged 20191213](https://docs.riscv.org/reference/isa/v20191213/_attachments/riscv-unprivileged.pdf) 的 RV64 整数访存、CSR、选定 A/C 指令规则，以及 [Privileged 20240411](https://docs.riscv.org/reference/isa/v20240411/_attachments/riscv-privileged.pdf) 的 M/S 1.13 中 §3.1.8、§3.3.2、§10.2.1、§10.3–10.4。只核查所列场景，不声明项目完整符合 1.13，不混入 V-lite、AI MMIO 或 DBT 自定义规则。

输入为单 hart、固定 C++ 夹具或汇编，无宿主随机注入。普通 RAM 默认 128 MiB；各既有夹具的 CSR、PTE、设备和停止预算由其源码初始化，未把所有测试假定为同一完整 board。新权限夹具在 U 模式使用 VA `0x4000` 代码、VA `0x6000` 数据、固定物理页和预设 A/D，单场景最多 128 后端 step。停止通过独立 M trap handler 的项目 `a7=93` 退出约定；这个约定不是标准 ISA 的 ECALL 功能。

## 运行证据

| E-ID | 命令或对象 | 实际结果 | 覆盖与限制 |
| --- | --- | --- | --- |
| E-U2-001 | `timeout 3s ./tests/host/backend_differential_smoke` | 通过，退出 0 | 26 场景的内部架构事件比较；范围见下一节 |
| E-U2-002 | `address_space_faults_smoke`（同上 3 秒） | 通过，退出 0 | result/legacy fault、权限、TLB、页表/A-D 总线失败断言 |
| E-U2-003 | `atomic_semantics_smoke` | 通过，退出 0 | 主要为旧 cpu_step 的 LR/SC/AMO；不等于 pipeline 全原子覆盖 |
| E-U2-004 | `pipeline_commit_trace_smoke` | 通过，退出 0 | 退休序列、wrong-path 清除、trap/mret 记录 |
| E-U2-005 | `pipeline_speculation_contracts_smoke` | 通过，退出 0 | ready speculative GPR 不提前可见、RAM/MMIO commit/replay/fault 路径 |
| E-U2-006 | `spike_differential_smoke` 无参数 | 失败，退出 1 | 既有工具自测：`expected delegated_user_ecall_to_supervisor to halt`；未继续到后续所有自测 |
| E-U2-007 | 首次新权限夹具构建 | 失败，Make 退出 2 | 本轮夹具缺少 Bus 头文件；保留编译器输出，补头文件后 E-U2-011 重建 |
| E-U2-011 | 权限夹具、`sv39_tlb_flush.elf` 构建 | 通过，退出 0 | 60 秒上限；完整参数已保存 |
| E-U2-012 | 新权限/跨页夹具 | 通过，退出 0 | 两后端各 RW、只读、无映射三场景；额外 direct AddressSpace 跨页边界 |
| E-U2-013 / 014 | `make test-sv39_tlb_flush` / `make test-pipeline-sv39_tlb_flush` | 均通过，输出 `TLB` | 单 hart、固定 guest；外层 15 秒，Make 内部 3 秒 |
| E-U2-015 / 016 | Spike 失败夹具探针构建/执行 | 均通过，退出 0 | 复现原 runner 超时，再用独立 M 退出 handler 控制组隔离原因；没有修原夹具 |
| E-U2-017 | 初始默认 Spike 入口 | 当时未运行 | PATH 未找到；随后用户提示本机已安装，使用下列绝对路径解除环境阻断 |
| E-U2-020–024 | 真实 Spike：alu_mem_csr、control_flow、predictable_branch_loop、trap_return、illegal_trap | 5 项匹配，退出 0 | functional 与独立参考的限定字段比较；每项外层 12 秒，内层 Spike 5 秒 |
| E-U2-025–029 | delegated_user_ecall_to_supervisor 及四项 Sv39 fault | 5 项失败，退出 1 | 均为 `myCPU exceeded step budget`，发生在该场景调用 Spike 前；不能记为参考分歧或参考通过 |

原始记录：[第一批与首次构建失败](system-software-evidence-c253dda/u2-runs.json)、[权限与 sfence 重跑](system-software-evidence-c253dda/u2-permissions-runs.json)、[Spike 夹具诊断](system-software-evidence-c253dda/u2-spike-fixture-runs.json)、[真实 Spike](system-software-evidence-c253dda/u2-real-spike-runs.json)。这些文件含顺序执行的采集源码 argv，不仅是工具终端摘要。U1 源码哈希清单与本轮已跟踪源码对照无变化；新夹具源文件与其 SHA-256 单列。客体机器码嵌于源文件，另一个汇编 guest 保存 ELF 哈希与编译命令。

真实参考为 `/home/liangjiaqi/.local/riscv-spike/bin/spike`，`--help` 标识 `Spike RISC-V ISA Simulator 1.1.1-dev`，SHA-256 为 `24c0d01da6a0720e3a95bd2c2d2ae1d987381993e59a4f2362d7abaa72f2d085`；`--version` 不受支持，退出 1。没有修改 PATH。每项显式设置 `SPIKE_PATH`，原 runner 使用 `--isa=rv64imac_zicsr_zicntr`、固定 PC 和内存区域、debug command 文件；默认单 hart、MSU，无本项目平台外设。10 项分别调用以免首项失败遮蔽后续结果，其中只有前 5 项完成真实对照。

## 实际执行路径与比较口径

functional：`functional_backend.cpp:179` 先 tick/处理 interrupt，fetch16/fetch32 result，decode，`InstructionSemantics::execute`，`apply_commit_boundary`，记录 profile，再加 cycle。pipeline：EX 在 `pipeline_backend_execute.cpp:307` 把重命名/forwarding 寄存器及投影 CSR 作为 SemanticInputs 交共享语义；RAM load 在 MEM，store 在 MEM 只标 ready；`pipeline_backend_cycle.cpp:195–219` 只由 ready ROB head 调统一提交边界。

`pipeline_commit_boundary.cpp:42–44` 先处理已知 trap；`:65–75` store 成功后再写 CSR/GPR；`:120–137` 处理 TLB flush 与 mret/sret；`:144–146` 成功退休才加 instret。错误处理并非所有内部状态回滚：页表 A/D、RAM speculative load/cache/TLB 等另有模型边界。MMIO load 等 ROB head，RAM/MMIO store 均在提交边界访问设备；E-U2-005 实证局部 wrong-path/replay 路径，而非任意错误访问的绝对无副作用证明。

旧 `cpu_step`（`cpu.cpp:35–37`）也用共享 semantics/commit，tick/fetch/观察包装独立。DBT prototype 有共享路径及 functional fallback；`dbt_helper_execution_bridge.cpp:167–209` 的 scalar helper 直接访存并写 GPR/PC/instret，没有调用统一 commit。这里只登记旁路及验证范围，未验收 DBT/JIT、未把旁路本身判缺陷。

`backend_differential_smoke.cpp:23–32,135–184` 每个架构事件比较 PC、instret、halt、privilege、32 GPR 和 20 个 tracked CSR。RAM watches 仅最终比较（`:285–292`），不排除中间短暂错误。事件由 instret/PC/halt/privilege/CSR 改变判定；不直接比较 FPR/vector、全内存、设备、cycle、TLB/cache。pipeline 独立 interrupt-entry 被剔除（`:229–235`），因为 functional 同一步进入中断并执行 handler 首条；这是事件粒度归一化，不是按相同步数对齐。

外部中断场景在执行前固定 MIP/SIP/使能/委托或 UART/PLIC fixture，并未引入同 step 数调度要求。返回型异常与中断归一化应与原 trace 一起复核，不能外推成任意外设负载的相同调度。

真实 Spike 比较器在 `spike_differential_smoke.cpp:508–527` 对非 M 初态清双方 MPIE/MEPC，排除 instret，返回型 trap 改用 first_trap_summary；`state_compare.h:143–151` 将 MSTATUS/SSTATUS mask 到选定权限/中断位，FS/SD 等不在比较范围。runner 不支持平台设备/configure hooks。5 项匹配只能支持这些场景与比较字段，不证明被排除字段或全部 ISA 一致。

## 一致性与副作用矩阵

| 场景 | functional | pipeline | 结论及边界 |
| --- | --- | --- | --- |
| 正常 load/store、控制流、选定 CSR | E-U2-001 | E-U2-001 | 规定架构事件内部一致；RAM 只在最终检查 |
| U ECALL 委托、sret/mret、Sv39 fault | E-U2-001 | E-U2-001 | 既有配对断言通过；不把原 Spike 夹具失败当该测试失败 |
| U RW store | 写入 `0x1122334455667788`，x6=6，cause=8 | 同值、同 x6、同 cause | 数据提交成功；后续 U ECALL 合法 trap；退休数均 4，cycle 分别 5/12 |
| U 只读页 store | cause=15，tval=`0x6000`，数据/x6=0 | 相同 | 拒绝写入及 younger GPR 不提交；只代表这条对齐 SD 和当前夹具 |
| U 无映射页 store | cause=15，tval=`0x6000`，数据/x6=0 | 相同 | 相同局部拒绝证据；只读/无映射场景退休数均 2，cycle 为 3/10 |
| 改叶 PTE 后 sfence.vma | `TLB` | `TLB` | 源程序先读旧页、改 PTE、未 fence 读旧页、fence 后新页；全刷是允许 over-fence |
| wrong-path/replay store/MMIO | 未作同一完整场景配对 | E-U2-005 | pipeline 的具体前后断言通过；不能扩成全部设备原子性 |
| 非对齐跨页 SD direct API | 前 4 字节 `0x55667788` 可见，后页 fault=15/tval=`0x7000` | 未运行该路径后端配对 | 已证实实现边界；规范 §10.3.1 允许部分 misaligned store 可见，未定性为缺陷 |
| 真实 Spike | 5 场景匹配，另 5 项 myCPU 步数耗尽 | 不适用 | 有限定外部参考证据；尚无 Sv39 fault/委托场景真实对照通过结论 |

改 PTE 未执行匹配 fence 时可使用旧或新翻译；E-U2-013/014 的“未 fence 仍旧值”属于此实现缓存行为，不能要求所有合规实现都如此。A/D 是否更新与访问类型、规范方案和实际执行路径有关，不能要求所有失败访存均不改变页表。

## F-U2-001：既有 Spike runner 自测的退出夹具不匹配

- 分类：已复现缺陷；归属为验证夹具，非已证实模拟器 ISA 缺陷；置信度高。
- 触发：运行原 `spike_differential_smoke` 自测的 `delegated_user_ecall_to_supervisor`。
- 预期/实际：原断言要求 halt 且最终 S 模式；原 runner 128 步耗尽，第一异常为 U ECALL cause 8，随后 S ECALL cause 9，陷入返回循环。
- 首个分歧：S handler 设 SPP=S 并 sret 后在 S 模式执行 `a7=93; ecall`；`system_ops.cpp:167` 只有 M 模式才触发项目退出约定，标准 S ECALL 进入 M trap。原 runner 的 MTVEC/STVEC 共用返回 handler（`spike_differential/mycpu_runner.h:137`），因此又回到退出指令。
- 最小复现：[独立夹具](../../myCPU/tests/host/system_software_spike_fixture_evidence.cpp) 复用原场景，输出 20 步轨迹及原 runner final。控制组只增加独立 M exit handler，保留原 U→S 委托和 guest，9 步 halt、最终 M 模式、首异常仍 cause 8。
- 影响：原工具自测在首个失败处停止，后续自测未覆盖；真实差分的同场景也不能据此作为验收依据。日常 CI 没有运行独立 Spike 目标，不代表云端 CI 已捕获该问题。
- 建议：后续单独调整 reference fixture 的停止约定和相应断言，保持 S ECALL 语义；再运行完整工具自测与真实 Spike。未创建 Issue，未修原测试或产品。

## 单元结论

Q1/Q2 的以上局部正常与负向路径有实际证据，未发现目标普通 fork 必须先修复的确定产品语义阻断；这不是全 ISA、全部异步行为或全后端正确性证明。真实参考环境已可用，5 项完成匹配，另 5 项因 myCPU 夹具预算失败而未到达参考执行；后四项是否与 F-U2-001 同因尚未验证。下一步 U3 检查观察副作用、关联字段/窗口和时间口径；最终建议由 U5 汇总。
