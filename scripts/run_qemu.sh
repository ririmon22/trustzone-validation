#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
configuration="${1:-baremetal}"
qemu_binary="${QEMU_SYSTEM_AARCH64:-qemu-system-aarch64}"
result_dir="${RESULT_DIR:-results/qemu}"
mkdir -p "$result_dir"
log="${result_dir}/${configuration}.log"
if ! command -v "$qemu_binary" >/dev/null; then
    printf 'EXECUTION BLOCKED: qemu-system-aarch64 is not installed\n' | tee "$log"
    exit 2
fi
"$qemu_binary" --version > "${result_dir}/version.log"
args=(-nodefaults -accel tcg,thread=single -machine virt-9.2,secure=on,virtualization=on,gic-version=2 -cpu cortex-a53 -smp 1 -m 1024 -display none -serial stdio -monitor none -nic none)
case "$configuration" in
    baremetal)
        python3 scripts/check_memory_map.py --platform qemu --elf build/qemu/el3.elf --elf build/qemu/secure.elf --elf build/qemu/nonsecure.elf
        make qemu-rom
        args+=(-bios build/qemu/baremetal-rom.bin)
        ;;
    tfa|optee)
        python3 scripts/prepare_qemu_firmware.py "$configuration"
        args+=(-bios "build/qemu/${configuration}_flash.bin")
        if [[ "$configuration" == optee ]]; then
            args+=(-serial "file:${result_dir}/optee-secure.log")
        fi
        ;;
    *) printf 'Unknown configuration\n' >&2; exit 2 ;;
esac
if [[ "$configuration" == optee ]]; then
    export QEMU_TIMEOUT_SECONDS="${QEMU_TIMEOUT_SECONDS:-240}"
fi
python3 scripts/capture_qemu.py "$log" "$qemu_binary" "${args[@]}"
if [[ "$configuration" == baremetal ]]; then
    python3 scripts/check_tz_audit.py "$log" --output "${result_dir}/tz-audit.json"
fi
parse_args=()
if [[ "$configuration" == optee ]]; then
    parse_args+=(--require-security)
    python3 scripts/check_security_evidence.py "$log" "${result_dir}/optee-secure.log" --output "${result_dir}/security-evidence.json"
fi
python3 scripts/parse_results.py "$log" "${parse_args[@]}" --configuration "$configuration" --output "${result_dir}/${configuration}.json"
if [[ "$configuration" == baremetal || "$configuration" == optee ]]; then
    python3 scripts/analyze_benchmarks.py "$log" --output "${result_dir}/${configuration}-benchmarks.csv"
fi
