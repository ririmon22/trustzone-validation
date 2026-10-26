#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
python3 scripts/qemu_diagnose.py --smoke --output results/phase3/host-qemu-diagnosis.json
make all
bash scripts/run_qemu.sh baremetal
