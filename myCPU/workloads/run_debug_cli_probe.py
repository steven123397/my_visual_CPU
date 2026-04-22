#!/usr/bin/env python3
import argparse
import json
import subprocess
import sys


def main() -> int:
    parser = argparse.ArgumentParser(description="Run a bounded debug-cli workload probe")
    parser.add_argument("--target", required=True)
    parser.add_argument("--image", required=True)
    parser.add_argument("--disk", default="")
    parser.add_argument("--block-transport", default="")
    parser.add_argument("--backend", default="functional")
    parser.add_argument("--step-cycles", type=int, default=4)
    args = parser.parse_args()

    load = {
        "cmd": "load",
        "image": args.image,
        "backend": args.backend,
    }
    if args.disk:
        load["disk"] = args.disk
    if args.block_transport:
        load["block_transport"] = args.block_transport

    script = "\n".join([
        json.dumps(load),
        json.dumps({"cmd": "step_cycle", "count": args.step_cycles}),
        json.dumps({"cmd": "uart_output", "offset": 0}),
        json.dumps({"cmd": "quit"}),
        "",
    ])

    proc = subprocess.run(
        [args.target, "--debug-cli"],
        input=script.encode(),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if proc.returncode != 0:
        sys.stdout.write(proc.stdout.decode())
        sys.stderr.write(proc.stderr.decode())
        return proc.returncode

    lines = [json.loads(line) for line in proc.stdout.decode().splitlines() if line.strip()]
    snapshot = next((line for line in lines if line.get("type") == "snapshot"), None)
    uart = next((line for line in lines if line.get("type") == "uart_output"), None)
    if snapshot is None:
        sys.stderr.write("debug-cli probe did not return a snapshot\n")
        return 1

    summary = snapshot["summary"]
    csrs = snapshot["csrs"]
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
    if uart is not None:
        print("uart:", uart.get("text", ""))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
