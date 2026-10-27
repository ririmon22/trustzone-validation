#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
if [[ -e build/qemu || -e build/external ]]; then
    echo 'CLEAN BUILD REQUIRED: use scripts/run_phase7.sh to create a fresh workspace' >&2
    exit 2
fi
mkdir -p results/phase7
toolchain_lock="${TZ_TOOLCHAIN_LOCK:-results/offline/toolchain-baseline.json}"
if [[ ! -f "$toolchain_lock" ]]; then
    echo 'BLOCKED: capture and review a local toolchain baseline; see docs/offline_transfer.md' >&2
    exit 2
fi
python3 scripts/toolchain_manifest.py --verify "$toolchain_lock" --output results/phase7/toolchain.json
python3 scripts/source_snapshot.py tfa-amd optee compiler-rt pyelftools --output results/phase7/source-verification.json
make all > results/phase7/build-qemu.log 2>&1
make PLATFORM=zcu106 all > results/phase7/build-zcu106.log 2>&1
make llvm-runtime qemu-external qemu-rom > results/phase7/build-clients.log 2>&1
make host-test > results/phase7/host-tests.log 2>&1
python3 scripts/build_external.py tfa --platform zynqmp --log-dir results/phase7
python3 scripts/build_external.py optee-sdk --log-dir results/phase7
python3 scripts/build_external.py ta-a --log-dir results/phase7
python3 scripts/build_external.py ta-b --log-dir results/phase7
python3 scripts/build_external.py optee --validation-pta --log-dir results/phase7 \
    --early-ta build/external/ta-a/6dc13418-534f-4642-9138-215876304910.stripped.elf \
    --early-ta build/external/ta-b/6dc13419-534f-4642-9138-215876304910.stripped.elf
python3 scripts/build_external.py tfa --configuration b1 --log-dir results/phase7
python3 scripts/build_external.py tfa --configuration b2 --log-dir results/phase7
python3 scripts/prepare_qemu_firmware.py tfa
python3 scripts/prepare_qemu_firmware.py optee
if [[ "${TZ_BUILD_ONLY:-0}" == 1 ]]; then
    echo 'BUILD PASS; QEMU EXECUTION NOT_RUN (build-only mode)'
    exit 0
fi
bash scripts/validate_phase7_built.sh
