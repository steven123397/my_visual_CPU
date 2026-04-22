#!/usr/bin/env python3
import argparse
import json
import pathlib
import subprocess
import sys


class ProbeAction(argparse.Action):
    def __call__(self, parser, namespace, values, option_string=None):
        actions = list(getattr(namespace, "probe_actions", []))
        if option_string == "--uart-wait":
            text, max_steps_text = values
            actions.append(
                {
                    "cmd": "run_until_uart_contains",
                    "text": text,
                    "max_steps": int(max_steps_text, 0),
                }
            )
        elif option_string == "--uart-input":
            actions.append({"cmd": "uart_input", "text": values})
        else:
            raise ValueError(f"unsupported probe action: {option_string}")
        setattr(namespace, "probe_actions", actions)


class BootAction(argparse.Action):
    def __call__(self, parser, namespace, values, option_string=None):
        boot_actions = list(getattr(namespace, "boot_actions", []))
        gpr_seeds = list(getattr(namespace, "gpr_seeds", []))
        if option_string == "--payload":
            image, addr_text = values
            boot_actions.append(
                {"cmd": "load_payload", "image": image, "addr": int(addr_text, 0)}
            )
        elif option_string == "--set-reg":
            reg, value_text = values
            value = int(value_text, 0)
            boot_actions.append({"cmd": "set_gpr", "reg": reg, "value": value})
            gpr_seeds.append({"reg": reg, "value": value})
        else:
            raise ValueError(f"unsupported boot action: {option_string}")
        setattr(namespace, "boot_actions", boot_actions)
        setattr(namespace, "gpr_seeds", gpr_seeds)


def create_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Run a bounded debug-cli workload probe")
    parser.add_argument("--target", required=True)
    parser.add_argument("--image", required=True)
    parser.add_argument("--disk", default="")
    parser.add_argument("--block-transport", default="")
    parser.add_argument("--backend", default="functional")
    parser.add_argument("--flat", action="store_true")
    parser.add_argument(
        "--addr",
        type=lambda value: int(value, 0),
        default=None,
        help="load address for flat binaries; required together with --flat",
    )
    parser.add_argument(
        "--payload",
        nargs=2,
        action=BootAction,
        metavar=("IMAGE", "ADDR"),
        help="load an extra flat payload into RAM after the primary image",
    )
    parser.add_argument(
        "--set-reg",
        nargs=2,
        action=BootAction,
        metavar=("REG", "VALUE"),
        help="seed an architectural integer register before the probe runs",
    )
    parser.add_argument("--step-cycles", type=int, default=4)
    parser.add_argument(
        "--uart-wait",
        nargs=2,
        action=ProbeAction,
        metavar=("TEXT", "MAX_STEPS"),
        help="run until UART output contains TEXT, or fail after MAX_STEPS cycles",
    )
    parser.add_argument(
        "--uart-input",
        action=ProbeAction,
        help="inject UART input after the previous probe stage",
    )
    parser.set_defaults(probe_actions=[], boot_actions=[], gpr_seeds=[])
    return parser


def parse_args(argv=None):
    args = create_parser().parse_args(argv)
    if args.flat and args.addr is None:
        create_parser().error("--flat requires --addr")
    if args.addr is not None and not args.flat:
        create_parser().error("--addr requires --flat")
    return args


def build_commands(args):
    commands = []
    load = {
        "cmd": "load",
        "image": args.image,
        "backend": args.backend,
    }
    if args.disk:
        load["disk"] = args.disk
    if args.block_transport:
        load["block_transport"] = args.block_transport
    if args.flat:
        load["flat"] = True
        load["addr"] = args.addr
    commands.append(load)
    commands.extend(args.boot_actions)

    if args.probe_actions:
        commands.extend(args.probe_actions)
    else:
        commands.append({"cmd": "step_cycle", "count": args.step_cycles})

    commands.extend(
        [
            {"cmd": "snapshot"},
            {"cmd": "uart_output", "offset": 0},
            {"cmd": "quit"},
        ]
    )
    return commands


def build_script(commands) -> str:
    return "\n".join([json.dumps(command) for command in commands] + [""])


def run_probe(target: str, commands):
    return subprocess.run(
        [target, "--debug-cli"],
        input=build_script(commands).encode(),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )


def load_mode_summary(args) -> list[str]:
    fields = [
        f"image={args.image}",
        f"format={'flat' if args.flat else 'elf'}",
    ]
    if args.flat:
        fields.append(f"addr={hex(args.addr)}")
    fields.append(f"disk={args.disk if args.disk else 'none'}")
    fields.append(
        f"block_transport={args.block_transport if args.block_transport else 'default'}"
    )
    return fields


def referenced_input_paths(args) -> list[tuple[str, str]]:
    paths = [("image", args.image)]
    if args.disk:
        paths.append(("disk", args.disk))
    for action in args.boot_actions:
        if action["cmd"] == "load_payload":
            paths.append(("payload", action["image"]))
    return paths


def missing_input_paths(args) -> list[tuple[str, str]]:
    missing = []
    for kind, path in referenced_input_paths(args):
        if not pathlib.Path(path).exists():
            missing.append((kind, path))
    return missing


def emit_top_profile_entries(profile) -> None:
    print(
        "profile:",
        f"retirements={profile.get('total_retirements', 0)}",
        f"traps={profile.get('total_traps', 0)}",
        f"memory={profile.get('total_memory_observations', 0)}",
    )

    hot_paths = profile.get("hot_paths", [])
    if hot_paths:
        top = hot_paths[0]
        print(
            "hot-path:",
            f"start={top.get('start_pc', '0x0')}",
            f"end={top.get('end_pc', '0x0')}",
            f"executions={top.get('executions', 0)}",
            f"retired={top.get('retired_instructions', 0)}",
        )

    traps = profile.get("traps", [])
    if traps:
        top = traps[0]
        print(
            "trap-top:",
            f"pc={top.get('pc', '0x0')}",
            f"cause={top.get('cause', '0x0')}",
            f"privilege={top.get('privilege', '?')}",
            f"interrupt={'true' if top.get('interrupt', False) else 'false'}",
            f"count={top.get('count', 0)}",
        )

    memory_regions = profile.get("memory_regions", [])
    if memory_regions:
        top = memory_regions[0]
        print(
            "memory-top:",
            f"label={top.get('label', '')}",
            f"kind={top.get('kind', '')}",
            f"accesses={top.get('accesses', 0)}",
            f"reads={top.get('reads', 0)}",
            f"writes={top.get('writes', 0)}",
            f"faults={top.get('faults', 0)}",
            f"bytes={top.get('bytes', 0)}",
        )


def emit_gpr_seed_summary(args) -> None:
    if not args.gpr_seeds:
        return
    formatted = [f"{seed['reg']}={hex(seed['value'])}" for seed in args.gpr_seeds]
    print("gpr-seeds:", *formatted)


def emit_payload_summary(args) -> None:
    payloads = [action for action in args.boot_actions if action["cmd"] == "load_payload"]
    if not payloads:
        return
    formatted = [f"{payload['image']}@{hex(payload['addr'])}" for payload in payloads]
    print("payloads:", *formatted)


def emit_probe_summary(args, lines) -> int:
    snapshot = next((line for line in reversed(lines) if line.get("type") == "snapshot"), None)
    uart = next((line for line in reversed(lines) if line.get("type") == "uart_output"), None)
    if snapshot is None:
        sys.stderr.write("debug-cli probe did not return a snapshot\n")
        return 1

    summary = snapshot["summary"]
    csrs = snapshot["csrs"]
    print("load:", *load_mode_summary(args))
    emit_payload_summary(args)
    emit_gpr_seed_summary(args)
    print(
        "summary:",
        f"cycle={summary['cycle']}",
        f"instret={summary['instret']}",
        f"pc={summary['pc']}",
        f"privilege={summary['privilege']}",
        f"backend={summary['backend']}",
    )
    print(
        "trap-m:",
        f"mcause={csrs['mcause']}",
        f"mepc={csrs['mepc']}",
        f"mtval={csrs['mtval']}",
    )
    print(
        "trap-s:",
        f"scause={csrs['scause']}",
        f"sepc={csrs['sepc']}",
        f"stval={csrs['stval']}",
        f"stvec={csrs['stvec']}",
        f"satp={csrs['satp']}",
    )
    emit_top_profile_entries(snapshot.get("profile", {}))
    uart_snapshot = snapshot.get("devices", {}).get("uart", {})
    print(
        "uart-tail:",
        f"bytes={uart_snapshot.get('output_size', 0)}",
        f"recent={json.dumps(uart_snapshot.get('recent_output', ''))}",
    )
    if uart is not None:
        print("uart:", uart.get("text", ""))
    return 0


def main(argv=None) -> int:
    args = parse_args(argv)
    missing = missing_input_paths(args)
    if missing:
        sys.stderr.write("missing input files:\n")
        for kind, path in missing:
            sys.stderr.write(f"  {kind}: {path}\n")
        return 1
    proc = run_probe(args.target, build_commands(args))
    if proc.returncode != 0:
        sys.stdout.write(proc.stdout.decode())
        sys.stderr.write(proc.stderr.decode())
        return proc.returncode

    lines = [json.loads(line) for line in proc.stdout.decode().splitlines() if line.strip()]
    return emit_probe_summary(args, lines)


if __name__ == "__main__":
    raise SystemExit(main())
