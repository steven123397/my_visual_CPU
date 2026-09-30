# 当前状态

- 所在分支：`codex/github-actions-ci`；核对基点：`e608911`。
- 工作范围：[GitHub Actions Plan](plans/2026-09-30-2214-chore-github-actions-ci-plan.md) 的 U1、U2、U3；用户要求全部本地变更完成后统一审查。
- U1 已实现：`test-unit-all`、`build-ci-core`、`test-ci-core`，核心 host 名单只在 Makefile 维护，旧测试入口保留。
- U2 已实现：日常 CI 的 simulator-core / frontend 两个 job 与手动完整回归配置；只读权限、Action 完整 SHA、PR 并发取消、环境与阶段结果摘要、14 天日志 artifact。
- 本地验证：从 `git archive d18eb46` 创建无生成物副本，应用本轮 Makefile，`make -j2 build-ci-core` 后 `make -j1 test-ci-core` 通过（61 unit、20 host 执行项）。测试阶段 dry-run 无新增编译或链接。
- 本地环境：Ubuntu 22.04.5 x86-64、GCC 11.4、RISC-V GCC 10.2、Python 3.10.12；不是计划中的 Ubuntu 24.04 托管 runner 验证。
- 本地验证：同一副本准备前端所需 simulator 与 3 个 guest 映像后，Node 24.16.0 `node --test` 为 169 通过、1 个外部 Linux console 场景跳过、0 失败。
- 本地验证：CI 元测试 3 项通过，覆盖执行名单、构建前置、外部 gate 排除与隔离非零退出 / 超时失败传播；`make test-verification-layers`、actionlint v1.7.12 与 `git diff --check` 通过。
- 冷构建有原有的 pipeline optional 与 AI 未使用函数警告；本轮未改相关源码，不将警告写成零警告通过。
- U3 本地指引已同步：AGENTS、README 和独立 Wiki Verification 草稿准确区分本地配置与云端结果。
- 统一审查：[审查记录](reviews/github-actions-ci.md)；一个日志隐藏目录问题已查证并改为 `ci-logs/`，固定版本 Action glob 的三 job 路径实跑通过，actionlint 复验通过。
- 用户已授权发布和验收；`cd65827` 已快进发布到 main，两份 workflow 已激活。首次日常 run `36735028119/1`、完整 run `36735075958/1` 均暴露新工具链的 CSR 架构声明问题，失败日志已下载。
- 兼容修复已发布：guest 与 xv6 共用编译器能力探测，旧 GCC 10 和 Ubuntu 24.04 / RISC-V GCC 13 两代环境均实跑 4 项元测试通过。
- 云端日常 `36736528427/1` 对应 `e608911`，core / frontend 全通过（61 unit、20 host；前端 169 通过、1 外部 skip），两份 artifact 已下载。
- 完整回归 `36736639580/1` 在 xv6 boot smoke 的旧编译布局基线失败；新旧内核反汇编同一 memset 指令、地址不同。已保留旧基线并新增 GCC 13 精确基线，两代 boot 与 functional / pipeline 探针通过；未知符号布局实际被拒绝，完整 debug probe 为 99 项、9 个外部场景跳过。
- 临时 PR #3 故障实验：`36736920565/1` 的 core 输出 Error 7，失败日志已下载；快速连续提交的旧 run `36738356921/1` 已取消，替代 run `36738372503/1` 失败。故障分支不合入 main，验收完成后清理。
- 用户决定：采用 NexusKit，待办使用 GitHub Issues；共享 InstructionSemantics / functional backend 是 ISA 真值来源，架构契约由 GitHub Wiki 承载。
- Wiki 现行已发布基点：`e75397b3845d70778061177c2e28c01132f52dd9`；Verification 本轮修改在 `/home/liangjiaqi/projects/my_visual_CPU.wiki`，尚未提交或推送。

## 阻断与已知缺口

U3 远端验收尚未完成：日常与失败日志已有证据，需发布 xv6 两代精确基线修复，再取得同版本日常与完整回归（含 xv6 shell）的成功证据并下载日志。
完整标准回归及 xv6 shell 本轮未在本地执行，计划使用托管 runner 完成。没有云端证据，不能宣称 CI 已启用或整个 Plan 完成。
真实 Linux / OSComp / Spike、部署、定时任务、缓存和 branch protection 不在范围内。

## 下一步

发布已复核的 xv6 测试兼容修复，继续 U3 的完整回归验收，再同步交付记录、清理临时故障 PR / 分支并发布 Wiki Verification。
