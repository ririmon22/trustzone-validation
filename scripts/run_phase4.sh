#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p results/phase4
python3 scripts/source_snapshot.py tfa-amd optee compiler-rt pyelftools --output results/phase4/source-verification-final.json
make all > results/phase4/build-qemu.log 2>&1
make PLATFORM=zcu106 all > results/phase4/build-zcu106.log 2>&1
make host-test > results/phase4/host-tests.log 2>&1
make llvm-runtime qemu-external > results/phase4/build-external-clients.log 2>&1
python3 scripts/build_external.py tfa --platform zynqmp
python3 scripts/build_external.py optee-sdk
python3 scripts/build_external.py ta-a
python3 scripts/build_external.py ta-b
python3 scripts/build_external.py optee \
    --early-ta build/external/ta-a/6dc13418-534f-4642-9138-215876304910.stripped.elf \
    --early-ta build/external/ta-b/6dc13419-534f-4642-9138-215876304910.stripped.elf
python3 scripts/build_external.py tfa --configuration b1
python3 scripts/build_external.py tfa --configuration b2
for configuration in baremetal tfa optee; do
    bash scripts/run_qemu.sh "$configuration" > "results/phase4/final-${configuration}-console.log" 2>&1
done
python3 scripts/collect_phase4_results.py
