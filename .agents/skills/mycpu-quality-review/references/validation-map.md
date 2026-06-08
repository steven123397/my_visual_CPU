# Validation Map

本文件只负责“怎么从现有仓库规则里挑最小验证”，不是新的权威验证来源。

权威顺序始终是：

1. 用户明确要求
2. 根 `AGENTS.md`
3. 最近子树 `AGENTS.md`
4. 当前相关 `status`
5. 本引用文件

## 0. 只读审查

如果当前是 `review-only`：

- 默认不跑测试
- 只给出建议验证命令
- 不因为“顺手验证一下”就扩大工作范围

## 1. 核心 contract 基线

只要触及 `myCPU/AGENTS.md` 中以下路径之一：

- `src/cpu.cpp`
- `src/trap.cpp`
- `src/arch/*`
- `src/mem/*`
- `src/devices/*`
- `src/loader/*`
- `tests/asm/*`
- `tests/unit/*`

至少守住：

```bash
cd myCPU && make test
```

## 2. 执行 / debug / workload 扩门

如果触及以下任一路径：

- `src/main.cpp`
- `src/platform/machine.cpp`
- `src/debug/*`
- `src/exec/*`
- `guest/*`
- `tests/host/*`
- `workloads/*`

额外守住：

```bash
cd myCPU && make test-pipeline
```

## 3. 中等宽度回归门

如果需要 fresh evidence，但改动范围又没大到一上来就必须全量扫仓，可优先使用仓库里已经存在的分层门：

```bash
cd myCPU && make test-fast-smoke
cd myCPU && make test-standard-regression
```

使用规则：

- 它们适合中等宽度收尾或收敛重构前后快速比对
- 不能替代 `AGENTS.md` 已明确要求的更宽 gate
- 用它们时要在报告里说明“这是分层回归门，不是全量完成声明”

## 4. loader / debug CLI / workload harness

这类改动优先跑最窄命中面：

```bash
cd myCPU && make test-unit-binary_loader
cd myCPU && make test-unit-machine_loader_reset
cd myCPU && make test-host-debug_cli_smoke
cd myCPU && make test-host-run_debug_cli_probe
cd myCPU && make test-host-virtio_blk_smoke
cd myCPU && make test-host-xv6_boot_smoke
cd myCPU && make test-host-xv6_shell_smoke
```

只有当改动明确依赖完整 workload path 时，才考虑：

```bash
cd myCPU && make run-workload-xv6
```

## 5. rename / ROB / LSQ / speculation / vector

如果改动主要集中在这些区域，优先用对应 host smoke：

```bash
cd myCPU && make test-host-physical_register_file_smoke
cd myCPU && make test-host-rename_map_smoke
cd myCPU && make test-host-reorder_buffer_smoke
cd myCPU && make test-host-load_store_queue_smoke
cd myCPU && make test-host-pipeline_rename_commit_smoke
cd myCPU && make test-host-pipeline_speculation_contracts_smoke
cd myCPU && make test-host-vector_pipeline_smoke
cd myCPU && make test-host-vector_cnn_smoke
```

## 6. frontend

如果是 frontend-only 改动，默认使用：

```bash
cd frontend && node --test
```

不要把 frontend 改动自动升级成 simulator 全量验证，除非：

- frontend 依赖 debug-server contract
- 改动同时触及 backend / protocol / session 行为

## 7. docs / governance

如果只改：

- `AGENTS.md`
- `docs/*`
- `README.md`
- `.agents/skills/*`

默认不跑 simulator 测试。

应验证的是：

- 单一事实来源有没有被破坏
- `design / plan / status` 分工有没有混
- 新增正式文档时 `docs/index.md` 是否需要同步
- skill / rule 有没有重复全局 superpowers 或制造并行流程

## 8. 收尾证据

在声称“完成 / 已修复 / 已收敛”前：

1. 重新运行本轮真正依赖的命令
2. 读完整输出和退出码
3. 只报告本轮 fresh evidence

不要用以下内容代替验证：

- 旧日志
- 之前某次通过
- 代理说成功
- “理论上应该没问题”
