# 当前状态

- 所在分支：`codex/github-actions-ci`；核对基点与已发布技术版本：`f69ea75b24deb6a2192452b8b970673b84a9ea8b`。
- 工作范围：[GitHub Actions Plan](plans/2026-09-30-2214-chore-github-actions-ci-plan.md) 的 U1、U2、U3 已完成；按用户要求完成统一审查。
- U1 已实现：`test-unit-all`、`build-ci-core`、`test-ci-core`，核心 host 名单只在 Makefile 维护，旧测试入口保留。
- U2 已实现：日常 CI 的 simulator-core / frontend 两个 job 与手动完整回归配置；只读权限、Action 完整 SHA、PR 并发取消、环境与阶段结果摘要、14 天日志 artifact。
- 本地验证：从 `git archive d18eb46` 创建无生成物副本，应用本轮 Makefile，`make -j2 build-ci-core` 后 `make -j1 test-ci-core` 通过（61 unit、20 host 执行项）。测试阶段 dry-run 无新增编译或链接。
- 本地环境：Ubuntu 22.04.5 x86-64、GCC 11.4、RISC-V GCC 10.2、Python 3.10.12；不是计划中的 Ubuntu 24.04 托管 runner 验证。
- 本地验证：同一副本准备前端所需 simulator 与 3 个 guest 映像后，Node 24.16.0 `node --test` 为 169 通过、1 个外部 Linux console 场景跳过、0 失败。
- 本地验证：CI 元测试 4 项通过，覆盖执行名单、构建前置、外部 gate 排除、非零退出 / 超时传播与 CSR / fence 编译；`make test-verification-layers`、actionlint v1.7.12 与 `git diff --check` 通过。
- 冷构建有原有的 pipeline optional 与 AI 未使用函数警告；本轮未改相关源码，不将警告写成零警告通过。
- U3 指引已同步：AGENTS、README 与独立 Wiki Verification 记录现行配置及真实云端证据。
- 统一审查：[审查记录](reviews/github-actions-ci.md)；一个日志隐藏目录问题已查证并改为 `ci-logs/`，固定版本 Action glob 的三 job 路径实跑通过，actionlint 复验通过。
- 用户已授权发布和验收；`cd65827` 已快进发布到 main，两份 workflow 已激活。首次日常 run `36735028119/1`、完整 run `36735075958/1` 均暴露新工具链的 CSR 架构声明问题，失败日志已下载。
- 兼容修复已发布：guest 与 xv6 共用编译器能力探测，旧 GCC 10 和 Ubuntu 24.04 / RISC-V GCC 13 两代环境均实跑 4 项元测试通过。
- 云端日常 `36736528427/1` 对应 `e608911`，core / frontend 全通过（61 unit、20 host；前端 169 通过、1 外部 skip），两份 artifact 已下载。
- 完整回归 `36736639580/1` 在 xv6 boot smoke 的旧编译布局基线失败；新旧内核反汇编同一 memset 指令、地址不同。已保留旧基线并新增 GCC 13 精确基线，两代 boot 与 functional / pipeline 探针通过；未知符号布局实际被拒绝，完整 debug probe 为 99 项、9 个外部场景跳过。
- 临时 PR #3 故障实验：`36738372503/1` 的 core 输出 Error 7，失败日志已下载；旧 run `36738356921/1` 已取消。checkout `e2e0df8fafa3cb014a50a8cdb98274813d75df02`，PR head `358135abdda0b60724762081caac427d6279e575`。
- `97fddea` 的日常 `36741082309/1` 全通过，两个 artifact 已下载；完整 `36741288238/1` 标准回归通过后，xv6 shell 耗尽 300 秒墙钟预算，完整结果为失败。
- shell 诊断：相同源码 GCC 10 / GCC 13 内核的完整 shell 序列均通过，分别约 508 / 470 秒；启动内存初始化即约 296 / 260 秒。仅把默认墙钟预算改为 1200 秒，保留所有 guest 步数预算与断言，补阶段日志及超时输出；1 秒覆盖实验仍使 Make 失败并保留阶段。针对性 correctness 复核无发现，元测试 4 项通过。
- 临时 PR #3 已关闭，故障远端 / 本地分支和 worktree 已清理，实验提交未进入 main。
- 用户决定：采用 NexusKit，待办使用 GitHub Issues；共享 InstructionSemantics / functional backend 是 ISA 真值来源，架构契约由 GitHub Wiki 承载。
- 同技术 SHA 日常 `36797856401/1` 成功：core 89 秒，61 unit、20 host、4 项元测试；frontend 48 秒，169 通过、1 外部 Linux console skip。
- 同 SHA 完整 `36797868411/1` 成功，16 分 31 秒：标准 functional / pipeline 与 xv6 shell 全部 16 条命令含 forktest / stressfs 通过；debug probe 99 项、9 外部 skip。shell 启动 304.651 秒，完整阶段 535.740 秒。
- 云端环境：Ubuntu 24.04.5 x86-64、host GCC 13.3、RISC-V GCC 13.2、Python 3.12.3、前端 Node 24.21.0。三份成功 artifact 已下载至 `/tmp/mycpu-cloud/<run>-1/`，实际 checkout SHA 已核对。
- Wiki Verification 已发布 `1c902fd04bcf3098f25a33640f9d41780964df5b`，远端 master SHA 已核对，工作区干净。

## 阻断与已知缺口

无阻断或待修审查发现。fork PR 未实跑，真实 Linux / OSComp / Spike 外部资产未验收，skip 不记为通过。部署、定时、缓存和 branch protection 不在范围内。

## 下一步

本 Plan 无剩余单元。文档收尾不改变代码和 workflow，沿用 `f69ea75` 的完整云端证据；后续工作未确定。
