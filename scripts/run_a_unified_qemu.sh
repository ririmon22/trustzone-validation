#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
result_dir="${RESULT_DIR:-results/a-unified}"
mkdir -p "$result_dir"
make PLATFORM=qemu a-unified build/qemu/bootstrap.bin
python3 scripts/pack_qemu_rom.py --unified --stub build/qemu/bootstrap.bin --output build/qemu/unified/baremetal-rom.bin build/qemu/unified/a_unified.elf
python3 scripts/capture_qemu.py "$result_dir/normal.log" "${QEMU_SYSTEM_AARCH64:-qemu-system-aarch64}" -nodefaults -accel tcg,thread=single -machine virt-9.2,secure=on,virtualization=on,gic-version=2 -cpu cortex-a53 -smp 1 -m 1024 -display none -serial stdio -monitor none -nic none -bios build/qemu/unified/baremetal-rom.bin
python3 scripts/check_tz_audit.py "$result_dir/normal.log" --output "$result_dir/tz-audit.json"
python3 scripts/parse_results.py "$result_dir/normal.log" --configuration baremetal --output "$result_dir/results.json"
