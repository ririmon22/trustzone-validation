import datetime
import json
import os
import shutil
import socket
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def socket_probe(kind, address):
    try:
        with socket.socket(socket.AF_INET, kind) as channel:
            channel.settimeout(2)
            channel.connect(address)
        return {"status": "AVAILABLE"}
    except OSError as error:
        return {"status": "BLOCKED", "errno": error.errno, "error": str(error)}


def main():
    report = {
        "recorded_at": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "sandbox_network_disabled": os.environ.get("CODEX_SANDBOX_NETWORK_DISABLED"),
        "qemu": shutil.which("qemu-system-aarch64"),
        "resolv_conf": Path("/etc/resolv.conf").read_text(),
        "dns_socket": socket_probe(socket.SOCK_DGRAM, ("127.0.0.53", 53)),
        "direct_tcp_socket": socket_probe(socket.SOCK_STREAM, ("1.1.1.1", 443)),
        "system_settings_changed": [],
        "conclusion": "See socket errno; DNS configuration has not been modified",
    }
    cache = subprocess.run(["apt-cache", "policy", "qemu-system-arm", "gcc-aarch64-linux-gnu"], capture_output=True, text=True, timeout=10)
    report["apt_candidates"] = cache.stdout + cache.stderr
    report["cached_debs"] = [str(path) for path in Path("/var/cache/apt/archives").glob("*.deb")]
    destination = ROOT / "results/phase2/environment-diagnosis.json"
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(json.dumps(report, indent=4) + "\n")
    print(destination)


if __name__ == "__main__":
    main()
