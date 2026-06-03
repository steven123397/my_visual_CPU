import json
import pathlib
import subprocess
import tempfile
import unittest

MYCPU_DIR = pathlib.Path(__file__).resolve().parents[2]
TOOL = MYCPU_DIR / "tools" / "linux_compat_rootfs_asset.py"


def rv64_elf(entry: int, elf_type: int = 2) -> bytes:
    image = bytearray(128)
    image[0:4] = b"\x7fELF"
    image[4] = 2
    image[5] = 1
    image[6] = 1
    image[16:18] = elf_type.to_bytes(2, "little")
    image[18:20] = (243).to_bytes(2, "little")
    image[20:24] = (1).to_bytes(4, "little")
    image[24:32] = entry.to_bytes(8, "little")
    image[32:40] = (64).to_bytes(8, "little")
    image[52:54] = (64).to_bytes(2, "little")
    image[54:56] = (56).to_bytes(2, "little")
    image[56:58] = (1).to_bytes(2, "little")
    image[64:68] = (1).to_bytes(4, "little")
    return bytes(image)


def rv64_exec_elf(entry: int) -> bytes:
    return rv64_elf(entry, 2)


def rv64_dyn_elf(entry: int) -> bytes:
    return rv64_elf(entry, 3)


class LinuxCompatRootfsAssetToolTest(unittest.TestCase):
    def test_directory_source_generates_c_asset_and_manifest(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = pathlib.Path(tmp) / "rootfs"
            out_c = pathlib.Path(tmp) / "linux_compat_rootfs_asset.c"
            out_json = pathlib.Path(tmp) / "linux_compat_rootfs_asset.json"
            (root / "bin").mkdir(parents=True)
            (root / "usr" / "bin").mkdir(parents=True)
            (root / "bin" / "busybox").write_bytes(rv64_exec_elf(0x401000))
            (root / "usr" / "bin" / "git").write_bytes(rv64_exec_elf(0x402000))

            proc = subprocess.run(
                [
                    "python3",
                    str(TOOL),
                    "--source-dir",
                    str(root),
                    "--out-c",
                    str(out_c),
                    "--out-manifest",
                    str(out_json),
                    "--path",
                    "/bin/busybox",
                    "--path",
                    "/usr/bin/git",
                ],
                cwd=MYCPU_DIR,
                text=True,
                capture_output=True,
            )

            self.assertEqual(proc.returncode, 0, proc.stderr)
            generated = out_c.read_text()
            self.assertIn("linux_compat_rootfs_source_name", generated)
            self.assertIn("/bin/busybox", generated)
            self.assertIn("0x7f", generated)
            self.assertIn('"source":"directory"', out_json.read_text())

    def test_missing_ext4_rootfs_fails_closed(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            out_c = pathlib.Path(tmp) / "linux_compat_rootfs_asset.c"
            proc = subprocess.run(
                [
                    "python3",
                    str(TOOL),
                    "--source-rootfs",
                    "/no/such/rootfs.ext4",
                    "--out-c",
                    str(out_c),
                    "--path",
                    "/bin/busybox",
                ],
                cwd=MYCPU_DIR,
                text=True,
                capture_output=True,
            )

            self.assertIn("--source-rootfs", proc.args)
            self.assertNotEqual(proc.returncode, 0)
            self.assertIn("rootfs image not found", proc.stderr)

    def test_optional_interpreter_missing_is_recorded_not_fatal(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = pathlib.Path(tmp) / "rootfs"
            out_c = pathlib.Path(tmp) / "linux_compat_rootfs_asset.c"
            out_json = pathlib.Path(tmp) / "linux_compat_rootfs_asset.json"
            (root / "bin").mkdir(parents=True)
            (root / "usr" / "bin").mkdir(parents=True)
            (root / "bin" / "busybox").write_bytes(rv64_exec_elf(0x401000))
            (root / "usr" / "bin" / "git").write_bytes(rv64_exec_elf(0x402000))

            proc = subprocess.run(
                [
                    "python3",
                    str(TOOL),
                    "--source-dir",
                    str(root),
                    "--out-c",
                    str(out_c),
                    "--out-manifest",
                    str(out_json),
                    "--path",
                    "/bin/busybox",
                    "--optional-path",
                    "/lib/ld-musl-riscv64.so.1",
                ],
                cwd=MYCPU_DIR,
                text=True,
                capture_output=True,
            )

            self.assertEqual(proc.returncode, 0, proc.stderr)
            manifest = out_json.read_text()
            self.assertIn('"path":"/lib/ld-musl-riscv64.so.1"', manifest)
            self.assertIn('"present":false', manifest)
            self.assertIn('"required":false', manifest)
            self.assertNotIn("/lib/ld-musl-riscv64.so.1", out_c.read_text())

    def test_optional_interpreter_present_is_generated_and_manifested(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = pathlib.Path(tmp) / "rootfs"
            out_c = pathlib.Path(tmp) / "linux_compat_rootfs_asset.c"
            out_json = pathlib.Path(tmp) / "linux_compat_rootfs_asset.json"
            (root / "bin").mkdir(parents=True)
            (root / "lib").mkdir(parents=True)
            (root / "bin" / "busybox").write_bytes(rv64_exec_elf(0x401000))
            (root / "lib" / "ld-musl-riscv64.so.1").write_bytes(
                rv64_dyn_elf(0x1000)
            )

            proc = subprocess.run(
                [
                    "python3",
                    str(TOOL),
                    "--source-dir",
                    str(root),
                    "--out-c",
                    str(out_c),
                    "--out-manifest",
                    str(out_json),
                    "--path",
                    "/bin/busybox",
                    "--optional-path",
                    "/lib/ld-musl-riscv64.so.1",
                ],
                cwd=MYCPU_DIR,
                text=True,
                capture_output=True,
            )

            self.assertEqual(proc.returncode, 0, proc.stderr)
            generated = out_c.read_text()
            manifest = out_json.read_text()
            self.assertIn("/lib", generated)
            self.assertIn("/lib/ld-musl-riscv64.so.1", generated)
            self.assertIn('"path":"/lib/ld-musl-riscv64.so.1"', manifest)
            self.assertIn('"present":true', manifest)
            self.assertIn('"entry":4096', manifest)

    def test_stage10_tools_and_shared_assets_are_manifested(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = pathlib.Path(tmp) / "rootfs"
            out_c = pathlib.Path(tmp) / "linux_compat_rootfs_asset.c"
            out_json = pathlib.Path(tmp) / "linux_compat_rootfs_asset.json"
            for guest_dir in ("bin", "usr/bin", "lib"):
                (root / guest_dir).mkdir(parents=True)
            for guest_path, entry in (
                ("bin/busybox", 0x401000),
                ("usr/bin/git", 0x402000),
                ("usr/bin/vim", 0x403000),
                ("usr/bin/gcc", 0x404000),
                ("usr/bin/rustc", 0x405000),
            ):
                (root / guest_path).write_bytes(rv64_exec_elf(entry))
            (root / "lib" / "ld-musl-riscv64.so.1").write_bytes(
                rv64_dyn_elf(0x1000)
            )

            proc = subprocess.run(
                [
                    "python3",
                    str(TOOL),
                    "--source-dir",
                    str(root),
                    "--out-c",
                    str(out_c),
                    "--out-manifest",
                    str(out_json),
                    "--path",
                    "/bin/busybox",
                    "--path",
                    "/usr/bin/git",
                    "--path",
                    "/usr/bin/vim",
                    "--path",
                    "/usr/bin/gcc",
                    "--path",
                    "/usr/bin/rustc",
                    "--optional-path",
                    "/lib/ld-musl-riscv64.so.1",
                    "--optional-shared-path",
                    "/lib/libgcc_s.so.1",
                ],
                cwd=MYCPU_DIR,
                text=True,
                capture_output=True,
            )

            self.assertEqual(proc.returncode, 0, proc.stderr)
            generated = out_c.read_text()
            manifest = json.loads(out_json.read_text())
            by_path = {entry["path"]: entry for entry in manifest["files"]}
            for tool in (
                "/bin/busybox",
                "/usr/bin/git",
                "/usr/bin/vim",
                "/usr/bin/gcc",
                "/usr/bin/rustc",
            ):
                self.assertEqual(by_path[tool]["kind"], "tool")
                self.assertTrue(by_path[tool]["required"])
                self.assertTrue(by_path[tool]["present"])
                self.assertIn(tool, generated)
            self.assertEqual(
                by_path["/lib/ld-musl-riscv64.so.1"]["kind"], "interpreter"
            )
            self.assertTrue(by_path["/lib/ld-musl-riscv64.so.1"]["present"])
            self.assertEqual(by_path["/lib/libgcc_s.so.1"]["kind"], "shared")
            self.assertFalse(by_path["/lib/libgcc_s.so.1"]["present"])

    def test_stage11_preflight_marks_optional_toolchain_and_shared_assets(
        self,
    ) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = pathlib.Path(tmp) / "rootfs"
            out_c = pathlib.Path(tmp) / "linux_compat_rootfs_asset.c"
            out_json = pathlib.Path(tmp) / "linux_compat_rootfs_asset.json"
            for guest_dir in (
                "bin",
                "usr/bin",
                "usr/lib/gcc/riscv64-linux-gnu",
                "lib",
            ):
                (root / guest_dir).mkdir(parents=True)
            for guest_path, entry in (
                ("bin/busybox", 0x401000),
                ("usr/bin/git", 0x402000),
                ("usr/bin/vim", 0x403000),
                ("usr/bin/gcc", 0x404000),
                ("bin/sh", 0x405000),
                ("usr/bin/as", 0x406000),
                ("usr/bin/ld", 0x407000),
                ("usr/bin/rustc", 0x408000),
                ("usr/lib/gcc/riscv64-linux-gnu/cc1", 0x409000),
            ):
                (root / guest_path).write_bytes(rv64_exec_elf(entry))
            (root / "lib" / "ld-musl-riscv64.so.1").write_bytes(
                rv64_dyn_elf(0x1000)
            )
            (root / "lib" / "libc.so.6").write_bytes(rv64_dyn_elf(0x2000))

            proc = subprocess.run(
                [
                    "python3",
                    str(TOOL),
                    "--source-dir",
                    str(root),
                    "--out-c",
                    str(out_c),
                    "--out-manifest",
                    str(out_json),
                    "--path",
                    "/bin/busybox",
                    "--path",
                    "/usr/bin/git",
                    "--path",
                    "/usr/bin/vim",
                    "--path",
                    "/usr/bin/gcc",
                    "--toolchain-path",
                    "/bin/sh",
                    "--toolchain-path",
                    "/usr/bin/as",
                    "--toolchain-path",
                    "/usr/bin/ld",
                    "--optional-toolchain-path",
                    "/usr/lib/gcc/riscv64-linux-gnu/cc1",
                    "--optional-tool-path",
                    "/usr/bin/rustc",
                    "--optional-path",
                    "/lib/ld-musl-riscv64.so.1",
                    "--optional-shared-path",
                    "/lib/libc.so.6",
                    "--optional-shared-path",
                    "/lib/libgcc_s.so.1",
                ],
                cwd=MYCPU_DIR,
                text=True,
                capture_output=True,
            )

            self.assertEqual(proc.returncode, 0, proc.stderr)
            generated = out_c.read_text()
            manifest = json.loads(out_json.read_text())
            by_path = {entry["path"]: entry for entry in manifest["files"]}
            for tool in (
                "/bin/busybox",
                "/usr/bin/git",
                "/usr/bin/vim",
                "/usr/bin/gcc",
            ):
                self.assertEqual(by_path[tool]["kind"], "tool")
                self.assertTrue(by_path[tool]["required"])
                self.assertTrue(by_path[tool]["present"])
            for toolchain_path in ("/bin/sh", "/usr/bin/as", "/usr/bin/ld"):
                self.assertEqual(by_path[toolchain_path]["kind"], "toolchain")
                self.assertTrue(by_path[toolchain_path]["required"])
                self.assertTrue(by_path[toolchain_path]["present"])
            self.assertEqual(
                by_path["/usr/lib/gcc/riscv64-linux-gnu/cc1"]["kind"],
                "toolchain",
            )
            self.assertFalse(
                by_path["/usr/lib/gcc/riscv64-linux-gnu/cc1"]["required"]
            )
            self.assertEqual(by_path["/usr/bin/rustc"]["kind"], "tool")
            self.assertFalse(by_path["/usr/bin/rustc"]["required"])
            self.assertTrue(by_path["/usr/bin/rustc"]["present"])
            self.assertEqual(
                by_path["/lib/ld-musl-riscv64.so.1"]["kind"], "interpreter"
            )
            self.assertEqual(by_path["/lib/libc.so.6"]["kind"], "shared")
            self.assertTrue(by_path["/lib/libc.so.6"]["present"])
            self.assertEqual(by_path["/lib/libgcc_s.so.1"]["kind"], "shared")
            self.assertFalse(by_path["/lib/libgcc_s.so.1"]["present"])
            self.assertIn("/usr/bin/rustc", generated)
            self.assertIn("/usr/lib/gcc/riscv64-linux-gnu/cc1", generated)
            self.assertNotIn("/lib/libgcc_s.so.1", generated)
