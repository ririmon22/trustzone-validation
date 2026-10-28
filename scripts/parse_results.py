import argparse
import json
import re
from pathlib import Path

STATUSES = ("PASS", "FAIL", "SKIP", "BLOCKED", "NOT_RUN")
ROOT = Path(__file__).resolve().parents[1]


def parse_log(text, expectations, required=None):
    if "FATAL" in text or text.count("\nEND\n") != 1:
        raise ValueError("fatal or incomplete execution log")
    headers = re.findall(r"^RUN platform=(\w+) configuration=(\w+)$", text, re.M)
    if len(headers) != 1:
        raise ValueError("one run header required")
    platform, configuration = headers[0]
    if platform not in ("qemu", "zcu106") or configuration not in ("baremetal", "tfa", "optee"):
        raise ValueError("unknown platform or configuration")
    rows = []
    seen = set()
    counts = dict.fromkeys(STATUSES, 0)
    result_lines = [line for line in text.splitlines() if line.startswith("RESULT")]
    for line in result_lines:
        match = re.fullmatch(r"RESULT ([A-Z0-9-]+) (PASS|FAIL|SKIP|BLOCKED|NOT_RUN) ticks=(\d+) detail=(.+)", line)
        if not match:
            raise ValueError("malformed result")
        test_id, status, ticks, detail = match.groups()
        if test_id not in expectations or test_id in seen:
            raise ValueError("unknown or duplicate test: " + test_id)
        seen.add(test_id)
        counts[status] += 1
        rows.append(dict(test_id=test_id, target_platform=platform, configuration=configuration, execution_status=status, expected_result=expectations[test_id]["expected_result"], actual_result=detail, execution_ticks=int(ticks)))
    if not rows or (required and not set(required) <= seen):
        raise ValueError("missing required tests")
    summaries = re.findall(r"^SUMMARY (.+)$", text, re.M)
    expected = " ".join(f"{key}={counts[key]}" for key in STATUSES)
    if summaries != [expected]:
        raise ValueError("summary counts mismatch")
    if text.index("SUMMARY") > text.index("\nEND\n") or any(text.index(line) > text.index("SUMMARY") for line in result_lines):
        raise ValueError("invalid completion order")
    return rows


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--configuration", choices=("baremetal", "tfa", "optee"), required=True)
    parser.add_argument("--require-security", action="store_true")
    args = parser.parse_args()
    expectations = json.loads((ROOT / "tests/expectations.json").read_text())
    required = [f"TZ-SYS-{i:02}" for i in range(1, 8)] if args.configuration != "optee" else [f"TEE-{i:02}" for i in range(1, 9)]
    if args.require_security:
        if args.configuration != "optee":
            parser.error("security tests require optee")
        required += ["TEE-AUTH-TA", "TEE-AUTH-NS", "TEE-SHM-01", "TEE-KEY-API", "TEE-KEY-MEM", "TEE-ISO-TA", "TEE-ISO-CORE", "TEE-STRESS-01", "TEE-SESSION-01", "TEE-ABI-01", "TEE-BOUNDARY-01", "B2-SMCCC-01", "TEE-SESSION-CORE-01", "TEE-PARAM-01", "TEE-KEY-ATTR-01"]
    if args.configuration == "tfa":
        required.append("B1-SMCCC-01")
    try:
        rows = parse_log(args.log.read_text(), expectations, required)
        if any(row["configuration"] != args.configuration for row in rows):
            raise ValueError("configuration mismatch")
    except (OSError, ValueError) as error:
        parser.exit(1, f"RESULT_PARSE FAIL: {error}\n")
    for row in rows:
        row["evidence"] = str(args.log)
        row["build_status"] = "UNKNOWN"
        row["limitation"] = "QEMU evidence is not hardware evidence" if row["target_platform"] == "qemu" else ""
    args.output.write_text(json.dumps(rows, indent=4) + "\n")
    if any(row["execution_status"] == "FAIL" for row in rows):
        parser.exit(1, "TEST FAIL\n")
    print("RESULT_PARSE PASS; inspect BLOCKED/SKIP separately")


if __name__ == "__main__":
    main()
