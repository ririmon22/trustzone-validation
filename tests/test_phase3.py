import hashlib
import importlib.util
import json
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]


def module(name):
    spec = importlib.util.spec_from_file_location(name, ROOT / "scripts" / (name + ".py"))
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


memory = module("check_memory_map")
offline = module("import_dependency_bundle")
qemu = module("qemu_diagnose")
external = module("build_external")


class ProtectionDomainTests(unittest.TestCase):
    def setUp(self):
        self.mapping = json.loads((ROOT / "platform/zcu106/memory_map.json").read_text())

    def test_separate_buffer_valid(self):
        memory.validate(self.mapping)
        self.assertEqual(next(r for r in self.mapping["regions"] if r["name"] == "protected_buffer")["size"], 0x10000)

    def test_code_cannot_be_swallowed(self):
        domain = self.mapping["protection_domains"][0]
        domain.update(base=0x60000000, end=0x60100000)
        with self.assertRaises(ValueError):
            memory.validate(self.mapping)

    def test_stack_overlap_rejected(self):
        stack = next(r for r in self.mapping["regions"] if r["name"] == "secure_stack")
        stack.update(base=0x62030000, end=0x62040000)
        with self.assertRaisesRegex(ValueError, "overlaps allocation"):
            memory.validate(self.mapping)

    def test_buffer_cannot_escape(self):
        buffer = next(r for r in self.mapping["regions"] if r["name"] == "protected_buffer")
        buffer.update(base=0x62200000, end=0x62210000)
        with self.assertRaisesRegex(ValueError, "outside protection"):
            memory.validate(self.mapping)

    def test_fsbl_conflict_rejected(self):
        self.mapping["external_reservations"][0].update(confirmed=True, base=0x62080000, end=0x62090000)
        with self.assertRaisesRegex(ValueError, "firmware"):
            memory.validate(self.mapping)

    def test_unknown_firmware_blocks_preparation(self):
        with self.assertRaisesRegex(ValueError, "BLOCKED"):
            memory.validate(self.mapping, require_protection=True)


class QemuDiagnosticTests(unittest.TestCase):
    def test_absent_qemu_does_not_claim_startup(self):
        with tempfile.TemporaryDirectory() as directory, patch.object(qemu.shutil, "which", return_value=None):
            result = qemu.diagnose("missing", Path(directory) / "diagnosis.json", True)
        self.assertEqual(result["qemu_presence"], "ABSENT")
        self.assertEqual(result["qemu_startup"], "NOT_ATTEMPTED")
        self.assertEqual(result["qemu_functional_execution"], "NOT_RUN")
        self.assertIn("UNKNOWN", result["host_os_socket_policy"])

    def test_sdk_target_is_explicit(self):
        command = external.build_plan("optee-sdk", "aarch64-linux-gnu-", Path("/tmp/sdk-out"))
        self.assertIn("ta_dev_kit", command)
        self.assertIn("CFG_USER_TA_TARGETS=ta_arm64", command)


@unittest.skipUnless(shutil.which("git"), "local Git bundle fixture requires git; ZIP builds do not")
class OfflineBundleTests(unittest.TestCase):
    def test_bundle_import_and_wrong_digest(self):
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory)
            source, destination = base / "source", base / "destination"
            offline.git("init", source)
            (source / "Makefile").write_text("all:\n\t@true\n")
            offline.git("add", "Makefile", cwd=source)
            offline.git("-c", "user.name=Test", "-c", "user.email=test@example.invalid", "commit", "-m", "fixture", cwd=source)
            commit = offline.git("rev-parse", "HEAD", cwd=source)
            bundle = base / "source.bundle"
            offline.git("bundle", "create", bundle, "HEAD", cwd=source)
            digest = hashlib.sha256(bundle.read_bytes()).hexdigest()
            with self.assertRaisesRegex(ValueError, "SHA-256"):
                offline.import_bundle(bundle, destination, commit, "https://example.invalid/source.git", "0" * 64)
            self.assertFalse(destination.exists())
            offline.import_bundle(bundle, destination, commit, "https://example.invalid/source.git", digest)
            self.assertEqual(offline.git("rev-parse", "HEAD", cwd=destination), commit)
            (destination / "Makefile").write_text("local changes\n")
            with self.assertRaisesRegex(ValueError, "local changes"):
                offline.import_bundle(bundle, destination, commit, "https://example.invalid/source.git", digest)


class BackendRegisterTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = tempfile.TemporaryDirectory()
        cls.binary = Path(cls.directory.name) / "backends"
        subprocess.run(["clang", "-std=c11", "-Wall", "-Wextra", "-Werror", "-Wconversion", "-Wshadow",
                        "-I" + str(ROOT / "platform/zcu106/include"),
                        str(ROOT / "tests/fixtures/backends_harness.c"),
                        *[str(ROOT / "platform/zcu106" / name) for name in ("zdma.c", "gic.c", "violation.c", "access_probe.c", "dma_validation.c", "hardware_tests.c", "interrupt_dispatch.c")],
                        "-o", str(cls.binary)], check=True)

    @classmethod
    def tearDownClass(cls):
        cls.directory.cleanup()


def scenario(name):
    def test(self):
        subprocess.run([str(self.binary), name], check=True)
    return test


for name in ("dma-copy", "dma-blocked", "dma-error", "dma-timeout", "dma-readback", "dma-stale", "dma-range",
             "adapter-valid", "adapter-blocked", "adapter-master", "adapter-modified",
             "gic-dispatch", "gic-wrong-handler", "gic-groups", "gic-blocked", "gic-pending", "gic-eoi", "gic-spurious",
             "xpu-capture", "xpu-blocked", "xpu-master", "xpu-page", "abort32-valid", "abort-valid", "abort-address"):
    setattr(BackendRegisterTests, "test_" + name.replace("-", "_"), scenario(name))
