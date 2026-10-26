#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p results/phase6
python3 scripts/source_snapshot.py tfa-amd optee compiler-rt pyelftools --output results/phase6/source-verification.json
make all > results/phase6/build-qemu.log 2>&1
make PLATFORM=zcu106 all > results/phase6/build-zcu106.log 2>&1
make host-test > results/phase6/host-tests.log 2>&1
make llvm-runtime qemu-external > results/phase6/build-clients.log 2>&1
python3 scripts/build_external.py optee-sdk --log-dir results/phase6
python3 scripts/build_external.py ta-a --log-dir results/phase6
python3 scripts/build_external.py ta-b --log-dir results/phase6
python3 scripts/build_external.py optee --validation-pta --log-dir results/phase6 \
    --early-ta build/external/ta-a/6dc13418-534f-4642-9138-215876304910.stripped.elf \
    --early-ta build/external/ta-b/6dc13419-534f-4642-9138-215876304910.stripped.elf
python3 scripts/build_external.py tfa --configuration b1 --log-dir results/phase6
python3 scripts/build_external.py tfa --configuration b2 --log-dir results/phase6
python3 scripts/check_signed_ta.py --ta build/external/ta-a/6dc13418-534f-4642-9138-215876304910.ta --uuid 6dc13418-534f-4642-9138-215876304910 --output results/phase6/signed-ta > results/phase6/signed-ta-console.log 2>&1
for configuration in baremetal tfa optee; do
    RESULT_DIR=results/phase6/qemu bash scripts/run_qemu.sh "$configuration" > "results/phase6/final-${configuration}-console.log" 2>&1
done
python3 scripts/check_boot_plan.py --output results/phase6/boot-plan-check.json || test "$?" -eq 2
python3 scripts/check_memory_map.py --platform zcu106 --require-protection > results/phase6/zcu106-load-gate.log 2>&1 || test "$?" -eq 1
python3 scripts/check_hardware_profile.py > results/phase6/zcu106-profile-gate.log 2>&1 || test "$?" -eq 2
python3 scripts/check_boot_plan.py --inventory build/zcu106/el3.elf build/zcu106/secure.elf build/zcu106/nonsecure.elf build/zcu106/tfa_bl33.elf build/zcu106/optee_bl33.elf build/external/tfa-zynqmp/zynqmp/release/bl31/bl31.elf --output results/phase6/zcu106-elf-inventory.json
python3 scripts/report_performance.py
python3 scripts/collect_phase6_results.py
