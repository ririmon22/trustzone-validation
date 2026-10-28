import argparse
import csv
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RUNTIME_SOURCES = ['optee_standalone/tests/' + name for name in ('main.c','security_tests.c','stress_tests.c','boundary_tests.c','extended_tests.c')]


def check(root=ROOT):
    inventory = json.loads((root/'tests/b2_inventory.json').read_text())['entries']
    expected = json.loads((root/'tests/expectations.json').read_text())
    issues = []
    if set(inventory) != set(expected):
        issues.append('Inventory/expectation IDs differ')
    emitted, measured = set(), set()
    for name in RUNTIME_SOURCES:
        text = (root/name).read_text()
        emitted.update(re.findall(r'test_report\("([A-Z0-9-]+)"', text))
        measured.update(re.findall(r'benchmark_run(?:_scoped)?\("(PERF-\d+)"', text))
    for ident, entry in inventory.items():
        if any(not (root/name).is_file() for name in entry['sources']):
            issues.append(ident + ': missing source/reference')
        if entry['scope'] == 'B2' and entry['availability'] in ('CURRENT','VALIDATION_OVERLAY'):
            if entry['implementation'] != 'IMPLEMENTED' or ident not in emitted | measured:
                issues.append(ident + ': current-scope implementation/output missing')
        if entry.get('remaining_current_implementation'):
            issues.append(ident + ': feasible but unfinished')
        if entry['hardware_execution'] != 'NOT_RUN':
            issues.append(ident + ': no hardware evidence may be asserted in this inventory')
    if (emitted | measured) - set(inventory):
        issues.append('Uncatalogued emitted IDs')
    for ident in emitted | measured:
        if ident in inventory and inventory[ident]['scope'] != 'B2':
            issues.append(ident + ': B2 output cannot be classified as another configuration')
    return dict(static_status='FAIL' if issues else 'PASS', issues=issues, entries=inventory,
                runtime_ids=sorted(emitted), performance_ids=sorted(measured),
                hardware_execution='NOT_RUN', scope='source/ID coverage only; execution evidence is separate')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--results', type=Path)
    parser.add_argument('--benchmarks', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    try:
        report = check()
        if args.results:
            rows = json.loads(args.results.read_text())
            observed = [row['test_id'] for row in rows]
            if len(set(observed)) != len(observed) or set(observed) != set(report['runtime_ids']):
                report['issues'].append('Runtime result IDs do not cover current source exactly')
                report['static_status'] = 'FAIL'
            report['execution_results'] = rows
            report['execution_counts'] = {status:sum(row['execution_status'] == status for row in rows) for status in ('PASS','FAIL','BLOCKED','SKIP','NOT_RUN')}
        if args.benchmarks:
            with args.benchmarks.open() as stream:
                rows = list(csv.DictReader(stream))
            observed = [row['test_id'] for row in rows]
            if len(set(observed)) != len(observed) or set(observed) != set(report['performance_ids']):
                report['issues'].append('Benchmark IDs do not cover current source exactly')
                report['static_status'] = 'FAIL'
            report['performance_results'] = rows
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(report,indent=4)+'\n')
    except (OSError, ValueError, KeyError, TypeError) as error:
        parser.exit(1, "B2 INVENTORY FAIL: " + str(error) + "\n")
    print(report['static_status'], len(report['runtime_ids']), 'runtime IDs;', len(report['performance_ids']), 'performance IDs')
    raise SystemExit(report['static_status'] != 'PASS')
