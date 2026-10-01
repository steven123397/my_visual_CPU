# 当前状态

- 所在分支：`main`；核对基点：`adbbdbf`，本次工作前已推送到远端 `main`。用户指定本次文档改动在本地 main 完成；CI 分支已快进发布并删除。
- 工作范围：将根目录 AI 建议转为候选 Issues、添加 MIT 许可、更新仓库描述，补齐 AGENTS 中的分支与 CI 合并规则，并按用户指定的 `nk-handoff` 保存交接。本轮不进行后续产品规划或开发。
- 新建未排期的 enhancement：[#4 模型与数值精度](https://github.com/steven123397/my_visual_CPU/issues/4)、[#5 计算与时序模型](https://github.com/steven123397/my_visual_CPU/issues/5)、[#6 AI 可观察性](https://github.com/steven123397/my_visual_CPU/issues/6)。已查重并对照当前实现，原建议的过时缺口及未经验证的性能数字未作为现行事实；原稿随本次提交删除，可从核对基点追溯。
- 用户选定 MIT；根目录 `LICENSE` 与 README 许可入口已添加，xv6 原版权与许可保留。本次文档交付已进入本地 main，尚未发布到远端 `main`。
- GitHub 仓库描述已更新为“myCPU：可运行、可观察的 RISC-V 系统模拟器原型，包含共享指令语义、多执行后端、系统 workload、MMIO AI 加速器与浏览器调试工作台。”
- [main 保护规则](https://github.com/steven123397/my_visual_CPU/rules/24284085) 已启用：PR 合并、`simulator-core` 与 `frontend` 检查、禁止强推及删除；审批人数为 0，无管理员绕过。已回读规则并确认适用于 main。
- 重复 [PR #2](https://github.com/steven123397/my_visual_CPU/pull/2) 已关闭且未合并；[Issue #1](https://github.com/steven123397/my_visual_CPU/issues/1) 已按完成关闭。`e608911` 的工具链能力探测已在 main；本会话重跑 `python3 tests/host/ci_targets_test.py`，4 项通过。GCC 14.2 未直接验证。
- 日常 CI 覆盖全部默认 unit、核心 host 与前端；手动完整回归覆盖标准 functional / pipeline 与 xv6 shell。现行契约见 [Wiki Verification](https://github.com/steven123397/my_visual_CPU/wiki/Verification)，Wiki 发布提交为 `1c902fd04bcf3098f25a33640f9d41780964df5b`。
- 技术版本 `f69ea75b24deb6a2192452b8b970673b84a9ea8b` 的 [日常 36797856401/1](https://github.com/steven123397/my_visual_CPU/actions/runs/36797856401/attempts/1) 成功：61 unit、20 host、4 项元测试；前端 169 通过、1 外部 skip；三份 artifact 已下载核对实际 checkout SHA。
- 同版本 [完整 36797868411/1](https://github.com/steven123397/my_visual_CPU/actions/runs/36797868411/attempts/1) 成功，16 分 31 秒；debug probe 99 项、9 外部 skip；shell 全部 16 条命令含 forktest / stressfs 通过，阶段总耗时 535.740 秒。本会话回读确认 conclusion 与 head SHA；未重跑完整回归。
- 文档基点的 [日常 36799850023/1](https://github.com/steven123397/my_visual_CPU/actions/runs/36799850023/attempts/1) 成功；`f69ea75` 后只有文档变化，完整代码证据仍适用。云端环境为 Ubuntu 24.04.5 x86-64、host GCC 13.3、RISC-V GCC 13.2、Python 3.12.3、前端 Node 24.21.0。
- [故障 36738372503/1](https://github.com/steven123397/my_visual_CPU/actions/runs/36738372503/attempts/1) 实际 Error 7 且日志可下载；checkout `e2e0df8fafa3cb014a50a8cdb98274813d75df02`、PR head `358135abdda0b60724762081caac427d6279e575`。旧 run `36738356921/1` 已取消；PR #3、故障分支与 worktree 已清理，实验代码未进入 main。失败、取消和 skip 不计为通过，既有编译警告保留。
- CI 累计审查范围 `d18eb46..cbd9ee8`，Plan 来源 `9886a62`；统一审查与三个修复复核、主会话 pre-merge 核对已完成，无待修或阻断发现。已完成 Plan 和 CI 审查报告在知识承接后删除，历史提交可追溯。Wiki 迁移审查报告也经用户授权删除：有效去向与验证边界已由 [Migration](https://github.com/steven123397/my_visual_CPU/wiki/Migration) 承接，无新增 solution 或遗留 Issue。
- 长期经验：[xv6 工具链、精确基线与预算](solutions/build-errors/xv6-ci-toolchain-baselines-and-budget.md)、[隐藏 artifact 日志目录](solutions/workflow-issues/github-actions-hidden-artifact-logs.md)。尚无新增合格领域术语。
- 用户决定：采用 NexusKit 与 GitHub Issues，共享 InstructionSemantics / functional backend 为 ISA 真值，架构契约由独立 Wiki 承载；安全扫描及 Dependabot 暂不接入。Codespaces 暂不推进，本地开发配合云端 CI，未创建云端实例。

## 阻断与已知缺口

- 无代码交付阻断；本次许可、README、AGENTS、建议稿删除与交接记录尚未发布到 main。仓库描述与 Issues 已在 GitHub 生效。
- fork PR、自定义 guest / xv6 工具链跨代组合、真实 Linux / OSComp / Spike 外部资产未验证。部署、定时、多平台与缓存仍未规划；本次未重新运行产品测试。
- AI Issues 仅为后续方向讨论的材料，尚未认领、排期或形成实施 Plan。云端维护 agent（提交 PR / Issue、按 main 变化维护 Wiki）仅是用户构想，工具、权限、费用与审核边界均未决定，未安装或授权 bot。

## 下一步

- 用户已授权通过 `codex/open-source-handoff` 发布分支推送本次文档并创建 PR；远端 main 尚未合入，PR 与 CI 状态以 GitHub 为准，不直接推送 main 或绕过保护。
- 本对话到交接结束。新对话再以 `nk-brainstorm` 澄清项目演进与调整、部分重写的目标；可引用 #4、#5、#6，方向确认后才由 `nk-plan` 形成 `docs/plans/` 下的实施 Plan。
- 云端维护 agent 留待新对话独立评估，不默认采用 Grok bot，也不自动创建相关 Issue 或 workflow。
