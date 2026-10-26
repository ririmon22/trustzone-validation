import copy
import json
import struct
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
from check_boot_plan import elf_inventory, range_issues, check_plan


class BootPlanTests(unittest.TestCase):
    def test_default_plan_blocked(self):
        result = check_plan(json.loads((ROOT / 'platform/zcu106/boot_plan.json').read_text()))
        self.assertEqual(result['status'], 'BLOCKED')
        self.assertTrue(any('fsbl' in value for value in result['issues']))
        self.assertTrue(any('MMIO' in value for value in result['issues']))

    def test_adjacent_runtime_regions_accepted(self):
        self.assertEqual(range_issues([dict(name='stack', base=0x1000, end=0x2000, alignment=4096), dict(name='heap', base=0x2000, end=0x3000, alignment=4096)]), [])

    def test_runtime_stack_heap_overlap_rejected(self):
        self.assertTrue(any('overlap' in issue for issue in range_issues([dict(name='stack', base=0x1000, end=0x3000, alignment=4096), dict(name='heap', base=0x2000, end=0x4000, alignment=4096)])))

    def test_boolean_address_rejected(self):
        self.assertTrue(range_issues([dict(name='bad', base=True, end=4096, alignment=4096)]))

    def test_overflow_rejected(self):
        self.assertTrue(range_issues([dict(name='bad', base=0, end=(1 << 64) + 4096, alignment=4096)]))

    def test_unaligned_runtime_region_rejected(self):
        self.assertTrue(range_issues([dict(name='bad', base=4097, end=8192, alignment=4096)]))

    def test_bss_is_included_in_inventory(self):
        image = elf_inventory(ROOT / 'build/zcu106/nonsecure.elf')
        self.assertTrue(any(segment['zero_fill_bytes'] > 0 for segment in image['loads']))
        self.assertTrue(all(segment['runtime_bytes'] >= segment['file_bytes'] for segment in image['loads']))

    def test_forged_confirmation_cannot_remove_legacy_gate(self):
        plan = json.loads((ROOT / 'platform/zcu106/boot_plan.json').read_text())
        for review in plan['reviews'].values():
            review.update(verified=True, evidence='live_register_snapshot')
        self.assertEqual(check_plan(plan)['status'], 'BLOCKED')

    def test_elf32_microblaze_inventory(self):
        data = bytearray(88)
        data[:6] = b'\x7fELF\x01\x02'
        struct.pack_into('>H', data, 18, 189)
        struct.pack_into('>II', data, 24, 0x1000, 52)
        struct.pack_into('>HH', data, 42, 32, 1)
        struct.pack_into('>IIIIIIII', data, 52, 1, 84, 0x1000, 0x1000, 4, 16, 5, 1)
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'pmu.elf'
            path.write_bytes(data)
            image = elf_inventory(path)
        self.assertEqual((image['machine'], image['elf_class']), (189, 32))
        self.assertEqual(image['loads'][0]['zero_fill_bytes'], 12)

class ClientLifecycleTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        import subprocess
        cls.directory = tempfile.TemporaryDirectory()
        cls.binary = Path(cls.directory.name) / 'client-lifecycle'
        subprocess.run(['clang', '-std=c11', '-Wall', '-Wextra', '-Werror', '-DTEE_CLIENT_HOST_TEST',
                        '-I' + str(ROOT / 'tests/fixtures/client_runtime'), '-I' + str(ROOT / 'optee_standalone/client'),
                        str(ROOT / 'tests/fixtures/client_lifecycle.c'), str(ROOT / 'optee_standalone/client/client.c'), '-o', str(cls.binary)], check=True)

    @classmethod
    def tearDownClass(cls):
        cls.directory.cleanup()

    def test_dead_sessions_are_released_512_times(self):
        import subprocess
        subprocess.run([str(self.binary)], check=True)

    def test_failed_close_stops_transport(self):
        import subprocess
        subprocess.run([str(self.binary), 'close-error'], check=True)


if __name__ == '__main__':
    unittest.main()
