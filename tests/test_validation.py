import copy
import importlib.util
import json
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def module(name):
    spec = importlib.util.spec_from_file_location(name, ROOT / "scripts" / f"{name}.py")
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


memory = module("check_memory_map")
results = module("parse_results")
bench = module("analyze_benchmarks")


class MemoryMapTests(unittest.TestCase):
    def setUp(self):
        self.mapping = json.loads((ROOT / "platform/qemu/memory_map.json").read_text())

    def test_valid_map(self):
        memory.validate(self.mapping)

    def test_overlap_rejected(self):
        region = self.mapping["regions"][1]
        region["base"] = self.mapping["regions"][0]["base"]
        region["end"] = region["base"] + region["size"]
        with self.assertRaisesRegex(ValueError, "overlap"):
            memory.validate(self.mapping)

    def test_end_rejected(self):
        self.mapping["regions"][0]["end"] += 1
        with self.assertRaisesRegex(ValueError, "end"):
            memory.validate(self.mapping)

    def test_alignment_rejected(self):
        self.mapping["regions"][0]["alignment"] = 3
        with self.assertRaisesRegex(ValueError, "alignment"):
            memory.validate(self.mapping)

    def test_header_mismatch(self):
        with self.assertRaisesRegex(ValueError, "header"):
            memory.validate(self.mapping, "")

    def test_secure_window(self):
        self.mapping["protection_window"] = [0, 4096]
        with self.assertRaisesRegex(ValueError, "protection window"):
            memory.validate(self.mapping)

    def test_shared_security(self):
        next(r for r in self.mapping["regions"] if r["name"] == "shared_memory")["security"] = "secure"
        with self.assertRaisesRegex(ValueError, "shared"):
            memory.validate(self.mapping)

    def test_unconfirmed_protection(self):
        zcu = json.loads((ROOT / "platform/zcu106/memory_map.json").read_text())
        with self.assertRaisesRegex(ValueError, "BLOCKED"):
            memory.validate(zcu, require_protection=True)

    def test_invalid_elf(self):
        with tempfile.NamedTemporaryFile() as file:
            file.write(b"invalid")
            file.flush()
            with self.assertRaises(ValueError):
                memory.load_segments(file.name)

    def test_linker_mismatch(self):
        with self.assertRaisesRegex(ValueError, "linker"):
            memory.validate(self.mapping, linkers={"el3": ". = 0x0;"})


class ResultTests(unittest.TestCase):
    def setUp(self):
        self.expected = {"TZ-SYS-01": {"expected_result": "counter isolation"}}
        self.log = "RUN platform=qemu configuration=baremetal\nRESULT TZ-SYS-01 PASS ticks=42 detail=executed\nSUMMARY PASS=1 FAIL=0 SKIP=0 BLOCKED=0 NOT_RUN=0\nEND\n"

    def test_valid(self):
        self.assertEqual(results.parse_log(self.log, self.expected)[0]["execution_ticks"], 42)

    def test_incomplete(self):
        with self.assertRaises(ValueError):
            results.parse_log(self.log.replace("END\n", ""), self.expected)

    def test_fatal(self):
        with self.assertRaises(ValueError):
            results.parse_log(self.log + "FATAL", self.expected)

    def test_duplicate(self):
        with self.assertRaises(ValueError):
            results.parse_log(self.log.replace("SUMMARY", "RESULT TZ-SYS-01 PASS ticks=1 detail=duplicate\nSUMMARY"), self.expected)

    def test_summary_tamper(self):
        with self.assertRaises(ValueError):
            results.parse_log(self.log.replace("PASS=1", "PASS=7"), self.expected)

    def test_missing_required(self):
        with self.assertRaises(ValueError):
            results.parse_log(self.log, self.expected, ["TZ-SYS-02"])

    def test_unknown(self):
        with self.assertRaises(ValueError):
            results.parse_log(self.log.replace("TZ-SYS-01", "TZ-SYS-99"), self.expected)

    def test_malformed(self):
        with self.assertRaises(ValueError):
            results.parse_log(self.log.replace("ticks=42", "ticks=NaN"), self.expected)

    def test_fail_kept(self):
        text = self.log.replace("TZ-SYS-01 PASS", "TZ-SYS-01 FAIL").replace("PASS=1 FAIL=0", "PASS=0 FAIL=1")
        self.assertEqual(results.parse_log(text, self.expected)[0]["execution_status"], "FAIL")


class BenchmarkTests(unittest.TestCase):
    def test_statistics(self):
        stats = bench.summarize(list(range(1, 101)))
        self.assertEqual(stats, dict(count=100, minimum=1, maximum=100, mean=50.5, median=50.5, p95=95, p99=99))

    def test_empty(self):
        with self.assertRaises(ValueError):
            bench.summarize([])


class CRunnerTests(unittest.TestCase):
    def test_actual_c_runner_status_counts(self):
        source = r'''#include "test_runner.h"
#include <stdio.h>
#include <stdlib.h>
void console_write(const char *text) { fputs(text, stdout); }
void console_uint(uint64_t value) { printf("%llu", (unsigned long long)value); }
int log_printf(const char *format, ...) { va_list args; va_start(args, format); int result = vprintf(format, args); va_end(args); return result; }
void log_set_state(uint32_t state) { (void)state; }
void debug_stage(uint64_t stage, uint64_t reason) { (void)stage; (void)reason; }
_Noreturn void platform_halt(void) { exit(9); }
int main(int argc, char **argv) {
    (void)argv;
    test_begin("baremetal");
    if (argc > 1) { test_report("TZ-SYS-01", (enum test_status)99, "invalid", 0); }
    test_report("TZ-SYS-01", TEST_PASS, "executed", 42);
    test_report("TZ-SYS-02", TEST_FAIL, "injected-failure", 9);
    test_report("TZ-SYS-03", TEST_SKIP, "not-applicable", 0);
    test_report("TZ-SYS-04", TEST_BLOCKED, "missing", 0);
    test_report("TZ-SYS-05", TEST_NOT_RUN, "pending", 0);
    test_finish();
    return 0;
}
'''
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            (path / "runner.c").write_text(source)
            subprocess.run(["clang", "-std=c11", "-Wall", "-Wextra", "-Werror", "-I" + str(ROOT / "common/include"), "-I" + str(ROOT / "platform/qemu"), str(path / "runner.c"), str(ROOT / "common/test_runner/runner.c"), "-o", str(path / "runner")], check=True)
            run = subprocess.run([str(path / "runner")], check=True, capture_output=True, text=True)
            expected = {f"TZ-SYS-{i:02}": {"expected_result": "fixture"} for i in range(1, 6)}
            parsed = results.parse_log(run.stdout, expected)
            self.assertEqual([row["execution_status"] for row in parsed], list(results.STATUSES))
            bad = subprocess.run([str(path / "runner"), "invalid"], capture_output=True, text=True)
            self.assertEqual(bad.returncode, 9)
            self.assertIn("FATAL invalid status", bad.stdout)


if __name__ == "__main__":
    unittest.main()
