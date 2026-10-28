import argparse
import json
from pathlib import Path
from check_boot_plan import elf_inventory
from check_b2_integration import elf_symbols
from check_memory_map import validate_a_observations

ROOT = Path(__file__).resolve().parents[1]


def inspect(platform, path):
    mapping = json.loads((ROOT / 'platform' / platform / 'memory_map.json').read_text())
    regions = {r['name']: r for r in mapping['regions']}
    image = elf_inventory(path)
    _, _, symbols = elf_symbols(path)
    code_names = {'el3': 'el3_firmware', 'secure': 'secure_code', 'nonsecure': 'nonsecure_code'}
    if image['machine'] != 183 or image['elf_class'] != 64:
        raise ValueError('AArch64 ELF64 required')
    loads = image['loads']
    ordered = sorted(loads, key=lambda s: s['base'])
    if any(a['end'] > b['base'] for a, b in zip(ordered, ordered[1:])):
        raise ValueError('LOAD overlap')
    if image['entry'] != regions['el3_firmware']['base']:
        raise ValueError('entry must be EL3 base')
    for role, region_name in code_names.items():
        base = regions[region_name]['base']
        if symbols[role + '__start']['address'] != base:
            raise ValueError(role + ' entry mismatch')
        for name in ('debug_ram_log', 'debug_exception', 'debug_sync_exception', 'debug_boot'):
            symbol = symbols[role + '_' + name]
            if not any(segment['base'] <= symbol['address'] and symbol['address'] + symbol['size'] <= segment['end'] and segment['flags'] == 6 for segment in loads):
                raise ValueError(role + ' diagnostic outside writable LOAD')
        vector = symbols[role + '_exception_vectors']['address']
        if vector % 2048 or not any(s['base'] <= vector and vector + 2048 <= s['end'] and s['flags'] == 5 for s in loads):
            raise ValueError(role + ' invalid vector')
        start, end = symbols[role + '___bss_start']['address'], symbols[role + '___bss_end']['address']
        if not any(s['base'] <= start <= end <= s['end'] and s['flags'] == 6 for s in loads):
            raise ValueError(role + ' BSS bounds')
        stack = regions[role + '_stack']
        if not any(s['base'] == stack['base'] and s['end'] == stack['end'] and s['file_bytes'] == 0 and s['flags'] == 6 for s in loads):
            raise ValueError(role + ' runtime stack missing')
    allowed = [r for r in mapping['regions'] if r['owner'] in code_names]
    if any(not any(r['base'] <= s['base'] and s['end'] <= r['end'] for r in allowed) for s in loads):
        raise ValueError('LOAD outside A allocation')
    if platform == 'zcu106':
        observations = json.loads((ROOT / 'platform/zcu106/vitis_observations.json').read_text())
        validate_a_observations(mapping, observations)
        firmware = [segment for fw in observations['firmware'].values() for segment in fw['segments']]
        if any(s['base'] < f['end'] and f['physical'] < s['end'] for s in loads for f in firmware):
            raise ValueError('reported FSBL/PMUFW conflict')
    return dict(static_status='PASS', image=image, hardware_load='BLOCKED' if platform == 'zcu106' else 'NOT_APPLICABLE', hardware_execution='NOT_RUN', scope='software layout and reported ranges only; no protection proof')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--platform', choices=('qemu', 'zcu106'), required=True)
    parser.add_argument('--elf', type=Path, required=True)
    args = parser.parse_args()
    try:
        print(json.dumps(inspect(args.platform, args.elf), indent=4))
    except (OSError, ValueError, KeyError) as error:
        parser.exit(1, 'UNIFIED MAP FAIL: ' + str(error) + '\n')
