import argparse
import json
import os
import subprocess
import time
from pathlib import Path
from check_b2_integration import elf_symbols
from decode_exception import decode

ROOT = Path(__file__).resolve().parents[1]


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--output-dir', type=Path, required=True)
    args = parser.parse_args()
    out = args.output_dir.resolve()
    out.mkdir(parents=True, exist_ok=True)
    elf = ROOT / 'build/qemu/unified-fault/a_unified.elf'
    _, _, symbols = elf_symbols(elf)
    address = symbols['nonsecure_debug_exception']['address']
    process = subprocess.Popen([os.environ.get('QEMU_SYSTEM_AARCH64', 'qemu-system-aarch64'), '-nodefaults', '-accel', 'tcg,thread=single', '-machine', 'virt-9.2,secure=on,virtualization=on,gic-version=2', '-cpu', 'cortex-a53', '-smp', '1', '-m', '1024', '-display', 'none', '-serial', 'file:' + str(out/'uart.log'), '-monitor', 'stdio', '-nic', 'none', '-bios', str(ROOT/'build/qemu/unified-fault/rom.bin')], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    try:
        # 停止後のrecord自体を検査する。待ち時間だけで成功を判定しない。
        time.sleep(1)
        first = 'stop\npmemsave '+hex(address)+' 328 \"'+str(out/'first-exception.bin')+'\"\ncont\n'
        process.stdin.write(first)
        process.stdin.flush()
        time.sleep(0.1)
        commands = 'stop\npmemsave '+hex(address)+' 328 "'+str(out/'exception.bin')+'"\nquit\n'
        output, _ = process.communicate(commands, timeout=10)
        (out/'monitor.log').write_text(output)
        record = decode((out/'exception.bin').read_bytes())
        passed = record['valid'] == 2 and record['level'] == 1 and record['vector'] == 4 and record['ec'] == 0 and record['x'][:2] == [0x1234, 0x5678] and (out/'first-exception.bin').read_bytes() == (out/'exception.bin').read_bytes()
        (out/'result.json').write_text(json.dumps(dict(status='PASS' if passed else 'FAIL', record=record, scope='QEMU undefined-instruction diagnostic capture; not hardware protection'), indent=4)+'\n')
        print('EXCEPTION_CAPTURE', 'PASS' if passed else 'FAIL')
        raise SystemExit(0 if passed else 1)
    finally:
        if process.poll() is None:
            process.kill()
            process.wait()
