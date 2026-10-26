import argparse
import hashlib
import json
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def git(*args, cwd=None):
    return subprocess.check_output(["git", *map(str, args)], cwd=cwd, text=True, stderr=subprocess.STDOUT).strip()


def import_bundle(bundle, destination, commit, repository, expected_sha256):
    bundle = bundle.resolve()
    if hashlib.sha256(bundle.read_bytes()).hexdigest() != expected_sha256:
        raise ValueError("bundle SHA-256 mismatch")
    heads = [line.split(maxsplit=1) for line in git("bundle", "list-heads", bundle).splitlines()]
    reference = next((ref for sha, ref in heads if sha == commit), None)
    if reference is None:
        raise ValueError("bundle does not advertise the pinned commit")
    # 空のrepositoryで検証し、差分bundleの未充足prerequisiteを拒否する。
    with tempfile.TemporaryDirectory() as directory:
        git("init", directory)
        git("bundle", "verify", bundle, cwd=directory)
    if destination.exists():
        if not (destination / ".git").is_dir():
            raise ValueError("destination is not a Git checkout")
        if git("status", "--porcelain", cwd=destination):
            raise ValueError("destination contains local changes")
        if git("remote", "get-url", "origin", cwd=destination) != repository:
            raise ValueError("destination remote mismatch")
    else:
        destination.mkdir(parents=True)
        git("init", destination)
        git("remote", "add", "origin", repository, cwd=destination)
    git("fetch", "--no-tags", bundle, reference, cwd=destination)
    if git("rev-parse", "FETCH_HEAD", cwd=destination) != commit:
        raise ValueError("fetched commit mismatch")
    git("checkout", "--detach", commit, cwd=destination)
    if git("rev-parse", "HEAD", cwd=destination) != commit or git("status", "--porcelain", cwd=destination):
        raise ValueError("checkout verification failed")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("name", choices=("tfa-amd", "optee", "embeddedsw"))
    parser.add_argument("bundle", type=Path)
    parser.add_argument("--sha256", required=True)
    args = parser.parse_args()
    entry = json.loads((ROOT / "third_party/dependencies.lock.json").read_text())[args.name]
    if not entry["commit"]:
        parser.exit(2, "BLOCKED: dependency commit is unresolved\n")
    try:
        import_bundle(args.bundle, ROOT / "third_party" / args.name, entry["commit"], entry["repository"], args.sha256)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"OFFLINE_IMPORT FAIL: {error}\n")
    print(f"OFFLINE_IMPORT VERIFIED {args.name} {entry['commit']}")


if __name__ == "__main__":
    main()
