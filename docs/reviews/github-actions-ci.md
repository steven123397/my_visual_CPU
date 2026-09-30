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

U3 尚无托管 runner 的日常 core/frontend、完整标准回归、xv6 shell、云端故障实验与 artifact 下载证据，亦未验证 PR 连续提交取消行为、实际冷构建耗时或 fork PR 实跑。文档均保留这些未验证边界。取消或 runner 终止可能阻止 `always()` 上传，不能据此推断成功。

没有修改产品 API、数据库、缓存、异步执行或模块职责边界，因此未选 API、迁移、性能、maintainability 及平台专属角色；无已有 PR 评论或 solutions 语料，未选对应角色。精简三个视角无发现。三个日志目录修复后的证据可复用，未机械重跑无关产品测试。
