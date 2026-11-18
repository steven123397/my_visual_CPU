# U5：系统软件取证回传

本文汇总 U1–U4 的实际取证结果，供原讨论会话决定后续调整。普通 fork、父子写入隔离与 exit/wait 在本次 functional、单 hart、固定 xv6 版本下成立；生产观察能力仍需小范围补充。报告与证据总入口见 [索引](system-software-evidence-c253dda.md)。

本分支交付提交：U1 `1b54928`、U2 `d164cfc`、U3 `e4ab34b`、U4 `d59e551`；U5 整理基点为 U4 提交，最终自身版本通过 `git log -1 --format='%H %s' -- docs/reviews/system-software-evidence-c253dda-u5.md` 取得。统一审查见 [U5 审查记录](system-software-evidence-u5-review.md)，确定性检查见 [u5-validation.json](system-software-evidence-c253dda/u5-validation.json)。最终提交哈希在交付回复中给出，不在自身内容预填。

## 决策结论

现有证据支持针对边界做小范围调整，没有证据要求重写 CPU、MMU、平台或整个观察系统。五项真实 Spike 场景匹配，内部 26 场景及选定权限/fence 检查通过；这些只能支持所列输入和比较字段。现有短程序可用状态与独立输出人工关联，但生产观察协议不足以直接解释父子进程的页归属及完整因果过程。

三项已复现问题分别归属验证夹具、调试 setter 与 MMU A/D 写回，不混为普通 fork 已被证实阻断。后续修复和能力建设均未在本轮实施。

## Q1–Q6 结论矩阵

| 问题 | 分类与结论 | 证据 | 覆盖限制与建议 |
| --- | --- | --- | --- |
| Q1 后端一致性 | 已证实局部行为；已复现参考夹具缺陷 F-U2-001 | E-U2-001/004/005、020–029；[U2](system-software-evidence-c253dda-u2.md) | 26 场景内部比较、5 项真实参考匹配；另 5 项未调用 Spike。排除字段/事件归一化保留；先修夹具再补 Sv39/委托参考，不称全 ISA 正确 |
| Q2 隔离基础 | 选定权限拒绝/fence 行为已证实；MMU 写回缺陷 F-U3-002 已复现 | E-U2-002/012–014、E-U3-006 | 对齐负向 store 局部无数据/younger GPR 提交；旧翻译允许，旧 PPN 写回不符合当前 PTE 检查。direct API 复现不等于 xv6 已触发 |
| Q3 观察副作用 | 固定 snapshot/step 配对已证实；能力缺口 F-U5-001；setter 缺陷 F-U3-001 | E-U3-006、012/013；[U3](system-software-evidence-c253dda-u3.md) | 两后端独立 RAM/UART 与选定统计吻合；设备 helper 消费状态、false 翻译填 TLB、planner 覆盖 Bus。需明确定义纯查询契约 |
| Q4 事件关联 | 短程序与专用 fork 探针关联成立；生产关联与保留能力缺口 F-U5-002 | E-U3-006、E-U4-003 与字段/窗口走查 | 缺统一 ID、VA/地址空间/翻译路径、完整保留和丢失信号；standalone xv6 证据不能冒充已发布协议能力 |
| Q5 职责边界 | 实际调用链及 OS 解释依赖已核查；OS 适配能力缺口 F-U5-003 | U3 frontend 链、U4 ELF/结构适配走查 | CPU/MMU 产硬件事实，平台负责设备；PID/fork/页归属由 xv6 解释，当前只能专用采集；前端不应推断核心语义 |
| Q6 时间口径 | 已证实为简化模型推进 | E-U3-006、013 与 U3 时间表 | cycle 每后端 step、instret 为退休数、mtime 随 tick；不同后端同退休数不同时间。宿主秒数不是客体性能，无真实频率声明 |

## 问题登记

| F-ID | 分类、归属与置信度 | 首个分歧或缺口 | 最小证据与影响 |
| --- | --- | --- | --- |
| F-U2-001 | 已复现缺陷；reference fixture；高 | S 模式退出 ECALL 进入 M trap，原返回 handler 使其循环 | 原 runner 耗尽 128 步；独立 M handler 控制 9 步停止。影响委托场景验收，其他四项同因尚未确认 |
| F-U3-001 | 已复现缺陷；平台/调试到 pipeline；高 | setter 写 core，却没有同步执行物理寄存器视图 | snapshot x5=41；guest RAM 加法签名 functional=42、pipeline=1。影响调试初始化，不证明正常 guest GPR 写错 |
| F-U3-002 | 已复现缺陷；MMU A/D 更新；高 | 缓存旧 PTE 的 A 位更新覆盖 RAM 新 PPN | B 的 PPN 524294 被改回 A 的 524293；随后 flush 仍读 17。提前 fence 控制保留 B/读34；未证明 xv6 已触发 |
| F-U5-001 | 能力缺口；观察查询契约；高 | 没有独立无副作用翻译/设备窥视通道 | helper UART/PLIC 消费，false 翻译填 TLB，planner 覆盖最新 Bus；CLI 尚无独立 page-query，不能说前端已暴露该错误 |
| F-U5-002 | 能力缺口；记录/关联/保留/协议；高 | latest 槽覆盖，事件字段分散，bounded trace 静默覆盖 | Bus 两槽、DebugEvent 64、pipeline retire 16、前端 snapshot 32；无 drop 计数。窗口边界源码确定，未动态跑满 |
| F-U5-003 | 能力缺口；xv6 解释与前端呈现；高 | 没有生产 PID、fork/wait、页归属适配 | 固定 ELF/DWARF 和 RAM 可由专用探针解释；版本依赖不能暗藏在 CPU/MMU 或浏览器猜测中 |

问题的触发、独立期望、实际输出、代码位置、归属和边界详见 U2/U3/U4。能力缺口不作为 ISA 缺陷。各 E-ID 的 argv/cwd、停止条件、预算、退出码和日志入口由对应单元报告与 Git 内 JSON 承载；真实参考和 mock 自测分别记录。

## 最小后续范围

| 顺序 | 建议与理由 | 备选及验收依据 |
| --- | --- | --- |
| 正确性优先 | 单独修 MMU 当前 PTE 的 A/D 原子检查/重试；单独定义并修 pipeline 暂停 setter 同步；修 reference fixture 的退出约定 | 对应最小触发+正确 fence/guest 自初始化控制；增加换 PPN/权限与运行后暂停回归；全自测和真实参考分别验收。不要因这三处问题重写系统 |
| 场景必需能力 | 分离纯查询与执行访问；为目标访存补 PC/VA/PA/地址空间/权限/结果/提交关联，并提供有界采集与 drop 信号 | 可先专用导出，或小型事件接口；按关闭/开启/重复查询的独立结果对照验收，同时检验窗口覆盖与暂停/继续关联 |
| 场景必需能力 | 固定版本 xv6 适配与普通 fork 时间线呈现 | 独立解释 ELF/布局/PID/页归属；可先只支持本 vendored 版本。用同 VA 不同 PA、真实 U store、exit/wait/局部回收复验；不让前端成为 MMU 真值来源 |
| 可选整理 | 导出已有 retire cycle/target，公开 top-N 省略与模型时间含义，整理已证明影响修复的跨层入口 | 窄范围协议变更或额外导出；按字段契约和旧客户端兼容验证；源码体量不是重构理由 |
| 暂不处理 | COW、全 OoO/JIT/ISA 验收、多 hart、Linux/OSComp 扩展、真实性能、全事件平台、AI 扩展 | 仅目标场景证据不足时再单独规划；本轮没有收益/正确性数据，不将建议升级为授权 |

F-U3-002 对未来页表观察有直接风险，但目前不能宣称它阻断普通 xv6；F-U3-001 也不影响本轮使用 guest 自初始化的取证路径。先修对应边界与回归，再让纯查询、关联记录和 OS 呈现进入各自后续计划。

## 复现、版本与限制

产品基点为 `fa5dfeac584863b0a0c98ac2747b99f1aa1ae90c`，实际开始 checkout `c253ddad1862a8e222c74d633f9269390e418624`；两者产品相同，U1 的 651 项产品文件总指纹为 `df561a7560a7c661002784dee668cd1e2b248bb3e6c36b6610dd8709724247f2`。后续单元只添加研究报告、证据夹具/脚本及本轮 current 状态。各单元记录版本及源码/资产指纹；U4 仅直接采集序列起始和结束 HEAD，fork 启动 HEAD 为 reflog 推断，限制见 U4。不能把 HEAD 的文档差异说成产品语义改变。

本机 RISC-V GCC 10.2，host GCC 11.4，单 hart、128 MiB 默认 RAM；xv6 使用 rv64ima/lp64。真实 Spike 1.1.1-dev 的实际路径、SHA-256、帮助输出与比较配置见 U2；没有 PATH 修改、批量安装或第三方资产替换。

Git 内提供原始文本记录、guest 指令字和 SHA-256、最终夹具源文件、完整构建与运行 argv。临时 binary/fs.img 不入 Git；可按 U1 重建命令及 U4 独立构建脚本取得，不依赖永久保留 `/tmp`。重建是取得方式，不宣称不同主机能生成相同二进制哈希；哈希用于核对本次实际资产。

U3 前两版探针仅有历史日志/哈希，旧全文未留存，无法精确重建；两个产品发现依赖最终保存的探针和控制组。U4 首次夹具构建失败也只保留历史诊断，旧全文/指纹未留存；成功构建及最终源码可取得。debug CLI 在 15 秒超时但未定位场景；默认 Python 因共享 xv6 镜像排他执行未运行。五项真实参考未到达 Spike，窗口溢出、L1D 开启、代码 PTE A/D 等仍未动态验证。历史 CI 只作为入口/预算线索，没有替代本次结果。

没有故障注入、COW、产品修复、公共协议/CI/Wiki 变更、Issue 创建、推送、PR 或合并。外来 current 与两个 PR 审查文件保持未提交，未由本计划接管。

## 普通 fork 与统一复核

E-U4-001 的 5000 步启动 checkpoint、E-U4-002 完整 shell 和 E-U4-003 独立 fork 均退出 0；后两者耗时分别为 853.961125 和 501.935403 秒，均在独立 1200 秒上限内。完整 shell 的 16 条命令含 forktest/stressfs 均通过，独立探针又在同次 boot 中运行 forktest。

本次 guest 标记得到父 PID 3、子 PID 4；它们不是预设值。实时父 root 为 `0x87f25000`，子 root 为 `0x87f54000`，两页均为 U/R/W 叶 PTE。下面的十六进制数是对原始整数的格式化，事实来自 [U4](system-software-evidence-c253dda-u4.md) 和其 JSONL，不是生产前端自动提供的解释。

| VA | 父 PA | 子 PA | fork 后共同初值 | 子写后值 | 父 wait 后值 |
| --- | --- | --- | --- | --- | --- |
| `0x2000` | `0x87f1e000` | `0x87f29000` | `0x11223344` | `0xaabbccdd` | `0x11223344` |
| `0x3000` | `0x87f1d000` | `0x87f2a000` | `0x55667788` | `0xeeff0011` | `0x55667788` |

时间线：父预写 → ordinary fork 分配/复制 → BEFORE 映射 checkpoint（cycle 434550710）→ 两条实际 U store（PC `0xe8`/`0xfc`，cycle 434568484/434568489，child SATP `0x8000000000087f54`）→ AFTER 映射 checkpoint（435337009）→ 子 ZOMBIE/status37（435391372）→ 父 wait 返回 child/status37 并校验原值 → 子进程原槽 UNUSED/PID/root 清理、两张子数据页成为 freelist 成员 → shell/forktest 成功。

PID/VA 标记、映射/寄存器/总线与进程状态等字段由 guest/探针自动采集，普通 fork、页归属和职责的含义由对应 xv6 源码与 ELF 解释。RAM PTE 不代表 CPU 实时 TLB；回收只验两张用户数据页，不证明所有页表页、trapframe 或长期 kernel stack 都已回收。64 位原始 JSONL 整数须用任意精度解析，不能经 JS Number 舍入后校验 SATP。

因此目标场景可成立，但若要成为可暂停、可追溯的日常调试体验，仍需纯查询/结构化关联与显式 xv6 解释；无需先实现 COW，也没有证据支持全系统重写。U5 的独立复查、确定性验证和最终版本识别见对应审查记录及 Git 中主题带 `(U5)` 的提交；本报告不预填尚未生成的自身提交哈希。
