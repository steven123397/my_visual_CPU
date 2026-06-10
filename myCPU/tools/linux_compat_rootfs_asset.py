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
PT_LOAD = 1
PT_DYNAMIC = 2
DT_NULL = 0
DT_NEEDED = 1
DT_STRTAB = 5
DT_STRSZ = 10
DEFAULT_SHARED_LIBRARY_DIRS = (
    "/lib",
    "/usr/lib",
    "/lib64",
    "/usr/local/lib",
    "/usr/lib/riscv64-linux-gnu",
    "/usr/lib/gcc/riscv64-linux-gnu",
)
GCC_SEARCH_ROOTS = (
    "/usr/libexec/gcc",
    "/usr/lib/gcc",
)
TARGET_BINUTILS_NAMES = (
    "as",
    "ld",
    "ld.bfd",
)
PAGE_SIZE = 4096


class Source:
    def __init__(self, kind: str, path: pathlib.Path) -> None:
        self.kind = kind
        self.path = path


def normalize_guest_path(guest_path: str) -> str:
    pure = pathlib.PurePosixPath(guest_path)
    if not pure.is_absolute():
        pure = pathlib.PurePosixPath("/") / pure
    parts: list[str] = []
    for part in pure.parts:
        if part in ("", "/", "."):
            continue
        if part == "..":
            if parts:
                parts.pop()
            continue
        parts.append(part)
    return "/" + "/".join(parts)


def resolve_symlink_guest_path(guest_path: str, target: str) -> str:
    target_path = pathlib.PurePosixPath(target)
    if target_path.is_absolute():
        return normalize_guest_path(str(target_path))
    parent = pathlib.PurePosixPath(guest_path).parent
    return normalize_guest_path(str(parent / target_path))


def align_up(value: int, alignment: int = PAGE_SIZE) -> int:
    return (value + alignment - 1) & ~(alignment - 1)


def ext4_stat(rootfs: pathlib.Path, guest_path: str) -> str:
    proc = subprocess.run(
        ["debugfs", "-R", f"stat {guest_path}", str(rootfs)],
        text=True,
        capture_output=True,
    )
    if proc.returncode != 0:
        raise RuntimeError(f"debugfs failed stat {guest_path}: {proc.stderr}")
    return proc.stdout


def read_ext4_file_raw(rootfs: pathlib.Path, guest_path: str) -> bytes:
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


def read_ext4_file(rootfs: pathlib.Path,
                   guest_path: str,
                   seen: set[str] | None = None) -> bytes:
    guest_path = normalize_guest_path(guest_path)
    seen = seen or set()
    if guest_path in seen:
        raise RuntimeError(f"symlink loop while resolving {guest_path}")
    seen.add(guest_path)

    stat_output = ext4_stat(rootfs, guest_path)
    if "Type: symlink" in stat_output:
        for line in stat_output.splitlines():
            prefix = "Fast link dest: "
            if line.startswith(prefix):
                target = line[len(prefix):].strip().strip('"')
                return read_ext4_file(
                    rootfs,
                    resolve_symlink_guest_path(guest_path, target),
                    seen,
                )
        raise RuntimeError(f"debugfs did not report symlink target for {guest_path}")
    return read_ext4_file_raw(rootfs, guest_path)


def read_directory_file(root: pathlib.Path,
                        guest_path: str,
                        seen: set[str] | None = None) -> bytes:
    guest_path = normalize_guest_path(guest_path)
    seen = seen or set()
    if guest_path in seen:
        raise RuntimeError(f"symlink loop while resolving {guest_path}")
    seen.add(guest_path)

    relative = guest_path.removeprefix("/")
    path = root / relative
    if path.is_symlink():
        return read_directory_file(
            root,
            resolve_symlink_guest_path(guest_path, str(path.readlink())),
            seen,
        )
    return path.read_bytes()


def read_source_file(source: Source, guest_path: str) -> bytes:
    if source.kind == "ext4":
        return read_ext4_file(source.path, guest_path)
    return read_directory_file(source.path, guest_path)


def elf_program_headers(path: str, data: bytes) -> list[dict[str, int]]:
    e_phoff = struct.unpack_from("<Q", data, 32)[0]
    e_phentsize = struct.unpack_from("<H", data, 54)[0]
    e_phnum = struct.unpack_from("<H", data, 56)[0]
    headers = []
    if e_phentsize < 56:
        raise ValueError(f"{path}: unsupported ELF program header size")
    for index in range(e_phnum):
        offset = e_phoff + index * e_phentsize
        if offset + 56 > len(data):
            raise ValueError(f"{path}: truncated ELF program headers")
        p_type, p_flags = struct.unpack_from("<II", data, offset)
        p_offset, p_vaddr, _p_paddr, p_filesz, p_memsz, p_align = (
            struct.unpack_from("<QQQQQQ", data, offset + 8)
        )
        headers.append(
            {
                "type": p_type,
                "flags": p_flags,
                "offset": p_offset,
                "vaddr": p_vaddr,
                "filesz": p_filesz,
                "memsz": p_memsz,
                "align": p_align,
            }
        )
    return headers


def elf_vaddr_to_offset(headers: list[dict[str, int]],
                        vaddr: int,
                        data_size: int) -> int | None:
    for header in headers:
        if header["type"] != PT_LOAD:
            continue
        start = header["vaddr"]
        end = start + header["filesz"]
        if start <= vaddr < end:
            return header["offset"] + (vaddr - start)
    if 0 <= vaddr < data_size:
        return vaddr
    return None


def elf_c_string(data: bytes, offset: int, max_size: int | None = None) -> str:
    if offset < 0 or offset >= len(data):
        return ""
    end_limit = len(data) if max_size is None else min(len(data), offset + max_size)
    end = offset
    while end < end_limit and data[end] != 0:
        end += 1
    return data[offset:end].decode("ascii", errors="replace")


def inspect_rv64_elf(path: str, data: bytes) -> dict:
    if len(data) < 64 or data[:4] != b"\x7fELF":
        raise ValueError(f"{path}: bad ELF header")
    if data[4] != 2 or data[5] != 1:
        raise ValueError(f"{path}: unsupported ELF class/data")
    elf_type, machine = struct.unpack_from("<HH", data, 16)
    entry = struct.unpack_from("<Q", data, 24)[0]
    if elf_type not in (ET_EXEC, ET_DYN) or machine != EM_RISCV:
        raise ValueError(f"{path}: unsupported ELF type/machine")
    headers = elf_program_headers(path, data)
    mapped_size = len(data)
    needed_offsets = []
    strtab_vaddr = None
    strtab_size = None
    for header in headers:
        if header["type"] == PT_LOAD:
            mapped_size = max(mapped_size, header["offset"] + header["memsz"])
    for header in headers:
        if header["type"] != PT_DYNAMIC:
            continue
        offset = header["offset"]
        end = min(len(data), offset + header["filesz"])
        while offset + 16 <= end:
            tag, value = struct.unpack_from("<QQ", data, offset)
            offset += 16
            if tag == DT_NULL:
                break
            if tag == DT_NEEDED:
                needed_offsets.append(value)
            elif tag == DT_STRTAB:
                strtab_vaddr = value
            elif tag == DT_STRSZ:
                strtab_size = value
    needed = []
    if needed_offsets:
        if strtab_vaddr is None:
            raise ValueError(f"{path}: dynamic section missing string table")
        strtab_offset = elf_vaddr_to_offset(headers, strtab_vaddr, len(data))
        if strtab_offset is None:
            raise ValueError(f"{path}: dynamic string table is outside file")
        for needed_offset in needed_offsets:
            max_size = None
            if strtab_size is not None and needed_offset <= strtab_size:
                max_size = strtab_size - needed_offset
            name = elf_c_string(data, strtab_offset + needed_offset, max_size)
            if not name:
                raise ValueError(f"{path}: empty DT_NEEDED entry")
            needed.append(name)
    return {
        "entry": entry,
        "size": len(data),
        "type": elf_type,
        "needed": needed,
        "mapped_size": align_up(mapped_size),
    }


def source_path_exists(source: Source, guest_path: str) -> bool:
    try:
        read_source_file(source, guest_path)
        return True
    except (OSError, RuntimeError):
        return False


def list_ext4_directory(rootfs: pathlib.Path, guest_path: str) -> list[tuple[str, bool]]:
    if shutil.which("debugfs") is None:
        return []
    proc = subprocess.run(
        ["debugfs", "-R", f"ls -p {guest_path}", str(rootfs)],
        text=True,
        capture_output=True,
    )
    if proc.returncode != 0:
        return []
    entries: list[tuple[str, bool]] = []
    for line in proc.stdout.splitlines():
        if not line.startswith("/"):
            continue
        fields = line.split("/")
        if len(fields) < 7:
            continue
        mode = fields[2]
        name = fields[5]
        if name in ("", ".", ".."):
            continue
        entries.append((name, mode.startswith("04")))
    return entries


def list_source_directory(source: Source, guest_path: str) -> list[tuple[str, bool]]:
    guest_path = normalize_guest_path(guest_path)
    if source.kind == "ext4":
        return list_ext4_directory(source.path, guest_path)
    path = source.path / guest_path.removeprefix("/")
    try:
        return [
            (child.name, child.is_dir())
            for child in path.iterdir()
            if child.name not in (".", "..")
        ]
    except OSError:
        return []


def discover_gcc_lto_plugin_paths(source: Source) -> list[str]:
    paths: set[str] = set()
    for search_root in GCC_SEARCH_ROOTS:
        for target_name, target_is_dir in list_source_directory(source, search_root):
            if not target_is_dir:
                continue
            target_root = normalize_guest_path(f"{search_root}/{target_name}")
            for version_name, version_is_dir in list_source_directory(
                source, target_root
            ):
                if not version_is_dir:
                    continue
                plugin_path = normalize_guest_path(
                    f"{target_root}/{version_name}/liblto_plugin.so"
                )
                if source_path_exists(source, plugin_path):
                    paths.add(plugin_path)
    return sorted(paths)


def discover_target_binutils_paths(
    source: Source, required_toolchain_paths: set[str]
) -> list[str]:
    paths: set[str] = set()
    for target_name, target_is_dir in list_source_directory(source, "/usr"):
        if not target_is_dir:
            continue
        target_bin = normalize_guest_path(f"/usr/{target_name}/bin")
        for tool_name in TARGET_BINUTILS_NAMES:
            if normalize_guest_path(f"/usr/bin/{tool_name}") not in (
                required_toolchain_paths
            ):
                continue
            tool_path = normalize_guest_path(f"{target_bin}/{tool_name}")
            if source_path_exists(source, tool_path):
                paths.add(tool_path)
    return sorted(paths)


def resolve_shared_library_path(source: Source, name: str) -> str | None:
    if name.startswith("/"):
        guest_path = normalize_guest_path(name)
        return guest_path if source_path_exists(source, guest_path) else None
    for guest_dir in DEFAULT_SHARED_LIBRARY_DIRS:
        guest_path = normalize_guest_path(str(pathlib.PurePosixPath(guest_dir) / name))
        if source_path_exists(source, guest_path):
            return guest_path
    return None


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
    source: Source,
    required_paths: list[str],
    optional_paths: list[str],
    optional_shared_paths: list[str],
    optional_tool_paths: list[str] | None = None,
    toolchain_paths: list[str] | None = None,
    optional_toolchain_paths: list[str] | None = None,
) -> list[dict]:
    records = []
    by_path: dict[str, dict] = {}
    optional_tool_paths = optional_tool_paths or []
    toolchain_paths = toolchain_paths or []
    optional_toolchain_paths = optional_toolchain_paths or []
    path_groups = (
        (True, "tool", required_paths),
        (True, "toolchain", toolchain_paths),
        (False, "toolchain", optional_toolchain_paths),
        (False, "tool", optional_tool_paths),
        (False, "interpreter", optional_paths),
        (False, "shared", optional_shared_paths),
    )

    def add_record(required: bool, kind: str, guest_path: str) -> dict:
        guest_path = normalize_guest_path(guest_path)
        existing = by_path.get(guest_path)
        if existing is not None:
            if required and not existing["present"]:
                raise RuntimeError(
                    f"{guest_path}: required asset missing: {existing['reason']}"
                )
            if required:
                existing["required"] = True
            return existing
        try:
            data = read_source_file(source, guest_path)
            elf = inspect_rv64_elf(guest_path, data)
        except (OSError, RuntimeError, ValueError) as exc:
            if required:
                raise
            record = {
                "path": guest_path,
                "kind": kind,
                "required": False,
                "present": False,
                "reason": str(exc),
            }
            records.append(record)
            by_path[guest_path] = record
            return record
        record = {
            "path": guest_path,
            "kind": kind,
            "required": required,
            "present": True,
            "data": data,
            "entry": elf["entry"],
            "size": elf["size"],
            "type": elf["type"],
            "needed": elf["needed"],
            "mapped_size": elf["mapped_size"],
            "mode": mode_for_file(source, guest_path),
        }
        records.append(record)
        by_path[guest_path] = record
        return record

    for required, kind, paths in path_groups:
        for guest_path in paths:
            add_record(required, kind, guest_path)

    required_toolchain_paths = {
        normalize_guest_path(guest_path) for guest_path in toolchain_paths
    }
    for guest_path in discover_target_binutils_paths(source, required_toolchain_paths):
        add_record(True, "toolchain", guest_path)

    gcc_record = by_path.get("/usr/bin/gcc")
    if gcc_record is not None and gcc_record["present"]:
        for guest_path in discover_gcc_lto_plugin_paths(source):
            add_record(True, "toolchain", guest_path)

    index = 0
    while index < len(records):
        record = records[index]
        index += 1
        if not record["present"] or not record["required"]:
            continue
        for needed_name in record.get("needed", []):
            shared_path = resolve_shared_library_path(source, needed_name)
            if shared_path is None:
                raise RuntimeError(
                    f"{record['path']}: required shared library "
                    f"{needed_name} not found in rootfs library paths"
                )
            add_record(True, "shared", shared_path)
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
    optional_shared_paths: list[str],
    optional_tool_paths: list[str],
    toolchain_paths: list[str],
    optional_toolchain_paths: list[str],
) -> None:
    records = collect_asset_records(
        source,
        required_paths,
        optional_paths,
        optional_shared_paths,
        optional_tool_paths,
        toolchain_paths,
        optional_toolchain_paths,
    )
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
        padded_size = max(align_up(len(entry["data"])), entry["mapped_size"])
        padded_data = entry["data"] + bytes(padded_size - len(entry["data"]))
        lines.append(
            f"static const uint8_t k_asset_{entry['index']}[] "
            f"__attribute__((aligned(4096))) = "
            f"{{{c_byte_array(padded_data)}}};"
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
            f"{entry['size']}U, "
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
                   optional_paths: list[str],
                   optional_shared_paths: list[str],
                   optional_tool_paths: list[str],
                   toolchain_paths: list[str],
                   optional_toolchain_paths: list[str]) -> None:
    if out_manifest is None:
        return
    files = []
    for record in collect_asset_records(
        source,
        required_paths,
        optional_paths,
        optional_shared_paths,
        optional_tool_paths,
        toolchain_paths,
        optional_toolchain_paths,
    ):
        entry = {
            "path": record["path"],
            "kind": record["kind"],
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
    parser.add_argument("--toolchain-path", action="append", default=[])
    parser.add_argument("--optional-tool-path", action="append", default=[])
    parser.add_argument("--optional-toolchain-path", action="append", default=[])
    parser.add_argument("--optional-path", action="append", default=[])
    parser.add_argument("--optional-shared-path", action="append", default=[])
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
        write_c_asset(
            args.out_c, source, args.path, args.optional_path,
            args.optional_shared_path, args.optional_tool_path,
            args.toolchain_path, args.optional_toolchain_path
        )
        write_manifest(
            args.out_manifest, source, args.path, args.optional_path,
            args.optional_shared_path, args.optional_tool_path,
            args.toolchain_path, args.optional_toolchain_path
        )
    except (OSError, RuntimeError, ValueError) as exc:
        print(str(exc), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
