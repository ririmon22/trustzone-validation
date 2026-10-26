import struct
import re
import sys
import json
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
from check_security_evidence import validate_faults
from stage_optee_overlay import stage
from check_signed_ta import mutated_images, SHDR_MAGIC, SHDR_BOOTSTRAP


class FaultEvidenceTests(unittest.TestCase):
    def setUp(self):
        self.normal = 'PROBE_FAULT domain=ta address=0x400613b0 write=0\nPROBE_FAULT domain=ta address=0x400613b0 write=1\nRESULT TEE-ISO-TA PASS ticks=0 detail=probe\n'
        self.secure = 'User mode data-abort at address 0x400613b0 (translation fault)\nE/TC: esr 0x92000007\nUser mode data-abort at address 0x400613b0 (translation fault)\nE/TC: esr 0x92000047\n'

    def test_read_write_evidence_accepted(self):
        self.assertEqual(len(validate_faults(self.normal, self.secure)), 2)

    def test_wrong_far_rejected(self):
        with self.assertRaises(ValueError):
            validate_faults(self.normal, self.secure.replace('0x400613b0', '0x400613b4', 1))

    def test_wrong_write_direction_rejected(self):
        with self.assertRaises(ValueError):
            validate_faults(self.normal, self.secure.replace('0x92000047', '0x92000007'))

    def test_wrong_exception_rejected(self):
        with self.assertRaises(ValueError):
            validate_faults(self.normal, self.secure.replace('0x92000007', '0x96000007'))

    def test_missing_secure_fault_rejected(self):
        with self.assertRaises(ValueError):
            validate_faults(self.normal, '')

    def test_read_only_cannot_pass_read_write(self):
        with self.assertRaisesRegex(ValueError, 'read and write'):
            validate_faults(self.normal.splitlines()[0] + '\nRESULT TEE-ISO-TA PASS ticks=0 detail=probe\n', '\n'.join(self.secure.splitlines()[:2]))

    def test_fault_address_invalid_rejected(self):
        with self.assertRaises(ValueError):
            validate_faults(self.normal, self.secure.replace('0x92000007', '0x92000407'))

    def test_core_pass_requires_core_faults(self):
        with self.assertRaisesRegex(ValueError, 'core PASS'):
            validate_faults(self.normal + 'RESULT TEE-ISO-CORE PASS ticks=0 detail=canary\n', self.secure)

    def test_combined_pass_requires_core_pass(self):
        with self.assertRaisesRegex(ValueError, 'combined'):
            validate_faults(self.normal + 'RESULT TEE-03 PASS ticks=0 detail=canary\n', self.secure)


class OverlayTests(unittest.TestCase):
    def test_original_preserved_and_overlay_explicit(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            base = root / 'third_party/optee'
            (base / 'core/pta').mkdir(parents=True)
            original = b'srcs-y += original.c\n'
            (base / 'core/pta/sub.mk').write_bytes(original)
            (base / '.source_manifest.json').write_text(json.dumps({'commit': '1' * 40}))
            own = root / 'optee_standalone/pta'
            own.mkdir(parents=True)
            for name in ('validation_canary.c', 'validation_canary.h'):
                (own / name).write_text('owned source\n')
            destination, manifest = stage(root)
            self.assertEqual((base / 'core/pta/sub.mk').read_bytes(), original)
            self.assertIn(b'CFG_TZ_VALIDATION_PTA', (destination / 'core/pta/sub.mk').read_bytes())
            self.assertEqual(len(manifest['changes']), 3)
            self.assertFalse((destination / '.source_manifest.json').exists())
            (destination / 'injected.c').write_text('unexpected')
            stage(root)
            self.assertFalse((destination / 'injected.c').exists())


class MessageAbiTests(unittest.TestCase):
    def test_tmem_attribute_matches_pinned_core(self):
        source = ROOT / 'third_party/optee/core/include/optee_msg.h'
        if not source.exists():
            self.skipTest('pinned OP-TEE source is not available')
        core = re.search(r'#define OPTEE_MSG_ATTR_TYPE_TMEM_INPUT\s+U\((0x[0-9a-f]+)\)', source.read_text())
        client = re.search(r'#define OPTEE_MSG_ATTR_TMEM_INPUT (\d+)UL', (ROOT / 'optee_standalone/client/client.c').read_text())
        self.assertIsNotNone(core)
        self.assertIsNotNone(client)
        self.assertEqual(int(core[1], 0), int(client[1]))


class SignedTaFixtureTests(unittest.TestCase):
    def test_signature_and_payload_mutations_are_separate(self):
        image = struct.pack('<IIIIHH', SHDR_MAGIC, SHDR_BOOTSTRAP, 1, 0x70004830, 32, 256) + bytes(32 + 256 + 20) + b'A'
        cases = mutated_images(image)
        self.assertEqual([i for i, (a, b) in enumerate(zip(image, cases['signature'])) if a != b], [52])
        self.assertEqual([i for i, (a, b) in enumerate(zip(image, cases['payload'])) if a != b], [len(image) - 1])

    def test_short_signed_image_rejected(self):
        with self.assertRaises(ValueError):
            mutated_images(bytes(19))

    def test_truncated_bootstrap_rejected(self):
        with self.assertRaises(ValueError):
            mutated_images(struct.pack('<IIIIHH', SHDR_MAGIC, SHDR_BOOTSTRAP, 32, 0x70004830, 32, 256))


if __name__ == '__main__':
    unittest.main()
