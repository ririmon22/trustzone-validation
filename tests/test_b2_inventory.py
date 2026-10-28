import copy
import json
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'scripts'))
from check_b2_inventory import check, RUNTIME_SOURCES
from report_performance_phase7 import validate_series_keys


class B2InventoryTests(unittest.TestCase):
    def test_every_global_expectation_classified_and_no_current_gap(self):
        report = check()
        self.assertEqual(report['static_status'], 'PASS', report['issues'])
        self.assertEqual(len(report['runtime_ids']), 23)
        self.assertEqual(len(report['performance_ids']), 8)
        self.assertTrue(all(not row['remaining_current_implementation'] for row in report['entries'].values()))
        self.assertEqual(report['hardware_execution'], 'NOT_RUN')
        self.assertNotIn('execution_results', report)

    def test_missing_catalog_row_and_implementation_are_detected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for name in ('tests/b2_inventory.json','tests/expectations.json', *RUNTIME_SOURCES):
                target = root/name
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes((ROOT/name).read_bytes())
            inventory = json.loads((root/'tests/b2_inventory.json').read_text())
            for entry in inventory['entries'].values():
                for name in entry['sources']:
                    path = root/name
                    if not path.exists():
                        path.parent.mkdir(parents=True, exist_ok=True)
                        path.write_text('fixture')
            inventory['entries'].pop('TEE-01')
            inventory['entries']['TEE-PARAM-01']['remaining_current_implementation'] = True
            (root/'tests/b2_inventory.json').write_text(json.dumps(inventory))
            issues = check(root)['issues']
            self.assertTrue(any('IDs differ' in item for item in issues))
            self.assertTrue(any('unfinished' in item for item in issues))

    def test_performance_profiles_keep_configuration_distinct(self):
        base = [('baremetal','PERF-01'),('baremetal','PERF-02')] + [('optee', ident) for ident in ('PERF-05','PERF-06','PERF-08','PERF-09','PERF-10','PERF-11')]
        self.assertEqual(validate_series_keys(base), 8)
        self.assertEqual(validate_series_keys(base + [('optee','PERF-01'),('optee','PERF-02')]), 10)
        for invalid in (base[:-1], base + [base[0]], base + [('tfa','PERF-01')]):
            with self.assertRaises(ValueError):
                validate_series_keys(invalid)

class ARunbookSafetyTests(unittest.TestCase):
    def test_prepared_a_plan_remains_blocked(self):
        import subprocess
        script = 'source {'+str(ROOT/'scripts/jtag/a_unified.tcl')+'}\nset tz_a_load_authorized 0\n'
        script += 'foreach call {{tz_a_load unused} {tz_a_run_to unused finish 100}} {if {![catch {eval $call} result]} {exit 1}; puts $result}\n'
        run = subprocess.run(['tclsh'],input=script,text=True,capture_output=True,check=True)
        self.assertEqual(run.stdout.count('BLOCKED:'),2)
        self.assertNotIn('invalid command',run.stdout+run.stderr)

class StaleRunLogTests(unittest.TestCase):
    def test_cli_rejects_stale_run_without_writing_text(self):
        import struct
        import subprocess
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            dump = path/'ram.bin'
            dump.write_bytes(struct.pack('<6IQ',0x545a4c47,1,16,4,0,2,77)+b'END\n'+bytes(12))
            output = path/'log.txt'
            command = [sys.executable,str(ROOT/'scripts/decode_ram_log.py'),str(dump),'--output',str(output),'--expected-run-id']
            failed = subprocess.run(command+['78'],capture_output=True,text=True)
            self.assertEqual(failed.returncode,1)
            self.assertIn('run ID mismatch',failed.stderr)
            self.assertFalse(output.exists())
            subprocess.run(command+['0x4d'],check=True,capture_output=True)
            self.assertEqual(output.read_text(),'END\n')
