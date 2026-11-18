# 审查记录：远端 PR #8 至 #13

- **范围**：`53d79ff001d8ad540772da51562e41fc79831e36` 到下表各 PR 快照；候选组合涉及 17 个文件。
- **意图**：清理未使用代码与补充注释，优化 FP32 GEMM 内存访问，补充限流窗口边界测试。
- **路径**：NexusKit pre-merge，主会话合并前检查，零专项代理；不宣称独立全面审查。
- **覆盖**：累计 diff、删除符号调用、消费者与数值累加顺序、Wiki 契约、CI 配置及实际 checkout、本地组合验证。
- **审查轮次**：第 1 轮，2026-10-01。
- **概要**：待修 0 / 已修复 0 / 遗留-转 Issue 0。范围内未发现需处理问题。

## 快照与处理

| PR | 被审 head | 结论 | 远端结果 |
| --- | --- | --- | --- |
| [#8](https://github.com/steven123397/my_visual_CPU/pull/8) | `3a10ffc682e5efe21667c6c87234425c28b0f78d` | 删除项无仓库调用；LSQ 值复制保持观察行为；注释不影响运行 | 合并提交 `1a8b50a0c57d34879067475b72357b6fbed857d4` |
| [#9](https://github.com/steven123397/my_visual_CPU/pull/9) | `1a88d121518b822a4d0a2260ce1aa2b6e893aee9` | 与 #10 重复；选择把行偏移放在外层的 #10 | 已关闭，未合并 |
| [#10](https://github.com/steven123397/my_visual_CPU/pull/10) | `05678bab353681093eb012b5a4f67103321cda0b` | 保留每个输出元素按 depth 累加的顺序，连续访问 rhs/out | 合并提交 `df3887735fb7fe8921a5baba2c95c3daf7e1c8e3` |
| [#11](https://github.com/steven123397/my_visual_CPU/pull/11) | `e6b36452e86c2aeaeb7891eb076dbfddb1158f2f` | 覆盖空窗口、全部过期、全部有效、等于 cutoff 和混合窗口；运行逻辑未改 | 合并提交 `192482adbebfd1adb981f945ee488b88b83f1cf8` |

#8 描述提到删除 `SimpleStorage::clear_image`，实际 diff 没有删除该函数；`machine_loader_reset.cpp` 仍调用它。审查以 diff 为准。已对照最新独立 Wiki，删除符号未形成已记录的对外契约，其他变化也未改变已记录行为，无需同步 Wiki。

## 验证证据

四个 PR 的 `simulator-core` 和 `frontend` 均为 SUCCESS。已有证据针对各自与原 main 的 GitHub 合成提交；候选组合在隔离 worktree 内顺序应用 #8、#10、#11 后验证，暂存 tree 为 `982f7470080efbd7e428f5909413290d846ae506`，与最终远端 main 的 tree 无差异。

| PR | run / attempt | 实际 checkout SHA | 读取方式 |
| --- | --- | --- | --- |
| #8 | [36813106447/1](https://github.com/steven123397/my_visual_CPU/actions/runs/36813106447/attempts/1) | `3d3997df0b5bce01c5db802ba3b197dbd9ee8ab2` | 两项 artifact |
| #9 | [36813121972/1](https://github.com/steven123397/my_visual_CPU/actions/runs/36813121972/attempts/1) | `f44924691463d9bfb7813491b1967727fb9e85c5` | 两项 artifact |
| #10 | [36814603469/1](https://github.com/steven123397/my_visual_CPU/actions/runs/36814603469/attempts/1) | `5592b7394cc9227e38461b09a6014481e42f13b7` | 两项 job 的完整日志；artifact 下载 TLS 超时 |
| #11 | [36816084685/1](https://github.com/steven123397/my_visual_CPU/actions/runs/36816084685/attempts/1) | `47b7a7c28e9da92a5ef27eed6acbab480e7dde61` | 两项 job 的完整日志；artifact 下载 TLS 超时 |

- 组合 C++ 验证：`test-host-ai_tensor_golden_ops_smoke`、`test-host-execution_profile_smoke`、`test-host-pipeline_backend_smoke-core` 均通过。
- 组合前端验证：security、terminal_input_pump、terminal_projection、load_error_message 四个测试文件共 13 项通过，无跳过。
- 临时 GEMM 差分程序：128 个矩形 shape/输入案例通过，有限值与有符号零按位比较，NaN 按分类比较；含无穷、NaN 与次正规数。256 立方矩阵结果也按位相同。项目 `-O2` 编译选项下单次计时，原算法 20.98 ms，优化算法 12.31 ms；仅说明本机本次观察，不承诺 PR 描述中的固定倍率。
- `git diff --cached --check` 通过。未跟踪文件不纳入被审产品快照；临时差分程序不提交。
- 复用 `docs/solutions/workflow-issues/github-actions-hidden-artifact-logs.md` 的 run、attempt 与 checkout 核对方法。

## 覆盖限制

未重跑完整回归及真实 Linux、OSComp、Spike 外部资产。#10 的云端前端为 169 项通过、1 项外部 skip；#11 为 170 项通过、1 项外部 skip，skip 不计为通过。特殊浮点检查不保证 NaN payload 或所有编译器/架构的按位一致性。主会话完成四项聚焦检查：目标闭环、接口一致、后续改动影响、组合遗漏；已有 CI 没有被本次变更弱化。

最终 main `192482adbebfd1adb981f945ee488b88b83f1cf8` 的 [合并后 CI 36817064276/1](https://github.com/steven123397/my_visual_CPU/actions/runs/36817064276/attempts/1) 为 SUCCESS，已回读 run 的 SHA、attempt 和两项 job 结果。临时验证 worktree 已移除，本地 main 已快进同步。本报告为本地审查产物，尚未提交或发布。

## 第 2 轮：审查期间新增的 PR #12、#13

基线为第一轮合并后的 `192482adbebfd1adb981f945ee488b88b83f1cf8`。仍使用主会话 pre-merge 路径，零专项代理；待修、已修复、遗留计数均为 0。

| PR | 被审 head | 实际 CI checkout | run / attempt | 合并提交 |
| --- | --- | --- | --- | --- |
| [#12](https://github.com/steven123397/my_visual_CPU/pull/12) | `e7e36f17e9a262bb10a95be665cb8b3a0f637370` | `4e6be6837b3f4b0757d45354f701a7f589737600` | [36816333253/1](https://github.com/steven123397/my_visual_CPU/actions/runs/36816333253/attempts/1) | `1375eec101fadf97609af64f66e7b2688366d187` |
| [#13](https://github.com/steven123397/my_visual_CPU/pull/13) | `1481f359bf89a2297478a50520caedb44a2964b8` | `27e69dcf39936f9c8381732ac1da277cd5bec2ed` | [36816605189/1](https://github.com/steven123397/my_visual_CPU/actions/runs/36816605189/attempts/1) | `57b16e7ed9f68132449a40c91ce93566d4c4e46c` |

两项 CI 均通过，已读取完整 job 日志核对 checkout；各自前端 169 项通过、1 项外部 skip。#12 先解码两份输入，再调整循环顺序，保持每个输出的 depth 累加顺序；新增临时内存为 `4 * (M*K + K*N)` 字节，作为减少重复解码的明确代价。#13 影响 INT8/INT16 到 INT32 的现有模板，不新增 INT32 输入支持；有符号累加溢出问题是原实现已有边界，本次未扩展输入数值契约。

在隔离 worktree 上组合 #12、#13，tree `77b7b1b10bccd072ddffa24849070e2e6f812ca2` 与最终远端 main 完全一致。`ai_tensor_golden_ops_smoke` 通过；临时差分程序在 `-O2` 与 UBSan 下验证 108 个 FP16/BF16/INT8/INT16 矩形矩阵案例通过。低精度输入随机生成 16 位模式，浮点有限结果按位比较，NaN 按分类比较；INT16 输入限制在累加不溢出的范围。未验证低内存极限、所有 NaN payload 或有符号溢出结果，也未新增固定性能倍率声明。`git diff --cached --check` 通过。

四项聚焦检查均完成，未发现需处理问题。代码接口及 Wiki 已记录行为未改变；第一轮无关内容的审查结论继续复用。远端开放 PR 已回读为空；最终 main `57b16e7ed9f68132449a40c91ce93566d4c4e46c` 的 [CI 36817526084/1](https://github.com/steven123397/my_visual_CPU/actions/runs/36817526084/attempts/1) 为 SUCCESS，已回读 head SHA、attempt 与 `simulator-core`、`frontend` 两项成功结果。临时 worktree 已移除，本地 main 与远端同步，仅本报告未提交。
