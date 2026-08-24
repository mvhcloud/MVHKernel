#!/usr/bin/env bash
set -euo pipefail

image=${1:-build/mvh-kernel.iso}
disk=$(mktemp)
first_log=$(mktemp)
second_log=$(mktemp)
trap 'rm -f "$disk" "$first_log" "$second_log"' EXIT
truncate -s 32M "$disk"

run_kernel() {
  local commands=$1 output=$2 status=0
  (sleep 3; printf '%s' "$commands"; sleep 6) | timeout 15s qemu-system-x86_64 \
    -accel tcg -machine q35 -cpu qemu64 -m 512 -smp 2 \
    -display none -monitor none -no-reboot -no-shutdown -serial stdio \
    -drive "file=$disk,format=raw,if=none,id=vdisk" \
    -device virtio-blk-pci,disable-modern=on,drive=vdisk \
    -cdrom "$image" -boot d >"$output" 2>&1 || status=$?
  [[ $status -eq 0 || $status -eq 124 ]]
}

run_kernel $'pmkfs 0\npwrite virtio survives-virtio-reboot\n' "$first_log"
grep -F "VirtIO block queue initialized" "$first_log" || { cat "$first_log"; exit 1; }
grep -F "MVHFS formatted and mounted" "$first_log" || { cat "$first_log"; exit 1; }
run_kernel $'pmount 0\npcat virtio\n' "$second_log"
grep -F "MVHFS mounted" "$second_log" || { cat "$second_log"; exit 1; }
grep -F "survives-virtio-reboot" "$second_log" || { cat "$second_log"; exit 1; }
printf 'QEMU VirtIO block persistence passed across a cold reboot\n'
