# 系统软件可信度修复回传

日期：2026-10-02。依据 [第二份计划](../plans/2026-10-02-1519-fix-system-software-trust-plan.md)，在 `codex/system-software-evidence-plan` 从 U1 顺序交付到 U4；开工 HEAD 为 `59c0a4e348820b25ea8a8933565efed5ed889520`。未推送、创建 PR、合并或发布 Wiki。

## 交付与问题对应

| 单元 | 提交 | 问题 | 行为与证据 |
| --- | --- | --- | --- |
| U1 | `36f41f5` | F-U3-002 | A/D 写回先比较当前 PTE；失配后丢弃缓存翻译、重走页表和权限检查，不使用旧 PA。 |
| U2 | `999832c` | F-U3-001 | 有效非 x0 GPR setter 从架构 PC 同步后端，清空在途工作、回滚重命名和物理寄存器视图，保留提交历史与设备。 |
| U3 | `111c3ef` | F-U2-001 及四项 Sv39 夹具失败 | 分离 S handler 与 M 退出区；五个失败场景保持原委托、异常和 S 返回语义，真实 Spike 十项均匹配。 |
| U4 | 本报告所在提交 | R4/R5 | 组合验证、统一审查与本地 Wiki 契约记录。 |

### U1 的负向证据与边界

新增回归在原实现因 `A/D update must preserve replacement PPN` 失败，见 [Red 日志](system-software-trust-fixes/trust-u1-red.log)。实现后 [V1 日志](system-software-trust-fixes/trust-u1-green.log) 与 [补充读取失败回归](system-software-trust-fixes/trust-u1-final.log) 退出 0。

回归覆盖无 A 更新的预查询后 A→B、A=1/D=0 缓存后 A→B store、撤销写权限、无效 PTE、load 只置 A，以及 cached A/D 检查读取失败的三种 access fault；原只读页表测试继续覆盖写回失败。替换成功时 B 的 PPN 和权限保持，B 收到新 store，A 不变，flush 后仍读到 B；重查失败时 cause=15、tval 为原 VA，PTE 与用户数据保持。

完整 V1 命令：

```sh
make -C myCPU test-host-address_space_faults_smoke test-host-backend_differential_smoke \
  test-sv39_tlb_ad_bits test-pipeline-sv39_tlb_ad_bits \
  test-sv39_tlb_flush test-pipeline-sv39_tlb_flush
```

依据 [Privileged 20240411 §10.3.2](https://docs.riscv.org/reference/isa/v20240411/_attachments/riscv-privileged.pdf) step 7，对当前 PTE 比较失配需重新翻译。当前单 hart、同线程同步 Bus 中比较和写回间没有 guest 推进；没有新增宿主并发或跨 hart 原子性承诺。没有 A/D 更新且未 fence 的陈旧 TLB 控制组保持旧行为。

### U2 的负向证据与边界

新增暂停修改测试在原 pipeline 上因 `resumed execution must use edited GPR at committed PC` 失败，见 [Red 日志](system-software-trust-fixes/trust-u2-red.log)。[修复回归](system-software-trust-fixes/trust-u2-green.log) 与 [V2 八项目标](system-software-trust-fixes/trust-u2-v2.log) 退出 0；debug CLI 使用既有 30 秒预算。

`machine_loader_reset` 在两后端 0–23 个已推进 cycle 的暂停点修改 x5，并验证别名、重复修改、x0、非法名称、未加载镜像与 JSON 协议。setter 前后 PC/cycle/instret/mtime、Bus 访问记录、UART 输出、RAM、退休历史长度、累计退休/访存/trap 数与序列分配器保持。pipeline 的在途槽位、ROB、LSQ 清空；恢复后的 RAM/UART 值取决于暂停前是否已退休相关计算，UART 两次提交恰好各发生一次，总退休数为 9，历史序列递增。

完整 V2 命令：

```sh
make -C myCPU test-unit-machine_loader_reset test-host-debug_protocol_command_smoke \
  test-host-pipeline_core_state_smoke test-host-pipeline_rename_commit_smoke \
  test-host-pipeline_speculation_contracts_smoke test-host-pipeline_commit_trace_smoke \
  test-host-execution_profile_smoke test-host-debug_cli_smoke
```

Machine 的同一入口服务 DebugSession、协议和启动初始化；functional 钩子无须额外同步。未重建 backend，未清除 profile 或历史，未改变普通 guest GPR 写入。该承诺仅覆盖 GPR setter；已发生的推测 cache/TLB/A 位效果不逆转，后续模型周期可能因调试干预而改变。

### U3 的逐场景诊断与参考边界

修复前完整夹具自测因委托场景不停止而失败，见 [日志](system-software-trust-fixes/trust-u3-red.log)。对五项分别运行 20 步诊断，见 [轨迹](system-software-trust-fixes/trust-u3-before-trace.log) 和 [可重建诊断源码](system-software-trust-fixes/before-fixture-trace.cpp)。轨迹在 U2 代码 `999832c` 上生成；诊断源码经格式整理后保存，不宣称与原临时源字节相同。构建使用 `g++ -O2 -std=c++17`，链接 `myCPU/src/*.o` 和 `myCPU/src/*/*.o` 中除 `src/main.o` 外的 core objects。此历史诊断只对原夹具有意义。

| 场景 | 修复前首异常 | 原阻断位置 | 修复后 |
| --- | --- | --- | --- |
| delegated_user_ecall_to_supervisor | cause=8，epc=0x80000000，tval=0 | S 返回后 0x8000000c ECALL，M cause=9 错入 S handler | 首异常保持，S 返回到 0x80000008，独立 M 退出 |
| sv39_instruction_page_fault | cause=12，epc/tval=0x80001000 | 同一 S ECALL；随后错误 SRET 到 U，继续异常 | 首异常保持，独立 M 退出 |
| sv39_load_page_fault | cause=13，epc=0x80000000，tval=0xc0000000 | 同一 S ECALL；随后错误 SRET 到 U | 首异常保持，独立 M 退出 |
| sv39_store_page_fault | cause=15，epc=0x80000000，tval=0xc0001000 | 同一 S ECALL；随后错误 SRET 到 U | 首异常保持，独立 M 退出 |
| sv39_reserved_non_leaf_fault | cause=13，epc=0x80000000，tval=0x80400000 | 同一 S ECALL；随后错误 SRET 到 U | 首异常保持，独立 M 退出 |

这五项各自诊断后确认相同退出问题，没有据首项结果推断其余通过。未修改生产 S/U ECALL 语义；myCPU 实际执行 S ECALL → M trap → M 退出 ECALL。Spike 保留 S ECALL，仅将最后的 M 退出 ECALL 替换为单步 `jal x0,+4`，到 0x800000c8 停止。双方最终特权级均为 M；MCAUSE=9、MEPC=0x8000000c、MTVAL=0，SCAUSE/STVAL 保留首异常，SEPC 为返回点 0x80000008。独立首异常检查点位于 S handler 的 0x80000080，直接比较原 cause/epc/tval/privilege。

原比较器仍比较 PC、特权级、32 个 GPR、20 个 CSR（沿用既有 mstatus/sstatus 掩码）、所有声明的内存 watch。这五项去掉了旧 MPIE/MEPC 排除；返回场景使用首异常 summary，不以最终退出覆盖原异常。instret 沿用原排除，因为 Spike M bootstrap 退休数不等于 myCPU 直接初始化；未扩大其他 mask，未屏蔽任一失败场景。

真实参考：`/home/liangjiaqi/.local/riscv-spike/bin/spike`，版本 `1.1.1-dev`，SHA-256 为 `24c0d01da6a0720e3a95bd2c2d2ae1d987381993e59a4f2362d7abaa72f2d085`。ISA 为 `rv64imac_zicsr_zicntr`，单 hart。逐项命令：

```sh
SPIKE_DIFF_KEEP_TEMPS=1 SPIKE_PATH=/home/liangjiaqi/.local/riscv-spike/bin/spike \
python3 docs/reviews/system-software-trust-fixes/run-validation.py \
  --timeout 12 --cwd myCPU spike-final-SCENARIO -- \
  ./tests/host/spike_differential_smoke --run-differential SCENARIO --evidence
```

| 场景 | myCPU 完成 | Spike 启动/退出 | 比较 | 证据前缀 |
| --- | --- | --- | --- | --- |
| alu_mem_csr | 是 | 是 / 0 | 匹配，含 data watch | `spike-final-alu_mem_csr` |
| control_flow | 是 | 是 / 0 | 匹配 | `spike-final-control_flow` |
| predictable_branch_loop | 是 | 是 / 0 | 匹配 | `spike-final-predictable_branch_loop` |
| trap_return | 是 | 是 / 0 | 匹配，含首异常 | `spike-final-trap_return` |
| illegal_trap | 是 | 是 / 0 | 匹配，含异常 | `spike-final-illegal_trap` |
| delegated_user_ecall_to_supervisor | 是 | 是 / 0 | 匹配，含首异常与最终 M CSR | `spike-final-delegated_user_ecall_to_supervisor` |
| sv39_instruction_page_fault | 是 | 是 / 0 | 匹配，含首异常与最终 M CSR | `spike-final-sv39_instruction_page_fault` |
| sv39_load_page_fault | 是 | 是 / 0 | 匹配，含首异常与最终 M CSR | `spike-final-sv39_load_page_fault` |
| sv39_store_page_fault | 是 | 是 / 0 | 匹配，含首异常与最终 M CSR | `spike-final-sv39_store_page_fault` |
| sv39_reserved_non_leaf_fault | 是 | 是 / 0 | 匹配，含首异常与最终 M CSR | `spike-final-sv39_reserved_non_leaf_fault` |

每项 `.json` 保存 checkout SHA、源码指纹、有限预算、argv、退出码、耗时、ELF 哈希、debug 脚本及其哈希；`.stdout` 保留真实 Spike 快照。第二次执行用于补齐组合 SHA 和生成 ELF/脚本指纹，未扩大 guest 步数（各场景保持 64/128，参考进程 5000 ms，外层 12 秒）。二进制留在 `/tmp`，不提交。统一审查补齐 Make 依赖后实际重建工具，十项又分别启动、退出 0、匹配，最终工具证据为 `spike-rebuild-SCENARIO`；其 JSON 明确记录仅 Makefile 的未提交差异，不冒充无差异 checkout。

工具自测完整通过，见 [V3 日志](system-software-trust-fixes/trust-u3-final.log)，包含错误输出、非零退出、超时、缺工具的失败路径及五项独立异常断言。`ci_targets_test.py` 四项通过，见 [日志](system-software-trust-fixes/trust-u3-ci-targets.log)。默认 `test-ci-core` 注册 A/D 与 Spike 工具自测，真实 Spike 仍须显式运行。

## U4 组合验收

组合代码 SHA：`111c3efc3aa18cfbc7f86b8c21795226eb58f809`，运行时无未提交产品差异。host GCC 11.4.0、RISC-V GCC 10.2.0。历史云端通过记录不作为本次修复验收。

| 入口 | 当前结果 | 证据 |
| --- | --- | --- |
| `make -C myCPU test-ci-core` | 61 unit、22 host 入口通过，退出 0，93.105 秒 | `ci-core.json/.stdout/.stderr` |
| `make -C myCPU test-host-run_debug_cli_probe` | 101 项中 92 通过、9 外部 skip，退出 0，13.128 秒 | `python-probe.json/.stdout/.stderr` |
| xv6 boot + 完整 shell | 退出 0，16 条命令全通过，含 forktest/stressfs，842.965 秒 | `xv6.json/.stdout/.stderr` |
| 独立 fork 构建与执行 | 构建退出 0，3.919 秒；执行退出 0，492.737 秒 | `fork-build`、`fork` 证据 |

Python 外部 skip 不计通过。xv6 shell 沿用 Make 的 1200 秒上限，boot+构建+shell 外层为 1320 秒；独立 fork 外层及探针内部均为 1200 秒。按 [原构建脚本](system-software-evidence-c253dda/u4-build.sh) 重建隔离 fs，沿用 `XV6_EVIDENCE_MARCH=rv64ima`，不改变普通 fork 语义断言。Python probe 与 xv6 实验串行。

9 项 skip（按 unittest 的实际发现顺序与进度输出逐项配对）：

- `test_linux_distribution_curated_alpine_busybox_userland_abi_view`
- `test_linux_distribution_curated_alpine_fp_static_surface_view`
- `test_linux_distribution_runtime_curated_alpine_auxv_hwcap_view`
- `test_linux_distribution_runtime_curated_alpine_boot_log_reports_compressed_isa`
- `test_linux_distribution_runtime_curated_alpine_busybox_awk_fp_matrix`
- `test_linux_distribution_runtime_curated_alpine_fcsr_syscall_roundtrip_probe`
- `test_linux_distribution_runtime_curated_alpine_proc_cpuinfo_isa_view`
- `test_linux_distribution_runtime_reaches_shell_prompt_and_command_when_requested`
- `test_linux_proto_block_mode_runtime_reaches_fourth_stage_when_requested`

这些外部 runtime/curated rootfs 场景未启用，不表示真实 Linux 验收成功。工具完整版本和哈希见 `environment-final.json`；`environment-during-shell.json` 中默认 xv6 ELF/fs 指纹采集于 shell 运行期间，不冒充启动前指纹；独立 fork 的输入和输出指纹由 runner 在调用前后分别保存。

独立探针在 functional/单 hart 上确认：父 PID=3、子 PID=4 的 VA=0x2000/0x3000 初值继承，两页分别映射不同 PA；观察到子进程实际 U 态 store 后，父页保持原值，子页变为 0xaabbccdd/0xeeff0011。子进程 zombie 的 xstate=37，wait 成功，两页重新进入 freelist、proc 恢复 unused；同次启动的原 forktest 也通过。探针内部耗时 476.728 秒，外层含装载等为 492.737 秒；没有人工补判。它证明普通 fork 的两页行为，不外推所有页面或 COW。

## Wiki 与统一审查

Wiki 路径：`/home/liangjiaqi/projects/my_visual_CPU.wiki`。维护前 `git pull --ff-only` 已拉取到 `1c902fd04bcf3098f25a33640f9d41780964df5b`。Memory-System、Debug-and-Lab、Pipeline 的契约修改已审查并本地提交 `d1244c7`，关联 U1/U2 代码与验证；工作区干净，未推送或发布。U3 是测试夹具变化，无新增生产 ISA 契约。

统一审查范围为 `59c0a4e` → `111c3ef` 的 15 个产品/测试文件、本报告与三份证据工具，以及上述三页 Wiki；审查后唯一代码改动是 Make prerequisite。常规路径通过三个只读子代理分组执行 correctness/API-contract/reliability、testing/adversarial/project-standards、simplify reuse/quality/efficiency 与 maintainability/performance/security。独立新 validator 核实保留发现；没有不合格或低置信度条目，三个来源按根因去重为一项。概要：待修 0 / 已修复 1 / 遗留-转 Issue 0。

### [R1-F01] P2 · 已修复 · 默认 Spike 门禁缺少夹具头文件依赖

- **位置**：`myCPU/Makefile:1749`（被审快照）。
- **来源**：correctness、testing/adversarial、simplify/maintainability（置信度 75）；独立 validator 确认，置信度 100。
- **问题**：增量构建已有工具后，只改夹具头文件会继续运行旧二进制，默认门禁可能误报通过。通用依赖遗漏原已存在，本次修改这些头文件并将工具加入默认门禁，使它直接影响交付可信度；干净 CI 构建不受此触发影响。
- **建议与修复**：沿用 terminal harness 的显式 prerequisite 模式，为 Spike 工具添加 `$(wildcard tests/host/spike_differential/*.h)`，保持原编译 recipe。
- **证据**：`myCPU/Makefile:1749 -- tests/host/%: tests/host/%.cpp $(CORE_OBJS)`；`build-dependency-red.stdout` 中 header `-W` 仍显示 up to date，CPP 控制会重建。修复后 `build-dependency-green.stdout` 逐一断言五个头文件都输出 g++；`dependency-fix-self-test.stdout` 记录实际重建及完整自测退出 0；`spike-rebuild-*` 保留十项真实匹配。新 validator 再次独立执行 header `-W` 并确认 resolved，编译 recipe 保持；修复随本报告 U4 提交。重建后的工具与环境指纹见 `environment-after-review-fix.json`。

覆盖限制：未触发 migration、PR previous-comments、frontend/iOS 专属角色；没有匹配本次修复机制的 solution 需要额外 researcher。fresh page-walk 在首次读取与比较之间被设备改变 PTE 的 Retry 分支，经代码审查但没有动态设备定向回归；缓存失配和读取失败有负向实测。这是一项测试缺口，未发现对应产品缺陷。云端/前端未运行，9 项 Python 外部 skip 不计通过。外来 current 补记、`2026-10-01-pr14-17-pre-merge.md` 修改及 `2026-10-01-remote-pr-pre-merge.md` 排除并保留，不纳入本轮提交。

## 后续边界

本轮不交付纯查询 API、事件关联/持久化、PID 自动解释、前端时间线、COW、多 hart、Linux/OSComp 或完整调试平台。组合验收已通过，具备进入另行规划的观察能力建设的基础；未证明原两项产品缺陷会阻断普通 xv6 fork。云端验收与前端新版本测试未运行；后续获准推送并提出 PR 时，须在对应 checkout SHA 上通过 simulator-core/frontend，不复用旧云端状态替代。
