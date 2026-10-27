import copy
import json
import sys
import unittest
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
import check_memory_map as memory


class Zcu106AStaticTests(unittest.TestCase):
    def setUp(self):
        directory = ROOT / 'platform/zcu106'
        self.mapping = json.loads((directory / 'memory_map.json').read_text())
        self.observations = json.loads((directory / 'vitis_observations.json').read_text())

    def test_reported_ranges_pass_without_load_permission(self):
        result = memory.validate_a_observations(self.mapping, self.observations)
        self.assertEqual(result['static_status'], 'PASS')
        self.assertEqual(result['load_status'], 'BLOCKED')
        self.assertEqual(result['hardware_execution'], 'NOT_RUN')
        self.assertEqual(len(result['reported_firmware_loads']), 7)

    def test_ddr1_cannot_be_enabled_as_a_allocation_bank(self):
        self.observations['memory_banks'][1]['use_for_a_static_layout'] = True
        with self.assertRaisesRegex(ValueError, 'unresolved DDR1'):
            memory.validate_a_observations(self.mapping, self.observations)

    def test_ddr0_end_cannot_exclude_protected_window(self):
        self.observations['memory_banks'][0].update(size=0x61000000, end=0x61000000)
        with self.assertRaisesRegex(ValueError, 'outside reported DDR0'):
            memory.validate_a_observations(self.mapping, self.observations)

    def test_reported_pmufw_cannot_overlap_secure_code(self):
        segments = self.observations['firmware']['PMUFW']['segments']
        segments.append(dict(physical=0x60020000, mem_size=0x100, end=0x60020100))
        with self.assertRaisesRegex(ValueError, 'overlaps reported firmware'):
            memory.validate_a_observations(self.mapping, self.observations)

    def test_reported_pmufw_cannot_overlap_unused_part_of_protection_window(self):
        self.observations['firmware']['PMUFW']['segments'].append(
            dict(physical=0x62080000, mem_size=0x100, end=0x62080100))
        with self.assertRaisesRegex(ValueError, 'overlaps reported firmware'):
            memory.validate_a_observations(self.mapping, self.observations)

    def test_reported_segment_end_is_checked(self):
        self.observations['firmware']['FSBL']['segments'][0]['end'] += 1
        with self.assertRaisesRegex(ValueError, 'segment end'):
            memory.validate_a_observations(self.mapping, self.observations)

    def test_reported_entry_must_be_inside_a_segment(self):
        self.observations['firmware']['PMUFW']['entry'] = 0
        with self.assertRaisesRegex(ValueError, 'entry outside loads'):
            memory.validate_a_observations(self.mapping, self.observations)

    def test_fsbl_load_must_fit_reported_ocm(self):
        self.observations['memory_banks'][2].update(size=0x10000, end=0xfffd0000)
        with self.assertRaisesRegex(ValueError, 'FSBL load outside OCM'):
            memory.validate_a_observations(self.mapping, self.observations)

    def test_firmware_loads_may_not_overlap_each_other(self):
        first = copy.deepcopy(self.observations['firmware']['FSBL']['segments'][0])
        self.observations['firmware']['PMUFW']['segments'] = [first]
        self.observations['firmware']['PMUFW']['entry'] = first['physical']
        with self.assertRaisesRegex(ValueError, 'firmware load overlap'):
            memory.validate_a_observations(self.mapping, self.observations)

    def test_stack_reservation_cannot_be_removed_from_elf(self):
        with patch.object(memory, 'load_segments', return_value=[(0x60000000, 0x60000100, 5)]):
            with self.assertRaisesRegex(ValueError, 'stack reservation missing'):
                memory.validate(self.mapping, elf_paths=['el3.elf'])

    def test_executable_stack_is_rejected(self):
        segments = [(0x60000000, 0x60000100, 5), (0x60060000, 0x60070000, 7)]
        with patch.object(memory, 'load_segments', return_value=segments):
            with self.assertRaisesRegex(ValueError, 'stack reservation missing or invalid'):
                memory.validate(self.mapping, elf_paths=['el3.elf'])

    def test_linker_stack_size_mismatch_is_rejected(self):
        linker = (ROOT / 'platform/zcu106/linker_el3.ld').read_text()
        linker = linker.replace('. += 0x10000;', '. += 0x20000;')
        with self.assertRaisesRegex(ValueError, 'linker stack reservation mismatch'):
            memory.validate(self.mapping, linkers={'el3': linker})

    def test_reported_xmpu0_does_not_grant_authority(self):
        from check_hardware_profile import profile_issues
        from check_zcu106_a import validate_blocked_entry_definitions
        profile = json.loads((ROOT / 'platform/zcu106/board_profile.json').read_text())
        self.assertEqual(self.observations['xmpu']['xmpu0_base'], 0xfd000000)
        self.assertFalse(self.observations['xmpu']['security_policy_verified'])
        self.assertTrue(profile_issues(profile, self.mapping))
        header = (ROOT / 'platform/zcu106/memory_map.h').read_text()
        validate_blocked_entry_definitions(header)
        for original, changed in (('PLATFORM_PROTECTION_CONFIRMED 0', 'PLATFORM_PROTECTION_CONFIRMED 1'),
                                  ('PLATFORM_REQUIRE_EL3_ENTRY_GUARD 1', 'PLATFORM_REQUIRE_EL3_ENTRY_GUARD 0')):
            with self.subTest(changed=changed), self.assertRaises(ValueError):
                validate_blocked_entry_definitions(header.replace(original, changed))


if __name__ == '__main__':
    unittest.main()
