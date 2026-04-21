# 代码复查状态

## 文档定位

本文档用于集中记录代码审查 / 复查任务中发现的问题、当前处理状态和下一步。

它不记录完整修复过程；具体执行步骤应进入对应 `plan` 文档，已完成事项统一归档到 [../plan/history_plan.md](../plan/history_plan.md)。

## 关联文档

- 相关状态：
  - [mainline_status.md](mainline_status.md)
  - [project_priority_roadmap.md](project_priority_roadmap.md)
- 当前计划：
  - 当前无活跃计划。

## 当前状态

- 当前无活跃问题。
- 最近一轮集中复查的主要收口方向已经完成，包括：
  - `debug/frontend` 的 session staged swap、`loadedSession` 口径、`terminal collapsed` 状态表达、向量展示精度与事件文案统一。
  - guest runtime / VM 边界的 rollback、teardown、fail-closed 和 fault-range 合同补洞。
  - `kernel_runtime / kernel_bringup / storage / interrupt` 失败路径与 bring-up 清理合同补洞。
  - 向量访存对 live `MMIO`、非 RAM span 和 partial-write 的 fail-closed 边界。
- 当前正式设计口径已经同步收口：`Phase 3` 执行模型、decode 边界收窄与后续取舍判断已统一吸收到 [../design/phase3_ooo_execution_model_design.md](../design/phase3_ooo_execution_model_design.md)，不再分散维护专项设计碎片。

## 下一步

1. 如果后续再做代码审查或对抗性复查，新的活跃 findings 继续集中记录到本文档。
2. 如果当前仍无活跃问题，应继续明确保持“当前无活跃问题”，而不是在本文件持续堆积已关闭的详细流水账。
3. 若后续某轮复查形成可执行任务清单，再单独补对应 `plan` 文档，并在关闭后把结果摘要回写到这里。

## 记录规则

1. 问题按严重级别和影响面排序。
2. 每条问题至少写清影响范围、风险、建议动作和当前状态。
3. 如果问题进入修复，应补充对应 `plan` 或相关提交 / 分支说明。
4. 问题关闭后只保留简短结论，不在本文件堆积完整执行流水账。
