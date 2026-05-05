#!/usr/bin/env python3
import json
import pathlib
import struct
from dataclasses import dataclass


GRAPH_MAGIC = 0x31475041
GRAPH_VERSION = 1
GRAPH_BASE_HEADER_BYTES = 40
GRAPH_EXTENDED_HEADER_BYTES = 56
TENSOR_RECORD_BYTES = 36
OP_RECORD_BYTES = 28
DEPENDENCY_RECORD_BYTES = 4
MEMORY_PLAN_RECORD_BYTES = 20
DYNAMIC_TENSOR_RECORD_BYTES = 8
RUNTIME_SHAPE_RECORD_BYTES = 20


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
    "softmax": 7,
}

SHAPE_MODE = {
    "static": 0,
    "dynamic_bounded": 1,
}

TRAINING_MODE = {
    "inference": 0,
    "training_reserved": 1,
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


@dataclass
class DynamicTensor:
    tensor_index: int
    max_tensor_bytes: int


@dataclass
class RuntimeShape:
    tensor_index: int
    rank: int
    dims: tuple[int, int, int, int]


@dataclass
class GemmTaskSpec:
    name: str
    source_tag: int
    max_ticks: int
    input0_rows: list[list[int]]
    input1_rows: list[list[int]]


@dataclass
class CnnTaskSpec:
    name: str
    source_tag: int
    max_ticks: int
    input0_rows: list[list[int]]
    input1_rows: list[list[int]]


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
                            memory_plan: list[MemoryPlan],
                            shape_mode: str = "static",
                            training_mode: str = "inference",
                            dynamic_tensors: list[DynamicTensor] | None = None) -> bytes:
    dynamic_tensors = dynamic_tensors or []
    header_bytes = (
        GRAPH_EXTENDED_HEADER_BYTES
        if shape_mode != "static" or training_mode != "inference" or dynamic_tensors
        else GRAPH_BASE_HEADER_BYTES
    )
    tensors_offset = header_bytes
    ops_offset = tensors_offset + len(tensors) * TENSOR_RECORD_BYTES
    dependencies_offset = ops_offset + len(ops) * OP_RECORD_BYTES
    memory_plan_offset = dependencies_offset + len(dependencies) * DEPENDENCY_RECORD_BYTES
    dynamic_tensors_offset = memory_plan_offset + len(memory_plan) * MEMORY_PLAN_RECORD_BYTES
    package_bytes = dynamic_tensors_offset + len(dynamic_tensors) * DYNAMIC_TENSOR_RECORD_BYTES

    out = bytearray()
    append_u32(out, GRAPH_MAGIC)
    append_u16(out, GRAPH_VERSION)
    append_u16(out, header_bytes)
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
    if header_bytes == GRAPH_EXTENDED_HEADER_BYTES:
        append_u8(out, SHAPE_MODE[shape_mode])
        append_u8(out, TRAINING_MODE[training_mode])
        append_u16(out, len(dynamic_tensors))
        append_u32(out, dynamic_tensors_offset)
        append_u32(out, 0)
        append_u32(out, 0)

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

    for metadata in dynamic_tensors:
        append_u16(out, metadata.tensor_index)
        append_u16(out, 0)
        append_u32(out, metadata.max_tensor_bytes)

    return bytes(out)


def serialize_runtime_shape_table(runtime_shapes: list[RuntimeShape]) -> bytes:
    out = bytearray()
    for runtime_shape in runtime_shapes:
        append_u16(out, runtime_shape.tensor_index)
        append_u8(out, runtime_shape.rank)
        append_u8(out, 0)
        for dim in runtime_shape.dims:
            append_u32(out, dim)
    return bytes(out)


def fail(message: str) -> None:
    raise SystemExit(message)


def product(values: tuple[int, int, int, int], rank: int) -> int:
    count = 1
    for index in range(rank):
        count *= values[index]
    return count


def tensor_byte_size(tensor: Tensor) -> int:
    return product(tensor.dims, tensor.rank) * {
        "int8": 1,
        "int16": 2,
        "int32": 4,
        "fp16": 2,
        "bf16": 2,
        "fp32": 4,
    }[tensor.dtype]


def align_up(value: int, alignment: int) -> int:
    if alignment <= 1:
        return value
    return ((value + alignment - 1) // alignment) * alignment


def build_sequential_memory_plan(tensors: list[Tensor],
                                 alignment: int = 1,
                                 align_total: bool = False) -> tuple[list[MemoryPlan], int]:
    memory_plan: list[MemoryPlan] = []
    scratchpad_offset = 0
    for tensor_index, tensor in enumerate(tensors):
        scratchpad_offset = align_up(scratchpad_offset, alignment)
        byte_size = tensor_byte_size(tensor)
        memory_plan.append(
            MemoryPlan(
                tensor_index=tensor_index,
                system_offset=0,
                scratchpad_offset=scratchpad_offset,
                byte_size=byte_size,
                scratchpad_bytes=byte_size,
            )
        )
        scratchpad_offset += byte_size
    if align_total:
        scratchpad_offset = align_up(scratchpad_offset, alignment)
    return memory_plan, scratchpad_offset


def require_json_object(value: object, label: str) -> dict:
    if not isinstance(value, dict):
        fail(f"{label} must be a JSON object")
    return value


def require_string(value: object, label: str) -> str:
    if not isinstance(value, str) or value == "":
        fail(f"{label} must be a non-empty string")
    return value


def require_int(value: object, label: str) -> int:
    if not isinstance(value, int):
        fail(f"{label} must be an integer")
    return value


def require_i8_square_matrix(value: object,
                             label: str,
                             min_size: int,
                             max_size: int) -> list[list[int]]:
    if not isinstance(value, list):
        fail(f"{label} must be a 2D array")
    if len(value) < min_size or len(value) > max_size:
        fail(f"{label} row count must be between {min_size} and {max_size}")
    matrix: list[list[int]] = []
    expected_cols = len(value)
    for row_index, row in enumerate(value):
        if not isinstance(row, list) or len(row) != expected_cols:
            fail(f"{label} row {row_index} must have exactly {expected_cols} columns")
        parsed_row: list[int] = []
        for col_index, item in enumerate(row):
            if not isinstance(item, int) or item < -128 or item > 127:
                fail(f"{label} row {row_index} column {col_index} must be an int8 value")
            parsed_row.append(item)
        matrix.append(parsed_row)
    return matrix


def require_i8_matrix(value: object, label: str, expected_cols: int, min_rows: int, max_rows: int) -> list[list[int]]:
    if not isinstance(value, list):
        fail(f"{label} must be a 2D array")
    if len(value) < min_rows or len(value) > max_rows:
        fail(f"{label} row count must be between {min_rows} and {max_rows}")
    matrix: list[list[int]] = []
    for row_index, row in enumerate(value):
        if not isinstance(row, list) or len(row) != expected_cols:
            fail(f"{label} row {row_index} must have exactly {expected_cols} columns")
        parsed_row: list[int] = []
        for col_index, item in enumerate(row):
            if not isinstance(item, int) or item < -128 or item > 127:
                fail(f"{label} row {row_index} column {col_index} must be an int8 value")
            parsed_row.append(item)
        matrix.append(parsed_row)
    return matrix


def flatten_i8_matrix(rows: list[list[int]]) -> bytes:
    flat = [item for row in rows for item in row]
    return struct.pack(f"<{len(flat)}b", *flat)


def gemm_i8_i32(lhs_rows: list[list[int]], rhs_rows: list[list[int]]) -> list[int]:
    row_count = len(lhs_rows)
    k_dim = len(lhs_rows[0])
    col_count = len(rhs_rows[0])
    result: list[int] = []
    for row_index in range(row_count):
        for col_index in range(col_count):
            total = 0
            for k_index in range(k_dim):
                total += lhs_rows[row_index][k_index] * rhs_rows[k_index][col_index]
            result.append(total)
    return result


def conv2d_i8_i32(input_rows: list[list[int]], kernel_rows: list[list[int]]) -> list[list[int]]:
    output_rows = len(input_rows) - len(kernel_rows) + 1
    output_cols = len(input_rows[0]) - len(kernel_rows[0]) + 1
    result: list[list[int]] = []
    for row_index in range(output_rows):
        output_row: list[int] = []
        for col_index in range(output_cols):
            total = 0
            for kernel_row in range(len(kernel_rows)):
                for kernel_col in range(len(kernel_rows[0])):
                    total += (
                        input_rows[row_index + kernel_row][col_index + kernel_col]
                        * kernel_rows[kernel_row][kernel_col]
                    )
            output_row.append(total)
        result.append(output_row)
    return result


def relu_i32(rows: list[list[int]]) -> list[list[int]]:
    return [[max(0, item) for item in row] for row in rows]


def transpose_rows(rows: list[list[int]]) -> list[list[int]]:
    return [list(column) for column in zip(*rows)]


def reduce_sum_rows(rows: list[list[int]]) -> list[int]:
    return [sum(row) for row in rows]


def write_manifest(path: pathlib.Path,
                   name: str,
                   graph_package: str,
                   inputs: list[str],
                   outputs: list[str],
                   expected_outputs: list[str],
                   max_ticks: int,
                   source_tag: int,
                   runtime_shape_table: str | None = None) -> None:
    lines = [
        "format=ai_proto_manifest_v1",
        f"name={name}",
        f"graph_package={graph_package}",
    ]
    if runtime_shape_table is not None:
        lines.append(f"runtime_shape_table={runtime_shape_table}")
    lines.extend(f"input={item}" for item in inputs)
    lines.extend(f"output={item}" for item in outputs)
    lines.extend(f"expected_output={item}" for item in expected_outputs)
    lines.append(f"max_ticks={max_ticks}")
    lines.append(f"source_tag={source_tag}")
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def parse_task_spec(path: pathlib.Path) -> GemmTaskSpec | CnnTaskSpec:
    try:
        payload = json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError:
        fail(f"task spec file does not exist: {path}")
    except json.JSONDecodeError as exc:
        fail(f"task spec JSON decode failed: {exc}")

    spec = require_json_object(payload, "task spec")
    format_name = require_string(spec.get("format"), "task spec format")
    if format_name != "ai_task_spec_v1":
        fail("task spec format must be ai_task_spec_v1")
    task_kind = require_string(spec.get("task_kind"), "task spec task_kind")
    if task_kind == "bounded_dynamic_gemm_v1":
        return GemmTaskSpec(
            name=require_string(spec.get("name"), "bounded_dynamic_gemm_v1 name"),
            source_tag=require_int(spec.get("source_tag", 73), "bounded_dynamic_gemm_v1 source_tag"),
            max_ticks=require_int(spec.get("max_ticks", 128), "bounded_dynamic_gemm_v1 max_ticks"),
            input0_rows=require_i8_matrix(spec.get("input0"), "bounded_dynamic_gemm_v1 input0", 8, 1, 2),
            input1_rows=require_i8_matrix(spec.get("input1"), "bounded_dynamic_gemm_v1 input1", 4, 8, 8),
        )
    if task_kind == "bounded_dynamic_cnn_v1":
        return CnnTaskSpec(
            name=require_string(spec.get("name"), "bounded_dynamic_cnn_v1 name"),
            source_tag=require_int(spec.get("source_tag", 79), "bounded_dynamic_cnn_v1 source_tag"),
            max_ticks=require_int(spec.get("max_ticks", 128), "bounded_dynamic_cnn_v1 max_ticks"),
            input0_rows=require_i8_square_matrix(spec.get("input0"), "bounded_dynamic_cnn_v1 input0", 3, 4),
            input1_rows=require_i8_matrix(spec.get("input1"), "bounded_dynamic_cnn_v1 input1", 2, 2, 2),
        )
    fail(f"unsupported task spec task_kind: {task_kind}")


def build_dynamic_gemm_like(out_dir: pathlib.Path,
                            name: str,
                            source_tag: int,
                            max_ticks: int,
                            input0_rows: list[list[int]],
                            input1_rows: list[list[int]]) -> None:
    row_count = len(input0_rows)
    output_values = gemm_i8_i32(input0_rows, input1_rows)
    tensors = [
        Tensor("int8", "input", 2, (2, 8, 0, 0), (1, 8, 0, 0)),
        Tensor("int8", "weight", 2, (8, 4, 0, 0), (8, 4, 0, 0)),
        Tensor("int32", "output", 2, (2, 4, 0, 0), (1, 4, 0, 0)),
    ]
    ops = [
        Op("gemm", "int8", "int32", 0, 1, 0xFFFF, 2),
    ]
    memory_plan, scratchpad_budget = build_sequential_memory_plan(
        tensors,
        alignment=16,
        align_total=True,
    )
    graph = serialize_graph_package(
        scratchpad_budget,
        tensors,
        ops,
        [],
        memory_plan,
        shape_mode="dynamic_bounded",
        dynamic_tensors=[
            DynamicTensor(0, tensor_byte_size(tensors[0])),
            DynamicTensor(2, tensor_byte_size(tensors[2])),
        ],
    )
    runtime_shape_table = serialize_runtime_shape_table([
        RuntimeShape(0, 2, (row_count, 8, 0, 0)),
        RuntimeShape(2, 2, (row_count, 4, 0, 0)),
    ])
    (out_dir / f"{name}.graph.bin").write_bytes(graph)
    (out_dir / f"{name}.runtime_shape.bin").write_bytes(runtime_shape_table)
    (out_dir / f"{name}.input0.bin").write_bytes(flatten_i8_matrix(input0_rows))
    (out_dir / f"{name}.input1.bin").write_bytes(flatten_i8_matrix(input1_rows))
    (out_dir / f"{name}.output0.expected.bin").write_bytes(
        struct.pack(f"<{len(output_values)}i", *output_values)
    )
    write_manifest(
        out_dir / f"{name}.manifest",
        name=name,
        graph_package=f"{name}.graph.bin",
        runtime_shape_table=f"{name}.runtime_shape.bin",
        inputs=[f"{name}.input0.bin", f"{name}.input1.bin"],
        outputs=[f"{name}.output0.actual.bin"],
        expected_outputs=[f"{name}.output0.expected.bin"],
        max_ticks=max_ticks,
        source_tag=source_tag,
    )


def build_dynamic_cnn_like(out_dir: pathlib.Path,
                           name: str,
                           source_tag: int,
                           max_ticks: int,
                           input0_rows: list[list[int]],
                           input1_rows: list[list[int]]) -> None:
    input_rows = len(input0_rows)
    input_cols = len(input0_rows[0])
    conv = conv2d_i8_i32(input0_rows, input1_rows)
    relu = relu_i32(conv)
    transposed = transpose_rows(relu)
    output_values = reduce_sum_rows(transposed)
    reduced_rows = len(conv)
    reduced_cols = len(conv[0])
    tensors = [
        Tensor("int8", "input", 2, (4, 4, 0, 0), (2, 4, 0, 0)),
        Tensor("int8", "weight", 2, (2, 2, 0, 0), (2, 2, 0, 0)),
        Tensor("int32", "intermediate", 2, (3, 3, 0, 0), (2, 3, 0, 0)),
        Tensor("int32", "intermediate", 2, (3, 3, 0, 0), (2, 3, 0, 0)),
        Tensor("int32", "intermediate", 2, (3, 3, 0, 0), (3, 2, 0, 0)),
        Tensor("int32", "output", 1, (3, 0, 0, 0), (2, 0, 0, 0)),
    ]
    ops = [
        Op("conv2d", "int8", "int32", 0, 1, 0xFFFF, 2),
        Op("eltwise_relu", "int32", "int32", 2, 0xFFFF, 0xFFFF, 3),
        Op("layout_transpose", "int32", "int32", 3, 0xFFFF, 0xFFFF, 4),
        Op("reduce_sum", "int32", "int32", 4, 0xFFFF, 0xFFFF, 5),
    ]
    dependencies = [(0, 1), (1, 2), (2, 3)]
    memory_plan, scratchpad_budget = build_sequential_memory_plan(
        tensors,
        alignment=16,
        align_total=True,
    )
    graph = serialize_graph_package(
        scratchpad_budget,
        tensors,
        ops,
        dependencies,
        memory_plan,
        shape_mode="dynamic_bounded",
        dynamic_tensors=[
            DynamicTensor(0, tensor_byte_size(tensors[0])),
            DynamicTensor(2, tensor_byte_size(tensors[2])),
            DynamicTensor(3, tensor_byte_size(tensors[3])),
            DynamicTensor(4, tensor_byte_size(tensors[4])),
            DynamicTensor(5, tensor_byte_size(tensors[5])),
        ],
    )
    runtime_shape_table = serialize_runtime_shape_table([
        RuntimeShape(0, 2, (input_rows, input_cols, 0, 0)),
        RuntimeShape(2, 2, (reduced_rows, reduced_cols, 0, 0)),
        RuntimeShape(3, 2, (reduced_rows, reduced_cols, 0, 0)),
        RuntimeShape(4, 2, (reduced_cols, reduced_rows, 0, 0)),
        RuntimeShape(5, 1, (reduced_rows, 0, 0, 0)),
    ])
    (out_dir / f"{name}.graph.bin").write_bytes(graph)
    (out_dir / f"{name}.runtime_shape.bin").write_bytes(runtime_shape_table)
    (out_dir / f"{name}.input0.bin").write_bytes(flatten_i8_matrix(input0_rows))
    (out_dir / f"{name}.input1.bin").write_bytes(flatten_i8_matrix(input1_rows))
    (out_dir / f"{name}.output0.expected.bin").write_bytes(
        struct.pack(f"<{len(output_values)}i", *output_values)
    )
    write_manifest(
        out_dir / f"{name}.manifest",
        name=name,
        graph_package=f"{name}.graph.bin",
        runtime_shape_table=f"{name}.runtime_shape.bin",
        inputs=[f"{name}.input0.bin", f"{name}.input1.bin"],
        outputs=[f"{name}.output0.actual.bin"],
        expected_outputs=[f"{name}.output0.expected.bin"],
        max_ticks=max_ticks,
        source_tag=source_tag,
    )


def build_task_spec(task_spec_path: pathlib.Path, out_dir: pathlib.Path) -> str:
    task_spec = parse_task_spec(task_spec_path)
    if isinstance(task_spec, GemmTaskSpec):
        build_dynamic_gemm_like(
            out_dir=out_dir,
            name=task_spec.name,
            source_tag=task_spec.source_tag,
            max_ticks=task_spec.max_ticks,
            input0_rows=task_spec.input0_rows,
            input1_rows=task_spec.input1_rows,
        )
    elif isinstance(task_spec, CnnTaskSpec):
        build_dynamic_cnn_like(
            out_dir=out_dir,
            name=task_spec.name,
            source_tag=task_spec.source_tag,
            max_ticks=task_spec.max_ticks,
            input0_rows=task_spec.input0_rows,
            input1_rows=task_spec.input1_rows,
        )
    else:
        fail("unsupported parsed task spec")
    return task_spec.name
