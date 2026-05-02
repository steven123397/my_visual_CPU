# AI Proto Workloads

`ai_proto` 是 AI accelerator Wave 1 / Wave 2 的 host-side packaging/profile 入口。

当前覆盖 6 条固定 workload：

- `cnn`
  quantized `conv2d -> relu -> transpose -> reduce` 闭环。
- `gemm`
  semi-precision `fp16 gemm -> fp32 max-pool` 闭环。
- `tiny_model`
  Wave 2 的固定小模型风格 workload，当前收口为 `fp16 gemm -> fp32 relu -> fp32 max-pool` 闭环。
  这条线暂时不强行扩到 `conv -> relu -> pool -> fc`，避免为了 workload 打开新的 dtype / op 合同面。
- `dynamic_gemm`
  Wave 2 bounded dynamic shape 的固定 matmul-family workload：graph package 先声明 max dims，manifest 再通过 `runtime_shape_table` 提供本次 `2x8 -> 2x4` runtime dims。
- `dynamic_tiny_model`
  主线 Wave 4 的动态小模型 workload，复用现有 `fp16 gemm -> fp32 relu -> fp32 max-pool` 算子面；graph package 声明 max batch，manifest 通过 runtime shape table 运行本次 `1x3 -> 1x2 -> 1x1` 闭环。
- `dynamic_cnn`
  基于现有 `conv2d -> relu -> transpose -> reduce` 算子面的 bounded dynamic shape workload；graph package 声明 `4x4` 上界，manifest 默认通过 runtime shape table 运行 `3x3 -> 2x2 -> 2` 闭环。
- `tiny_attention_static`
  主线 Wave 4 的 stretch workload，固定为 `fp16 gemm -> fp32 softmax -> fp32 gemm` 的极小静态 attention-like 闭环；只证明当前 graph / scheduler / profile path 能表达这一方向，不代表完整 Transformer runtime。

## 生成打包产物

```bash
python3 workloads/ai_proto/pack_graph.py --workload cnn --out-dir workloads/ai_proto/generated
python3 workloads/ai_proto/pack_graph.py --workload gemm --out-dir workloads/ai_proto/generated
python3 workloads/ai_proto/pack_graph.py --workload tiny_model --out-dir workloads/ai_proto/generated
python3 workloads/ai_proto/pack_graph.py --workload dynamic_gemm --out-dir workloads/ai_proto/generated
python3 workloads/ai_proto/pack_graph.py --workload dynamic_tiny_model --out-dir workloads/ai_proto/generated
python3 workloads/ai_proto/pack_graph.py --workload dynamic_cnn --out-dir workloads/ai_proto/generated
python3 workloads/ai_proto/pack_graph.py --workload tiny_attention_static --out-dir workloads/ai_proto/generated
```

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

## 通过 workloads 体系运行

```bash
make run-workload WORKLOAD_NAME=ai_proto AI_PROTO_WORKLOAD=cnn
make run-workload WORKLOAD_NAME=ai_proto AI_PROTO_WORKLOAD=gemm
make run-workload WORKLOAD_NAME=ai_proto AI_PROTO_WORKLOAD=tiny_model
make run-workload WORKLOAD_NAME=ai_proto AI_PROTO_WORKLOAD=dynamic_gemm
make run-workload WORKLOAD_NAME=ai_proto AI_PROTO_WORKLOAD=dynamic_tiny_model
make run-workload WORKLOAD_NAME=ai_proto AI_PROTO_WORKLOAD=dynamic_cnn
make run-workload WORKLOAD_NAME=ai_proto AI_PROTO_WORKLOAD=tiny_attention_static
```

运行入口会调用：

```bash
./mycpu --ai-profile-manifest workloads/ai_proto/generated/<name>.manifest
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
