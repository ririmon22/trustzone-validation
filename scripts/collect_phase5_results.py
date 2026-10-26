import hashlib
import json
import shutil
from pathlib import Path
from parse_results import parse_log
from check_security_evidence import validate_faults
from render_report import main as render_report

ROOT = Path(__file__).resolve().parents[1]
SECURITY_IDS = ['TEE-AUTH-TA', 'TEE-AUTH-NS', 'TEE-SHM-01', 'TEE-KEY-API', 'TEE-KEY-MEM', 'TEE-ISO-TA', 'TEE-ISO-CORE']


def main():
    directory = ROOT / 'results/phase5'
    status_path = ROOT / 'results/status.json'
    indexed = {(r['target_platform'], r['configuration'], r['test_id']): r for r in json.loads(status_path.read_text())}
    expectations = json.loads((ROOT / 'tests/expectations.json').read_text())
    summary = {}
    for configuration in ('baremetal', 'tfa', 'optee'):
        log = directory / 'qemu' / (configuration + '.log')
        required = [f'TZ-SYS-{i:02}' for i in range(1, 8)] if configuration != 'optee' else [f'TEE-{i:02}' for i in range(1, 9)] + SECURITY_IDS
        if configuration == 'tfa':
            required.append('B1-SMCCC-01')
        rows = parse_log(log.read_text(), expectations, required)
        if any(r['target_platform'] != 'qemu' or r['configuration'] != configuration or r['execution_status'] == 'FAIL' for r in rows):
            raise ValueError('configuration mismatch or failed test')
        summary[configuration] = {}
        for row in rows:
            summary[configuration][row['execution_status']] = summary[configuration].get(row['execution_status'], 0) + 1
            row.update(build_status='BUILD PASS (executed image)', evidence=[str(log.relative_to(ROOT))], execution_time=None,
                       limitation='QEMU TCG only; not ZCU106 hardware evidence')
            if configuration == 'optee':
                row['evidence'] += ['results/phase5/qemu/optee-secure.log', 'results/phase5/qemu/security-evidence.json']
            indexed[('qemu', configuration, row['test_id'])] = row
    validate_faults((directory / 'qemu/optee.log').read_text(), (directory / 'qemu/optee-secure.log').read_text())
    for configuration in ('baremetal', 'optee'):
        for key, row in indexed.items():
            if key[:2] == ('qemu', configuration) and row['test_id'] in (('PERF-01', 'PERF-02') if configuration == 'baremetal' else ('PERF-05', 'PERF-06')):
                row.update(evidence=[f'results/phase5/qemu/{configuration}.log', f'results/phase5/qemu/{configuration}-benchmarks.csv'],
                           actual_result='Measurement acquired; see current CSV for 128 samples and statistics',
                           limitation='QEMU measurement acquisition only; no performance threshold or hardware claim')
    for test_id in SECURITY_IDS:
        indexed[('zcu106', 'optee', test_id)] = dict(
            test_id=test_id, target_platform='zcu106', configuration='optee',
            build_status='Client BUILD PASS; ZCU106 Core integration NOT_BUILT', execution_status='NOT_RUN',
            expected_result=expectations[test_id]['expected_result'],
            actual_result='No physical execution; QEMU validation result is not hardware evidence',
            execution_time=None, evidence=['results/phase5/build-zcu106.log', 'results/phase5/zcu106-load-gate.log', 'results/phase5/zcu106-profile-gate.log'],
            limitation='Unverified board routing/reservations and OP-TEE port; load is prohibited')
    status_path.write_text(json.dumps(list(indexed.values()), indent=4) + '\n')
    (directory / 'qemu-summary.json').write_text(json.dumps(summary, indent=4) + '\n')
    artifacts = {}
    for pattern in ('build/qemu/*.elf', 'build/zcu106/*.elf', 'build/qemu/external/*.elf', 'build/external/ta-*/*.ta', 'build/external/ta-*/*.stripped.elf', 'build/external/optee/core/tee.elf', 'build/qemu/*flash.bin'):
        for path in ROOT.glob(pattern):
            artifacts[str(path.relative_to(ROOT))] = dict(size=path.stat().st_size, sha256=hashlib.sha256(path.read_bytes()).hexdigest())
    (directory / 'artifacts.json').write_text(json.dumps(artifacts, indent=4) + '\n')
    for name in ('tfa', 'optee'):
        shutil.copyfile(ROOT / 'build/qemu' / (name + '_map_verified.json'), directory / (name + '-layout.json'))
    render_report()


if __name__ == '__main__':
    main()
