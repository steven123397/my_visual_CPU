#!/usr/bin/env python3
import argparse
import json
import pathlib
import shutil
import stat
import struct
import subprocess
import sys
import tempfile

EM_RISCV = 243
ET_EXEC = 2
ET_DYN = 3


class Source:
    def __init__(self, kind: str, path: pathlib.Path) -> None:
        self.kind = kind
        self.path = path


def read_ext4_file(rootfs: pathlib.Path, guest_path: str) -> bytes:
    if not rootfs.is_file():
        raise FileNotFoundError(f"rootfs image not found: {rootfs}")
    if shutil.which("debugfs") is None:
        raise RuntimeError("debugfs is required to read ext4 rootfs images")
    with tempfile.TemporaryDirectory() as tmp:
        out = pathlib.Path(tmp) / pathlib.PurePosixPath(guest_path).name
        proc = subprocess.run(
            ["debugfs", "-R", f"dump {guest_path} {out}", str(rootfs)],
            text=True,
            capture_output=True,
        )
        if proc.returncode != 0 or not out.exists():
            raise RuntimeError(
                f"debugfs failed extracting {guest_path}: {proc.stderr}"
            )
        return out.read_bytes()


def read_source_file(source: Source, guest_path: str) -> bytes:
    if source.kind == "ext4":
        return read_ext4_file(source.path, guest_path)
    relative = guest_path.removeprefix("/")
    return (source.path / relative).read_bytes()


def inspect_rv64_elf(path: str, data: bytes) -> dict[str, int]:
    if len(data) < 64 or data[:4] != b"\x7fELF":
        raise ValueError(f"{path}: bad ELF header")
    if data[4] != 2 or data[5] != 1:
        raise ValueError(f"{path}: unsupported ELF class/data")
    elf_type, machine = struct.unpack_from("<HH", data, 16)
    entry = struct.unpack_from("<Q", data, 24)[0]
    if elf_type not in (ET_EXEC, ET_DYN) or machine != EM_RISCV:
        raise ValueError(f"{path}: unsupported ELF type/machine")
    return {"entry": entry, "size": len(data), "type": elf_type}


def c_string(value: str) -> str:
    return '"' + value.replace("\\", "\\\\").replace('"', '\\"') + '"'


def c_byte_array(data: bytes) -> str:
    return ", ".join(f"0x{byte:02x}" for byte in data)


def mode_for_file(source: Source, guest_path: str) -> int:
    if source.kind == "ext4":
        return 0o555
    relative = guest_path.removeprefix("/")
    try:
        return stat.S_IMODE((source.path / relative).stat().st_mode)
    except OSError:
        return 0o555


def collect_asset_records(
    source: Source, required_paths: list[str], optional_paths: list[str]
) -> list[dict]:
    records = []
    for required, paths in ((True, required_paths), (False, optional_paths)):
        for guest_path in paths:
            try:
                data = read_source_file(source, guest_path)
                elf = inspect_rv64_elf(guest_path, data)
            except (OSError, RuntimeError, ValueError) as exc:
                if required:
                    raise
                records.append(
                    {
                        "path": guest_path,
                        "required": False,
                        "present": False,
                        "reason": str(exc),
                    }
                )
                continue
            records.append(
                {
                    "path": guest_path,
                    "required": required,
                    "present": True,
                    "data": data,
                    "entry": elf["entry"],
                    "size": elf["size"],
                    "type": elf["type"],
                    "mode": mode_for_file(source, guest_path),
                }
            )
    return records


def directory_paths_for(paths: list[str]) -> list[str]:
    dirs = {"/"}
    for guest_path in paths:
        current = pathlib.PurePosixPath(guest_path).parent
        while str(current) not in ("", "."):
            dirs.add(str(current))
            if str(current) == "/":
                break
            current = current.parent
    return sorted(dirs, key=lambda value: (value.count("/"), value))


def dir_mode_for(path: str) -> str:
    if path == "/":
        return (
            "LINUX_COMPAT_S_IFDIR | LINUX_COMPAT_S_IRUSR | "
            "LINUX_COMPAT_S_IWUSR | LINUX_COMPAT_S_IXUSR | "
            "LINUX_COMPAT_S_IRGRP | LINUX_COMPAT_S_IXGRP | "
            "LINUX_COMPAT_S_IROTH | LINUX_COMPAT_S_IXOTH"
        )
    return (
        "LINUX_COMPAT_S_IFDIR | LINUX_COMPAT_S_IRUSR | "
        "LINUX_COMPAT_S_IXUSR | LINUX_COMPAT_S_IRGRP | "
        "LINUX_COMPAT_S_IXGRP | LINUX_COMPAT_S_IROTH | "
        "LINUX_COMPAT_S_IXOTH"
    )


def write_c_asset(
    out_c: pathlib.Path,
    source: Source,
    required_paths: list[str],
    optional_paths: list[str],
) -> None:
    records = collect_asset_records(source, required_paths, optional_paths)
    entries = [record for record in records if record["present"]]
    for index, entry in enumerate(entries):
        entry["index"] = index

    lines = [
        "#include <stdbool.h>",
        "#include <stddef.h>",
        "#include <stdint.h>",
        "",
        '#include "linux_compat_rootfs.h"',
        "",
    ]
    for entry in entries:
        lines.append(
            f"static const uint8_t k_asset_{entry['index']}[] = "
            f"{{{c_byte_array(entry['data'])}}};"
        )
    lines.extend(["", "static const linux_compat_rootfs_node_t k_rootfs_nodes[] = {"])
    directory_paths = directory_paths_for([entry["path"] for entry in entries])
    for index, directory in enumerate(directory_paths):
        lines.append(
            "    {"
            f"{c_string(directory)}, 0, 0, false, true, {index + 1}U, "
            f"{dir_mode_for(directory)}"
            "},"
        )
    file_inode_base = len(directory_paths) + 1
    for offset, entry in enumerate(entries):
        lines.append(
            "    {"
            f"{c_string(entry['path'])}, "
            f"k_asset_{entry['index']}, "
            f"sizeof(k_asset_{entry['index']}), "
            "true, false, "
            f"{file_inode_base + offset}U, "
            "LINUX_COMPAT_S_IFREG | LINUX_COMPAT_S_IRUSR | "
            "LINUX_COMPAT_S_IXUSR | LINUX_COMPAT_S_IRGRP | "
            "LINUX_COMPAT_S_IXGRP | LINUX_COMPAT_S_IROTH | "
            "LINUX_COMPAT_S_IXOTH},"
        )
    lines.extend(
        [
            "};",
            "",
            "const char* linux_compat_rootfs_source_name(void) {",
            '    return "external";',
            "}",
            "",
            "size_t linux_compat_rootfs_node_count(void) {",
            "    return sizeof(k_rootfs_nodes) / sizeof(k_rootfs_nodes[0]);",
            "}",
            "",
            "const linux_compat_rootfs_node_t* linux_compat_rootfs_node_at(size_t index) {",
            "    if (index >= linux_compat_rootfs_node_count()) {",
            "        return 0;",
            "    }",
            "    return &k_rootfs_nodes[index];",
            "}",
            "",
        ]
    )
    out_c.parent.mkdir(parents=True, exist_ok=True)
    out_c.write_text("\n".join(lines))


def write_manifest(out_manifest: pathlib.Path | None,
                   source: Source,
                   required_paths: list[str],
                   optional_paths: list[str]) -> None:
    if out_manifest is None:
        return
    files = []
    for record in collect_asset_records(source, required_paths, optional_paths):
        entry = {
            "path": record["path"],
            "required": record["required"],
            "present": record["present"],
        }
        if record["present"]:
            entry["entry"] = record["entry"]
            entry["size"] = record["size"]
            entry["type"] = record["type"]
        else:
            entry["reason"] = record["reason"]
        files.append(entry)
    out_manifest.parent.mkdir(parents=True, exist_ok=True)
    out_manifest.write_text(
        json.dumps({"source": source.kind, "files": files}, separators=(",", ":"))
    )


def parse_args(argv: list[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate a C Linux compat rootfs asset provider"
    )
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--source-dir", type=pathlib.Path)
    source.add_argument("--source-rootfs", type=pathlib.Path)
    source.add_argument("--source", type=pathlib.Path)
    parser.add_argument("--out-c", type=pathlib.Path, required=True)
    parser.add_argument("--out-manifest", type=pathlib.Path)
    parser.add_argument("--path", action="append", default=[])
    parser.add_argument("--optional-path", action="append", default=[])
    return parser.parse_args(argv)


def resolve_source(args: argparse.Namespace) -> Source:
    if args.source_dir is not None:
        return Source("directory", args.source_dir)
    if args.source_rootfs is not None:
        return Source("ext4", args.source_rootfs)
    if args.source.is_dir():
        return Source("directory", args.source)
    if args.source.is_file():
        return Source("ext4", args.source)
    raise FileNotFoundError(f"source not found: {args.source}")


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    if not args.path:
        print("at least one --path is required", file=sys.stderr)
        return 2
    try:
        source = resolve_source(args)
        write_c_asset(args.out_c, source, args.path, args.optional_path)
        write_manifest(args.out_manifest, source, args.path, args.optional_path)
    except (OSError, RuntimeError, ValueError) as exc:
        print(str(exc), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
