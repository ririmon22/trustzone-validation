import hashlib
import importlib.util
import json
import struct
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
import prepare_qemu_firmware as firmware
from source_snapshot import verify_source


class FirmwareTests(unittest.TestCase):
    def make_fip(self):
        uuid = bytes(range(16))
        return struct.pack('<IIQ', firmware.FIP_MAGIC, 0, 0) + struct.pack('<16sQQQ', uuid, 96, 4, 0) + bytes(40) + b'test'

    def test_fip_accepts_exact_payload(self):
        self.assertEqual(list(firmware.fip_entries(self.make_fip()).values()), [b'test'])

    def test_fip_truncation_rejected(self):
        with self.assertRaises(ValueError):
            firmware.fip_entries(self.make_fip()[:-1])

    def test_fip_table_overlap_rejected(self):
        data = bytearray(self.make_fip())
        struct.pack_into('<Q', data, 32, 16)
        with self.assertRaisesRegex(ValueError, 'table'):
            firmware.fip_entries(data)

    def test_fip_magic_rejected(self):
        with self.assertRaises(ValueError):
            firmware.fip_entries(bytes(100))

    def test_region_cannot_escape(self):
        self.assertFalse(firmware.inside(0x0EFFFFF0, 32, firmware.REGIONS['optee']))

    def test_ns_elf_in_secure_reservation_rejected(self):
        elf = ROOT / 'build/qemu/external/tfa_bl33.elf'
        if not elf.exists():
            self.skipTest('external BL33 not built')
        with self.assertRaisesRegex(ValueError, 'reservation'):
            firmware.validate_elf(elf, 'optee')


class SourceSnapshotTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.entry = {'repository': 'https://example.invalid/source.git', 'commit': '1' * 40}
        data = b'content\n'
        (self.root / 'source.c').write_bytes(data)
        record = dict(path='source.c', sha=hashlib.sha1(b'blob 8\0' + data).hexdigest(), size=8, mode='100644')
        self.manifest = dict(self.entry, source_mode='verified-build-subset', files=[record])
        self.save()

    def save(self):
        (self.root / '.source_manifest.json').write_text(json.dumps(self.manifest))

    def test_original_blob_accepted(self):
        self.assertEqual(verify_source(self.root, self.entry)['verified_files'], 1)

    def test_modified_source_rejected(self):
        (self.root / 'source.c').write_text('modified')
        with self.assertRaisesRegex(ValueError, 'blob mismatch'):
            verify_source(self.root, self.entry)

    def test_unmanifested_source_rejected(self):
        (self.root / 'injected.c').write_text('injected')
        with self.assertRaisesRegex(ValueError, 'unmanifested'):
            verify_source(self.root, self.entry)

    def test_wrong_commit_rejected(self):
        self.manifest['commit'] = '2' * 40
        self.save()
        with self.assertRaisesRegex(ValueError, 'provenance'):
            verify_source(self.root, self.entry)

    def test_escaping_path_rejected(self):
        self.manifest['files'][0]['path'] = '../source.c'
        self.save()
        with self.assertRaisesRegex(ValueError, 'unsafe'):
            verify_source(self.root, self.entry)


if __name__ == '__main__':
    unittest.main()
