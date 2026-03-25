#!/bin/sh

set -eu

target="$1"
hello_elf="$2"
test_timeout="${3:-2s}"

expect_output() {
    label="$1"
    shift
    expected="$1"
    shift

    output="$(timeout "$test_timeout" "./$target" "$@" "$hello_elf" 2>&1)"
    status=$?
    if [ "$status" -eq 124 ]; then
        echo "Test timed out: $label" >&2
        exit 1
    fi
    if [ "$status" -ne 0 ]; then
        printf '%s\n' "$output" >&2
        exit "$status"
    fi
    if [ "$output" != "$expected" ]; then
        echo "Unexpected output for $label" >&2
        printf 'Expected: %s\n' "$expected" >&2
        printf 'Actual: %s\n' "$output" >&2
        exit 1
    fi
}

expect_output "backend default" "Hello, RISC-V!"
expect_output "backend functional" "Hello, RISC-V!" --backend functional
expect_output "backend pipeline" "Hello, RISC-V!" --backend pipeline

tmp_output="$(mktemp)"
set +e
timeout "$test_timeout" "./$target" --backend invalid "$hello_elf" >"$tmp_output" 2>&1
status=$?
set -e
output="$(cat "$tmp_output")"
rm -f "$tmp_output"
if [ "$status" -eq 0 ]; then
    echo "Expected invalid backend to fail" >&2
    printf 'Actual: %s\n' "$output" >&2
    exit 1
fi
if [ "$status" -eq 124 ]; then
    echo "Test timed out: backend invalid" >&2
    exit 1
fi
case "$output" in
    *"unknown backend"*)
        ;;
    *)
        echo "Unexpected error output for invalid backend" >&2
        printf 'Actual: %s\n' "$output" >&2
        exit 1
        ;;
esac
