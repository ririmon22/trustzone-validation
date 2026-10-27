import argparse
import copy
import json
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
from build_external import configured_plan
from check_b2_integration import check_contract, elf_symbols
from check_boot_plan import B2_ARTIFACT_PATHS, b2_binding_issues


class B2IntegrationTests(unittest.TestCase):
    def setUp(self):
        self.plan = json.loads((ROOT / 'platform/zcu106/boot_plan.json').read_text())

    def test_boot_plan_selects_integrated_artifacts(self):
        self.assertEqual(b2_binding_issues(self.plan), [])
        for role, expected in B2_ARTIFACT_PATHS.items():
            self.assertEqual(self.plan['images'][role]['path'], expected)
            self.assertIsNone(self.plan['images'][role]['sha256'])
            self.assertFalse(self.plan['images'][role]['translation_verified'])

    def test_b1_bl31_in_b2_plan_is_blocked(self):
        self.plan['images']['bl31']['path'] = 'build/external/tfa-zynqmp/zynqmp/release/bl31/bl31.elf'
        self.assertTrue(any('bl31' in issue for issue in b2_binding_issues(self.plan)))

    def test_b1_client_in_b2_plan_is_blocked(self):
        self.plan['images']['bl33']['path'] = 'build/zcu106/tfa_bl33.elf'
        self.assertTrue(b2_binding_issues(self.plan))

    def test_missing_dispatcher_binding_is_blocked(self):
        self.plan['integration']['required_spd'] = 'none'
        self.assertTrue(any('opteed' in issue for issue in b2_binding_issues(self.plan)))

    def test_matching_cached_shm_contract(self):
        self.assertEqual(check_contract((0x42000000,0x42200000),(0x42000000,0x42200000),True), [])

    def test_board_shm_range_mismatch_is_not_pass(self):
        self.assertTrue(check_contract((0x70000000,0x80000000),(0x10500000,0x10600000),False))

    def test_cache_mismatch_is_not_hidden_by_matching_address(self):
        self.assertTrue(any('cached' in issue for issue in check_contract((0x42000000,0x42200000),(0x42000000,0x42200000),False)))

    def test_zynqmp_b2_default_entry_matches_own_bl33_map(self):
        args=argparse.Namespace(component='tfa',cross_compile='aarch64-none-elf-',compiler='clang',
                                llvm_dir=Path('/actual/llvm'),platform='zynqmp',configuration='b2',
                                early_ta=[],jobs=1,host_cc=None)
        output,command=configured_plan(args)
        mapping=json.loads((ROOT/'platform/zcu106/memory_map.json').read_text())
        entry=next(r['base'] for r in mapping['regions'] if r['name']=='nonsecure_code')
        self.assertIn('SPD=opteed',command)
        self.assertIn('PRELOADED_BL33_BASE='+hex(entry),command)
        self.assertEqual(output.name,'tfa-zynqmp-b2')
        args.configuration='b1'
        output,command=configured_plan(args)
        self.assertNotIn('SPD=opteed',command)
        self.assertEqual(output.name,'tfa-zynqmp')

    def test_a_plan_does_not_require_b2_bindings(self):
        plan=copy.deepcopy(self.plan);plan['configuration']='A'
        self.assertEqual(b2_binding_issues(plan),[])


if __name__ == '__main__':
    unittest.main()
