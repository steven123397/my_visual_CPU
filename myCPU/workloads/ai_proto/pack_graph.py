#!/usr/bin/env python3
import argparse
import pathlib
import struct
from task_spec_lowering import (
    DynamicTensor,
    MemoryPlan,
    Op,
    RuntimeShape,
    Tensor,
    build_dynamic_cnn_like,
    build_dynamic_gemm_like,
    build_task_spec as lower_build_task_spec,
    serialize_graph_package,
    serialize_runtime_shape_table,
    write_manifest,
)


def build_cnn(out_dir: pathlib.Path) -> None:
    name = "cnn"
    tensors = [
        Tensor("int8", "input", 2, (4, 4, 0, 0), (4, 4, 0, 0)),
        Tensor("int8", "weight", 2, (2, 2, 0, 0), (2, 2, 0, 0)),
        Tensor("int32", "intermediate", 2, (3, 3, 0, 0), (3, 3, 0, 0)),
        Tensor("int32", "intermediate", 2, (3, 3, 0, 0), (3, 3, 0, 0)),
        Tensor("int32", "intermediate", 2, (3, 3, 0, 0), (3, 3, 0, 0)),
        Tensor("int32", "output", 1, (3, 0, 0, 0), (3, 0, 0, 0)),
    ]
    ops = [
        Op("conv2d", "int8", "int32", 0, 1, 0xFFFF, 2),
        Op("eltwise_relu", "int32", "int32", 2, 0xFFFF, 0xFFFF, 3),
        Op("layout_transpose", "int32", "int32", 3, 0xFFFF, 0xFFFF, 4),
        Op("reduce_sum", "int32", "int32", 4, 0xFFFF, 0xFFFF, 5),
    ]
    dependencies = [(0, 1), (1, 2), (2, 3)]
    memory_plan = [
        MemoryPlan(0, 0, 0, 16, 16),
        MemoryPlan(1, 0, 16, 4, 4),
        MemoryPlan(2, 0, 32, 36, 36),
        MemoryPlan(3, 0, 80, 36, 36),
        MemoryPlan(4, 0, 128, 36, 36),
        MemoryPlan(5, 0, 176, 12, 12),
    ]
    graph = serialize_graph_package(192, tensors, ops, dependencies, memory_plan)
    (out_dir / f"{name}.graph.bin").write_bytes(graph)
    (out_dir / f"{name}.input0.bin").write_bytes(
        struct.pack("<16b", 1, -2, 3, -4, 5, -6, 7, -8, 9, -10, 11, -12, 13, -14, 15, -16)
    )
    (out_dir / f"{name}.input1.bin").write_bytes(struct.pack("<4b", 1, 0, -1, 2))
    (out_dir / f"{name}.output0.expected.bin").write_bytes(struct.pack("<3i", 0, 78, 0))
    write_manifest(
        out_dir / f"{name}.manifest",
        name=name,
        graph_package=f"{name}.graph.bin",
        inputs=[f"{name}.input0.bin", f"{name}.input1.bin"],
        outputs=[f"{name}.output0.actual.bin"],
        expected_outputs=[f"{name}.output0.expected.bin"],
        max_ticks=128,
        source_tag=23,
    )


def build_gemm(out_dir: pathlib.Path) -> None:
    name = "gemm"
    tensors = [
        Tensor("fp16", "input", 2, (2, 2, 0, 0), (2, 2, 0, 0)),
        Tensor("fp16", "weight", 2, (2, 2, 0, 0), (2, 2, 0, 0)),
        Tensor("fp32", "intermediate", 2, (2, 2, 0, 0), (2, 2, 0, 0)),
        Tensor("fp32", "output", 2, (1, 1, 0, 0), (1, 1, 0, 0)),
    ]
    ops = [
        Op("gemm", "fp16", "fp32", 0, 1, 0xFFFF, 2),
        Op("pool_max", "fp32", "fp32", 2, 0xFFFF, 0xFFFF, 3, (2, 2, 2, 2)),
    ]
    dependencies = [(0, 1)]
    memory_plan = [
        MemoryPlan(0, 0, 0, 8, 8),
        MemoryPlan(1, 0, 8, 8, 8),
        MemoryPlan(2, 0, 16, 16, 16),
        MemoryPlan(3, 0, 32, 4, 4),
    ]
    graph = serialize_graph_package(48, tensors, ops, dependencies, memory_plan)
    (out_dir / f"{name}.graph.bin").write_bytes(graph)
    (out_dir / f"{name}.input0.bin").write_bytes(struct.pack("<4H", 0x3C00, 0x4000, 0x3800, 0xBC00))
    (out_dir / f"{name}.input1.bin").write_bytes(struct.pack("<4H", 0x3C00, 0x4000, 0x3E00, 0x3800))
    (out_dir / f"{name}.output0.expected.bin").write_bytes(struct.pack("<f", 4.0))
    write_manifest(
        out_dir / f"{name}.manifest",
        name=name,
        graph_package=f"{name}.graph.bin",
        inputs=[f"{name}.input0.bin", f"{name}.input1.bin"],
        outputs=[f"{name}.output0.actual.bin"],
        expected_outputs=[f"{name}.output0.expected.bin"],
        max_ticks=128,
        source_tag=29,
    )


def build_tiny_model(out_dir: pathlib.Path) -> None:
    name = "tiny_model"
    tensors = [
        Tensor("fp16", "input", 2, (2, 3, 0, 0), (2, 3, 0, 0)),
        Tensor("fp16", "weight", 2, (3, 2, 0, 0), (3, 2, 0, 0)),
        Tensor("fp32", "intermediate", 2, (2, 2, 0, 0), (2, 2, 0, 0)),
        Tensor("fp32", "intermediate", 2, (2, 2, 0, 0), (2, 2, 0, 0)),
        Tensor("fp32", "output", 2, (1, 1, 0, 0), (1, 1, 0, 0)),
    ]
    ops = [
        Op("gemm", "fp16", "fp32", 0, 1, 0xFFFF, 2),
        Op("eltwise_relu", "fp32", "fp32", 2, 0xFFFF, 0xFFFF, 3),
        Op("pool_max", "fp32", "fp32", 3, 0xFFFF, 0xFFFF, 4, (2, 2, 2, 2)),
    ]
    dependencies = [(0, 1), (1, 2)]
    memory_plan = [
        MemoryPlan(0, 0, 0, 12, 12),
        MemoryPlan(1, 0, 12, 12, 12),
        MemoryPlan(2, 0, 24, 16, 16),
        MemoryPlan(3, 0, 40, 16, 16),
        MemoryPlan(4, 0, 56, 4, 4),
    ]
    graph = serialize_graph_package(64, tensors, ops, dependencies, memory_plan)
    (out_dir / f"{name}.graph.bin").write_bytes(graph)
    (out_dir / f"{name}.input0.bin").write_bytes(
        struct.pack("<6H", 0x3C00, 0xC000, 0x4200, 0x3800, 0x4000, 0xBC00)
    )
    (out_dir / f"{name}.input1.bin").write_bytes(
        struct.pack("<6H", 0x3C00, 0xBC00, 0x4000, 0x3800, 0xBC00, 0x3E00)
    )
    (out_dir / f"{name}.output0.expected.bin").write_bytes(struct.pack("<f", 5.5))
    write_manifest(
        out_dir / f"{name}.manifest",
        name=name,
        graph_package=f"{name}.graph.bin",
        inputs=[f"{name}.input0.bin", f"{name}.input1.bin"],
        outputs=[f"{name}.output0.actual.bin"],
        expected_outputs=[f"{name}.output0.expected.bin"],
        max_ticks=128,
        source_tag=37,
    )


def build_guest_ai_accel_demo(out_dir: pathlib.Path) -> None:
    name = "guest_ai_accel_demo"
    tensors = [
        Tensor("int32", "input", 2, (1, 3, 0, 0), (1, 3, 0, 0)),
        Tensor("int32", "output", 1, (1, 0, 0, 0), (1, 0, 0, 0)),
    ]
    ops = [
        Op("reduce_sum", "int32", "int32", 0, 0xFFFF, 0xFFFF, 1),
    ]
    memory_plan = [
        MemoryPlan(0, 0, 0, 12, 12),
        MemoryPlan(1, 0, 12, 4, 4),
    ]
    graph = serialize_graph_package(16, tensors, ops, [], memory_plan)
    (out_dir / f"{name}.graph.bin").write_bytes(graph)
    (out_dir / f"{name}.input0.bin").write_bytes(struct.pack("<3i", 1, 2, 3))
    (out_dir / f"{name}.output0.expected.bin").write_bytes(struct.pack("<i", 6))
    write_manifest(
        out_dir / f"{name}.manifest",
        name=name,
        graph_package=f"{name}.graph.bin",
        inputs=[f"{name}.input0.bin"],
        outputs=[f"{name}.output0.actual.bin"],
        expected_outputs=[f"{name}.output0.expected.bin"],
        max_ticks=128,
        source_tag=0x33,
    )


def build_dynamic_gemm(out_dir: pathlib.Path) -> None:
    build_dynamic_gemm_like(
        out_dir=out_dir,
        name="dynamic_gemm",
        source_tag=41,
        max_ticks=128,
        input0_rows=[
            [1, 2, 3, 4, 5, 6, 7, 8],
            [-1, 0, 1, 2, 3, 4, 5, 6],
        ],
        input1_rows=[
            [1, 0, 0, 0],
            [0, 1, 0, 0],
            [0, 0, 1, 0],
            [0, 0, 0, 0],
            [0, 0, 0, 0],
            [0, 0, 0, 0],
            [0, 0, 0, 0],
            [0, 0, 0, 1],
        ],
    )


def build_dynamic_tiny_model(out_dir: pathlib.Path) -> None:
    name = "dynamic_tiny_model"
    tensors = [
        Tensor("fp16", "input", 2, (2, 3, 0, 0), (1, 3, 0, 0)),
        Tensor("fp16", "weight", 2, (3, 2, 0, 0), (3, 2, 0, 0)),
        Tensor("fp32", "intermediate", 2, (2, 2, 0, 0), (1, 2, 0, 0)),
        Tensor("fp32", "intermediate", 2, (2, 2, 0, 0), (1, 2, 0, 0)),
        Tensor("fp32", "output", 2, (2, 1, 0, 0), (1, 1, 0, 0)),
    ]
    ops = [
        Op("gemm", "fp16", "fp32", 0, 1, 0xFFFF, 2),
        Op("eltwise_relu", "fp32", "fp32", 2, 0xFFFF, 0xFFFF, 3),
        Op("pool_max", "fp32", "fp32", 3, 0xFFFF, 0xFFFF, 4, (1, 2, 1, 2)),
    ]
    dependencies = [(0, 1), (1, 2)]
    memory_plan = [
        MemoryPlan(0, 0, 0, 12, 12),
        MemoryPlan(1, 0, 12, 12, 12),
        MemoryPlan(2, 0, 24, 16, 16),
        MemoryPlan(3, 0, 40, 16, 16),
        MemoryPlan(4, 0, 56, 8, 8),
    ]
    graph = serialize_graph_package(
        64,
        tensors,
        ops,
        dependencies,
        memory_plan,
        shape_mode="dynamic_bounded",
        dynamic_tensors=[
            DynamicTensor(0, 12),
            DynamicTensor(2, 16),
            DynamicTensor(3, 16),
            DynamicTensor(4, 8),
        ],
    )
    runtime_shape_table = serialize_runtime_shape_table([
        RuntimeShape(0, 2, (1, 3, 0, 0)),
        RuntimeShape(2, 2, (1, 2, 0, 0)),
        RuntimeShape(3, 2, (1, 2, 0, 0)),
        RuntimeShape(4, 2, (1, 1, 0, 0)),
    ])
    (out_dir / f"{name}.graph.bin").write_bytes(graph)
    (out_dir / f"{name}.runtime_shape.bin").write_bytes(runtime_shape_table)
    (out_dir / f"{name}.input0.bin").write_bytes(struct.pack("<3H", 0x3C00, 0xC000, 0x4200))
    (out_dir / f"{name}.input1.bin").write_bytes(
        struct.pack("<6H", 0x3C00, 0xBC00, 0x4000, 0x3800, 0xBC00, 0x3E00)
    )
    (out_dir / f"{name}.output0.expected.bin").write_bytes(struct.pack("<f", 2.5))
    write_manifest(
        out_dir / f"{name}.manifest",
        name=name,
        graph_package=f"{name}.graph.bin",
        runtime_shape_table=f"{name}.runtime_shape.bin",
        inputs=[f"{name}.input0.bin", f"{name}.input1.bin"],
        outputs=[f"{name}.output0.actual.bin"],
        expected_outputs=[f"{name}.output0.expected.bin"],
        max_ticks=128,
        source_tag=45,
    )


def build_dynamic_cnn(out_dir: pathlib.Path) -> None:
    build_dynamic_cnn_like(
        out_dir=out_dir,
        name="dynamic_cnn",
        source_tag=67,
        max_ticks=128,
        input0_rows=[
            [1, -2, 3],
            [-4, 5, -6],
            [7, -8, 9],
        ],
        input1_rows=[
            [1, 0],
            [-1, 2],
        ],
    )


def build_tiny_attention_static(out_dir: pathlib.Path) -> None:
    name = "tiny_attention_static"
    tensors = [
        Tensor("fp16", "input", 2, (1, 2, 0, 0), (1, 2, 0, 0)),
        Tensor("fp16", "weight", 2, (2, 2, 0, 0), (2, 2, 0, 0)),
        Tensor("fp32", "intermediate", 2, (1, 2, 0, 0), (1, 2, 0, 0)),
        Tensor("fp32", "intermediate", 2, (1, 2, 0, 0), (1, 2, 0, 0)),
        Tensor("fp32", "weight", 2, (2, 1, 0, 0), (2, 1, 0, 0)),
        Tensor("fp32", "output", 2, (1, 1, 0, 0), (1, 1, 0, 0)),
    ]
    ops = [
        Op("gemm", "fp16", "fp32", 0, 1, 0xFFFF, 2),
        Op("softmax", "fp32", "fp32", 2, 0xFFFF, 0xFFFF, 3),
        Op("gemm", "fp32", "fp32", 3, 4, 0xFFFF, 5),
    ]
    dependencies = [(0, 1), (1, 2)]
    memory_plan = [
        MemoryPlan(0, 0, 0, 4, 4),
        MemoryPlan(1, 0, 8, 8, 8),
        MemoryPlan(2, 0, 16, 8, 8),
        MemoryPlan(3, 0, 24, 8, 8),
        MemoryPlan(4, 0, 32, 8, 8),
        MemoryPlan(5, 0, 48, 4, 4),
    ]
    graph = serialize_graph_package(64, tensors, ops, dependencies, memory_plan)
    (out_dir / f"{name}.graph.bin").write_bytes(graph)
    (out_dir / f"{name}.input0.bin").write_bytes(struct.pack("<2H", 0x0000, 0x0000))
    (out_dir / f"{name}.input1.bin").write_bytes(struct.pack("<4H", 0x3C00, 0x3C00, 0x3C00, 0x3C00))
    (out_dir / f"{name}.input2.bin").write_bytes(struct.pack("<2f", 1.0, 3.0))
    (out_dir / f"{name}.output0.expected.bin").write_bytes(struct.pack("<f", 2.0))
    write_manifest(
        out_dir / f"{name}.manifest",
        name=name,
        graph_package=f"{name}.graph.bin",
        inputs=[f"{name}.input0.bin", f"{name}.input1.bin", f"{name}.input2.bin"],
        outputs=[f"{name}.output0.actual.bin"],
        expected_outputs=[f"{name}.output0.expected.bin"],
        max_ticks=128,
        source_tag=61,
    )


def create_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Pack fixed AI accelerator graph workloads")
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument(
        "--workload",
        choices=[
            "cnn",
            "gemm",
            "tiny_model",
            "guest_ai_accel_demo",
            "dynamic_gemm",
            "dynamic_tiny_model",
            "dynamic_cnn",
            "tiny_attention_static",
            "all",
        ],
    )
    mode.add_argument("--task-spec")
    parser.add_argument("--out-dir", required=True)
    return parser


def main(argv: list[str] | None = None) -> int:
    args = create_parser().parse_args(argv)
    out_dir = pathlib.Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    if args.task_spec is not None:
        task_name = lower_build_task_spec(pathlib.Path(args.task_spec), out_dir)
        print(f"packed task_spec={args.task_spec} task_name={task_name} out_dir={out_dir}")
        return 0

    if args.workload in ("cnn", "all"):
        build_cnn(out_dir)
    if args.workload in ("gemm", "all"):
        build_gemm(out_dir)
    if args.workload in ("tiny_model", "all"):
        build_tiny_model(out_dir)
    if args.workload in ("guest_ai_accel_demo", "all"):
        build_guest_ai_accel_demo(out_dir)
    if args.workload in ("dynamic_gemm", "all"):
        build_dynamic_gemm(out_dir)
    if args.workload in ("dynamic_tiny_model", "all"):
        build_dynamic_tiny_model(out_dir)
    if args.workload in ("dynamic_cnn", "all"):
        build_dynamic_cnn(out_dir)
    if args.workload in ("tiny_attention_static", "all"):
        build_tiny_attention_static(out_dir)

    print(f"packed workload={args.workload} out_dir={out_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
