import datetime
import json
import platform
import shutil
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TOOLS = ("clang", "ld.lld", "make", "python3", "git", "qemu-system-aarch64", "aarch64-none-elf-gcc", "aarch64-linux-gnu-gcc", "vitis", "bootgen", "dtc")


def main():
    report = {"recorded_at": datetime.datetime.now(datetime.timezone.utc).isoformat(), "host": platform.platform(), "os_release": Path("/etc/os-release").read_text(), "tools": {}}
    for tool in TOOLS:
        path = shutil.which(tool)
        entry = {"path": path, "status": "AVAILABLE" if path else "MISSING"}
        if path:
            try:
                result = subprocess.run([path, "--version"], capture_output=True, text=True, timeout=5)
                entry["version_output"] = (result.stdout + result.stderr)[:2000]
                entry["exit_code"] = result.returncode
            except (subprocess.TimeoutExpired, OSError) as error:
                entry["error"] = str(error)
        report["tools"][tool] = entry
    destination = ROOT / "results/build/environment.json"
    destination.write_text(json.dumps(report, indent=4) + "\n")
    print(destination)


if __name__ == "__main__":
    main()
