import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FIELDS = (
    ("Test ID", "test_id"), ("Target platform", "target_platform"),
    ("Build status", "build_status"), ("Execution status", "execution_status"),
    ("Expected result", "expected_result"), ("Actual result", "actual_result"),
    ("Evidence", "evidence"), ("Limitation", "limitation"),
)


def main():
    records = json.loads((ROOT / "results/status.json").read_text())
    sections = ["# 検証報告（Phase 6）\n\nBUILD、QEMU実行、実機結果を区別する。構成別の集約と再現方法は docs/phase6_progress.md。HARDWARE PASSはない。"]
    for record in records:
        lines = []
        for label, key in FIELDS:
            value = record[key]
            if isinstance(value, list):
                value = ", ".join(value)
            lines.append(f"{label}: {value}")
        lines.append("Configuration: " + record["configuration"])
        ticks = record.get("execution_ticks")
        lines.append("Execution time: " + (str(ticks) + " generic-counter ticks" if ticks else "unmeasured (see execution status)"))
        sections.append("\n\n".join(lines))
    (ROOT / "results/report.md").write_text("\n\n---\n\n".join(sections) + "\n")


if __name__ == "__main__":
    main()
