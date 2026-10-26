import argparse
import csv
import math
import re
import statistics
from pathlib import Path


def summarize(values):
    if not values or any(value < 0 for value in values):
        raise ValueError("nonnegative samples required")
    ordered = sorted(values)
    return dict(count=len(values), minimum=min(values), maximum=max(values), mean=statistics.mean(values), median=statistics.median(values), p95=ordered[math.ceil(len(values) * 0.95) - 1], p99=ordered[math.ceil(len(values) * 0.99) - 1])


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("log", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    text = args.log.read_text()
    if "FATAL" in text or "\nEND\n" not in text:
        parser.exit(1, "incomplete benchmark log\n")
    rows = []
    for test_id, metadata in re.findall(r"^BENCH_META (PERF-\d+) (.+)$", text, re.M):
        samples = [(int(index), int(value)) for index, value in re.findall(r"^SAMPLE " + re.escape(test_id) + r" (\d+) (\d+)$", text, re.M)]
        if [index for index, _ in samples] != list(range(128)):
            parser.exit(1, "missing/duplicate/out-of-order benchmark samples\n")
        rows.append(dict(test_id=test_id, unit="generic-counter-ticks", metadata=metadata, **summarize([value for _, value in samples])))
    if not rows:
        parser.exit(1, "no benchmark data\n")
    with args.output.open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)


if __name__ == "__main__":
    main()
