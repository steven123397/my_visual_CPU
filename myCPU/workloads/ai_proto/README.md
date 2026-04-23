# AI Proto Workloads

`ai_proto` 是 AI accelerator wave 1 的 host-side packaging/profile 入口。

当前只覆盖两条固定 workload：

- `cnn`
  quantized `conv2d -> relu -> transpose -> reduce` 闭环。
- `gemm`
  semi-precision `fp16 gemm -> fp32 max-pool` 闭环。

## 生成打包产物

```bash
python3 workloads/ai_proto/pack_graph.py --workload cnn --out-dir workloads/ai_proto/generated
python3 workloads/ai_proto/pack_graph.py --workload gemm --out-dir workloads/ai_proto/generated
```

每个 workload 会生成：

- `<name>.graph.bin`
  graph package 二进制。
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

当前这版 `timed-simple` 仍明确采用 `DMA + compute` 不重叠的保守语义，`baseline` 也只输出 `none`，不回退到宿主机 wall-clock。
