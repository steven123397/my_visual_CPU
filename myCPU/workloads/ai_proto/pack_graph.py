#!/usr/bin/env python3
import argparse
import pathlib
import struct
from dataclasses import dataclass


GRAPH_MAGIC = 0x31475041
GRAPH_VERSION = 1
GRAPH_HEADER_BYTES = 40
TENSOR_RECORD_BYTES = 36
OP_RECORD_BYTES = 28
DEPENDENCY_RECORD_BYTES = 4
MEMORY_PLAN_RECORD_BYTES = 20


DTYPE = {
    "int8": 1,
    "int16": 2,
    "int32": 3,
    "fp16": 4,
    "bf16": 5,
    "fp32": 6,
}

ROLE = {
    "input": 1,
    "output": 2,
    "weight": 3,
    "intermediate": 4,
    "constant": 5,
}

OPCODE = {
    "gemm": 1,
    "conv2d": 2,
    "eltwise_relu": 3,
    "pool_max": 4,
    "reduce_sum": 5,
    "layout_transpose": 6,
}


@dataclass
class Tensor:
    dtype: str
    role: str
    rank: int
    dims: tuple[int, int, int, int]
    tile_dims: tuple[int, int, int, int]


@dataclass
class Op:
    opcode: str
    input_dtype: str
    accum_dtype: str
    input0: int
    input1: int
    input2: int
    output: int
    attrs: tuple[int, int, int, int] = (0, 0, 0, 0)


@dataclass
class MemoryPlan:
    tensor_index: int
    system_offset: int
    scratchpad_offset: int
    byte_size: int
    scratchpad_bytes: int


def append_u8(out: bytearray, value: int) -> None:
    out += struct.pack("<B", value)


def append_u16(out: bytearray, value: int) -> None:
    out += struct.pack("<H", value)


def append_u32(out: bytearray, value: int) -> None:
    out += struct.pack("<I", value)


def append_i32(out: bytearray, value: int) -> None:
    out += struct.pack("<i", value)


def serialize_graph_package(scratchpad_budget_bytes: int,
                            tensors: list[Tensor],
                            ops: list[Op],
                            dependencies: list[tuple[int, int]],
                            memory_plan: list[MemoryPlan]) -> bytes:
    tensors_offset = GRAPH_HEADER_BYTES
    ops_offset = tensors_offset + len(tensors) * TENSOR_RECORD_BYTES
    dependencies_offset = ops_offset + len(ops) * OP_RECORD_BYTES
    memory_plan_offset = dependencies_offset + len(dependencies) * DEPENDENCY_RECORD_BYTES
    package_bytes = memory_plan_offset + len(memory_plan) * MEMORY_PLAN_RECORD_BYTES

    out = bytearray()
    append_u32(out, GRAPH_MAGIC)
    append_u16(out, GRAPH_VERSION)
    append_u16(out, GRAPH_HEADER_BYTES)
    append_u32(out, scratchpad_budget_bytes)
    append_u16(out, len(tensors))
    append_u16(out, len(ops))
    append_u16(out, len(dependencies))
    append_u16(out, len(memory_plan))
    append_u32(out, tensors_offset)
    append_u32(out, ops_offset)
    append_u32(out, dependencies_offset)
    append_u32(out, memory_plan_offset)
    append_u32(out, package_bytes)

    for tensor in tensors:
        append_u8(out, DTYPE[tensor.dtype])
        append_u8(out, ROLE[tensor.role])
        append_u8(out, tensor.rank)
        append_u8(out, 0)
        for dim in tensor.dims:
            append_u32(out, dim)
        for tile_dim in tensor.tile_dims:
            append_u32(out, tile_dim)

    for op in ops:
        append_u8(out, OPCODE[op.opcode])
        append_u8(out, DTYPE[op.input_dtype])
        append_u8(out, DTYPE[op.accum_dtype])
        append_u8(out, 0)
        append_u16(out, op.input0)
        append_u16(out, op.input1)
        append_u16(out, op.input2)
        append_u16(out, op.output)
        for attr in op.attrs:
            append_i32(out, attr)

    for source_op, target_op in dependencies:
        append_u16(out, source_op)
        append_u16(out, target_op)

    for entry in memory_plan:
        append_u16(out, entry.tensor_index)
        append_u16(out, 0)
        append_u32(out, entry.system_offset)
        append_u32(out, entry.scratchpad_offset)
        append_u32(out, entry.byte_size)
        append_u32(out, entry.scratchpad_bytes)

    return bytes(out)


def write_manifest(path: pathlib.Path,
                   name: str,
                   graph_package: str,
                   inputs: list[str],
                   outputs: list[str],
                   expected_outputs: list[str],
                   max_ticks: int,
                   source_tag: int) -> None:
    lines = [
        "format=ai_proto_manifest_v1",
        f"name={name}",
        f"graph_package={graph_package}",
    ]
    lines.extend(f"input={item}" for item in inputs)
    lines.extend(f"output={item}" for item in outputs)
    lines.extend(f"expected_output={item}" for item in expected_outputs)
    lines.append(f"max_ticks={max_ticks}")
    lines.append(f"source_tag={source_tag}")
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


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


def create_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Pack fixed AI accelerator graph workloads")
    parser.add_argument("--workload", choices=["cnn", "gemm", "all"], default="all")
    parser.add_argument("--out-dir", required=True)
    return parser


def main(argv: list[str] | None = None) -> int:
    args = create_parser().parse_args(argv)
    out_dir = pathlib.Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)

    if args.workload in ("cnn", "all"):
        build_cnn(out_dir)
    if args.workload in ("gemm", "all"):
        build_gemm(out_dir)

    print(f"packed workload={args.workload} out_dir={out_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
