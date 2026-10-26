import argparse
import datetime
import json
import os
import shutil
import socket
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def probe_socket(family, kind):
    try:
        with socket.socket(family, kind):
            pass
        return {"socket_creation": "AVAILABLE"}
    except OSError as error:
        return {"socket_creation": "DENIED", "errno": error.errno, "error": str(error)}


def smoke_command(binary, uart):
    return [binary, "-nodefaults", "-machine", "virt-9.2,secure=on,virtualization=on,gic-version=2",
            "-accel", "tcg,thread=single", "-cpu", "cortex-a53", "-smp", "1", "-m", "1024",
            "-display", "none", "-monitor", "none", "-serial", "file:" + str(uart), "-nic", "none", "-S"]


def diagnose(binary, output, smoke=False):
    resolved = shutil.which(binary)
    process_status = Path("/proc/self/status").read_text()
    report = {
        "recorded_at": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "qemu_path": resolved,
        "qemu_presence": "PRESENT" if resolved else "ABSENT",
        "qemu_startup": "NOT_ATTEMPTED",
        "qemu_functional_execution": "NOT_RUN",
        "sandbox_network_disabled": os.environ.get("CODEX_SANDBOX_NETWORK_DISABLED"),
        "process_restrictions": [line for line in process_status.splitlines() if line.startswith(("Seccomp:", "Seccomp_filters:", "NoNewPrivs:"))],
        "sockets": {name: probe_socket(family, kind) for name, family, kind in (
            ("unix_stream", socket.AF_UNIX, socket.SOCK_STREAM),
            ("ipv4_stream", socket.AF_INET, socket.SOCK_STREAM),
            ("ipv4_datagram", socket.AF_INET, socket.SOCK_DGRAM))},
        "host_os_socket_policy": "UNKNOWN: compare with the same command in a normal host terminal",
        "system_settings_changed": [],
    }
    if resolved:
        version = subprocess.run([resolved, "--version"], capture_output=True, text=True, timeout=5)
        report["version"] = version.stdout + version.stderr
        report["version_exit_code"] = version.returncode
        command = smoke_command(resolved, output.parent / "startup-uart.log")
        report["smoke_command"] = command
        if smoke:
            log = output.parent / "startup-stderr.log"
            with log.open("wb") as stream:
                process = subprocess.Popen(command, stdin=subprocess.DEVNULL, stdout=stream, stderr=subprocess.STDOUT)
                try:
                    code = process.wait(timeout=2)
                    report["qemu_startup"] = "EXITED_EARLY"
                    report["startup_exit_code"] = code
                except subprocess.TimeoutExpired:
                    report["qemu_startup"] = "RUNNING_PAUSED_SMOKE_ONLY"
                    process.terminate()
                    try:
                        process.wait(timeout=2)
                    except subprocess.TimeoutExpired:
                        process.kill()
                        process.wait()
            report["startup_log"] = str(log)
    else:
        report["blocker"] = "QEMU executable absent; no QEMU startup or QEMU socket failure was observed"
    return report


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--qemu", default=os.environ.get("QEMU_SYSTEM_AARCH64", "qemu-system-aarch64"))
    parser.add_argument("--smoke", action="store_true")
    parser.add_argument("--output", type=Path, default=ROOT / "results/phase3/qemu-diagnosis.json")
    args = parser.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    try:
        report = diagnose(args.qemu, args.output, args.smoke)
    except (OSError, subprocess.SubprocessError) as error:
        args.output.write_text(json.dumps({"diagnosis_error": str(error), "functional_execution": "NOT_RUN"}, indent=4) + "\n")
        print("DIAGNOSIS ERROR: " + str(error))
        return 1
    args.output.write_text(json.dumps(report, indent=4) + "\n")
    print(json.dumps(report, indent=4))
    return 2 if report["qemu_presence"] == "ABSENT" or report["qemu_startup"] == "EXITED_EARLY" else 0


if __name__ == "__main__":
    raise SystemExit(main())
