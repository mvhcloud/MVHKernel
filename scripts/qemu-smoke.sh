#!/usr/bin/env bash
set -euo pipefail

image=${1:-build/mvh-kernel.iso}
machine=${2:-pc}
memory=${3:-128}
cpus=${4:-1}
serial_log=$(mktemp)
error_log=$(mktemp)
trap 'rm -f "$serial_log" "$error_log"' EXIT

status=0
timeout 15s qemu-system-x86_64 \
  -accel tcg -machine "$machine" -cpu qemu64 -m "$memory" -smp "$cpus" \
  -display none -monitor none -no-reboot -no-shutdown \
  -serial "file:$serial_log" -cdrom "$image" -boot d 2>"$error_log" || status=$?

if [[ $status -ne 0 && $status -ne 124 ]]; then
  cat "$error_log" >&2
  exit "$status"
fi
grep -F "MVH Kernel 1.1.8/2 build" "$serial_log"
grep -F "MVH kernel ready" "$serial_log"
grep -F "mvh>" "$serial_log"
printf 'QEMU smoke passed: machine=%s memory=%sMiB cpus=%s\n' "$machine" "$memory" "$cpus"
