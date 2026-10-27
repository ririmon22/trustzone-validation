import json
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
from zcu106_b2_layout import load_layout


class B2LayoutTests(unittest.TestCase):
    def test_proposal_remains_unverified(self):
        plan,regions=load_layout(ROOT)
        self.assertFalse(plan['load_authorized'])
        self.assertFalse(plan['hardware_confirmed'])
        self.assertEqual(regions['shared_memory']['base'],0x10500000)

    def test_header_drift_is_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            root=Path(temporary);folder=root/'platform/zcu106/b2'
            shutil.copytree(ROOT/'platform/zcu106/b2',folder)
            shutil.copy2(ROOT/'platform/zcu106/memory_map.h',folder.parent/'memory_map.h')
            path=folder/'memory_map.h'
            path.write_text(path.read_text().replace('SHARED_MEMORY_BASE 0x10500000','SHARED_MEMORY_BASE 0x70000000'))
            with self.assertRaisesRegex(ValueError,'header/JSON mismatch'):
                load_layout(root)

    def test_runtime_gate_cannot_be_enabled(self):
        with tempfile.TemporaryDirectory() as temporary:
            root=Path(temporary);folder=root/'platform/zcu106/b2'
            shutil.copytree(ROOT/'platform/zcu106/b2',folder)
            shutil.copy2(ROOT/'platform/zcu106/memory_map.h',folder.parent/'memory_map.h')
            path=folder/'memory_map.h'
            path.write_text(path.read_text().replace('B2_RUNTIME_READY 0','B2_RUNTIME_READY 1'))
            with self.assertRaisesRegex(ValueError,'gate disabled'):
                load_layout(root)


class B2PageTableTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary=tempfile.TemporaryDirectory();cls.binary=Path(cls.temporary.name)/'tables'
        subprocess.run(['clang','-std=c11','-Wall','-Wextra','-Werror','-Wconversion','-Wshadow',
            '-DTEE_CLIENT_HOST_TEST','-I'+str(ROOT/'platform/zcu106/b2'),
            str(ROOT/'platform/zcu106/b2/client_mmu.c'),str(ROOT/'tests/fixtures/b2_mmu_harness.c'),'-o',str(cls.binary)],check=True)

    @classmethod
    def tearDownClass(cls):
        cls.temporary.cleanup()

    def test_generated_tables(self):
        for scenario in ('code-rx','vector-rx','shared-cache','device-only-page','unmapped-secure-ddr1','bad-vector'):
            with self.subTest(scenario=scenario):
                subprocess.run([str(self.binary),scenario],check=True)
