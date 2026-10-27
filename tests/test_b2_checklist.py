import copy
import hashlib
import json
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
from check_b2_hardware_checklist import validate_record


class B2ChecklistTests(unittest.TestCase):
    def setUp(self):
        self.template=json.loads((ROOT/'docs/b2_hardware_checklist.json').read_text())
        self.record=copy.deepcopy(self.template)
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.root=Path(self.temp.name)

    def check(self):
        return validate_record(self.record,self.template,self.root)

    def record_live_fixture(self):
        item=next(item for item in self.record['items'] if item['id']=='B2-HW-11')
        path=self.root/'fixture.txt';path.write_text('TEST FIXTURE ONLY: no hardware was accessed\n')
        item.update(status='PASS',measured={'example':0},manual_review=True,reviewer='fixture reviewer',
                    reviewed_at_utc='2026-10-28T00:00:00Z',
                    evidence=[dict(path='fixture.txt',kind='live',sha256=hashlib.sha256(path.read_bytes()).hexdigest())],
                    context=dict(board='fixture',processor='fixture',stop_point='fixture',execution_level='EL3',security_state='secure',run_id='unit-test'))
        return item

    def test_blank_template_is_not_load_ready(self):
        report=self.check();self.assertEqual(report['record_validation_status'],'PASS')
        self.assertEqual(report['hardware_load_status'],'BLOCKED')
        self.assertEqual(report['counts']['PASS'],0)

    def test_missing_item_is_rejected(self):
        self.record['items'].pop()
        self.assertEqual(self.check()['record_validation_status'],'FAIL')

    def test_duplicate_item_is_rejected(self):
        self.record['items'].append(copy.deepcopy(self.record['items'][0]))
        self.assertEqual(self.check()['record_validation_status'],'FAIL')

    def test_static_evidence_cannot_stand_in_for_live(self):
        item=self.record_live_fixture();item['evidence'][0]['kind']='artifact'
        self.assertEqual(self.check()['record_validation_status'],'FAIL')

    def test_context_is_required(self):
        item=self.record_live_fixture();item['context']['stop_point']=None
        self.assertEqual(self.check()['record_validation_status'],'FAIL')

    def test_blank_measurement_object_is_rejected(self):
        item=self.record_live_fixture();item['measured']={'register':None,'notes':''}
        self.assertEqual(self.check()['record_validation_status'],'FAIL')

    def test_tampered_evidence_is_rejected(self):
        self.record_live_fixture();(self.root/'fixture.txt').write_text('changed')
        self.assertEqual(self.check()['record_validation_status'],'FAIL')

    def test_no_review_is_rejected(self):
        item=self.record_live_fixture();item['manual_review']=False
        self.assertEqual(self.check()['record_validation_status'],'FAIL')

    def test_evidence_path_cannot_escape(self):
        item=self.record_live_fixture();item['evidence'][0]['path']='../outside'
        self.assertEqual(self.check()['record_validation_status'],'FAIL')

    def test_valid_record_entry_still_cannot_allow_load(self):
        self.record_live_fixture();report=self.check()
        self.assertEqual(report['record_validation_status'],'PASS')
        self.assertEqual(report['hardware_load_status'],'BLOCKED')
        self.assertEqual(report['hardware_verification'],'NOT_ASSERTED_BY_THIS_TOOL')

    def test_readiness_flag_is_rejected(self):
        self.record['runtime_ready']=1
        self.assertEqual(self.check()['record_validation_status'],'FAIL')
