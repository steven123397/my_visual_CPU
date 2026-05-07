# AI Proto Workloads

`ai_proto` 是 AI accelerator Wave 1 / Wave 2 的 host-side packaging/profile 入口。

在 `Post-Wave 7` 新主线下，这里也开始承接第一批受限用户任务入口：

- `pack_graph.py --task-spec <file> --out-dir <dir>`
  当前支持 `ai_task_spec_v1 / bounded_dynamic_gemm_v1`、
  `ai_task_spec_v1 / bounded_dynamic_cnn_v1` 和
  `ai_task_spec_v1 / bounded_dynamic_tiny_model_v1`，以及
  `ai_task_spec_v1 / static_tiny_attention_v1`。
  它会在 host 侧完成校验、lower 到统一 graph package、生成 runtime shape table、
  expected output、manifest，以及最小顺序 scratchpad memory plan。
  同时还会生成 `<name>.memory_plan.txt`，把同一份 graph package memory-plan
  事实来源以可读 sidecar 形式导出，方便 host-side compile/profile contract 做只读校验。
- `pack_graph.py --demo-v1 --out-dir <dir>`
  固定生成 `Demo V1` 的展示资产：`guest_ai_accel_demo` bridge workload、
  4 条正向 task-spec 样例，以及 1 条只用于 fail-closed 观察的负向 task-spec。
- `run_demo_v1.py --out-dir <dir>`
  固定执行 `Demo V1` 的推荐演示路径：`pack -> run 4 条正向样例 -> 验证 1 条 fail-closed 样例`。

## Demo V1 固定边界

本轮 `Demo V1` 只把下面 5 条入口作为正式展示范围：

- `bounded_dynamic_gemm_v1`
- `bounded_dynamic_cnn_v1`
- `bounded_dynamic_tiny_model_v1`
- `static_tiny_attention_v1`
- `guest_ai_accel_demo`

这 5 条入口共同证明的是：

- host-side `task spec -> pack -> run -> summary` 已经形成固定路径；
- 4 条 task-spec 入口继续复用现有 `dynamic_gemm / dynamic_cnn / dynamic_tiny_model /
  tiny_attention_static` graph package、runtime shape table 和 profile contract；
- `guest_ai_accel_demo` 继续作为 guest/host 共用 submission contract 的 bridge workload。

`Demo V1` 明确不代表：

- 任意模型上传
- 更宽 op family 或通用 compiler
- `DMA + compute overlap`、tile scheduler、multi outstanding queue
- Linux-facing NPU driver
- 大范围 frontend 产品化改造

当前覆盖 8 条固定 workload：

- `cnn`
  quantized `conv2d -> relu -> transpose -> reduce` 闭环。
- `gemm`
  semi-precision `fp16 gemm -> fp32 max-pool` 闭环。
- `tiny_model`
  Wave 2 的固定小模型风格 workload，当前收口为 `fp16 gemm -> fp32 relu -> fp32 max-pool` 闭环。
  这条线暂时不强行扩到 `conv -> relu -> pool -> fc`，避免为了 workload 打开新的 dtype / op 合同面。
- `guest_ai_accel_demo`
  guest `ai_accel_demo` 的 host-side 镜像 workload，固定为 `1x3 -> 1` 的
  `int32 reduce_sum` 静态闭环，用来把 guest demo 的 submission contract 接回现有
  `--ai-profile-manifest` 路径。
- `dynamic_gemm`
  Wave 2 bounded dynamic shape 的固定 matmul-family workload：graph package 先声明 max dims，manifest 再通过 `runtime_shape_table` 提供本次 `2x8 -> 2x4` runtime dims。
- `dynamic_tiny_model`
  主线 Wave 4 的动态小模型 workload，复用现有 `fp16 gemm -> fp32 relu -> fp32 max-pool` 算子面；graph package 声明 max batch，manifest 通过 runtime shape table 运行本次 `1x3 -> 1x2 -> 1x1` 闭环。
- `dynamic_cnn`
  基于现有 `conv2d -> relu -> transpose -> reduce` 算子面的 bounded dynamic shape workload；graph package 声明 `4x4` 上界，manifest 默认通过 runtime shape table 运行 `3x3 -> 2x2 -> 2` 闭环。
- `tiny_attention_static`
  主线 Wave 4 的 stretch workload，固定为 `fp16 gemm -> fp32 softmax -> fp32 gemm` 的极小静态 attention-like 闭环；只证明当前 graph / scheduler / profile path 能表达这一方向，不代表完整 Transformer runtime。

## Demo V1 推荐演示路径

最短固定入口：

```bash
python3 workloads/ai_proto/run_demo_v1.py --out-dir workloads/ai_proto/generated/demo_v1
```

这条路径会固定做 3 件事：

1. 生成 `Demo V1` 所需的全部打包产物。
2. 运行 4 条推荐正向样例：
   `guest_ai_accel_demo`、`custom_dynamic_gemm`、`custom_dynamic_cnn`、
   `custom_dynamic_tiny_model`。
3. 验证 1 条 fail-closed 样例：
   `custom_dynamic_gemm_fail_closed.task_spec.json`。

默认不执行 `custom_tiny_attention_static.manifest`，但 `--demo-v1` 会把它一并打包出来；
如果演示需要第五条正向样例，可单独运行：

```bash
./mycpu --ai-profile-manifest workloads/ai_proto/generated/demo_v1/custom_tiny_attention_static.manifest
```

推荐观察点：

- `guest_ai_accel_demo`
  看 guest/host bridge 是否稳定输出 `name=guest_ai_accel_demo` 与 `ai_profile_op opcode=reduce_sum`。
- `custom_dynamic_gemm`
  看 `shape_mode=dynamic_bounded`、`runtime_shapes=t0:2x8,t2:2x4` 与单 op GEMM summary。
- `custom_dynamic_cnn`
  看 `conv2d -> eltwise_relu -> layout_transpose -> reduce_sum` 四段 itemized summary。
- `custom_dynamic_tiny_model`
  看 `gemm -> relu -> pool_max` 三段 itemized summary 与 `fp16 -> fp32` 小模型路径。
- fail-closed 样例
  看 host-side 直接报
  `bounded_dynamic_gemm_v1 task spec has unexpected top-level key: unexpected_extra`，
  而不是继续生成 manifest 或把错误延后到 device 路径。

## 生成打包产物

```bash
python3 workloads/ai_proto/pack_graph.py --workload cnn --out-dir workloads/ai_proto/generated
python3 workloads/ai_proto/pack_graph.py --workload gemm --out-dir workloads/ai_proto/generated
python3 workloads/ai_proto/pack_graph.py --workload tiny_model --out-dir workloads/ai_proto/generated
python3 workloads/ai_proto/pack_graph.py --workload guest_ai_accel_demo --out-dir workloads/ai_proto/generated
python3 workloads/ai_proto/pack_graph.py --workload dynamic_gemm --out-dir workloads/ai_proto/generated
python3 workloads/ai_proto/pack_graph.py --workload dynamic_tiny_model --out-dir workloads/ai_proto/generated
python3 workloads/ai_proto/pack_graph.py --workload dynamic_cnn --out-dir workloads/ai_proto/generated
python3 workloads/ai_proto/pack_graph.py --workload tiny_attention_static --out-dir workloads/ai_proto/generated
python3 workloads/ai_proto/pack_graph.py --demo-v1 --out-dir workloads/ai_proto/generated/demo_v1
```

受限用户任务入口示例：

```bash
cat > workloads/ai_proto/generated/custom_dynamic_cnn.task_spec.json <<'EOF'
{
  "format": "ai_task_spec_v1",
  "task_kind": "bounded_dynamic_cnn_v1",
  "name": "custom_dynamic_cnn",
  "source_tag": 79,
  "max_ticks": 128,
  "input0": [
    [1, -2, 3],
    [-4, 5, -6],
    [7, -8, 9]
  ],
  "input1": [
    [1, 0],
    [-1, 2]
  ]
}
EOF

python3 workloads/ai_proto/pack_graph.py \
  --task-spec workloads/ai_proto/generated/custom_dynamic_cnn.task_spec.json \
  --out-dir workloads/ai_proto/generated
```

`bounded_dynamic_tiny_model_v1` 示例：

```bash
cat > workloads/ai_proto/generated/custom_dynamic_tiny_model.task_spec.json <<'EOF'
{
  "format": "ai_task_spec_v1",
  "task_kind": "bounded_dynamic_tiny_model_v1",
  "name": "custom_dynamic_tiny_model",
  "source_tag": 83,
  "max_ticks": 128,
  "input0": [
    [0.5, 2.0, -1.0]
  ]
}
EOF

python3 workloads/ai_proto/pack_graph.py \
  --task-spec workloads/ai_proto/generated/custom_dynamic_tiny_model.task_spec.json \
  --out-dir workloads/ai_proto/generated
```

`static_tiny_attention_v1` 示例：

```bash
cat > workloads/ai_proto/generated/custom_tiny_attention_static.task_spec.json <<'EOF'
{
  "format": "ai_task_spec_v1",
  "task_kind": "static_tiny_attention_v1",
  "name": "custom_tiny_attention_static",
  "source_tag": 89,
  "max_ticks": 128,
  "value_vector": [2.0, 6.0]
}
EOF

python3 workloads/ai_proto/pack_graph.py \
  --task-spec workloads/ai_proto/generated/custom_tiny_attention_static.task_spec.json \
  --out-dir workloads/ai_proto/generated
```

`Demo V1` 固定样例会自动生成以下 task-spec 文件：

- `custom_dynamic_gemm.task_spec.json`
- `custom_dynamic_cnn.task_spec.json`
- `custom_dynamic_tiny_model.task_spec.json`
- `custom_tiny_attention_static.task_spec.json`
- `custom_dynamic_gemm_fail_closed.task_spec.json`

`task-spec` 当前固定采用 fail-closed 合同：

- 顶层 `format` 必须是 `ai_task_spec_v1`，`task_kind` 只能是当前已开放的 4 个受限入口。
- 顶层还必须是 JSON object，`name` 必须先通过非空字符串校验；坏 envelope 会在 host-side 直接报错。
- 顶层 key 走白名单校验；未知字段和重复字段都会在 host-side 直接报错，不会被静默忽略或覆盖。
- `name` 必须是安全 basename，不能包含 `.` / `..`、路径分隔符或控制字符；打包产物不会被写出 `--out-dir`。
- `source_tag` 必须是非 `bool` 的 `uint32` 整数；`max_ticks` 必须是非 `bool` 的非零 `uint32` 整数。
- `bounded_dynamic_gemm_v1` / `bounded_dynamic_cnn_v1` 的 `int8` payload 不接受 JSON `bool` 伪装成整数。
- `bounded_dynamic_tiny_model_v1` 的 `input0` 必须是 finite 且能表示为 `fp16`；`static_tiny_attention_v1` 的 `value_vector` 必须是 finite 且能表示为 `fp32`。

每个 workload 会生成：

- `<name>.graph.bin`
  graph package 二进制。
- `<name>.runtime_shape.bin`
  bounded dynamic shape workload 使用的 runtime shape table；静态 workload 不生成。
- `<name>.input*.bin`
  输入 / 权重 tensor。
- `<name>.output0.expected.bin`
  预期输出。
- `<name>.manifest`
  host profile 入口读取的 manifest。
- `<name>.memory_plan.txt`
  host-side 可读 compile / memory-plan sidecar，固定回显 `shape_mode`、
  `scratchpad_budget_bytes`、tensor 数量和每个 memory-plan entry 的
  `role / dtype / system_offset / scratchpad_offset / byte_size / scratchpad_bytes`。
  它不是新的设备 ABI，也不替代 graph package；两者必须继续共享同一份事实来源。
- `<name>.resolved_memory_plan.txt`
  bounded dynamic workload 额外生成的 runtime-shape resolved sidecar。
  它固定回显同一次 manifest 运行会看到的真实 tensor byte_size / scratchpad_bytes，
  并要求继续和共享 runtime-shape resolve 合同保持一致；它同样不是新的设备 ABI。

`task-spec` 当前也生成同样一组产物，只是名字由 `task_spec.name` 决定。

## 通过 workloads 体系运行

```bash
make run-workload WORKLOAD_NAME=ai_proto AI_PROTO_WORKLOAD=cnn
make run-workload WORKLOAD_NAME=ai_proto AI_PROTO_WORKLOAD=gemm
make run-workload WORKLOAD_NAME=ai_proto AI_PROTO_WORKLOAD=tiny_model
make run-workload WORKLOAD_NAME=ai_proto AI_PROTO_WORKLOAD=guest_ai_accel_demo
make run-workload WORKLOAD_NAME=ai_proto AI_PROTO_WORKLOAD=dynamic_gemm
make run-workload WORKLOAD_NAME=ai_proto AI_PROTO_WORKLOAD=dynamic_tiny_model
make run-workload WORKLOAD_NAME=ai_proto AI_PROTO_WORKLOAD=dynamic_cnn
make run-workload WORKLOAD_NAME=ai_proto AI_PROTO_WORKLOAD=tiny_attention_static
```

运行入口会调用：

```bash
./mycpu --ai-profile-manifest workloads/ai_proto/generated/<name>.manifest
```

`Demo V1` 固定 manifest 运行示例：

```bash
./mycpu --ai-profile-manifest workloads/ai_proto/generated/demo_v1/guest_ai_accel_demo.manifest
./mycpu --ai-profile-manifest workloads/ai_proto/generated/demo_v1/custom_dynamic_gemm.manifest
./mycpu --ai-profile-manifest workloads/ai_proto/generated/demo_v1/custom_dynamic_cnn.manifest
./mycpu --ai-profile-manifest workloads/ai_proto/generated/demo_v1/custom_dynamic_tiny_model.manifest
./mycpu --ai-profile-manifest workloads/ai_proto/generated/demo_v1/custom_tiny_attention_static.manifest
python3 workloads/ai_proto/run_demo_v1.py --out-dir workloads/ai_proto/generated/demo_v1
```

输出口径固定为 `simulated cycles`，至少包含：

- `device_cycles`
- `dma_cycles`
- `compute_cycles`
- `stall_cycles`
- `bytes_moved`
- `retired_ops`

如果 workload 使用 bounded dynamic shape，summary 还会额外输出：

- `shape_mode`
- `runtime_shapes`

Wave 3 当前还新增了稳定的 itemized 文本出口：

- `ai_profile_aggregate`
  暴露 `tile_count / scratchpad_peak_bytes / op_count`
- `ai_profile_op`
  按 op 顺序暴露 `op_index / opcode / retired_ops / compute_cycles / stall_cycles / tile_count`

当前这版 `timed-simple` 仍明确采用 `DMA + compute` 不重叠的保守语义，`baseline` 也只输出 `none`，不回退到宿主机 wall-clock。

## Demo V1 最小验证矩阵

- 文档层：
  `git diff --check`
- fresh gate：
  `cd myCPU && make test`
  `cd myCPU && make test-pipeline`
- 演示入口定向 smoke：
  `cd myCPU && make test-host-ai_accelerator_profile_smoke`
