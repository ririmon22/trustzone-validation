#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p results/build
make host-test 2>&1 | tee results/build/host-tests.log
make all 2>&1 | tee results/build/qemu-build.log
bash scripts/run_qemu.sh baremetal
