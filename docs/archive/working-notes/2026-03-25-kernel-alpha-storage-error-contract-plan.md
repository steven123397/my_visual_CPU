# kernel_alpha storage 错误合同扩展实现计划

> 归档说明：该计划对应工作已经完成，当前正式状态请以 [docs/status/kernel_alpha_bringup_status.md](/home/liangjiaqi/projects/my_visual_CPU/docs/status/kernel_alpha_bringup_status.md) 为准。本文件仅保留当时的实现过程记录。

> **面向 AI 代理的工作者：** 必需子技能：使用 superpowers:subagent-driven-development（推荐）或 superpowers:executing-plans 逐任务实现此计划。步骤使用复选框（`- [ ]`）语法来跟踪进度。

**目标：** 为独立 `kernel_alpha` 路径补齐 guest 侧最小 storage 错误合同消费能力，并新增 `BAD_BLOCK_COUNT` 独立负向回归。

**架构：** 保持 `SimpleStorage` 设备实现不变，只在 guest platform / storage helper 上补最小接口，再通过新的 `kernel_alpha` 负向 demo 消费这些合同。现有 `kernel_alpha_demo`、`kernel_alpha_fault_demo`、`kernel_alpha_storage_no_media_demo` 行为保持不变。

**技术栈：** C11、RISC-V assembly、GNU Make、guest platform MMIO layer、kernel_alpha demos

---

### 任务 1：新增失败的 `kernel_alpha` storage 错误合同回归

**文件：**
- 修改：`myCPU/Makefile`
- 创建：`myCPU/guest/kernel_alpha/storage_bad_block_count_main.c`

- [ ] **步骤 1：先把新 demo 挂进构建和测试入口**

在 `Makefile` 中新增：

- `GUEST_KERNEL_ALPHA_STORAGE_BAD_BLOCK_COUNT_DEMO`
- 对应的对象列表与链接规则
- `test-guest-kernel_alpha_storage_bad_block_count_demo`
- 将该测试纳入 `make test`

预期输出固定为 `KMVBX`。

- [ ] **步骤 2：先放一个最小占位 demo，让新测试失败**

新建 `storage_bad_block_count_main.c`，先保留最小入口或错误 marker，使：

运行：`cd myCPU && make test-guest-kernel_alpha_storage_bad_block_count_demo`

预期：FAIL，原因是输出不等于 `KMVBX`。

- [ ] **步骤 3：确认失败是“行为未实现”，不是构建错误**

要求：测试能完成编译和运行，失败点应是输出不匹配。

### 任务 2：补 guest storage/platform 最小错误合同 helper

**文件：**
- 修改：`myCPU/guest/include/platform.h`
- 修改：`myCPU/guest/include/platform_drivers.inc`
- 修改：`myCPU/guest/include/storage.h`
- 修改：`myCPU/guest/kernel/storage.c`

- [ ] **步骤 1：扩 platform 接口**

新增最小 helper：

- `platform_storage_write_u64`
- `platform_storage_read_status`
- `platform_storage_read_error`
- `platform_storage_issue_command`
- `platform_storage_read_block_custom`

- [ ] **步骤 2：扩 platform 汇编实现**

在 `platform_drivers.inc` 中按当前 MMIO 合同实现以上 helper，保持同步 polling 模型。

- [ ] **步骤 3：扩 storage 高层 helper**

在 `storage.h` / `storage.c` 中新增：

- `storage_status`
- `storage_error`
- `storage_clear_error`
- `storage_read_block_with_count`

并保持现有：

- `storage_probe`
- `storage_read_block`

继续兼容旧行为。

- [ ] **步骤 4：运行针对性构建与已有 storage 回归**

运行：

- `cd myCPU && make test-guest-kernel_alpha_storage_no_media_demo`

预期：PASS，输出 `KMVNX`。

### 任务 3：实现 `BAD_BLOCK_COUNT` 负向 demo

**文件：**
- 修改：`myCPU/guest/kernel_alpha/storage_bad_block_count_main.c`

- [ ] **步骤 1：复用现有 kernel_alpha VM bring-up 模式**

实现：

- `memory_init`
- `runtime_context_reset`
- trap context activate
- `pmm_init`
- kernel 显式映射 + storage lazy map
- 启用 VM

输出阶段 marker：

- `K`
- `M`
- `V`

- [ ] **步骤 2：实现错误合同校验**

使用 `storage_read_block_with_count(0, 2, buffer)` 触发：

- 返回 `STORAGE_ERR_BAD_BLOCK_COUNT`
- `storage_status()` 含 `STORAGE_STATUS_ERROR`
- `storage_error()` 为 `STORAGE_ERR_BAD_BLOCK_COUNT`

成功后输出 `B`。

- [ ] **步骤 3：实现清错合同校验**

调用 `storage_clear_error()` 后校验：

- `storage_status()` 不再含 `STORAGE_STATUS_ERROR`
- `storage_error()` 为 `STORAGE_ERR_NONE`

最后进入既有 `panic_shutdown()`，形成结尾 `X`。

- [ ] **步骤 4：运行新回归并验证通过**

运行：

- `cd myCPU && make test-guest-kernel_alpha_storage_bad_block_count_demo`

预期：PASS，输出 `KMVBX`。

### 任务 4：同步状态文档并跑全量验证

**文件：**
- 修改：`docs/status/kernel_alpha_bringup_status.md`
- 修改：`readme.md`

- [ ] **步骤 1：同步状态文档**

更新：

- 新增 `guest_kernel_alpha_storage_bad_block_count_demo`
- 说明其覆盖 `BAD_BLOCK_COUNT` + clear-error 合同

- [ ] **步骤 2：运行 kernel_alpha 相关回归**

运行：

- `cd myCPU && make test-guest-kernel_alpha_demo`
- `cd myCPU && make test-guest-kernel_alpha_fault_demo`
- `cd myCPU && make test-guest-kernel_alpha_storage_no_media_demo`
- `cd myCPU && make test-guest-kernel_alpha_storage_bad_block_count_demo`

预期：全部 PASS。

- [ ] **步骤 3：运行全量验证**

运行：

- `cd myCPU && make test`

预期：全部 PASS。
