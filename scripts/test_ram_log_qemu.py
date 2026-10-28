import argparse
import json
import os
import subprocess
import time
from pathlib import Path
from check_b2_integration import elf_symbols
from decode_ram_log import decode
from parse_results import parse_log

ROOT = Path(__file__).resolve().parents[1]


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--output-dir', type=Path, required=True)
    args = parser.parse_args()
    out = args.output_dir.resolve()
    out.mkdir(parents=True, exist_ok=True)
    _, _, symbols = elf_symbols(ROOT/'build/qemu/unified-ram/a_unified.elf')
    symbol = symbols['nonsecure_debug_ram_log']
    process = subprocess.Popen([os.environ.get('QEMU_SYSTEM_AARCH64', 'qemu-system-aarch64'), '-nodefaults', '-accel', 'tcg,thread=single', '-machine', 'virt-9.2,secure=on,virtualization=on,gic-version=2', '-cpu', 'cortex-a53', '-smp', '1', '-m', '1024', '-display', 'none', '-serial', 'null', '-monitor', 'stdio', '-nic', 'none', '-bios', str(ROOT/'build/qemu/unified-ram/rom.bin')], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    try:
        time.sleep(1)
        commands = 'stop\npmemsave '+hex(symbol['address'])+' '+str(symbol['size'])+' "'+str(out/'normal.bin')+'"\nquit\n'
        output, _ = process.communicate(commands, timeout=10)
        (out/'monitor.log').write_text(output)
        record = decode((out/'normal.bin').read_bytes())
        (out/'normal.txt').write_text(record['text'])
        expectations = json.loads((ROOT/'tests/expectations.json').read_text())
        rows = parse_log(record['text'], expectations, ['TZ-SYS-01', 'TZ-SYS-02', 'TZ-SYS-03'])
        passed = record['collection_status'] == 'COMPLETE' and all(row['execution_status'] == 'PASS' for row in rows if row['test_id'] in ('TZ-SYS-01','TZ-SYS-02','TZ-SYS-03'))
        record.pop('text')
        (out/'result.json').write_text(json.dumps(dict(status='PASS' if passed else 'FAIL', log=record, results=rows, uart_enabled=False, scope='QEMU RAM collection; XSDB and hardware NOT_RUN'), indent=4)+'\n')
        print('RAM_ONLY_COLLECTION', 'PASS' if passed else 'FAIL')
        raise SystemExit(0 if passed else 1)
    finally:
        if process.poll() is None:
            process.kill()
            process.wait()
