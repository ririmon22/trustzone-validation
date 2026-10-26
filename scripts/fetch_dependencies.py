import argparse
import json
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("names", nargs="*", default=["tfa-amd", "optee"])
    args = parser.parse_args()
    dependencies = json.loads((ROOT / "third_party/dependencies.lock.json").read_text())
    for name in args.names:
        if name not in dependencies:
            parser.exit(2, "unknown dependency\n")
        entry = dependencies[name]
        commit = entry["commit"]
        if not commit or not re.fullmatch(r"[0-9a-f]{40}", commit):
            parser.exit(2, f"BLOCKED: {name} has no resolved full commit ID\n")
        destination = ROOT / "third_party" / name
        if (destination / ".source_manifest.json").exists():
            from source_snapshot import verify_source
            result = verify_source(destination, entry)
            print(f"SNAPSHOT VERIFIED (no fetch): {name} {result['commit']}")
            continue
        if destination.exists() and not (destination / ".git").exists():
            parser.exit(2, "existing dependency is not a checkout or verified snapshot; preserve it\n")
        if not destination.exists():
            destination.mkdir()
            subprocess.run(["git", "init", str(destination)], check=True)
            subprocess.run(["git", "-C", str(destination), "remote", "add", "origin", entry["repository"]], check=True)
        remote = subprocess.check_output(["git", "-C", str(destination), "remote", "get-url", "origin"], text=True).strip()
        if remote != entry["repository"]:
            parser.exit(2, "dependency remote mismatch\n")
        dirty = subprocess.check_output(["git", "-C", str(destination), "status", "--porcelain"], text=True)
        if dirty:
            parser.exit(2, "dependency contains local changes; preserve and inspect them\n")
        subprocess.run(["git", "-C", str(destination), "fetch", "--depth", "1", "origin", commit], check=True, timeout=60)
        subprocess.run(["git", "-C", str(destination), "checkout", "--detach", commit], check=True)
        actual = subprocess.check_output(["git", "-C", str(destination), "rev-parse", "HEAD"], text=True).strip()
        if actual != commit:
            parser.exit(2, "dependency commit mismatch\n")
        print(f"PIN VERIFIED: {name} {actual}")


if __name__ == "__main__":
    try:
        main()
    except (subprocess.CalledProcessError, subprocess.TimeoutExpired, OSError) as error:
        raise SystemExit(f"DEPENDENCY BLOCKED: fetch/checkout could not complete: {error}")
