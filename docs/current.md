# 当前状态

## 阶段冻结与下一阶段主线（2026-12-20 初试后启动）

- **当前阶段**：研究生初试备考冻结期（2026-11-18 至 **2026-12-20**）。本仓库已完成取证、可信度修复与有界观测采集分支的 PR 收尾归档，暂停功能开发。
- **下一阶段唯一入口**：2026-12-20 初试结束后，直接读取并基于 [v2 现代化重构蓝图与确认意图](intent/mycpu-v2-modernization.md) 启动 `spec-driven-development` 与全仓无禁区重构（目标在 2027 年 3 月末复试前完成）：
  - 彻底砍除自定义 MMIO AI 加速器、`V-lite`、冗余教学 demo 内核及绑定前端面板；
  - 全仓迁移至现代 CMake 与高内聚低耦合的现代 C++ 架构；
  - 新增声明式 ISA-DSL 代码生成管线，并将 `pipeline` 补全为带 `IssueQueue` 的真乱序发射后端、将 `dbt` 补全为可独立跑真实负载的真 x86-64 JIT 后端，保留并强化零污染系统可观测验证。

## 现行基线与取证交付

- 所在分支：`codex/system-software-evidence-plan`（收尾归档合并至 `main`）；核对基点：`9f6bd7389c44abc45edd5a474402c3bfc2d49097` 加 U6 最终产品差异 `f20a8b7fe937`。
- 当前交付：[第三份计划](plans/2026-10-02-1632-feat-trusted-observation-capture-plan.md) U1–U6 顺序完成、逐单元本地提交。[回传与统一审查](reviews/trusted-observation-capture.md)：两后端纯 RAM/页表查询、functional 实际整数及 LR/SC/AMO 记录、有界采集与正式 CLI/无损导出、fork 探针迁移。最终 61 unit/24 host、4 项元测试、Python 94 通过/9 外部 skip、前端 172 通过/1 外部 skip、十项真实 Spike、完整 xv6 shell 16 命令均通过；fork 关闭/开启与自动配对通过，两条实际 store 无 dropped/unsupported。统一审查五项发现已修复并独立复核，无待修发现。Wiki 已发布至 `a83dadfe068ea4505c074ac791d49732b183fb7a`。
- 上轮交付：[可信度修复计划](plans/2026-10-02-1519-fix-system-software-trust-plan.md) U1（`36f41f5`）、U2（`999832c`）、U3（`111c3ef`）、U4 均完成，逐单元提交。[回传与统一审查](reviews/system-software-trust-fixes.md)：A/D 失配重查、暂停 GPR 同步、独立 M 退出夹具已验证；默认 CI 61 unit/22 host、Python 92 通过/9 外部 skip、真实 Spike 十项、xv6 完整 shell 16 命令及独立两页普通 fork 通过。统一审查唯一 P2（Spike 头文件构建依赖）已修复并独立复核，重建后十项真实 Spike 再通过，无待修发现。Wiki 已含 `d1244c7` 并随 `a83dadf` 发布。
- 工作范围：[系统软件取证计划](plans/2026-10-02-1038-system-software-evidence-plan.md) 的 U1–U5 均完成，逐单元审查并提交。[最终回传](reviews/system-software-evidence-c253dda-u5.md) 提供 Q1–Q6、问题分类和最小后续范围；[总索引](reviews/system-software-evidence-c253dda.md) 关联全部证据。完整 shell 16 条命令通过，独立普通 fork 确认父子两页隔离、exit/status37/wait 与局部回收，仅 functional/单 hart。未修改产品语义。
- 原审查会话保留记录：批量审查 Jules PR #14 至 #17。#14、#16、#17 已合并；#17 保留 profile_int/profile_hex 两组测试。审查记录见 [PR #14 至 #17](reviews/2026-10-01-pr14-17-pre-merge.md)。本地 security 测试 9 项、Python helper 测试 2 项通过；Python 新测试不在日常 CI 执行范围内。#17 整合 head `95616ec` 的 CI 36882931461/2 成功，原前端 attempt 1 取消不计通过；最终 main CI 36884322845/1 的 simulator-core 与 frontend 均成功，已回读精确 SHA。
- 新建未排期的 enhancement：[#4 模型与数值精度](https://github.com/steven123397/my_visual_CPU/issues/4)、[#5 计算与时序模型](https://github.com/steven123397/my_visual_CPU/issues/5)、[#6 AI 可观察性](https://github.com/steven123397/my_visual_CPU/issues/6)。已查重并对照当前实现，原建议的过时缺口及未经验证的性能数字未作为现行事实；原稿随本次提交删除，可从核对基点追溯。
- 用户选定 MIT；根目录 `LICENSE` 与 README 许可入口已通过 PR #7 发布，xv6 原版权与许可保留。
- GitHub 仓库描述已更新为“myCPU：可运行、可观察的 RISC-V 系统模拟器原型，包含共享指令语义、多执行后端、系统 workload、MMIO AI 加速器与浏览器调试工作台。”
- [main 保护规则](https://github.com/steven123397/my_visual_CPU/rules/24284085) 已启用：PR 合并、`simulator-core` 与 `frontend` 检查、禁止强推及删除；审批人数为 0，无管理员绕过。已回读规则并确认适用于 main。
- 重复 [PR #2](https://github.com/steven123397/my_visual_CPU/pull/2) 已关闭且未合并；[Issue #1](https://github.com/steven123397/my_visual_CPU/issues/1) 已按完成关闭。`e608911` 的工具链能力探测已在 main；本会话重跑 `python3 tests/host/ci_targets_test.py`，4 项通过。GCC 14.2 未直接验证。
- 日常 CI 覆盖全部默认 unit、核心 host 与前端；手动完整回归覆盖标准 functional / pipeline 与 xv6 shell。现行契约见 [Wiki Verification](https://github.com/steven123397/my_visual_CPU/wiki/Verification)，Wiki 发布提交为 `1c902fd04bcf3098f25a33640f9d41780964df5b`。
- 技术版本 `f69ea75b24deb6a2192452b8b970673b84a9ea8b` 的 [日常 36797856401/1](https://github.com/steven123397/my_visual_CPU/actions/runs/36797856401/attempts/1) 成功：61 unit、20 host、4 项元测试；前端 169 通过、1 外部 skip；三份 artifact 已下载核对实际 checkout SHA。
- 同版本 [完整 36797868411/1](https://github.com/steven123397/my_visual_CPU/actions/runs/36797868411/attempts/1) 成功，16 分 31 秒；debug probe 99 项、9 外部 skip；shell 全部 16 条命令含 forktest / stressfs 通过，阶段总耗时 535.740 秒。本会话回读确认 conclusion 与 head SHA；未重跑完整回归。
- 文档基点的 [日常 36799850023/1](https://github.com/steven123397/my_visual_CPU/actions/runs/36799850023/attempts/1) 成功；`f69ea75` 后只有文档变化，完整代码证据仍适用。云端环境为 Ubuntu 24.04.5 x86-64、host GCC 13.3、RISC-V GCC 13.2、Python 3.12.3、前端 Node 24.21.0。
- [故障 36738372503/1](https://github.com/steven123397/my_visual_CPU/actions/runs/36738372503/attempts/1) 实际 Error 7 且日志可下载；checkout `e2e0df8fafa3cb014a50a8cdb98274813d75df02`、PR head `358135abdda0b60724762081caac427d6279e575`。旧 run `36738356921/1` 已取消；PR #3、故障分支与 worktree 已清理，实验代码未进入 main。失败、取消和 skip 不计为通过，既有编译警告保留。
- CI 累计审查范围 `d18eb46..cbd9ee8`，Plan 来源 `9886a62`；统一审查与三个修复复核、主会话 pre-merge 核对已完成，无待修或阻断发现。已完成 Plan 和 CI 审查报告在知识承接后删除，历史提交可追溯。Wiki 迁移审查报告也经用户授权删除：有效去向与验证边界已由 [Migration](https://github.com/steven123397/my_visual_CPU/wiki/Migration) 承接，无新增 solution 或遗留 Issue。
- 长期经验：[xv6 工具链、精确基线与预算](solutions/build-errors/xv6-ci-toolchain-baselines-and-budget.md)、[隐藏 artifact 日志目录](solutions/workflow-issues/github-actions-hidden-artifact-logs.md)。领域术语“页表解释”和“执行访存记录”见 [CONCEPTS.md](../CONCEPTS.md)。
- 用户决定：采用 NexusKit 与 GitHub Issues，共享 InstructionSemantics / functional backend 为 ISA 真值，架构契约由独立 Wiki 承载；安全扫描及 Dependabot 暂不接入。Codespaces 暂不推进，本地开发配合云端 CI，未创建云端实例。

## 阻断与已知缺口

- 可信度修复环境：RISC-V GCC 10.2、host GCC 11.4；真实 Spike 位于 `/home/liangjiaqi/.local/riscv-spike/bin/spike`。F-U3-001/F-U3-002 与五项夹具阻断已修复并负向验证；没有证明原缺陷阻断普通 fork。fresh walk 的动态 PTE 失配仅经审查，未做设备注入回归。本轮新增纯查询与采集的关联/丢失契约；旧 helper/翻译/planner 仍不承诺纯观察。历史失败证据保留，旧 CI 不替代本轮验收。

- PR #15 已按用户要求关闭且未合并，由 [Issue #18](https://github.com/steven123397/my_visual_CPU/issues/18) 承接 firstForwardedIp 的边界测试与异常输入契约。null、数字及数组的 TypeError 是原 PR 分支引入的回归，未进入 main；正常 HTTP 字符串头未复现异常，不作为已证实的远程漏洞。Issue 尚未认领或实施。
- 历史未验范围仍包括 fork PR、跨代工具链组合、真实 Linux/OSComp 和部署/多平台；不代表本次 Spike/xv6 未运行。本轮十项真实 Spike、默认核心回归、前端、完整 xv6 shell 与独立 fork 配对已通过；9 个外部 Linux Python 场景、1 个前端外部场景 skip，本轮云端 CI 未运行。
- AI Issues 仅为后续方向讨论的材料，尚未认领、排期或形成实施 Plan。云端维护 agent（提交 PR / Issue、按 main 变化维护 Wiki）仅是用户构想，工具、权限、费用与审核边界均未决定，未安装或授权 bot。

## 下一步

- **2026-12-20 初试后重启**：以 [v2 现代化重构蓝图与确认意图](intent/mycpu-v2-modernization.md) 为基线启动 `spec-driven-development`，按 Phase 0–4 实施大减法裁剪（移除 AI 加速器 / V-lite / 冗余 demo）与全仓重构（CMake + ISA-DSL 代码生成 + 真乱序 `IssueQueue` + 真 x86-64 DBT JIT + 精简可观测调试工作台）。
- 原 PR 审查补记（[PR #14 至 #17](reviews/2026-10-01-pr14-17-pre-merge.md) 与 [远端 PR #8 至 #13](reviews/2026-10-01-remote-pr-pre-merge.md)）及本分支取证/修复/采集全部成果随本次收尾 PR 一并合入 `main`；历史 AI Issues（`#4`、`#5`、`#6`）与 `#18` 在下一阶段按新重构范围重新评估处置。
- 独立 Wiki 已推送至 `a83dadfe068ea4505c074ac791d49732b183fb7a`；工作区构建产物不提交。
