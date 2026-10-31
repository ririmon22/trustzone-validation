import argparse
import hashlib
import json
from pathlib import Path
from check_b2_physical_reservation import check

ROOT = Path(__file__).resolve().parents[1]
BANKS = tuple(0xfd000000 + i * 0x10000 for i in range(6))
REVIEW_KEYS = ('route', 'master', 'lock', 'boundary', 'runtime_reservations',
               'poison', 'priority', 'other_masters', 'secure_mmio', 'recovery')


def prepare(config, base, root=ROOT):
    # 静的配置だけでは実機予約の成立を認めず、別途証跡を要求する。
    layout = check(root)
    region = layout['region']
    if config.get('schema') != 1:
        raise ValueError('schema must be 1')
    evidence = {}
    for key in REVIEW_KEYS:
        item = config.get('reviews', {}).get(key, {})
        if item.get('confirmed') is not True:
            raise ValueError('unconfirmed review: ' + key)
        path = base / item['path']
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        if digest != item['sha256'] or len(digest) != 64:
            raise ValueError('evidence hash mismatch: ' + key)
        evidence[key] = digest
    master, mask = config['master_id'], config['master_mask']
    if type(master) is not int or type(mask) is not int or not 0 <= master <= 1023 or not 0 <= mask <= 1023:
        raise ValueError('confirmed 10-bit master ID/mask required')
    banks = config['banks']
    if not banks or len({b['base'] for b in banks}) != len(banks):
        raise ValueError('complete reviewed route bank list required')
    expected = {}
    writes = []
    for bank in banks:
        address, index = bank['base'], bank['region']
        if address not in BANKS or type(index) is not int or not 0 <= index < 16:
            raise ValueError('invalid DDR bank/region')
        ctrl, poison = bank['ctrl'], bank['poison']
        if type(ctrl) is not int or type(poison) is not int or ctrl & 12 != 8 or not poison & (1 << 20):
            raise ValueError('reviewed 1 MiB alignment/poison mode required; no global CTRL write')
        expected[address] = ctrl
        expected[address + 12] = poison
        if len(bank['regions']) != 16 or not bank.get('lock_checks'):
            raise ValueError('all 16 regions and official lock readback checks required')
        for i, fields in enumerate(bank['regions']):
            if len(fields) != 4 or any(type(v) is not int or not 0 <= v <= 0xffffffff for v in fields):
                raise ValueError('invalid region snapshot')
            if i != index and fields[3] & 1:
                active_start = fields[0] << 12
                active_end = (fields[1] << 12) + region['alignment']
                if active_start < region['end'] and region['base'] < active_end:
                    raise ValueError('existing enabled region overlaps dedicated test window')
            for j, value in enumerate(fields):
                expected[address + 0x100 + i * 16 + j * 4] = value
            if i == index and fields[3] & 1:
                raise ValueError('selected region already enabled')
        for lock in bank['lock_checks']:
            offset, value = lock['offset'], lock['expected']
            if type(offset) is not int or offset % 4 or not 0 <= offset < 0x100 or not lock.get('official_reference'):
                raise ValueError('confirmed lock offset/reference required; never inferred')
            if type(value) is not int or not 0 <= value <= 0xffffffff:
                raise ValueError('invalid lock readback value')
            if address + offset in expected and expected[address + offset] != value:
                raise ValueError('conflicting lock snapshot')
            expected[address + offset] = value
        target = address + 0x100 + index * 16
        values = (region['base'] >> 12, (region['end'] - region['alignment']) >> 12,
                  master | mask << 16, 7)
        for i, value in enumerate(values):
            writes.append((target + i * 4, value))
    return evidence, expected, writes


def main():
    parser = argparse.ArgumentParser(description='PC-only XMPU test plan; never accesses hardware')
    parser.add_argument('--input', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        raise ValueError('preserve existing plan; choose new output')
    config = json.loads(args.input.read_text())
    evidence, expected, writes = prepare(config, args.input.resolve().parent)
    # Tclへ入力文字列を埋め込まず、検査済み整数だけを出力する。
    lines = ['set tz_xmpu_plan [dict create schema 1 expected {']
    lines += [f'    {a} {v}' for a, v in sorted(expected.items())]
    lines += ['} writes {'] + [f'    {a} {v}' for a, v in writes] + ['}]']
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text('\n'.join(lines) + '\n')
    args.output.with_suffix('.json').write_text(json.dumps(dict(evidence=evidence, expected=expected,
        writes=writes, hardware='NOT_RUN', boot_gate='BLOCKED', scope='dedicated XMPU test window only'), indent=2) + '\n')
    print('PLAN GENERATED; HARDWARE NOT_RUN; BOOT GATE BLOCKED')


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, KeyError, TypeError) as error:
        raise SystemExit('BLOCKED: ' + str(error))
