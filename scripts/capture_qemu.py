import os
import math
import selectors
import subprocess
import sys
import time
from pathlib import Path


def main():
    log = Path(sys.argv[1])
    timeout = float(os.environ.get("QEMU_TIMEOUT_SECONDS", "30"))
    if not math.isfinite(timeout) or not 1 <= timeout <= 300:
        raise ValueError("QEMU timeout must be within 1..300 seconds")
    process = subprocess.Popen(sys.argv[2:], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    captured = bytearray()
    complete = False
    deadline = time.monotonic() + timeout
    selector = selectors.DefaultSelector()
    selector.register(process.stdout, selectors.EVENT_READ)
    try:
        while time.monotonic() < deadline:
            events = selector.select(timeout=0.1)
            for key, _ in events:
                chunk = key.fileobj.read1(4096)
                if not chunk:
                    break
                captured.extend(chunk)
                if b"FATAL" in captured:
                    raise RuntimeError("guest fatal exception")
                if b"\nEND\n" in captured:
                    complete = True
                    return
            if process.poll() is not None:
                break
        raise RuntimeError("QEMU stopped or timed out without END")
    finally:
        selector.close()
        if process.poll() is None:
            process.terminate()
        try:
            process.wait(timeout=2)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()
        log.write_bytes(captured)
        sys.stdout.buffer.write(captured)
        if not complete:
            print("EXECUTION FAIL: no complete test result", file=sys.stderr)


if __name__ == "__main__":
    main()
