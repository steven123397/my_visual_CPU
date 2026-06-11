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
    parser.add_argument(
        "--l1d",
        action="store_true",
        help="enable the opt-in L1 data cache model for this probe",
    )
    parser.add_argument(
        "--translation-plan",
        action="store_true",
        help="emit an opt-in dry-run translation plan for the top hot path candidate",
    )
    parser.add_argument(
        "--jit-dispatch",
        action="store_true",
        help="emit an opt-in non-executable JIT dispatch dry-run summary",
    )
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
    if args.l1d:
        load["l1d"] = True
    commands.append(load)
    commands.extend(args.boot_actions)

    if args.probe_actions:
        commands.extend(args.probe_actions)
    else:
        commands.append({"cmd": "step_cycle", "count": args.step_cycles})

    if args.translation_plan:
        commands.append({"cmd": "translation_plan"})
    if args.jit_dispatch:
        commands.append({"cmd": "jit_dispatch"})

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
    if args.l1d:
        fields.append("l1d=on")
    return fields


def load_mode_payload(args) -> dict:
    payload = {
        "image": args.image,
        "format": "flat" if args.flat else "elf",
        "disk": args.disk if args.disk else "none",
        "block_transport": args.block_transport if args.block_transport else "default",
    }
    if args.flat:
        payload["addr"] = hex(args.addr)
    if args.l1d:
        payload["l1d"] = True
    return payload


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

    shadow_cache = profile.get("shadow_cache", {})
    if shadow_cache:
        print(
            "shadow-cache:",
            f"line_size={shadow_cache.get('line_size_bytes', 0)}",
            f"capacity_lines={shadow_cache.get('capacity_lines', 0)}",
            f"resident_lines={shadow_cache.get('resident_lines', 0)}",
            f"line_accesses={shadow_cache.get('line_accesses', 0)}",
            f"hits={shadow_cache.get('hits', 0)}",
            f"misses={shadow_cache.get('misses', 0)}",
            f"evictions={shadow_cache.get('evictions', 0)}",
            f"bypasses={shadow_cache.get('bypasses', 0)}",
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

    emit_translation_candidate_summary(profile)

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

    pc_costs = profile.get("pc_costs", [])
    if pc_costs:
        top = pc_costs[0]
        print(
            "pc-cost:",
            f"pc={profile_hex(top.get('pc', '0x0'))}",
            f"raw={profile_hex(top.get('raw', '0x0'))}",
            f"retirements={profile_int(top.get('retirements', 0))}",
            f"cycles={profile_int(top.get('cycles', 0))}",
            f"memory={profile_int(top.get('memory_observations', 0))}",
            f"reads={profile_int(top.get('memory_reads', 0))}",
            f"writes={profile_int(top.get('memory_writes', 0))}",
            f"faults={profile_int(top.get('memory_faults', 0))}",
            f"bytes={profile_int(top.get('memory_bytes', 0))}",
        )

    branch_targets = profile.get("branch_targets", [])
    if branch_targets:
        top = branch_targets[0]
        print(
            "branch-target:",
            f"pc={profile_hex(top.get('pc', '0x0'))}",
            f"raw={profile_hex(top.get('raw', '0x0'))}",
            f"target={profile_hex(top.get('target_pc', '0x0'))}",
            f"executions={profile_int(top.get('executions', 0))}",
            f"redirects={profile_int(top.get('redirects', 0))}",
        )


def build_probe_observation_event(args, snapshot) -> dict:
    summary = snapshot.get("summary", {})
    profile = snapshot.get("profile", {})
    hot_paths = profile.get("hot_paths", [])
    memory_regions = profile.get("memory_regions", [])
    backend = summary.get("backend", args.backend)
    cycle = profile_int(summary.get("cycle", 0))
    pc = summary.get("pc", "0x0")

    return {
        "schema_version": 1,
        "event_id": f"debug-probe:{backend}:{cycle}:{pc}",
        "source": "debug-probe",
        "phase": "probe-summary",
        "subject": {
            "target": args.target,
            "backend": backend,
            "pc": pc,
            "privilege": summary.get("privilege", "?"),
        },
        "timestamp_or_step": {
            "cycle": cycle,
            "instret": profile_int(summary.get("instret", 0)),
        },
        "effect": "observed",
        "payload": {
            "load": load_mode_payload(args),
            "profile": {
                "total_retirements": profile_int(profile.get("total_retirements", 0)),
                "total_traps": profile_int(profile.get("total_traps", 0)),
                "total_memory_observations": profile_int(
                    profile.get("total_memory_observations", 0)
                ),
                "shadow_cache": profile.get("shadow_cache", {}),
                "top_hot_path": hot_paths[0] if hot_paths else None,
                "top_memory_region": memory_regions[0] if memory_regions else None,
            },
        },
        "evidence_ref": {
            "debug_json": "snapshot",
            "profile_json": "snapshot.profile",
        },
    }


def emit_probe_observation_event(args, snapshot) -> None:
    event = build_probe_observation_event(args, snapshot)
    print(
        "observation-event:",
        json.dumps(event, sort_keys=True, separators=(",", ":")),
    )


def build_memory_observation_event(args, snapshot):
    summary = snapshot.get("summary", {})
    profile = snapshot.get("profile", {})
    memory_regions = profile.get("memory_regions", [])
    pc_costs = profile.get("pc_costs", [])
    total_memory_observations = profile_int(profile.get("total_memory_observations", 0))
    top_pc_cost = next(
        (
            cost
            for cost in pc_costs
            if profile_int(cost.get("memory_observations", 0)) > 0
        ),
        pc_costs[0] if pc_costs else None,
    )
    if total_memory_observations == 0 and not memory_regions and top_pc_cost is None:
        return None

    backend = summary.get("backend", args.backend)
    cycle = profile_int(summary.get("cycle", 0))
    pc = summary.get("pc", "0x0")
    return {
        "schema_version": 1,
        "event_id": f"memory-observation:{backend}:{cycle}:{pc}",
        "source": "memory-observation",
        "phase": "profile-summary",
        "subject": {
            "target": args.target,
            "backend": backend,
            "pc": pc,
            "privilege": summary.get("privilege", "?"),
        },
        "timestamp_or_step": {
            "cycle": cycle,
            "instret": profile_int(summary.get("instret", 0)),
        },
        "effect": "observed",
        "payload": {
            "total_memory_observations": total_memory_observations,
            "top_memory_region": memory_regions[0] if memory_regions else None,
            "top_pc_cost": top_pc_cost,
        },
        "evidence_ref": {
            "debug_json": "snapshot",
            "profile_json": "snapshot.profile.memory_regions",
            "pc_cost_json": "snapshot.profile.pc_costs",
        },
    }


def emit_memory_observation_event(args, snapshot) -> None:
    event = build_memory_observation_event(args, snapshot)
    if event is None:
        return
    print(
        "observation-event:",
        json.dumps(event, sort_keys=True, separators=(",", ":")),
    )


def build_cache_shadow_observation_event(args, snapshot):
    summary = snapshot.get("summary", {})
    shadow_cache = snapshot.get("profile", {}).get("shadow_cache", {})
    if not shadow_cache:
        return None

    backend = summary.get("backend", args.backend)
    cycle = profile_int(summary.get("cycle", 0))
    pc = summary.get("pc", "0x0")
    return {
        "schema_version": 1,
        "event_id": f"cache-shadow:{backend}:{cycle}:{pc}",
        "source": "cache-shadow",
        "phase": "profile-summary",
        "subject": {
            "target": args.target,
            "backend": backend,
            "pc": pc,
            "privilege": summary.get("privilege", "?"),
        },
        "timestamp_or_step": {
            "cycle": cycle,
            "instret": profile_int(summary.get("instret", 0)),
        },
        "effect": "observed",
        "payload": shadow_cache,
        "evidence_ref": {
            "debug_json": "snapshot",
            "profile_json": "snapshot.profile.shadow_cache",
        },
    }


def emit_cache_shadow_observation_event(args, snapshot) -> None:
    event = build_cache_shadow_observation_event(args, snapshot)
    if event is None:
        return
    print(
        "observation-event:",
        json.dumps(event, sort_keys=True, separators=(",", ":")),
    )


def profile_int(value, default=0) -> int:
    try:
        if isinstance(value, str):
            return int(value, 0)
        return int(value)
    except (TypeError, ValueError):
        return default


def profile_hex(value) -> str:
    if isinstance(value, str):
        return value
    return hex(profile_int(value))


def select_translation_candidate(profile):
    hot_paths = profile.get("hot_paths", [])
    if not hot_paths:
        return None, "no-hot-paths"

    candidates = sorted(
        hot_paths,
        key=lambda entry: (
            -profile_int(entry.get("executions", 0)),
            -profile_int(entry.get("retired_instructions", 0)),
            profile_int(entry.get("start_pc", 0)),
            profile_int(entry.get("end_pc", 0)),
        ),
    )
    top = candidates[0]
    if profile_int(top.get("executions", 0)) < 2:
        return None, "insufficient-repetition"
    if profile_int(top.get("retired_instructions", 0)) == 0:
        return None, "empty-hot-path"
    return top, ""


def emit_translation_candidate_summary(profile) -> None:
    candidate, reason = select_translation_candidate(profile)
    if candidate is None:
        print("translation-candidate:", f"none reason={reason}")
        return
    print(
        "translation-candidate:",
        f"start={profile_hex(candidate.get('start_pc', '0x0'))}",
        f"end={profile_hex(candidate.get('end_pc', '0x0'))}",
        f"executions={profile_int(candidate.get('executions', 0))}",
        f"retired={profile_int(candidate.get('retired_instructions', 0))}",
    )


def emit_translation_plan_summary(plan) -> None:
    if not plan:
        print("translation-plan:", "none reason=missing-translation-plan")
        return

    status = plan.get("status", "none")
    if status == "none":
        print("translation-plan:", f"none reason={plan.get('reason', 'unknown')}")
        return

    fields = [
        f"start={profile_hex(plan.get('start_pc', '0x0'))}",
        f"end={profile_hex(plan.get('end_pc', '0x0'))}",
        f"executions={profile_int(plan.get('executions', 0))}",
        f"retired={profile_int(plan.get('retired_instructions', 0))}",
        f"inlineable={profile_int(plan.get('inlineable_instructions', 0))}",
    ]
    if status == "fallback":
        fields.append(f"fallback_pc={profile_hex(plan.get('fallback_pc', '0x0'))}")
        fields.append(f"reason={plan.get('reason', 'unknown')}")
        boundary_kind = plan.get("boundary_kind", "")
        if boundary_kind:
            fields.append(f"boundary={boundary_kind}")
    print("translation-plan:", status, *fields)


def emit_jit_dispatch_summary(dispatch) -> None:
    if not dispatch:
        print("jit-dispatch:", "none reason=missing-jit-dispatch")
        return

    print(
        "jit-dispatch:",
        f"source={dispatch.get('source', 'none')}",
        f"action={dispatch.get('action', 'none')}",
        f"ok={'true' if dispatch.get('ok', False) else 'false'}",
        f"start={profile_hex(dispatch.get('start_pc', '0x0'))}",
        f"end={profile_hex(dispatch.get('end_pc', '0x0'))}",
        f"cache={dispatch.get('cache_state', 'unknown')}",
        f"planned={'true' if dispatch.get('planned', False) else 'false'}",
        f"translated={'true' if dispatch.get('translated', False) else 'false'}",
        f"lowered={'true' if dispatch.get('lowered', False) else 'false'}",
        f"fallback={'true' if dispatch.get('fallback_to_reference', False) else 'false'}",
        f"lowered_ops={profile_int(dispatch.get('lowered_instruction_count', 0))}",
        f"executions={profile_int(dispatch.get('candidate_executions', 0))}",
        f"retired={profile_int(dispatch.get('candidate_retired_instructions', 0))}",
        f"reject={dispatch.get('reject_kind', 'none')}",
        f"reason={dispatch.get('reject_reason', 'none')}",
        f"helper={dispatch.get('helper_replay_kind', 'none')}",
        f"host_code={'true' if dispatch.get('host_code', False) else 'false'}",
        f"executable_memory={'true' if dispatch.get('executable_memory', False) else 'false'}",
        f"guest_execution={'true' if dispatch.get('guest_execution', False) else 'false'}",
    )


def emit_jit_dispatch_observation_event(dispatch) -> None:
    event = dispatch.get("observation_event") if dispatch else None
    if not event:
        return
    print(
        "observation-event:",
        json.dumps(event, sort_keys=True, separators=(",", ":")),
    )


def emit_l1d_cache_summary(snapshot) -> None:
    cache = snapshot.get("l1_data_cache", {})
    if not cache or not cache.get("enabled", False):
        return
    print(
        "l1d-cache:",
        f"enabled={'true' if cache.get('enabled', False) else 'false'}",
        f"line_size={cache.get('line_size_bytes', 0)}",
        f"capacity_lines={cache.get('capacity_lines', 0)}",
        f"loads={cache.get('loads', 0)}",
        f"stores={cache.get('stores', 0)}",
        f"hits={cache.get('hits', 0)}",
        f"misses={cache.get('misses', 0)}",
        f"evictions={cache.get('evictions', 0)}",
        f"bypasses={cache.get('bypasses', 0)}",
        f"write_through_stores={cache.get('write_through_stores', 0)}",
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
    translation_plan = next(
        (line for line in reversed(lines) if line.get("type") == "translation_plan"),
        None,
    )
    jit_dispatch = next(
        (line for line in reversed(lines) if line.get("type") == "jit_dispatch"),
        None,
    )
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
    emit_probe_observation_event(args, snapshot)
    emit_memory_observation_event(args, snapshot)
    emit_cache_shadow_observation_event(args, snapshot)
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
    if args.translation_plan:
        emit_translation_plan_summary(translation_plan)
    if args.jit_dispatch:
        emit_jit_dispatch_summary(jit_dispatch)
        emit_jit_dispatch_observation_event(jit_dispatch)
    emit_l1d_cache_summary(snapshot)
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
