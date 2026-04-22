#!/usr/bin/env python3
import contextlib
import importlib.util
import io
import pathlib
import subprocess
import unittest


MYCPU_DIR = pathlib.Path(__file__).resolve().parents[2]
MODULE_PATH = pathlib.Path(__file__).resolve().parents[2] / "workloads" / "run_debug_cli_probe.py"
MODULE_SPEC = importlib.util.spec_from_file_location("run_debug_cli_probe", MODULE_PATH)
if MODULE_SPEC is None or MODULE_SPEC.loader is None:
    raise RuntimeError(f"failed to load module spec for {MODULE_PATH}")
PROBE = importlib.util.module_from_spec(MODULE_SPEC)
MODULE_SPEC.loader.exec_module(PROBE)


class RunDebugCliProbeTest(unittest.TestCase):
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
        proc = subprocess.run(
            [
                "make",
                "-n",
                "run-workload",
                "WORKLOAD_NAME=linux_proto",
            ],
            cwd=MYCPU_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )

        self.assertEqual(proc.returncode, 0, msg=proc.stderr)
        self.assertIn("--image external/linux-riscv/arch/riscv/boot/Image", proc.stdout)
        self.assertIn("--flat", proc.stdout)
        self.assertIn("--addr 0x80200000", proc.stdout)
        self.assertIn("--payload external/linux-riscv/mycpu_virt.dtb 0x88000000", proc.stdout)
        self.assertIn("--payload external/linux-riscv/rootfs.cpio 0x84000000", proc.stdout)
        self.assertIn("--set-reg a0 0x0", proc.stdout)
        self.assertIn("--set-reg a1 0x88000000", proc.stdout)


if __name__ == "__main__":
    unittest.main()
