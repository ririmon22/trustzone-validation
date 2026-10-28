import argparse
import hashlib
import json
from pathlib import Path
from check_b2_integration import elf_symbols
from check_boot_plan import elf_inventory


def plan(path):
    image = elf_inventory(path)
    _, _, symbols = elf_symbols(path)
    records = []
    for name, symbol in symbols.items():
        if not any(name.endswith(suffix) for suffix in ('debug_ram_log', 'debug_exception', 'debug_sync_exception', 'debug_boot')):
            continue
        if not name.replace('_', '').isalnum() or symbol['size'] <= 0:
            raise ValueError('invalid diagnostic symbol')
        if not any(segment['virtual'] == segment['base'] and segment['base'] <= symbol['address'] and symbol['address'] + symbol['size'] <= segment['end'] and segment['flags'] == 6 for segment in image['loads']):
            raise ValueError('diagnostic needs reviewed identity-mapped writable RAM')
        records.append((name, symbol['address'], symbol['size']))
    if not records:
        raise ValueError('diagnostic symbols missing')
    return sorted(records)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('elf', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    records = plan(args.elf)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text('# ELF SHA256: ' + hashlib.sha256(args.elf.read_bytes()).hexdigest() + '\nset tz_capture_plan {\n' + ''.join('    {' + n + ' ' + hex(a) + ' ' + str(s) + '}\n' for n, a, s in records) + '}\n')
    args.output.with_suffix('.json').write_text(json.dumps(dict(elf=str(args.elf), sha256=hashlib.sha256(args.elf.read_bytes()).hexdigest(), records=records, hardware_execution='NOT_RUN'), indent=4) + '\n')
    print(args.output)
