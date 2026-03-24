# AGENTS.md

## 适用范围

本文件适用于 [docs](/home/liangjiaqi/projects/my_visual_CPU/docs) 子树下的规划、契约、设计、审查和说明类文档。

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

- [cpp_refactor_plan.md](/home/liangjiaqi/projects/my_visual_CPU/docs/cpp_refactor_plan.md)
  解释为什么要做当前这轮 C++ 结构重构，以及希望得到的模块边界。
- [platform_mmio_contract.md](/home/liangjiaqi/projects/my_visual_CPU/docs/platform_mmio_contract.md)
  当前共享 MMIO 平台契约，属于 guest / simulator 共同依赖的实现边界文档。
- [request.md](/home/liangjiaqi/projects/my_visual_CPU/docs/request.md)
  项目背景和原始目标说明。
- [code_self_review_2026-03-24.md](/home/liangjiaqi/projects/my_visual_CPU/docs/code_self_review_2026-03-24.md)
  最近一次全面代码自检结果；当前应结合文内后续状态更新使用，而不是把它机械地当作“全部未修问题”清单。
- [kernel_alpha_bringup_plan_2026-03-25.md](/home/liangjiaqi/projects/my_visual_CPU/docs/kernel_alpha_bringup_plan_2026-03-25.md)
  第一次独立 kernel alpha bring-up 的里程碑定义、实现计划和首个可回归成功标记。

## 文档维护规则

- 根目录 `AGENTS.md` 不要再次膨胀成实现细节全集。
- 具体实现方式、局部规则、存留问题优先写入对应子树的 `AGENTS.md`。
- 长篇设计、审查、方案和契约文档放在 `docs/`，不要堆进 README。
- README 要持续可读，尤其 guest 相关描述保持概览化，不要写成长串内部细节。
- 历史审查文档在后续问题被修复后，应补状态更新，避免 dated review 与当前仓库状态长期脱节。
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
