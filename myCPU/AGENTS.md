# AGENTS.md

适用于模拟器、workload 和测试，遵循根 [AGENTS.md](../AGENTS.md)。

- ISA 语义修复落在共享语义层，其他 backend 消费共享语义。
- 改动后选择能覆盖行为与边界的验证；常用入口为
  `make test-fast-smoke`、`make test` 和 `make test-pipeline`。
- 真实 Linux 镜像、发行版 rootfs 和 Spike 属于外部资产，支持声明需相应实跑证据。
