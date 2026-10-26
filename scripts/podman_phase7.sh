#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
action="${1:-run}"
image="${TZ_IMAGE:-localhost/trustzone-validation:phase7}"
case "$action" in
    build)
        base="${TZ_BASE_IMAGE:?Set a verified Ubuntu 26.04 image reference including @sha256:}"
        if [[ ! "$base" =~ @sha256:[a-f0-9]{64}$ ]]; then
            echo 'CONTAINER BUILD BLOCKED: immutable base digest required' >&2
            exit 2
        fi
        mkdir -p results/phase7
        printf '%s\n' "$base" > results/phase7/container-base.txt
        podman build --arch=amd64 --pull=never --build-arg "BASE_IMAGE=$base" -f container/Containerfile -t "$image" container
        podman image inspect "$image" > results/phase7/container-image.json
        ;;
    run|build-only)
        extra=()
        if [[ "$action" == build-only ]]; then extra+=(-e TZ_BUILD_ONLY=1); fi
        podman run "${extra[@]}" --rm --network=none --userns=keep-id -v "$PWD:/workspace:rw" -w /workspace "$image" bash scripts/run_phase7.sh
        ;;
    *) echo 'Use build or run' >&2; exit 2 ;;
esac
