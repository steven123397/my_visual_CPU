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
