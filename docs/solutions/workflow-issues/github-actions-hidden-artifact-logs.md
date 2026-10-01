---
title: "GitHub Actions 日志 artifact 被隐藏目录过滤"
date: 2026-10-01
problem_type: integration_issue
component: validation
module: github_actions
severity: medium
symptoms:
  - "日志已经写入，但 upload-artifact 默认搜索不到文件"
root_cause: "隐藏日志目录被固定版本 Action 的默认 glob 过滤"
resolution_type: config_change
tags:
  - github-actions
  - artifact
  - hidden-files
---

# GitHub Actions 日志 artifact 被隐藏目录过滤

## Context

初审配置把三个 job 的日志放在 `.ci-logs/`。日志上传使用固定 Action `043fb46d1a93c77aae656e7c1c64a875d1fc6a0a`，设置 `if-no-files-found: error`；即使测试通过，上传也可能失败，无法形成可下载的验证证据。

## Root Cause

本次审查读取固定 Action 的 search 实现，并用其锁定的 `@actions/glob` 0.6.1 实跑：默认搜索隐藏目录返回零文件；允许隐藏文件或改成非隐藏目录后可发现日志。配置指向的路径存在，并不意味着上传器会遍历它。

## Guidance / Solution

本次采用专用非隐藏目录 `ci-logs/`，保持默认隐藏文件过滤。三个 job 的写入与上传路径同时修改，前端进入子目录后写入 `../ci-logs/`。当前完整回归定义见 [regression.yml](../../../.github/workflows/regression.yml) 的 `.github/workflows/regression.yml:24`、`:54`、`:85`；上传缺文件仍按 `:86` 失败。

## Why This Matters

测试退出码、上传状态和验证证据是不同的结果。目录存在检查或 YAML 解析无法发现上传器的遍历过滤；增加隐藏文件上传范围也需要确认是否会纳入日志之外的内容。

## Prevention & Detection

复用固定版本依赖做真实文件发现实验，覆盖每个 job 的实际工作目录。再用精确 run / attempt 下载 artifact，核对日志、环境与实际 checkout SHA；失败和取消不能记为通过，runner 终止也可能阻止上传。

已下载核对 [日常 36797856401/1](https://github.com/steven123397/my_visual_CPU/actions/runs/36797856401/attempts/1)、[完整 36797868411/1](https://github.com/steven123397/my_visual_CPU/actions/runs/36797868411/attempts/1) 和 [故障 36738372503/1](https://github.com/steven123397/my_visual_CPU/actions/runs/36738372503/attempts/1) 的日志。后续升级 Action 时，应重新核对其发现规则；本次固定版本实验不替代升级后的验证。
