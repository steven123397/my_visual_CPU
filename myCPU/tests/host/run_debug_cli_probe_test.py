#!/usr/bin/env python3
import contextlib
import importlib.util
import io
import pathlib
import subprocess
import tempfile
import unittest


MYCPU_DIR = pathlib.Path(__file__).resolve().parents[2]
MODULE_PATH = pathlib.Path(__file__).resolve().parents[2] / "workloads" / "run_debug_cli_probe.py"
MODULE_SPEC = importlib.util.spec_from_file_location("run_debug_cli_probe", MODULE_PATH)
if MODULE_SPEC is None or MODULE_SPEC.loader is None:
    raise RuntimeError(f"failed to load module spec for {MODULE_PATH}")
PROBE = importlib.util.module_from_spec(MODULE_SPEC)
MODULE_SPEC.loader.exec_module(PROBE)


class RunDebugCliProbeTest(unittest.TestCase):
    def test_missing_input_paths_reports_primary_image_and_payloads(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            temp_path = pathlib.Path(temp_dir)
            initrd = temp_path / "rootfs.cpio"
            initrd.write_bytes(b"initrd")
            args = PROBE.parse_args(
                [
                    "--target",
                    "./mycpu",
                    "--image",
                    str(temp_path / "Image"),
                    "--flat",
                    "--addr",
                    "0x80200000",
                    "--payload",
                    str(temp_path / "board.dtb"),
                    "0x88000000",
                    "--payload",
                    str(initrd),
                    "0x84000000",
                ]
            )

            missing = PROBE.missing_input_paths(args)

        self.assertEqual(
            missing,
            [
                ("image", str(temp_path / "Image")),
                ("payload", str(temp_path / "board.dtb")),
            ],
        )

    def test_main_fails_closed_before_probe_when_inputs_are_missing(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            temp_path = pathlib.Path(temp_dir)
            missing_image = temp_path / "Image"
            missing_dtb = temp_path / "board.dtb"
            called = False

            def fake_run_probe(*_args, **_kwargs):
                nonlocal called
                called = True
                raise AssertionError("main should not invoke run_probe when input files are missing")

            stdout = io.StringIO()
            stderr = io.StringIO()
            original_run_probe = PROBE.run_probe
            PROBE.run_probe = fake_run_probe
            try:
                with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
                    rc = PROBE.main(
                        [
                            "--target",
                            "./mycpu",
                            "--image",
                            str(missing_image),
                            "--flat",
                            "--addr",
                            "0x80200000",
                            "--payload",
                            str(missing_dtb),
                            "0x88000000",
                        ]
                    )
            finally:
                PROBE.run_probe = original_run_probe

        self.assertEqual(rc, 1)
        self.assertFalse(called)
        self.assertIn("missing input files:", stderr.getvalue())
        self.assertIn(str(missing_image), stderr.getvalue())
        self.assertIn(str(missing_dtb), stderr.getvalue())
        self.assertEqual(stdout.getvalue(), "")

    def test_flat_probe_load_contract_preserves_addr_without_disk(self) -> None:
        args = PROBE.parse_args(
            [
                "--target",
                "./mycpu",
                "--image",
                "Image",
                "--backend",
                "pipeline",
                "--block-transport",
                "virtio-blk",
                "--flat",
                "--addr",
                "0x80200000",
                "--step-cycles",
                "32",
            ]
        )

        commands = PROBE.build_commands(args)

        self.assertEqual(
            commands,
            [
                {
                    "cmd": "load",
                    "image": "Image",
                    "backend": "pipeline",
                    "block_transport": "virtio-blk",
                    "flat": True,
                    "addr": 0x80200000,
                },
                {"cmd": "step_cycle", "count": 32},
                {"cmd": "snapshot"},
                {"cmd": "uart_output", "offset": 0},
                {"cmd": "quit"},
            ],
        )

    def test_default_probe_keeps_single_step_cycle_command(self) -> None:
        args = PROBE.parse_args(
            [
                "--target",
                "./mycpu",
                "--image",
                "kernel.elf",
                "--step-cycles",
                "64",
            ]
        )

        commands = PROBE.build_commands(args)

        self.assertEqual(
            commands,
            [
                {"cmd": "load", "image": "kernel.elf", "backend": "functional"},
                {"cmd": "step_cycle", "count": 64},
                {"cmd": "snapshot"},
                {"cmd": "uart_output", "offset": 0},
                {"cmd": "quit"},
            ],
        )

    def test_uart_wait_and_input_actions_preserve_cli_order(self) -> None:
        args = PROBE.parse_args(
            [
                "--target",
                "./mycpu",
                "--image",
                "kernel.elf",
                "--uart-wait",
                "$ ",
                "100",
                "--uart-input",
                "echo SHELL_OK\r",
                "--uart-wait",
                "SHELL_OK",
                "200",
            ]
        )

        commands = PROBE.build_commands(args)

        self.assertEqual(
            commands,
            [
                {"cmd": "load", "image": "kernel.elf", "backend": "functional"},
                {"cmd": "run_until_uart_contains", "text": "$ ", "max_steps": 100},
                {"cmd": "uart_input", "text": "echo SHELL_OK\r"},
                {"cmd": "run_until_uart_contains", "text": "SHELL_OK", "max_steps": 200},
                {"cmd": "snapshot"},
                {"cmd": "uart_output", "offset": 0},
                {"cmd": "quit"},
            ],
        )

    def test_make_run_workload_keeps_xv6_elf_disk_contract(self) -> None:
        proc = subprocess.run(
            ["make", "-n", "run-workload-xv6"],
            cwd=MYCPU_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )

        self.assertEqual(proc.returncode, 0, msg=proc.stderr)
        self.assertIn("--image external/xv6-riscv/kernel/kernel", proc.stdout)
        self.assertIn("--disk external/xv6-riscv/fs.img", proc.stdout)
        self.assertIn("--block-transport virtio-blk", proc.stdout)
        self.assertNotIn("--flat", proc.stdout)

    def test_default_make_test_includes_xv6_observation_guardrails(self) -> None:
        proc = subprocess.run(
            ["make", "-n", "test"],
            cwd=MYCPU_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )

        self.assertEqual(proc.returncode, 0, msg=proc.stderr)
        self.assertIn("=== host:xv6_boot_smoke ===", proc.stdout)
        self.assertIn("=== host:run_debug_cli_probe ===", proc.stdout)

    def test_default_make_test_pipeline_includes_xv6_observation_guardrails(self) -> None:
        proc = subprocess.run(
            ["make", "-n", "test-pipeline"],
            cwd=MYCPU_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )

        self.assertEqual(proc.returncode, 0, msg=proc.stderr)
        self.assertIn("=== host:xv6_boot_smoke ===", proc.stdout)
        self.assertIn("=== host:run_debug_cli_probe ===", proc.stdout)

    def test_real_xv6_probe_emits_functional_profile_summary(self) -> None:
        proc = subprocess.run(
            [
                "python3",
                "workloads/run_debug_cli_probe.py",
                "--target",
                "./mycpu",
                "--image",
                "external/xv6-riscv/kernel/kernel",
                "--disk",
                "external/xv6-riscv/fs.img",
                "--block-transport",
                "virtio-blk",
                "--step-cycles",
                "5000",
            ],
            cwd=MYCPU_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )

        self.assertEqual(proc.returncode, 0, msg=proc.stderr)
        self.assertIn(
            "summary: cycle=5000 instret=5000 pc=0x800010dc privilege=S backend=functional",
            proc.stdout,
        )
        self.assertIn("profile: retirements=5000 traps=0 memory=1570", proc.stdout)
        self.assertIn(
            "shadow-cache: line_size=64 capacity_lines=64 resident_lines=20 line_accesses=1515 hits=1495 misses=20 evictions=0 bypasses=55",
            proc.stdout,
        )
        self.assertIn(
            "memory-top: label=ram kind=ram accesses=1515 reads=621 writes=894 faults=0 bytes=8562",
            proc.stdout,
        )
        self.assertIn("xv6 kernel is booting", proc.stdout)

    def test_make_run_workload_can_emit_flat_load_contract(self) -> None:
        proc = subprocess.run(
            [
                "make",
                "-n",
                "run-workload",
                "WORKLOAD_NAME=xv6",
                "WORKLOAD_IMAGE=tests/data/linux_like_image.bin",
                "WORKLOAD_BUILD_COMMAND=:",
                "WORKLOAD_IMAGE_FORMAT=flat",
                "WORKLOAD_LOAD_ADDR=0x80200000",
                "WORKLOAD_DISK=",
                "BOARD_BLOCK_TRANSPORT=virtio-blk",
                "WORKLOAD_PROBE_CYCLES=32",
            ],
            cwd=MYCPU_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )

        self.assertEqual(proc.returncode, 0, msg=proc.stderr)
        self.assertIn("--image tests/data/linux_like_image.bin", proc.stdout)
        self.assertIn("--flat", proc.stdout)
        self.assertIn("--addr 0x80200000", proc.stdout)
        self.assertIn("--block-transport virtio-blk", proc.stdout)
        self.assertIn("--step-cycles 32", proc.stdout)
        self.assertNotIn("--disk ", proc.stdout)

    def test_emit_probe_summary_exposes_linux_facing_checkpoint_fields(self) -> None:
        args = PROBE.parse_args(
            [
                "--target",
                "./mycpu",
                "--image",
                "Image",
                "--disk",
                "rootfs.img",
                "--block-transport",
                "virtio-blk",
                "--flat",
                "--addr",
                "0x80200000",
                "--payload",
                "board.dtb",
                "0x88000000",
                "--payload",
                "initrd.img",
                "0x84000000",
                "--set-reg",
                "a0",
                "0x0",
                "--set-reg",
                "a1",
                "0x88000000",
            ]
        )
        lines = [
            {
                "type": "snapshot",
                "summary": {
                    "cycle": 8192,
                    "instret": 4096,
                    "pc": "0x80200010",
                    "privilege": "S",
                    "backend": "functional",
                },
                "csrs": {
                    "mcause": "0x0",
                    "mepc": "0x80200000",
                    "mtval": "0x0",
                    "scause": "0xd",
                    "sepc": "0x80200010",
                    "stval": "0x80201000",
                    "stvec": "0x80200100",
                    "satp": "0x8000000000080200",
                },
                "profile": {
                    "total_retirements": 4096,
                    "total_traps": 1,
                    "total_memory_observations": 5,
                    "shadow_cache": {
                        "line_size_bytes": 64,
                        "capacity_lines": 64,
                        "resident_lines": 2,
                        "line_accesses": 5,
                        "hits": 3,
                        "misses": 2,
                        "evictions": 0,
                        "bypasses": 0,
                    },
                    "hot_paths": [
                        {
                            "start_pc": "0x80200000",
                            "end_pc": "0x80200020",
                            "executions": 3,
                            "retired_instructions": 9,
                        }
                    ],
                    "traps": [
                        {
                            "pc": "0x80200010",
                            "cause": "0xd",
                            "privilege": "S",
                            "interrupt": False,
                            "count": 1,
                        }
                    ],
                    "memory_regions": [
                        {
                            "label": "ram",
                            "kind": "ram",
                            "accesses": 5,
                            "reads": 3,
                            "writes": 2,
                            "faults": 0,
                            "bytes": 20,
                        }
                    ],
                },
                "devices": {
                    "uart": {
                        "output_size": 14,
                        "recent_output": "Booting Linux\n",
                    }
                },
            },
            {
                "type": "uart_output",
                "text": "Booting Linux\n",
            },
        ]

        stdout = io.StringIO()
        with contextlib.redirect_stdout(stdout):
            rc = PROBE.emit_probe_summary(args, lines)

        self.assertEqual(rc, 0)
        self.assertIn(
            "load: image=Image format=flat addr=0x80200000 disk=rootfs.img block_transport=virtio-blk",
            stdout.getvalue(),
        )
        self.assertIn(
            "payloads: board.dtb@0x88000000 initrd.img@0x84000000",
            stdout.getvalue(),
        )
        self.assertIn("gpr-seeds: a0=0x0 a1=0x88000000", stdout.getvalue())
        self.assertIn("profile: retirements=4096 traps=1 memory=5", stdout.getvalue())
        self.assertIn(
            "shadow-cache: line_size=64 capacity_lines=64 resident_lines=2 line_accesses=5 hits=3 misses=2 evictions=0 bypasses=0",
            stdout.getvalue(),
        )
        self.assertIn(
            "hot-path: start=0x80200000 end=0x80200020 executions=3 retired=9",
            stdout.getvalue(),
        )
        self.assertIn(
            "trap-top: pc=0x80200010 cause=0xd privilege=S interrupt=false count=1",
            stdout.getvalue(),
        )
        self.assertIn(
            "memory-top: label=ram kind=ram accesses=5 reads=3 writes=2 faults=0 bytes=20",
            stdout.getvalue(),
        )
        self.assertIn('uart-tail: bytes=14 recent="Booting Linux\\n"', stdout.getvalue())
        self.assertIn("uart: Booting Linux", stdout.getvalue())

    def test_payload_actions_follow_primary_load_command(self) -> None:
        args = PROBE.parse_args(
            [
                "--target",
                "./mycpu",
                "--image",
                "Image",
                "--flat",
                "--addr",
                "0x80200000",
                "--payload",
                "board.dtb",
                "0x88000000",
                "--payload",
                "initrd.img",
                "0x84000000",
                "--step-cycles",
                "8",
            ]
        )

        commands = PROBE.build_commands(args)

        self.assertEqual(
            commands,
            [
                {
                    "cmd": "load",
                    "image": "Image",
                    "backend": "functional",
                    "flat": True,
                    "addr": 0x80200000,
                },
                {"cmd": "load_payload", "image": "board.dtb", "addr": 0x88000000},
                {"cmd": "load_payload", "image": "initrd.img", "addr": 0x84000000},
                {"cmd": "step_cycle", "count": 8},
                {"cmd": "snapshot"},
                {"cmd": "uart_output", "offset": 0},
                {"cmd": "quit"},
            ],
        )

    def test_set_reg_actions_follow_load_stage_before_probe_steps(self) -> None:
        args = PROBE.parse_args(
            [
                "--target",
                "./mycpu",
                "--image",
                "Image",
                "--flat",
                "--addr",
                "0x80200000",
                "--payload",
                "board.dtb",
                "0x88000000",
                "--set-reg",
                "a0",
                "0x0",
                "--set-reg",
                "a1",
                "0x88000000",
                "--step-cycles",
                "8",
            ]
        )

        commands = PROBE.build_commands(args)

        self.assertEqual(
            commands,
            [
                {
                    "cmd": "load",
                    "image": "Image",
                    "backend": "functional",
                    "flat": True,
                    "addr": 0x80200000,
                },
                {"cmd": "load_payload", "image": "board.dtb", "addr": 0x88000000},
                {"cmd": "set_gpr", "reg": "a0", "value": 0x0},
                {"cmd": "set_gpr", "reg": "a1", "value": 0x88000000},
                {"cmd": "step_cycle", "count": 8},
                {"cmd": "snapshot"},
                {"cmd": "uart_output", "offset": 0},
                {"cmd": "quit"},
            ],
        )

    def test_make_run_workload_forwards_payload_args(self) -> None:
        proc = subprocess.run(
            [
                "make",
                "-n",
                "run-workload",
                "WORKLOAD_NAME=xv6",
                "WORKLOAD_IMAGE=tests/data/linux_like_image.bin",
                "WORKLOAD_BUILD_COMMAND=:",
                "WORKLOAD_PAYLOAD_ARGS=--payload board.dtb 0x88000000 --payload initrd.img 0x84000000",
                "WORKLOAD_PROBE_CYCLES=8",
            ],
            cwd=MYCPU_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )

        self.assertEqual(proc.returncode, 0, msg=proc.stderr)
        self.assertIn("--payload board.dtb 0x88000000", proc.stdout)
        self.assertIn("--payload initrd.img 0x84000000", proc.stdout)

    def test_make_run_workload_forwards_gpr_seed_args(self) -> None:
        proc = subprocess.run(
            [
                "make",
                "-n",
                "run-workload",
                "WORKLOAD_NAME=xv6",
                "WORKLOAD_IMAGE=tests/data/linux_like_image.bin",
                "WORKLOAD_BUILD_COMMAND=:",
                "WORKLOAD_GPR_SEED_ARGS=--set-reg a0 0x0 --set-reg a1 0x88000000",
                "WORKLOAD_PROBE_CYCLES=8",
            ],
            cwd=MYCPU_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )

        self.assertEqual(proc.returncode, 0, msg=proc.stderr)
        self.assertIn("--set-reg a0 0x0", proc.stdout)
        self.assertIn("--set-reg a1 0x88000000", proc.stdout)

    def test_make_run_workload_linux_proto_derives_boot_contract_from_profile(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            proc = subprocess.run(
                [
                    "make",
                    "-n",
                    "run-workload",
                    "WORKLOAD_NAME=linux_proto",
                    f"LINUX_PROTO_EXTERNAL_DIR={temp_dir}",
                ],
                cwd=MYCPU_DIR,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                check=False,
            )

        self.assertEqual(proc.returncode, 0, msg=proc.stderr)
        self.assertIn(
            "make workloads/linux_proto/linux_sbi_shim.bin workloads/linux_proto/rootfs.cpio workloads/linux_proto/mycpu_virt.dtb",
            proc.stdout,
        )
        self.assertIn("--image workloads/linux_proto/linux_sbi_shim.bin", proc.stdout)
        self.assertIn("--flat", proc.stdout)
        self.assertIn("--addr 0x80000000", proc.stdout)
        self.assertIn(f"--payload {temp_dir}/arch/riscv/boot/Image 0x80200000", proc.stdout)
        self.assertIn("--payload workloads/linux_proto/mycpu_virt.dtb 0x87f00000", proc.stdout)
        self.assertIn("--payload workloads/linux_proto/rootfs.cpio 0x84000000", proc.stdout)
        self.assertIn("--set-reg a0 0x0", proc.stdout)
        self.assertIn("--set-reg a1 0x87f00000", proc.stdout)
        self.assertIn("--set-reg a2 0x80200000", proc.stdout)

    def test_make_build_workload_linux_proto_generates_fallback_initrd_and_dtb(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            proc = subprocess.run(
                [
                "make",
                "build-workload",
                    "WORKLOAD_NAME=linux_proto",
                    f"LINUX_PROTO_EXTERNAL_DIR={temp_dir}",
                ],
                cwd=MYCPU_DIR,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                check=False,
            )

        self.assertEqual(proc.returncode, 0, msg=proc.stderr)

        generated_initrd = MYCPU_DIR / "workloads" / "linux_proto" / "rootfs.cpio"
        generated_dts = MYCPU_DIR / "workloads" / "linux_proto" / "mycpu_virt.dts"
        self.assertTrue(generated_initrd.exists())
        self.assertTrue(generated_dts.exists())
        initrd_listing = subprocess.run(
            ["cpio", "-it"],
            cwd=MYCPU_DIR,
            input=generated_initrd.read_bytes(),
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
        )
        self.assertEqual(initrd_listing.returncode, 0, msg=initrd_listing.stderr.decode())
        self.assertIn("init", initrd_listing.stdout.decode())
        self.assertIn("post-init-smoke", initrd_listing.stdout.decode())
        self.assertIn("post-init-data.txt", initrd_listing.stdout.decode())
        generated_text = generated_dts.read_text()
        self.assertIn("linux,initrd-start = <0x0 0x84000000>;", generated_text)
        self.assertNotIn("linux,initrd-end = <0x0 0x84000000>;", generated_text)

    def test_make_run_workload_linux_proto_forwards_optional_disk_alias(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            disk_path = pathlib.Path(temp_dir) / "rootfs.img"
            disk_path.write_bytes(b"")
            proc = subprocess.run(
                [
                    "make",
                    "-n",
                    "run-workload",
                    "WORKLOAD_NAME=linux_proto",
                    f"LINUX_PROTO_EXTERNAL_DIR={temp_dir}",
                    f"LINUX_PROTO_DISK={disk_path}",
                ],
                cwd=MYCPU_DIR,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                check=False,
            )

        self.assertEqual(proc.returncode, 0, msg=proc.stderr)
        self.assertIn(f"--disk {disk_path}", proc.stdout)

    def test_make_build_workload_linux_proto_honors_bootargs_alias(self) -> None:
        bootargs = "console=ttyS0,115200 root=/dev/vda rw loglevel=7"
        with tempfile.TemporaryDirectory() as temp_dir:
            proc = subprocess.run(
                [
                    "make",
                    "build-workload",
                    "WORKLOAD_NAME=linux_proto",
                    f"LINUX_PROTO_EXTERNAL_DIR={temp_dir}",
                    f"LINUX_PROTO_BOOTARGS={bootargs}",
                ],
                cwd=MYCPU_DIR,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                check=False,
            )

        self.assertEqual(proc.returncode, 0, msg=proc.stderr)
        generated_dts = MYCPU_DIR / "workloads" / "linux_proto" / "mycpu_virt.dts"
        self.assertIn(bootargs, generated_dts.read_text())

    def test_make_build_workload_linux_proto_block_mode_generates_fallback_disk(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            proc = subprocess.run(
                [
                    "make",
                    "build-workload",
                    "WORKLOAD_NAME=linux_proto",
                    "LINUX_PROTO_ROOTFS_MODE=block",
                    f"LINUX_PROTO_EXTERNAL_DIR={temp_dir}",
                ],
                cwd=MYCPU_DIR,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                check=False,
            )

        self.assertEqual(proc.returncode, 0, msg=proc.stderr)
        generated_disk = MYCPU_DIR / "workloads" / "linux_proto" / "rootfs.ext4"
        self.assertTrue(generated_disk.exists())
        stat_init = subprocess.run(
            ["debugfs", "-R", "stat /init", str(generated_disk)],
            cwd=MYCPU_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )
        self.assertEqual(stat_init.returncode, 0, msg=stat_init.stderr)
        stat_post_init = subprocess.run(
            ["debugfs", "-R", "stat /post-init-smoke", str(generated_disk)],
            cwd=MYCPU_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )
        self.assertEqual(stat_post_init.returncode, 0, msg=stat_post_init.stderr)
        cat_post_init_data = subprocess.run(
            ["debugfs", "-R", "cat /post-init-data.txt", str(generated_disk)],
            cwd=MYCPU_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )
        self.assertEqual(cat_post_init_data.returncode, 0, msg=cat_post_init_data.stderr)
        self.assertEqual(cat_post_init_data.stdout, "post-init-data-ok\n")
        generated_dts = MYCPU_DIR / "workloads" / "linux_proto" / "mycpu_virt.dts"
        generated_text = generated_dts.read_text()
        self.assertIn("root=/dev/vda", generated_text)
        self.assertNotIn("rdinit=/init", generated_text)
        self.assertIn("linux,initrd-end = <0x0 0x84000000>;", generated_text)

    def test_make_run_workload_linux_proto_block_mode_uses_generated_disk(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            proc = subprocess.run(
                [
                    "make",
                    "-n",
                    "run-workload",
                    "WORKLOAD_NAME=linux_proto",
                    "LINUX_PROTO_ROOTFS_MODE=block",
                    f"LINUX_PROTO_EXTERNAL_DIR={temp_dir}",
                ],
                cwd=MYCPU_DIR,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                check=False,
            )

        self.assertEqual(proc.returncode, 0, msg=proc.stderr)
        self.assertIn("make workloads/linux_proto/linux_sbi_shim.bin workloads/linux_proto/rootfs.ext4 workloads/linux_proto/mycpu_virt.dtb", proc.stdout)
        self.assertIn("--disk workloads/linux_proto/rootfs.ext4", proc.stdout)
        self.assertNotIn("--payload workloads/linux_proto/rootfs.cpio 0x84000000", proc.stdout)

    def test_make_build_workload_linux_proto_block_mode_embeds_mininit_stage_markers(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            proc = subprocess.run(
                [
                    "make",
                    "build-workload",
                    "WORKLOAD_NAME=linux_proto",
                    "LINUX_PROTO_ROOTFS_MODE=block",
                    f"LINUX_PROTO_EXTERNAL_DIR={temp_dir}",
                ],
                cwd=MYCPU_DIR,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                check=False,
            )

        self.assertEqual(proc.returncode, 0, msg=proc.stderr)
        mininit_elf = MYCPU_DIR / "workloads" / "linux_proto" / "linux_mininit.elf"
        strings_proc = subprocess.run(
            ["strings", str(mininit_elf)],
            cwd=MYCPU_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )
        self.assertEqual(strings_proc.returncode, 0, msg=strings_proc.stderr)
        self.assertIn("mycpu linux initrd: stage=console-opened", strings_proc.stdout)
        self.assertIn("mycpu linux initrd: stage=rootfs-rw-ok", strings_proc.stdout)
        self.assertIn("mycpu linux initrd: stage=proc-readable", strings_proc.stdout)
        self.assertIn("mycpu linux initrd: stage=sys-readable", strings_proc.stdout)
        self.assertIn("mycpu linux initrd: stage=execve-post-init", strings_proc.stdout)
        self.assertIn("mycpu linux initrd: write short", strings_proc.stdout)
        self.assertIn("mycpu linux initrd: rootfs smoke failed", strings_proc.stdout)
        self.assertIn("mycpu linux initrd: proc smoke failed", strings_proc.stdout)
        self.assertIn("mycpu linux initrd: sys smoke failed", strings_proc.stdout)
        self.assertIn("mycpu linux initrd: exec post-init failed", strings_proc.stdout)
        self.assertNotIn("mycpu linux initrd: devtmpfs mount failed", strings_proc.stdout)

    def test_make_build_workload_linux_proto_block_mode_builds_post_init_smoke_elf(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            proc = subprocess.run(
                [
                    "make",
                    "build-workload",
                    "WORKLOAD_NAME=linux_proto",
                    "LINUX_PROTO_ROOTFS_MODE=block",
                    f"LINUX_PROTO_EXTERNAL_DIR={temp_dir}",
                ],
                cwd=MYCPU_DIR,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                check=False,
            )

        self.assertEqual(proc.returncode, 0, msg=proc.stderr)
        post_init_elf = MYCPU_DIR / "workloads" / "linux_proto" / "linux_postinit_smoke.elf"
        strings_proc = subprocess.run(
            ["strings", str(post_init_elf)],
            cwd=MYCPU_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )
        self.assertEqual(strings_proc.returncode, 0, msg=strings_proc.stderr)
        self.assertIn("mycpu linux userland: stage=file-readable", strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=rootfs-rw-roundtrip-ok", strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=fork-child-wrote", strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=parent-wait4-ok", strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=execve-third-stage", strings_proc.stdout)
        self.assertIn("mycpu linux userland: file smoke failed", strings_proc.stdout)
        self.assertIn("mycpu linux userland: rootfs rw smoke failed", strings_proc.stdout)
        self.assertIn("mycpu linux userland: process smoke failed", strings_proc.stdout)
        self.assertIn("mycpu linux userland: exec third-stage failed", strings_proc.stdout)
        self.assertIn("mycpu linux userland: post-init reached", strings_proc.stdout)

        third_stage_elf = MYCPU_DIR / "workloads" / "linux_proto" / "linux_postinit_exec_smoke.elf"
        third_stage_strings_proc = subprocess.run(
            ["strings", str(third_stage_elf)],
            cwd=MYCPU_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )
        self.assertEqual(third_stage_strings_proc.returncode, 0, msg=third_stage_strings_proc.stderr)
        self.assertIn("mycpu linux userland: stage=third-stage-reached", third_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=mkdir-chdir-ok", third_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=nested-file-roundtrip-ok", third_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=getdents64-nested-visible", third_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=fstatat-nested-stat-ok", third_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=renameat2-syscall-ok", third_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=renameat2-nested-ok", third_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=renameat2-dirent-updated", third_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=renameat2-cleanup-ok", third_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=unlinkat-parent-dirent-gone", third_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=mkdirat-dir-name-reusable", third_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=mkdirat-reused-dir-empty", third_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=mkdirat-reused-dir-dot-only", third_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: stage=mkdirat-reused-dir-parent-stat-ok", third_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: third-stage dir smoke failed", third_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: third-stage nested file smoke failed", third_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: third-stage getdents64 smoke failed", third_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: third-stage fstatat smoke failed", third_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: third-stage renameat2 smoke failed", third_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: third-stage renameat2 dirent smoke failed", third_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: third-stage renameat2 cleanup failed", third_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: third-stage unlinkat dirent smoke failed", third_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: third-stage mkdirat reuse smoke failed", third_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: third-stage mkdirat reused dir smoke failed", third_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: third-stage mkdirat reused dir dot-only smoke failed", third_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: third-stage mkdirat reused dir parent stat smoke failed", third_stage_strings_proc.stdout)
        self.assertIn("mycpu linux userland: post-init reached", third_stage_strings_proc.stdout)

    def test_make_build_workload_linux_proto_block_mode_mininit_message_lengths_exclude_nul(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            proc = subprocess.run(
                [
                    "make",
                    "build-workload",
                    "WORKLOAD_NAME=linux_proto",
                    "LINUX_PROTO_ROOTFS_MODE=block",
                    f"LINUX_PROTO_EXTERNAL_DIR={temp_dir}",
                ],
                cwd=MYCPU_DIR,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                check=False,
            )

        self.assertEqual(proc.returncode, 0, msg=proc.stderr)
        mininit_elf = MYCPU_DIR / "workloads" / "linux_proto" / "linux_mininit.elf"
        nm_proc = subprocess.run(
            ["riscv64-unknown-elf-nm", "-n", str(mininit_elf)],
            cwd=MYCPU_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )
        self.assertEqual(nm_proc.returncode, 0, msg=nm_proc.stderr)
        expected_lengths = {
            "stage_console_opened_msg_len": len("mycpu linux initrd: stage=console-opened\n".encode()),
            "stage_rootfs_rw_ok_msg_len": len("mycpu linux initrd: stage=rootfs-rw-ok\n".encode()),
            "stage_proc_readable_msg_len": len("mycpu linux initrd: stage=proc-readable\n".encode()),
            "stage_sys_readable_msg_len": len("mycpu linux initrd: stage=sys-readable\n".encode()),
            "boot_ok_msg_len": len("mycpu linux initrd: /init reached\n".encode()),
        }
        observed_lengths = {}
        for line in nm_proc.stdout.splitlines():
            parts = line.split()
            if len(parts) != 3:
                continue
            symbol = parts[2]
            if symbol not in expected_lengths:
                continue
            observed_lengths[symbol] = int(parts[0], 16)

        self.assertEqual(observed_lengths, expected_lengths)

    def test_make_run_workload_linux_proto_reports_missing_assets_via_probe(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            proc = subprocess.run(
                [
                    "make",
                    "run-workload",
                    "WORKLOAD_NAME=linux_proto",
                    f"LINUX_PROTO_EXTERNAL_DIR={temp_dir}",
                ],
                cwd=MYCPU_DIR,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                check=False,
            )

        self.assertNotEqual(proc.returncode, 0)
        combined_output = proc.stdout + proc.stderr
        self.assertIn("missing input files:", combined_output)
        self.assertIn(f"payload: {temp_dir}/arch/riscv/boot/Image", combined_output)
        self.assertNotIn("No rule to make target", combined_output)


if __name__ == "__main__":
    unittest.main()
