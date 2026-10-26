import csv
import hashlib
import json
import shutil
from pathlib import Path
from parse_results import parse_log
from render_report import main as render_report

ROOT = Path(__file__).resolve().parents[1]


def main():
    path = ROOT / 'results/status.json'
    records = json.loads(path.read_text())
    indexed = {(row['target_platform'], row['configuration'], row['test_id']): row for row in records}
    expectations = json.loads((ROOT / 'tests/expectations.json').read_text())
    for configuration in ('baremetal', 'tfa', 'optee'):
        log = ROOT / 'results/qemu' / (configuration + '.log')
        required = [f'TEE-{i:02}' for i in range(1, 9)] if configuration == 'optee' else [f'TZ-SYS-{i:02}' for i in range(1, 8)]
        if configuration == 'tfa':
            required.append('B1-SMCCC-01')
        rows = parse_log(log.read_text(), expectations, required)
        for row in rows:
            if row['configuration'] != configuration or row['target_platform'] != 'qemu':
                raise ValueError('unexpected run header')
            row.update(build_status='BUILD PASS (executed QEMU image)',
                       evidence=[str(log.relative_to(ROOT))], execution_time=None,
                       limitation='QEMU TCG only; not ZCU106 hardware evidence')
            if configuration == 'optee':
                row['evidence'].append('results/qemu/optee-secure.log')
            indexed[('qemu', configuration, row['test_id'])] = row
        for row in records:
            if row['target_platform'] != 'qemu' or row['configuration'] != configuration or not row['test_id'].startswith('PERF'):
                continue
            row.update(actual_result='Measurement backend or applicable test is not integrated in this configuration',
                       evidence=['docs/known_limitations.md'], execution_status='BLOCKED',
                       limitation='Not measured; no hardware performance claim')
        benchmark_path = ROOT / 'results/qemu' / (configuration + '-benchmarks.csv')
        if configuration in ('baremetal', 'optee'):
            for measurement in csv.DictReader(benchmark_path.open()):
                row = indexed[('qemu', configuration, measurement['test_id'])]
                row.update(build_status='BUILD PASS', execution_status='PASS',
                           actual_result='Measurement acquisition succeeded: ' + json.dumps(measurement),
                           evidence=[str(log.relative_to(ROOT)), str(benchmark_path.relative_to(ROOT))],
                           limitation='128 emulator counter samples; PASS means measurement acquisition only, no performance threshold or ZCU106 claim')
    path.write_text(json.dumps(list(indexed.values()), indent=4) + '\n')
    artifacts = {}
    paths = [*ROOT.glob('build/qemu/*.elf'), *ROOT.glob('build/zcu106/*.elf'),
             *ROOT.glob('build/qemu/external/*.elf'), *ROOT.glob('build/qemu/*rom.bin'),
             *ROOT.glob('build/qemu/*flash.bin'), *ROOT.glob('build/external/tfa-*/**/bl31.elf'),
             *ROOT.glob('build/external/tfa-*/**/fip.bin'), *ROOT.glob('build/external/optee/core/tee*'),
             *ROOT.glob('build/external/ta-*/*.stripped.elf'), *ROOT.glob('build/external/ta-*/*.ta')]
    for artifact in sorted(set(paths)):
        if artifact.is_file():
            data = artifact.read_bytes()
            artifacts[str(artifact.relative_to(ROOT))] = dict(size=len(data), sha256=hashlib.sha256(data).hexdigest())
    (ROOT / 'results/phase4/artifacts.json').write_text(json.dumps(artifacts, indent=4) + '\n')
    for name in ('tfa', 'optee'):
        shutil.copyfile(ROOT / 'build/qemu' / (name + '_map_verified.json'), ROOT / 'results/phase4' / (name + '-layout.json'))
    render_report()


if __name__ == '__main__':
    main()
