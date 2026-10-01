# 审查记录：github-actions-ci

- **范围**：`d18eb46` → 本轮工作区；主仓库 8 个工件及独立 Wiki `Verification.md`。包含新建 workflow 与 CI 元测试，不包含基线前的文档迁移提交。
- **意图**：按 Plan U1、U2 建立全部默认 unit / 核心 host 的构建和执行入口，以及日常与手动完整 GitHub Actions 配置；U3 同步本地指引，真实云端验收仍待授权发布后完成。
- **覆盖**：correctness、testing、project-standards、security、reliability、adversarial、agent-native、code-reuse、code-quality、efficiency。使用只读叶子代理分批执行；一个合并发现经全新独立 validator 实跑复核。
- **路径**：常规 nk-review；用户要求三个单元末尾统一审查，修复仅针对相关条目复核。
- **审查轮次**：第 1 轮 2026-09-30。
- **概要**：待修 0 / 已修复 1 / 遗留-转 Issue 0。范围内没有剩余需处理发现；此结论不代表 U3 云端验收完成。

## 快照与验证

初审前后各 reviewer 核对工件 SHA-256，内容保持不变。主仓库纳入 `AGENTS.md`、`README.md`、`docs/current.md`、Plan、`myCPU/Makefile`、新 `ci_targets_test.py` 和两份新 workflow；独立 Wiki 纳入 `Verification.md` 草稿。没有排除的外来脏文件。

主会话实际验证：

- 无生成物的 `git archive d18eb46` 副本应用最终 Makefile，`make -j2 build-ci-core` 后 `make -j1 test-ci-core`：61 unit、20 host 执行项通过。构建后测试 dry-run 无新增编译或链接。
- 同一副本 `make -j2 mycpu tests/asm/hello.elf guest/interactive_os.elf guest/course_os_shell.elf` 后，Node 24.16.0 `node --test`：169 通过、1 外部 Linux console 场景跳过、0 失败。
- `python3 tests/host/ci_targets_test.py`：3 项通过；测试先于 Make 入口运行时因缺少入口而失败，新增入口后通过。覆盖默认 unit 执行、构建覆盖和外部 gate 排除，以及临时目录中单个 unit 非零退出 / 超时使两个聚合入口失败。
- `make test-verification-layers`、actionlint v1.7.12、主仓库及 Wiki `git diff --check`、更新文档本地链接检查通过。Wiki 现有检查脚本核对 18 页、66 个源码链接、59 个 Make 入口、28 个迁移映射，无错误。
- 本地构建环境为 Ubuntu 22.04.5 x86-64、GCC 11.4、RISC-V GCC 10.2、Python 3.10.12；不能替代 Ubuntu 24.04 托管 runner 验收。有基线源码的 optional 未初始化与未使用函数警告，本轮未调整相关产品实现。

## 发现

### [R1-F01] P1 · 已修复 · 隐藏日志目录被 Action 排除

- **位置**：`.github/workflows/ci.yml:94`、`:177`；`.github/workflows/regression.yml:85`。
- **来源**：correctness、adversarial、reliability；独立 validator 确认（置信度 100）。
- **问题**：初审的三个上传步骤均为 `path: .ci-logs/`，固定版本 upload-artifact 默认排除隐藏目录中的文件。即使测试通过，上传也会因 `if-no-files-found: error` 失败，且无法取得 R4 所需日志。
- **建议及处理**：两份 workflow 所有日志写入与上传路径统一改为非隐藏的 `ci-logs/`；前端切换目录后的写入路径对应为 `../ci-logs/`。保持专用日志范围和默认隐藏文件过滤。
- **证据**：初审 `.github/workflows/ci.yml:94 -- path: .ci-logs/`，下一行 `if-no-files-found: error`。固定 Action `043fb46d1a93c77aae656e7c1c64a875d1fc6a0a` 的 `src/shared/search.ts` 设置 `excludeHiddenFiles: !includeHiddenFiles`，锁定的 `@actions/glob` 0.6.1 在进入隐藏目录前跳过整个遍历分支。
- **复核**：全新 validator 用 Node 24 与锁定 glob 实跑：隐藏目录默认返回零文件；允许隐藏文件或非隐藏目录可发现日志。修复后主会话对三个最终路径实跑，均发现 `results.md` 与 `tests.log`，actionlint 复验通过；独立 validator 针对性复核修复。尚未实跑云端上传，不将路径发现写成上传成功。

## 覆盖限制

截至初审，U3 尚无托管 runner 的日常 core/frontend、完整标准回归、xv6 shell、云端故障实验与 artifact 下载证据，亦未验证 PR 连续提交取消行为、实际冷构建耗时或 fork PR 实跑。后续验收见下文；fork PR 未实跑。取消或 runner 终止可能阻止 `always()` 上传，不能据此推断成功。

没有修改产品 API、数据库、缓存、异步执行或模块职责边界，因此未选 API、迁移、性能、maintainability 及平台专属角色；无已有 PR 评论或 solutions 语料，未选对应角色。精简三个视角无发现。三个日志目录修复后的证据可复用，未机械重跑无关产品测试。

## 第 2 轮：云端工具链兼容修复的针对性复核

范围为 `cd65827` 后 `myCPU/Makefile`、board profile 与 `ci_targets_test.py` 的修复，不重新扫描已通过的无关配置。首次 Ubuntu 24.04 / RISC-V GCC 13 在 CSR 汇编上报 `extension zicsr required`，旧 GCC 10 实际拒绝新扩展名称与 GCC `-misa-spec` 参数。

主会话采用编译器能力探测：支持显式 `zicsr` / `zifencei` 名称时附加，旧编译器保留原 ISA 字符串；xv6 默认使用同一工具链并复用后缀。新增元测试实际用 guest 与 xv6 两组参数编译 CSR / fence.i。旧 GCC 10 上 4 项通过、分层检查通过；独立 correctness reviewer 聚焦复核无发现。现代工具链与后续云端回归待验收，不将此轮静态审查写成通过。

覆盖边界：自定义 `XV6_TOOLPREFIX` 与 `RV_CC` 指向不同代际工具链的组合未验证；默认 CI 二者使用同一工具链。

现代验证已补齐：Ubuntu 24.04 容器的 RISC-V GCC 13.2 实跑 4 项元测试通过，显式后缀实际生效；`e608911` 云端日常两个 job 成功，日志已下载。旧失败 run 不作为修复通过证据。

## 第 3 轮：xv6 两代编译布局精确基线的针对性复核

范围为 `e608911` 后 `xv6_boot_smoke.cpp` 与 `run_debug_cli_probe_test.py` 的 xv6 functional profile 方法。完整 run `36736639580/1` 实际在旧 PC 断言失败。相同 xv6 源码分别由 GCC 10.2 / Ubuntu GCC 13.2 编译，两份 memset 的逐条指令字节相同，5000 步均停在 `memset+0x24`，函数地址及启动路径计数不同。

修复保留原整套精确基线，增加现代整套精确基线。C++ 从同一 kernel 构建规则生成的 `kernel.sym` 中读取 main / memset 双地址，选择固定基线后仍独立比较 CPU / profile 实测值；缺失或未知布局失败。Python 比较完整 checkpoint 四行，必须整组匹配，禁止新旧字段混用。

主会话两代内核 boot smoke 与 functional / pipeline probe 实跑通过；将符号 main 地址故意改动 4 字节时实际拒绝（exit 1）。现代内核上的全部 debug probe 测试为 99 项、9 个外部 skip，无失败。新内核在旧 Python 断言上先 Red，修改后两代 Green。correctness、testing 叶子代理聚焦复核无发现；产品语义和超时没有调整。修复后的完整云端回归仍待取得，其他未审计编译布局明确不支持自动放行。

## 第 4 轮：shell 墙钟预算与超时日志的针对性复核

范围为 `97fddea` 后 Makefile 的 shell 墙钟预算、超时输出与 `xv6_shell_smoke.cpp` 的阶段日志。完整 `36741288238/1` 在标准 functional / pipeline 回归完成后，shell 被原 300 秒预算终止。独立副本阶段探针确认启动约 4.3 亿步，内存填充地址连续前进；旧 GCC 10 / Ubuntu GCC 13 内核完整 16 条 shell 命令含 forktest、stressfs 均 exit 0，启动分别 296.048 / 259.818 秒，总阶段耗时 508.384 / 470.412 秒。

默认预算改为有限的 1200 秒；不修改 guest 步数预算、断言、模拟器语义或 workflow job 的 120 分钟上限。steady_clock 日志写宿主 stderr，命令标签去掉行尾 CR；超时分支打印捕获的阶段输出后仍 exit 1。correctness 叶子复核和追加输出行的复核均无发现，主会话核对最终 diff；元测试 4 项通过，最终源码编译通过。真实 `XV6_SHELL_SMOKE_TIMEOUT=1s` 覆盖实验为 Make exit 2，日志含启动阶段与明确 timeout。诊断循环探针仅在 `/tmp` 副本，未进入交付文件；最终版本云端完整验收待运行。

## 最终云端证据与结论（2026-10-01）

`f69ea75b24deb6a2192452b8b970673b84a9ea8b` 的 [日常 36797856401/1](https://github.com/steven123397/my_visual_CPU/actions/runs/36797856401/attempts/1) 与 [完整 36797868411/1](https://github.com/steven123397/my_visual_CPU/actions/runs/36797868411/attempts/1) 均成功，三份 artifact 已下载并核对 checkout SHA。日常为 61 unit、20 host、4 项元测试，前端 169 通过、1 外部 skip；完整为标准 functional / pipeline 与实际 xv6 shell 全部 16 条命令，debug probe 99 项、9 外部 skip。完整 job 16 分 31 秒，shell 启动 304.651 秒，完整阶段 535.740 秒。

[故障 36738372503/1](https://github.com/steven123397/my_visual_CPU/actions/runs/36738372503/attempts/1) 的下载日志包含 `CI_FAILURE_PROBE_V4: intentional unit exit 7` 与 Make Error 7，core / job 失败；checkout `e2e0df8fafa3cb014a50a8cdb98274813d75df02`，PR head `358135abdda0b60724762081caac427d6279e575`。前一 run `36738356921/1` 实际取消。实验 PR 已关闭，分支与 worktree 已清理，故障代码未进入交付版本。

结论：统一审查发现已修复，随后三项兼容 / 预算修复的针对性复核无待修发现，U1 / U2 / U3 验收完成。文档收尾不改变代码或 workflow，复用上述证据；fork PR 与真实 Linux / OSComp / Spike 外部运行仍未验证，skip 不记为通过。主会话核对文档与证据，不将文档收尾称为新的独立审查。
