#!/usr/bin/env bash
set -euo pipefail

image=${1:-build/mvh-kernel.iso}
disk=$(mktemp)
first_log=$(mktemp)
second_log=$(mktemp)
trap 'rm -f "$disk" "$first_log" "$second_log"' EXIT
truncate -s 16M "$disk"

run_kernel() {
  local commands=$1
  local output=$2
  local status=0
  (sleep 3; printf '%s' "$commands"; sleep 6) | timeout 15s qemu-system-x86_64 \
    -accel tcg -machine pc -cpu qemu64 -m 128 -smp 1 \
    -display none -monitor none -no-reboot -no-shutdown -serial stdio \
    -drive "file=$disk,format=raw,if=ide,index=0" -cdrom "$image" -boot d \
    >"$output" 2>&1 || status=$?
  [[ $status -eq 0 || $status -eq 124 ]]
}

run_kernel $'pmkfs 0\npwrite persisted survives-reboot\n' "$first_log"
grep -F "MVHFS formatted and mounted" "$first_log"
run_kernel $'pmount 0\npcat persisted\n' "$second_log"
grep -F "MVHFS mounted" "$second_log"
grep -F "survives-reboot" "$second_log"
printf 'QEMU MVHFS persistence passed across a cold reboot\n'
