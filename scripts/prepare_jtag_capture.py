import argparse
import hashlib
import json
from pathlib import Path
from check_b2_integration import elf_symbols, symbol_payload
from check_boot_plan import elf_inventory
from zcu106_b2_layout import load_layout
ROOT=Path(__file__).resolve().parents[1]


def plan(path):
    image = elf_inventory(path)
    _,regions=load_layout(ROOT)
    if image['entry']!=regions['bl33_code']['base']:raise ValueError('ZCU106 BL33 entry required for capture')
    parsed = elf_symbols(path)
    _, _, symbols = parsed
    records = []
    for name, symbol in symbols.items():
        if not any(name.endswith(suffix) for suffix in ('debug_ram_log', 'debug_exception', 'debug_sync_exception', 'debug_boot', 'b2_results', 'b2_physical_record', 'debug_handoff', 'b2_signed_audit', 'b2_signed_loader_snapshot', 'b2_signed_snapshot_status', 'b2_review_results', 'b2_bench_results', 'b2_stress_results', 'b2_sgi_results')):
            continue
        if not name.replace('_', '').isalnum() or symbol['size'] <= 0:
            raise ValueError('invalid diagnostic symbol')
        if not any(segment['virtual'] == segment['base'] and segment['base'] <= symbol['address'] and symbol['address'] + symbol['size'] <= segment['end'] and segment['flags'] == 6 for segment in image['loads']):
            raise ValueError('diagnostic needs reviewed identity-mapped writable RAM')
        records.append((name, symbol['address'], symbol['size']))
    review_parent=int.from_bytes(symbol_payload(parsed,'b2_review_parent'),'little') if 'b2_review_parent' in symbols else 0
    if review_parent in (0,14) and 'b2_test_selection' in symbols and int.from_bytes(symbol_payload(parsed,'b2_test_selection'),'little') in (16,17):
        raw=symbol_payload(parsed,'b2_signed_source')
        base=int.from_bytes(raw[:8],'little');size=int.from_bytes(raw[8:],'little')
        if size!=0x100000 or base != 0x10600000:raise ValueError('unreviewed signed bundle geometry')
        if any(base < segment['end'] and segment['base'] < base+size for segment in image['loads']):raise ValueError('bundle overlaps ELF runtime')
        records.append(('signed_ta_bundle',base,size))
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
