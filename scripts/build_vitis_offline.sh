#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
python_binary="${PYTHON:-python3}"
exec "$python_binary" scripts/offline_build.py "$@"
