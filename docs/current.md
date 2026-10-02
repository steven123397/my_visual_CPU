# 当前状态
- U3 已完成 functional 实际标量、LR/SC/AMO 分段记录；定向采集、atomic、后端差分、profile 和 pipeline speculation 回归通过。U4–U6 待实施。
- U2 已完成纯 RAM 页表解释、固定上下文与共享权限判定；查询及执行权限、A/D、TLB flush 回归通过。U3–U6 待实施。

- 第三份计划实施：U1 已完成 const RAM 查询及 Machine/DebugSession 接口；`make -C myCPU test-host-debug_inspection_smoke` 通过，覆盖两后端、L1D 开关、边界与设备消费控制。后续按 U2–U6 顺序实施，统一审查尚未进行；本轮不推送或合并。

- 所在分支：`codex/system-software-evidence-plan`；核对基点：`3c9785732f757bc32490cca62375cbf7280a576d`。
- 当前交付：[可信观察与最小访存采集计划](plans/2026-10-02-1632-feat-trusted-observation-capture-plan.md) 已完成规划自检，尚未实施。U1–U6 覆盖无副作用 RAM/页表查询、functional 实际访存记录、有界采集、CLI/JSON 接口及 fork 探针迁移；pipeline 仅支持本轮纯查询，采集明确拒绝。本次仅文档，未运行产品测试。
- 上轮交付：[可信度修复计划](plans/2026-10-02-1519-fix-system-software-trust-plan.md) U1（`36f41f5`）、U2（`999832c`）、U3（`111c3ef`）、U4 均完成，逐单元提交。[回传与统一审查](reviews/system-software-trust-fixes.md)：A/D 失配重查、暂停 GPR 同步、独立 M 退出夹具已验证；默认 CI 61 unit/22 host、Python 92 通过/9 外部 skip、真实 Spike 十项、xv6 完整 shell 16 命令及独立两页普通 fork 通过。统一审查唯一 P2（Spike 头文件构建依赖）已修复并独立复核，重建后十项真实 Spike 再通过，无待修发现。Wiki 本地提交 `d1244c7`，未发布；主仓库未推送或合并。
- 工作范围：[系统软件取证计划](plans/2026-10-02-1038-system-software-evidence-plan.md) 的 U1–U5 均完成，逐单元审查并提交。[最终回传](reviews/system-software-evidence-c253dda-u5.md) 提供 Q1–Q6、问题分类和最小后续范围；[总索引](reviews/system-software-evidence-c253dda.md) 关联全部证据。完整 shell 16 条命令通过，独立普通 fork 确认父子两页隔离、exit/status37/wait 与局部回收，仅 functional/单 hart。未修改产品语义。
- 本分支基点已包含 PR #17 合并提交；此前 PR #14 至 #17 审查记录见 [报告](reviews/2026-10-01-pr14-17-pre-merge.md)。其云端状态与主工作区其他会话的补记，本轮不重新审查或代为提交。
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

- 可信度修复环境：RISC-V GCC 10.2、host GCC 11.4；真实 Spike 位于 `/home/liangjiaqi/.local/riscv-spike/bin/spike`。F-U3-001/F-U3-002 与五项夹具阻断已修复并负向验证；没有证明原缺陷阻断普通 fork。fresh walk 的动态 PTE 失配仅经审查，未做设备注入回归。snapshot 配对成立，helper/翻译/planner 的观察副作用与关联窗口/丢失信号仍属后续范围。当前 debug CLI 原预算通过，Python 已运行；历史失败证据仍保留，旧 CI 不替代本轮验收。

- 历史 PR #15 的边界输入发现及后续处置由原审查会话承接，本轮未核对远端状态，不作为当前 main 的已证实缺陷。
- 历史未验范围仍包括 fork PR、跨代工具链组合、真实 Linux/OSComp 和部署/多平台；不代表本次 Spike/xv6 未运行。本轮十项真实 Spike、默认核心回归、完整 xv6 shell 与独立 fork 已通过；9 个外部 Linux Python 场景 skip，云端及前端新版本验证未运行。
- AI Issues 仅为后续方向讨论的材料，尚未认领、排期或形成实施 Plan。云端维护 agent（提交 PR / Issue、按 main 变化维护 Wiki）仅是用户构想，工具、权限、费用与审核边界均未决定，未安装或授权 bot。

## 下一步

- 新会话在当前分支按 [第三份计划](plans/2026-10-02-1632-feat-trusted-observation-capture-plan.md) 从 U1 开始，顺序完成 U1–U6，最后组合验收与统一审查；无规划期开工阻断。回传入口为 `docs/reviews/trusted-observation-capture.md`。PID 自动解释和前端时间线后续另行规划；本轮不推送、创建 PR、合并或发布 Wiki。
- #4、#5、#6 不由本 Plan 认领；既有 PR/Issue 后续仍由原审查会话承接。
- 云端维护 agent 留待新对话独立评估，不默认采用 Grok bot，也不自动创建相关 Issue 或 workflow。

## 工作区未提交改动

- 本次交付第三份计划、两条领域术语、AGENTS 导航及对应 current 更新，无规划草稿残留。既有 current 审查补记、两个 PR 审查文件仍为外来未提交修改，未由本计划接管。
