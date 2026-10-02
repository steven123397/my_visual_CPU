# 可信观察与最小访存采集回传

## 范围与提交

本轮按 [第三份计划](../plans/2026-10-02-1632-feat-trusted-observation-capture-plan.md) 在 `codex/system-software-evidence-plan` 顺序实施。基线为 `6ad5a15a4793aeafb5257b5afa2bb429dd5a238c`，不包含前两份计划的实现，也不接管既有 PR 审查补记。未推送、创建 PR、合并或发布 Wiki。

| 单元 | 本地提交 | 交付 |
| --- | --- | --- |
| U1 | `e71f90d` | const RAM 查询，Machine/DebugSession 入口 |
| U2 | `52aa267` | Bare/Sv39 纯页表解释，共享权限判定 |
| U3 | `87a5d65` | functional 实际标量、LR/SC/AMO 分段记录 |
| U4 | `a74e083` | 有界缓冲、过滤、保留、分页与修改拒绝 |
| U5 | `9f6bd73` | 六个 JSON 命令族、无损字段、真实 CLI 验证 |
| U6 | 本报告所在提交（`git log --grep='(U6)'`） | fork 探针迁移、组合验收、审查修复与本报告 |

## 接口与边界

`Machine` / `DebugSession` 提供 `inspect_memory`、`inspect_page`、`capture_start/stop/clear/read` 和 `capture_metadata`。类型定义在 `myCPU/src/mem/address_space.h`、`myCPU/src/debug/memory_capture.h`。设备状态继续用现有类型化快照；没有通用 MMIO peek。旧 helper/profile/planner 不因此取得纯观察保证。

RAM 请求整段限制为 1 至 4096 字节；解释页表不查 TLB、不填 TLB、不写 A/D、不注入 trap。当前上下文考虑 MPRV，显式 SATP 必须同时给有效特权上下文。实际记录来自 AddressSpace/L1D 与 atomic 当次访问，结束时补退休、异常与模型计数，profile 补充观察前解除绑定。

采集仅 functional、单 hart。容量默认 4096，上限 65536；满后保留前缀、丢弃新记录并计数，客体继续执行。过滤为 operation、有效特权、精确 SATP、VA 半开区间相交的 AND。FP/vector 数据访问记 unsupported；取指、page-walk、DMA、pipeline speculation 明确排除。元数据不将 dropped、filtered、unsupported 混为一类；覆盖声明不能外推为全部数据访问或完整退休流。

游标为 capture_id、position、upper；首次读取固定上界，每页最多 256 条，继续运行后可从旧 position 建立新的上界。clear 使旧游标失效，start 必须先清空旧结果。capture_id 进程内不复用，instruction_id 包含过滤的支持尝试。recording 时正式控制入口拒绝 setter/reset/load/payload/input/planner/JIT；停止后恢复，旧记录保留原机器代次。底层可变 CPU/Bus/设备接口仍由 C++ 调用者遵守窗口约束。

新响应 `schema_version=1`；地址、SATP、值、ID、cycle/instret 和统计为十六进制字符串，缺失 PA/值是 null。旧 snapshot/step/run 格式未重写。调用者保存元数据与分页记录为 JSONL，协议不接受宿主输出路径。浏览器服务未增加新 action，公开支持范围为 C++/本地 CLI。

```json
{"cmd":"inspect-memory","addr":"0x80000000","size":8}
{"cmd":"inspect-page","addr":"0x2000","access":"store","satp":"0x8000000000087fff","privilege":"U"}
{"cmd":"capture-start","capacity":16,"operation":"store","privilege":"U","satp":"0x8000000000087fff","va_begin":"0x2000","va_end":"0x3008"}
{"cmd":"capture-stop"}
{"cmd":"capture-read","capture_id":"0x1","position":0,"limit":256}
```

显式 SATP 示例仅说明格式；capture_id 必须使用实际 start 返回值。

## 验证证据

- U1/U2 查询测试、两后端 address-space/A-D/TLB flush 回归通过。
- U3/U4 定向采集、atomic、后端差分、profile、pipeline speculation、旧 debug CLI 回归通过。
- U5 真实 CLI/parser 通过；Python 94 通过、9 外部 skip；前端 172 通过、1 外部 skip。
- U6 最终默认核心回归 61 unit/24 host、4 项 CI 元测试、10 项真实 Spike 匹配通过；最终完整 xv6 shell 全部 16 条命令通过。fork 关闭/开启及自动配对均通过；统一审查五项发现全部修复并独立复核，无待修发现。

原始记录位于 `/tmp/trusted-observation-evidence/`，由历史 `docs/reviews/system-software-trust-fixes/run-validation.py` 复制执行生成；每次含 checkout SHA、产品差异、源码指纹、预算、argv、退出码、耗时、输入资产和日志 SHA-256。[小型文本证据](trusted-observation-capture/) 已归档，构建产物不入库。

最终核心、权限、Python、前端、Spike 与 xv6 shell 均使用源码清单 `sources-1133f408f8f7.json`；其完整 SHA-256 为 `1133f408f8f7ebffb6721ed2f767831382c955c9a358c8e7b25e112ddc9bfb36`，主会话逐项重新计算确认无漂移。运行 checkout 为 U5 `9f6bd7389c44abc45edd5a474402c3bfc2d49097` 加记录内的 U6 产品 diff，不能误称纯 HEAD 的 CI。

| 入口 | 结果 | 归档前缀 |
| --- | --- | --- |
| 默认核心 | 退出 0，61 unit/24 host，15.840 秒 | `core-final` |
| 两后端权限、A/D 与 TLB flush | 退出 0，0.135 秒 | `permission-final` |
| Python CLI probe | 退出 0，94 通过、9 外部 skip，10.788 秒 | `python-probe-final` |
| 前端 | 退出 0，172 通过、1 外部 skip，3.675 秒 | `frontend-final` |
| CI 目标元测试 | 退出 0，4 项通过 | `ci-targets` |
| 实际 Spike | 十项分别启动实际 Spike，全部匹配、退出 0 | `spike-final-SCENARIO` |
| xv6 boot 与完整 shell | 退出 0，620.253 秒，16 条命令全通过 | `xv6-shell-final` |
| fork 关闭采集 | 退出 0，378.179 秒 | `fork-off` |
| fork 开启采集并重复查询 | 退出 0，382.613 秒 | `fork-capture` |

每个前缀都有 `.json/.stdout/.stderr`。环境为 host GCC 11.4、RISC-V GCC 10.2，独立 xv6 资产沿用 rv64ima/lp64；真实 Spike 使用 `/home/liangjiaqi/.local/riscv-spike/bin/spike`。skip 不计通过；未运行本轮云端 CI、真实 Linux/OSComp 或跨工具链组合。早于统一审查修复的日志不作为最终产品通过证据。

### 普通 fork 配对

[自动配对结果](trusted-observation-capture/fork-paired.json) 通过：7 条 mapping/zombie/reclaimed 事件（含 cycle/instret）完全一致，REAP 后 shell prompt 之前的 UART 167 字节完全一致，两次原 forktest 均通过。开启采集宿主耗时比关闭多 4.434 秒；仅各一次运行，不作为稳定性能承诺。

父 PID=3、子 PID=4，子 SATP=`0x8000000000087f54`。容量 16，store AND U AND 精确 child SATP AND VA `[0x2000,0x3008)`；attempted=`0x3a436`、filtered=`0x3a434`、recorded=2、dropped=0、unsupported=0。两条记录 ID=`0x1509/0x150a`，PC=`0xe8/0xfc`；VA=`0x2000/0x3000` 实际写 PA=`0x87f29000/0x87f2a000`，值=`0xaabbccdd/0xeeff0011`，均 retired、无 trap。父页原值不变，子两页已改变；zombie xstate=37、wait 成功、proc unused、两页进入 freelist。仅证明普通 fork 的两页行为，不外推全部页面、COW 或其他后端。

关闭/开启输入前后哈希均一致；两个 run 的探针、kernel、独立 fs、layout、guest ELF 五个资产及最终源码清单完全一致。kernel SHA-256=`1a6cdfcaa2959b68fa64846669dbd80b43846bfcd825c4e8f9c65beb3a4d2eb2`；独立 fs=`459cb48a1e217b201551fd1f7c9c8d064a3489271f67a7672fac0d25693c1220`。layout/guest ELF/探针二进制完整哈希见两份运行 JSON；guest fixture 未改语义。

### 重现入口

```bash
make -C myCPU test-ci-core
python3 myCPU/tests/host/ci_targets_test.py
make -C myCPU test-host-run_debug_cli_probe
npm --prefix frontend test
make -C myCPU test-host-xv6_boot_smoke test-host-xv6_shell_smoke
env XV6_EVIDENCE_MARCH=rv64ima bash docs/reviews/system-software-evidence-c253dda/u4-build.sh /tmp/trusted-observation-fork
make -C myCPU tests/host/system_software_fork_evidence
```

随后在 `myCPU/` 目录分别运行以下两个命令；每次外层与探针均保留 1200 秒预算，不并行运行共享资产：

```bash
timeout 1200s ./tests/host/system_software_fork_evidence external/xv6-riscv/kernel/kernel /tmp/trusted-observation-fork/fs.img /tmp/trusted-observation-fork/layout.txt off
timeout 1200s ./tests/host/system_software_fork_evidence external/xv6-riscv/kernel/kernel /tmp/trusted-observation-fork/fs.img /tmp/trusted-observation-fork/layout.txt capture
```

配对入口为 `python3 docs/reviews/trusted-observation-capture/compare-fork.py <off.stdout> <capture.stdout>`；它解析 JSON 字段，比较映射/OS 事件及 cycle/instret、UART through REAP/shell prompt，并核对正式 metadata 与两条 store。既有 forktest 在两次运行分别验证成功，UART 全文比较截至执行它之前，不宣称比较了其完整文本。

## 统一审查

路径：`nk-review` 常规组合审查，仅在 U1–U6 实现完成后执行。覆盖正确性、测试、对抗、复用、代码质量、效率、API、项目规范、安全、可靠性、可维护性、性能和历史经验；本轮不涉及 agent 集成，agent-native 角色不适用。审查者只读，不提交或改写源码；发现经独立验证后由主会话修复。

第一轮冻结 diff SHA-256 为 `db06ff981eb0959fe7327790fee38a9d832390ab2ea9f206d31d493aadcb17a0`（U1–U5 提交及 U6 fork 源码）。最终修复 diff SHA-256 为 `f20a8b7fe93728f18a4d90940bb7004e76978d5f1e017d5129f38a8db682dc6e`（相对 U5 HEAD，含 U6 与修复）。定向独立复核执行了现有查询/采集测试二进制，五项均关闭，无新增发现或待修项。

| ID | 级别 | 发现与处理 |
| --- | --- | --- |
| R1-F01 | P2 | 失败的 payload reset 提前推进代次。DebugSession 先完成候选机器的完整配置和 replay，成功后才发布共享 capture、代次和机器；失败 load 恢复配置。新增失败 reset/reload 的旧状态和代次保留断言。 |
| R1-F02 | P2 | 关闭采集时仍构造分段参数。scalar 与 atomic 所有分段入口先检查记录指针，关闭时仅走条件分支。 |
| R1-F03 | P2 | 原测试只能证明不刷新旧 TLB，缺少冷 TLB 不填充控制。新增先查询映射 A、改 PTE 为 B、首次实际翻译必须得到 B 的独立控制。 |
| R1-F04 | P2 | 缺少 fault 原特权和下一条 handler 的关联断言。新增 U/Sv39 load fault 委托到 S、下一条 handler load，并比较采集开关后的寄存器、CSR、计数、mtime、Bus latest 与 profile。 |
| R1-F05 | P3 | fork 探针迁移后残留无用 watch 参数。删除 step/until/marker 的转发参数。 |

另外补充压缩 c.ld/c.sd 的 raw/长度验证和 vector unsupported 计数。上述审查修复归入 U6 组合交付，没有另起外部 Issue 或扩展计划范围。

## Wiki 与后续

Wiki 在 `/home/liangjiaqi/projects/my_visual_CPU.wiki`，维护前 `git pull --ff-only` 回读为已最新，已有未发布提交 `d1244c7` 保留。Memory-System 和 Debug-and-Lab 的可信查询、实际记录、生命周期、协议及失败重载契约已本地提交为 `a83dadf`，工作区干净，未发布；提交关联 U1–U5 与 U6 修复 diff 指纹。PID 自动解释与前端时间线尚未实施；本轮事实接口具备后续规划基础，后续建设须另定范围与验收，不因本次完成而自动开工。
