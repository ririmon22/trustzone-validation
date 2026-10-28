import argparse
import csv
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FIELDS = ('minimum', 'maximum', 'mean', 'median', 'p95', 'p99')


def validate_series_keys(keys):
    keys = list(keys)
    base = {('baremetal','PERF-01'), ('baremetal','PERF-02')} | {('optee', ident) for ident in ('PERF-05','PERF-06','PERF-08','PERF-09','PERF-10','PERF-11')}
    expanded = base | {('optee','PERF-01'), ('optee','PERF-02')}
    if len(keys) != len(set(keys)) or set(keys) not in (base, expanded):
        raise ValueError('missing/duplicate/unexpected configuration-specific benchmark series')
    return len(keys)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--directory', type=Path, default=ROOT / 'results/phase7')
    args = parser.parse_args()
    directory = args.directory.resolve()
    records = []
    for configuration in ('baremetal', 'optee'):
        path = directory / 'qemu' / (configuration + '-benchmarks.csv')
        for raw in csv.DictReader(path.open()):
            frequency = re.search(r'timer_hz=(\d+)', raw['metadata'])
            if not frequency or int(frequency[1]) == 0 or raw['count'] != '128' or 'warmup=16' not in raw['metadata']:
                raise ValueError('incomplete benchmark metadata')
            records.append(dict(test_id=raw['test_id'], platform='qemu', configuration=configuration,
                                status='MEASURED', count=128, timer_hz=int(frequency[1]), cpu_hz=None,
                                ticks={field:float(raw[field]) for field in FIELDS},
                                microseconds={field:float(raw[field]) * 1e6 / int(frequency[1]) for field in FIELDS},
                                conditions=raw['metadata'], evidence=str(path.relative_to(ROOT))))
    expected = {'PERF-01','PERF-02','PERF-05','PERF-06','PERF-08','PERF-09','PERF-10','PERF-11'}
    validate_series_keys((row['configuration'], row['test_id']) for row in records)
    for test_id, reason in [('PERF-03','Secure interrupt/world routing and verified handler binding unavailable'), ('PERF-04','No verified DMA master/channel/route on accessible hardware'), ('PERF-07','No secure storage backend')]:
        records.append(dict(test_id=test_id, platform='qemu', status='BLOCKED', reason=reason, count=0, ticks=None, microseconds=None))
    for test_id in sorted(expected | {'PERF-03','PERF-04','PERF-07'}):
        records.append(dict(test_id=test_id, platform='zcu106', status='NOT_RUN', count=0, ticks=None, microseconds=None, reason='Hardware unavailable; load gate BLOCKED'))
    overhead = []
    for configuration in ('baremetal','optee'):
        log=(directory/'qemu'/(configuration+'.log')).read_text()
        for row in records:
            if row.get('configuration')!=configuration:
                continue
            samples=re.findall(r'^OVERHEAD '+re.escape(row['test_id'])+r' (\d+) (\d+)$',log,re.M)
            if [int(index) for index,value in samples]!=list(range(128)):
                raise ValueError('missing/duplicate overhead samples')
            from analyze_benchmarks import summarize
            overhead.append(dict(test_id=row['test_id'],configuration=configuration,counter='CNTPCT_EL0',
                                 scope='two counter reads plus empty indirect operation; not subtracted from raw results',
                                 **summarize([int(value) for index,value in samples])))
    (directory/'overhead.json').write_text(json.dumps(overhead,indent=4)+'\n')
    (directory / 'performance.json').write_text(json.dumps(records, indent=4) + '\n')
    lines = ['# 性能結果（Phase 7）', '', 'QEMU TCGのgeneric-counter測定。実機性能ではない。単位µs、各warmup16／sample128。CPU実クロック不明。条件と生ticksはperformance.json／UART／CSVを参照。', '', '|構成|ID|平均|中央値|p95|p99|最小|最大|', '|---|---|---|---|---|---|---|---|']
    for row in records:
        if row['platform'] == 'qemu' and row['status'] == 'MEASURED':
            values = row['microseconds']
            lines.append('|' + row['configuration'] + '|' + row['test_id'] + '|' + '|'.join(f'{values[field]:.3f}' for field in ('mean','median','p95','p99','minimum','maximum')) + '|')
    lines += ['', 'PERF-03/04/07はBLOCKED。ZCU106は全項目NOT_RUN。AはMMU/cache off、B2はonのため構成を跨ぐ直接比較はしない。PERF-10はpanicと診断出力を含む。PERF-11はTARGET_DEAD確認・dead-session解放後から再openとecho成功まで。']
    (directory / 'performance.md').write_text('\n'.join(lines) + '\n')


if __name__ == '__main__':
    main()
