import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class HardwareLogicTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = tempfile.TemporaryDirectory()
        cls.binary = Path(cls.directory.name) / "hardware-harness"
        subprocess.run([
            "clang", "-std=c11", "-Wall", "-Wextra", "-Werror", "-Wconversion", "-Wshadow",
            "-I" + str(ROOT / "platform/zcu106/include"),
            str(ROOT / "tests/fixtures/hardware_harness.c"),
            str(ROOT / "platform/zcu106/protection.c"),
            str(ROOT / "platform/zcu106/hardware_tests.c"), "-o", str(cls.binary)
        ], check=True)

    @classmethod
    def tearDownClass(cls):
        cls.directory.cleanup()


def scenario_test(name):
    def test(self):
        subprocess.run([str(self.binary), name], check=True)
    return test


for scenario in (
    "xmpu-postcheck", "xmpu-blocked", "xmpu-align", "xmpu-enable-last", "xmpu-snapshot", "xmpu-io", "xmpu-readback",
    "xppu-postcheck", "xppu-blocked", "xppu-expand", "xppu-profile", "xppu-valid",
    "access-blocked", "access-permitted", "access-modified", "access-valid",
    "dma-valid", "dma-modified", "dma-blocked", "irq-valid", "irq-wrong", "irq-missing", "irq-blocked"
):
    setattr(HardwareLogicTests, "test_" + scenario.replace("-", "_"), scenario_test(scenario))

class HardwareProfileTests(unittest.TestCase):
    def test_confirmed_flags_cannot_enable_missing_backends(self):
        import importlib.util
        import json
        spec = importlib.util.spec_from_file_location("hardware_profile", ROOT / "scripts/check_hardware_profile.py")
        checker = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(checker)
        profile = json.loads((ROOT / "platform/zcu106/board_profile.json").read_text())
        mapping = json.loads((ROOT / "platform/zcu106/memory_map.json").read_text())
        for flag in checker.FLAGS:
            profile[flag] = True
        mapping["confirmed"] = True
        mapping["protection_domains"][0]["base"] += 0x10000
        issues = checker.profile_issues(profile, mapping)
        self.assertTrue(any("1 MiB" in issue for issue in issues))
        self.assertTrue(any("backends" in issue for issue in issues))
