# AGENTS.md

## 适用范围

本文件适用于 [docs](/home/liangjiaqi/projects/my_visual_CPU/docs) 子树下的规划、契约、设计、审查和说明类文档。

## 目录结构

当前 `docs/` 目录按职责分层维护：

- [index.md](/home/liangjiaqi/projects/my_visual_CPU/docs/index.md)
  面向读者的文档入口和阅读顺序。
- [background](/home/liangjiaqi/projects/my_visual_CPU/docs/background)
  项目背景、原始目标和外部上下文。
- [design](/home/liangjiaqi/projects/my_visual_CPU/docs/design)
  结构设计和长期有效的方案文档。
- [contracts](/home/liangjiaqi/projects/my_visual_CPU/docs/contracts)
  guest / simulator 共享契约与平台接口说明。
- [status](/home/liangjiaqi/projects/my_visual_CPU/docs/status)
  当前状态跟踪文档。
- [templates](/home/liangjiaqi/projects/my_visual_CPU/docs/templates)
  模板类文档。
- [archive](/home/liangjiaqi/projects/my_visual_CPU/docs/archive)
  已完成工作的过程性记录或归档材料，不作为主维护入口。

## 文档分工

根目录与子目录文档职责如下：

- [AGENTS.md](/home/liangjiaqi/projects/my_visual_CPU/AGENTS.md)
  只保留项目总览、全局约定、workflow、阶段规划和子目录索引。
- [myCPU/AGENTS.md](/home/liangjiaqi/projects/my_visual_CPU/myCPU/AGENTS.md)
  记录 simulator 主体的实现基线、局部规则和已知问题。
- [myCPU/guest/AGENTS.md](/home/liangjiaqi/projects/my_visual_CPU/myCPU/guest/AGENTS.md)
  记录 guest runtime 的实现基线、局部规则和已知问题。
- [readme.md](/home/liangjiaqi/projects/my_visual_CPU/readme.md)
  面向读者的项目概览，保持可读、简洁，不承载过细实现流水账。

## 当前文档索引

- [background/request.md](/home/liangjiaqi/projects/my_visual_CPU/docs/background/request.md)
  项目背景和原始目标说明。
- [design/cpp_refactor_plan.md](/home/liangjiaqi/projects/my_visual_CPU/docs/design/cpp_refactor_plan.md)
  解释为什么要做当前这轮 C++ 结构重构，以及希望得到的模块边界。
- [design/pipeline_integration_prep.md](/home/liangjiaqi/projects/my_visual_CPU/docs/design/pipeline_integration_prep.md)
  记录 `phase1-stable` 冻结后，如何把后续拿到的旧基线流水线代码接回当前主线。
- [design/pipeline_core_integration.md](/home/liangjiaqi/projects/my_visual_CPU/docs/design/pipeline_core_integration.md)
  记录当前这一轮如何只把 Phase 2 pipeline core 重接到主线，同时排除 `debug/frontend`。
- [contracts/platform_mmio_contract.md](/home/liangjiaqi/projects/my_visual_CPU/docs/contracts/platform_mmio_contract.md)
  当前共享 MMIO 平台契约，属于 guest / simulator 共同依赖的实现边界文档。
- [status/code_self_review_2026-03-24.md](/home/liangjiaqi/projects/my_visual_CPU/docs/status/code_self_review_2026-03-24.md)
  最近一次全面代码自检的摘要与后续修复进展；当前用于跟踪仍然有效的风险点。
- [status/kernel_alpha_bringup_status.md](/home/liangjiaqi/projects/my_visual_CPU/docs/status/kernel_alpha_bringup_status.md)
  独立 `kernel_alpha` bring-up 的当前状态、关键历史节点和下一步。
- [templates/status_doc_template.md](/home/liangjiaqi/projects/my_visual_CPU/docs/templates/status_doc_template.md)
  未来状态类文档的统一模板；新增状态文档默认优先按这个结构编写。
- [archive/working-notes](/home/liangjiaqi/projects/my_visual_CPU/docs/archive/working-notes)
  已完成工作的实现过程记录，仅作归档留存，不作为主文档入口。

## 文档维护规则

- 根目录 `AGENTS.md` 不要再次膨胀成实现细节全集。
- 具体实现方式、局部规则、存留问题优先写入对应子树的 `AGENTS.md`。
- 长篇设计、审查、方案和契约文档放在 `docs/`，不要堆进 README。
- README 要持续可读，尤其 guest 相关描述保持概览化，不要写成长串内部细节。
- 状态类文档统一放在 `docs/status/`，优先保留当前状态、仍然有效的风险和少量关键历史节点，不要长期堆积已完成 checklist。
- 新增状态类文档时，默认先套用 [templates/status_doc_template.md](/home/liangjiaqi/projects/my_visual_CPU/docs/templates/status_doc_template.md)。
- 已经从“执行中计划”转成“持续状态跟踪”的文档，应主动收口成模板化结构，而不是继续累积历史步骤。
- 设计、计划、执行记录这类过程性文档在任务完成后应移入 `docs/archive/`，必要信息收口进正式状态文档。
- 报告和总结文档需要清楚区分：
  - 项目 owner 既有已完成工作
  - 已落地的当前重构成果
  - 正在进行的下一步工作
  - 更远期阶段规划

## 新文档建议

后续如果新增以下内容，优先放在 `docs/`：

- 结构设计文档
- bring-up 计划或阶段计划
- 系统性审查报告
- 较长的 known issues / risk 清单
- 平台契约或 guest / simulator 共享接口说明

不要把这类内容重新塞回根目录 `AGENTS.md`。
