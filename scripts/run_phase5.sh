#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p results/phase5
python3 scripts/source_snapshot.py tfa-amd optee compiler-rt pyelftools --output results/phase5/source-verification.json
make all > results/phase5/build-qemu.log 2>&1
make PLATFORM=zcu106 all > results/phase5/build-zcu106.log 2>&1
make host-test > results/phase5/host-tests.log 2>&1
make llvm-runtime qemu-external > results/phase5/build-clients.log 2>&1
python3 scripts/build_external.py optee-sdk --log-dir results/phase5
python3 scripts/build_external.py ta-a --log-dir results/phase5
python3 scripts/build_external.py ta-b --log-dir results/phase5
python3 scripts/build_external.py optee --validation-pta --log-dir results/phase5 \
    --early-ta build/external/ta-a/6dc13418-534f-4642-9138-215876304910.stripped.elf \
    --early-ta build/external/ta-b/6dc13419-534f-4642-9138-215876304910.stripped.elf
python3 scripts/build_external.py tfa --configuration b1 --log-dir results/phase5
python3 scripts/build_external.py tfa --configuration b2 --log-dir results/phase5
python3 scripts/check_signed_ta.py --ta build/external/ta-a/6dc13418-534f-4642-9138-215876304910.ta --uuid 6dc13418-534f-4642-9138-215876304910 > results/phase5/signed-ta-console.log 2>&1
for configuration in baremetal tfa optee; do
    RESULT_DIR=results/phase5/qemu bash scripts/run_qemu.sh "$configuration" > "results/phase5/final-${configuration}-console.log" 2>&1
done
python3 scripts/collect_phase5_results.py
