#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
toolchain_lock="${TZ_TOOLCHAIN_LOCK:-results/offline/toolchain-baseline.json}"
if [[ ! -f "$toolchain_lock" ]]; then
    echo 'BLOCKED: capture and review a local toolchain baseline; see docs/offline_transfer.md' >&2
    exit 2
fi
python3 scripts/toolchain_manifest.py --verify "$toolchain_lock" --output results/phase7/execution-toolchain.json
for configuration in baremetal tfa optee; do
    RESULT_DIR=results/phase7/qemu bash scripts/run_qemu.sh "$configuration" > "results/phase7/${configuration}-console.log" 2>&1
done
python3 scripts/check_signed_ta.py --ta build/external/ta-a/6dc13418-534f-4642-9138-215876304910.ta --uuid 6dc13418-534f-4642-9138-215876304910 --output results/phase7/signed-ta > results/phase7/signed-ta-console.log 2>&1
python3 scripts/check_boot_plan.py --output results/phase7/boot-plan-check.json || test "$?" -eq 2
python3 scripts/report_performance_phase7.py
python3 scripts/collect_phase7_results.py
