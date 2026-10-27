import argparse
import datetime
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TEMPLATE = ROOT / 'docs/b2_hardware_checklist.json'
STATUSES = {'PASS', 'FAIL', 'BLOCKED', 'NOT_RUN'}


def has_measurement(value):
    if isinstance(value, dict):
        return any(has_measurement(item) for item in value.values())
    if isinstance(value, list):
        return any(has_measurement(item) for item in value)
    return value is not None and value != ''


def validate_record(record, template, evidence_root):
    issues = []
    if record.get('schema_version') != 1 or record.get('configuration') != 'B2':
        issues.append('Unsupported checklist schema/configuration')
    if record.get('load_authorized') is not False or type(record.get('runtime_ready')) is not int or record.get('runtime_ready') != 0:
        issues.append('Load/runtime readiness must remain false/0')
    expected = {item['id']: item for item in template['items']}
    items = record.get('items', [])
    identifiers = [item.get('id') for item in items]
    if len(set(identifiers)) != len(identifiers) or set(identifiers) != set(expected):
        issues.append('Missing, unexpected or duplicate checklist IDs')
    counts = {status: 0 for status in sorted(STATUSES)}
    for item in items:
        ident = item.get('id')
        if ident not in expected:
            continue
        reference = expected[ident]
        if any(item.get(key) != reference[key] for key in ('kind','what','where','how','expected')):
            issues.append(str(ident) + ': canonical requirement changed')
        status = item.get('status')
        if status not in STATUSES:
            issues.append(str(ident) + ': invalid status')
            continue
        counts[status] += 1
        if status in ('BLOCKED','FAIL') and not item.get('reason'):
            issues.append(ident + ': reason is required')
        if status != 'PASS':
            continue
        measured = item.get('measured')
        if not isinstance(measured, dict) or not has_measurement(measured):
            issues.append(ident + ': PASS needs a nonempty measured-value object')
        if item.get('manual_review') is not True or not item.get('reviewer'):
            issues.append(ident + ': PASS needs explicit human review and reviewer')
        try:
            timestamp = datetime.datetime.fromisoformat(item.get('reviewed_at_utc', '').replace('Z','+00:00'))
            if timestamp.utcoffset() != datetime.timedelta(0):
                raise ValueError('UTC required')
        except (ValueError, AttributeError, TypeError):
            issues.append(ident + ': reviewed_at_utc requires timezone-aware UTC timestamp')
        if reference['kind'] in ('live','mixed'):
            context = item.get('context', {})
            if any(not context.get(key) for key in ('board','processor','stop_point','execution_level','security_state','run_id')):
                issues.append(ident + ': live evidence lacks board/CPU/stop/EL/security/run context')
        kinds = set()
        evidence = item.get('evidence', [])
        if not evidence:
            issues.append(ident + ': PASS requires evidence files')
        for entry in evidence:
            relative = Path(entry.get('path') or '__missing__')
            path = (evidence_root / relative).resolve()
            kind = entry.get('kind')
            if relative.is_absolute() or not path.is_relative_to(evidence_root.resolve()):
                issues.append(ident + ': evidence path escapes evidence root')
                continue
            if kind not in ('artifact','live'):
                issues.append(ident + ': evidence kind must be artifact/live')
                continue
            if not path.is_file():
                issues.append(ident + ': evidence file missing: ' + str(relative))
                continue
            if entry.get('sha256') != hashlib.sha256(path.read_bytes()).hexdigest():
                issues.append(ident + ': evidence hash mismatch: ' + str(relative))
                continue
            kinds.add(kind)
        required = {'artifact','live'} if reference['kind'] == 'mixed' else {reference['kind']}
        if not required <= kinds:
            issues.append(ident + ': PASS lacks required artifact/live evidence categories')
    return dict(record_validation_status='FAIL' if issues else 'PASS', counts=counts, issues=issues,
                hardware_load_status='BLOCKED', runtime_ready=0, load_authorized=False,
                hardware_verification='NOT_ASSERTED_BY_THIS_TOOL',
                scope='schema/hash/completeness check only; evidence content and provenance require human review; no gate/config updates')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('record', type=Path)
    parser.add_argument('--evidence-root', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--require-record-complete', action='store_true')
    args = parser.parse_args()
    if args.output.resolve() in (args.record.resolve(), TEMPLATE.resolve()):
        parser.error('Output must not overwrite the record or canonical template')
    try:
        report = validate_record(json.loads(args.record.read_text()), json.loads(TEMPLATE.read_text()),
                                 args.evidence_root or args.record.parent)
    except (OSError, ValueError, KeyError, TypeError, AttributeError) as error:
        report = dict(record_validation_status='FAIL', hardware_load_status='BLOCKED', issues=[str(error)])
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report,indent=4)+'\n')
    print(json.dumps(report,indent=2))
    if report['record_validation_status'] != 'PASS':
        return 1
    if args.require_record_complete and report['counts']['PASS'] != sum(report['counts'].values()):
        return 2
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
