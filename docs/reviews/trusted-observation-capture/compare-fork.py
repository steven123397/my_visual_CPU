#!/usr/bin/env python3
"""Compare observation-off/on fork evidence using structured protocol fields."""
import argparse
import hashlib
import json
from pathlib import Path


def read(path):
    return [json.loads(line) for line in path.read_text().splitlines() if line.startswith("{")]


def number(value):
    return int(value, 0) if isinstance(value, str) else value


def compare(off_path, on_path):
    off, on = read(off_path), read(on_path)
    events = {"mapping", "zombie", "reclaimed"}
    off_events = [row for row in off if row.get("event") in events]
    on_events = [row for row in on if row.get("event") in events]
    assert off_events == on_events, "mapping/OS events or guest counters differ"
    off_uart = [row for row in off if row.get("type") == "uart_output"]
    on_uart = [row for row in on if row.get("type") == "uart_output"]
    assert len(off_uart) == 1 and off_uart == on_uart, "UART through REAP/shell prompt differs"
    for rows in (off, on):
        results = [row for row in rows if row.get("event") == "result"]
        assert len(results) == 1 and results[0]["passed"] and results[0]["existing_forktest"]
    metadata = [row for row in on if row.get("type") == "capture-metadata"]
    records = [row for row in on if row.get("type") == "memory-record"]
    assert len(metadata) == 1 and len(records) == 2
    meta = metadata[0]
    assert meta["state"] == "stopped" and meta["supported_window_complete"]
    assert number(meta["recorded"]) == 2 and number(meta["dropped"]) == number(meta["unsupported"]) == 0
    assert number(meta["attempted"]) == number(meta["filtered"]) + 2
    before = {row["va"]: row for row in on_events if row.get("phase") == "before"}
    after = {row["va"]: row for row in on_events if row.get("phase") == "after"}
    assert len(before) == len(after) == 2
    assert len({row["instruction_id"] for row in records}) == 2
    assert {number(row["va"]) for row in records} == set(before)
    for row in records:
        mapping = after[number(row["va"])]
        assert row["schema_version"] == 1 and row["backend"] == "functional" and row["hart"] == 0
        assert row["capture_id"] == meta["capture_id"] and row["machine_generation"] == meta["machine_generation"]
        assert row["operation"] == "store" and row["privilege"] == row["effective_privilege"] == "U"
        assert number(row["satp"]) == (8 << 60) | (mapping["child_root"] >> 12)
        assert row["retired"] and row["trap"] is None and row["bytes"] == 8
        assert number(row["store_value"]) == mapping["child_value"]
        assert number(row["end_instret"]) == number(row["start_instret"]) + 1
        assert len(row["segments"]) == 1
        segment = row["segments"][0]
        assert number(segment["va"]) == mapping["va"] and number(segment["pa"]) == mapping["child_pa"]
        assert segment["write"] and segment["bytes"] == 8 and segment["outcome"] == "success"
        assert number(segment["value"]) == mapping["child_value"]
    return {
        "passed": True,
        "off_stdout_sha256": hashlib.sha256(off_path.read_bytes()).hexdigest(),
        "capture_stdout_sha256": hashlib.sha256(on_path.read_bytes()).hexdigest(),
        "identical_mapping_os_events": len(off_events),
        "identical_uart_bytes_through_reap": off_uart[0]["next_offset"],
        "existing_forktest_passed_both": True,
        "captured_actual_child_stores": len(records),
        "metadata": meta,
        "records": records,
        "scope": "functional/single hart; two user pages; UART comparison ends before existing forktest",
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("off", type=Path)
    parser.add_argument("capture", type=Path)
    args = parser.parse_args()
    print(json.dumps(compare(args.off, args.capture), ensure_ascii=True, indent=2))
