import argparse
import hashlib
import json
import struct
from pathlib import Path
from check_hardware_profile import profile_issues
from check_memory_map import validate

ROOT = Path(__file__).resolve().parents[1]
LIMIT = 1 << 64
B2_ARTIFACT_PATHS = {
    'bl31': 'build/external/tfa-zynqmp-b2/zynqmp/release/bl31/bl31.elf',
    'bl32': 'build/external/optee-zynqmp/core/tee.elf',
    'bl33': 'build/zcu106/optee_bl33.elf',
}


def b2_binding_issues(plan):
    if plan.get('configuration') != 'B2':
        return []
    issues = []
    for role, expected in B2_ARTIFACT_PATHS.items():
        if plan.get('images', {}).get(role, {}).get('path') != expected:
            issues.append('B2 artifact path differs from integrated build output: ' + role)
    if plan.get('integration', {}).get('required_spd') != 'opteed':
        issues.append('B2 requires explicit SPD=opteed build binding')
    return issues


def elf_inventory(path):
    data = Path(path).read_bytes()
    if len(data) < 52 or data[:4] != b'\x7fELF' or data[4] not in (1, 2) or data[5] not in (1, 2):
        raise ValueError('unsupported ELF')
    order = '<' if data[5] == 1 else '>'
    wide = data[4] == 2
    if wide and len(data) < 64:
        raise ValueError('short ELF64')
    machine = struct.unpack_from(order + 'H', data, 18)[0]
    entry, offset = struct.unpack_from(order + ('QQ' if wide else 'II'), data, 24)
    stride, count = struct.unpack_from(order + 'HH', data, 54 if wide else 42)
    expected = 56 if wide else 32
    if stride != expected or offset + count * stride > len(data):
        raise ValueError('invalid ELF program headers')
    loads = []
    for index in range(count):
        fields = struct.unpack_from(order + ('IIQQQQQQ' if wide else 'IIIIIIII'), data, offset + index * stride)
        if wide:
            kind, flags, source, virtual, physical, filesz, memsz, align = fields
        else:
            kind, source, virtual, physical, filesz, memsz, flags, align = fields
        if kind != 1 or not memsz:
            continue
        if filesz > memsz or source + filesz > len(data) or physical + memsz > LIMIT or virtual + memsz > LIMIT:
            raise ValueError('invalid ELF load bounds')
        if align > 1 and (align & (align - 1) or (virtual - source) % align):
            raise ValueError('invalid ELF load alignment')
        loads.append(dict(base=physical, end=physical + memsz, virtual=virtual, file_bytes=filesz, runtime_bytes=memsz, zero_fill_bytes=memsz - filesz, flags=flags))
    if not loads or not any(s['virtual'] <= entry < s['virtual'] + s['runtime_bytes'] and s['flags'] & 1 for s in loads):
        raise ValueError('entry outside executable load')
    return dict(path=str(path), sha256=hashlib.sha256(data).hexdigest(), elf_class=64 if wide else 32, machine=machine, entry=entry, loads=loads)


def range_issues(regions):
    issues, valid = [], []
    for region in regions:
        base, end, align = (region.get(k) for k in ('base', 'end', 'alignment'))
        if any(type(x) is not int for x in (base, end, align)) or not 0 <= base < end <= LIMIT or align <= 0 or align & (align - 1) or base % align or end % align:
            issues.append('invalid/unresolved allocation: ' + str(region.get('name')))
        else:
            valid.append(region)
    for index, left in enumerate(valid):
        for right in valid[index + 1:]:
            if left['base'] < right['end'] and right['base'] < left['end']:
                issues.append('runtime overlap: ' + left['name'] + '/' + right['name'])
    return issues


def check_plan(plan, root=ROOT):
    issues = b2_binding_issues(plan)
    if plan.get('platform') != 'zcu106' or plan.get('configuration') not in ('A', 'B1', 'B2'):
        issues.append('platform/configuration is unresolved')
    verified_evidence = set()
    for name in ('xsa', 'bsp_map', 'live_register_snapshot', 'debug_boot_review'):
        item = plan.get('evidence', {}).get(name, {})
        path = root / (item.get('path') or '__missing__')
        if item.get('verified') is not True or not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest() != item.get('sha256'):
            issues.append('missing/unverified evidence: ' + name)
        else:
            verified_evidence.add(name)
    regions = plan.get('allocations', [])
    issues += range_issues(regions)
    if not regions:
        issues.append('runtime allocations are missing')
    roles = ('el3', 'secure', 'nonsecure') if plan.get('configuration') == 'A' else ('bl31', 'bl33') + (('bl32',) if plan.get('configuration') == 'B2' else ())
    roles += ('fsbl', 'pmufw')
    inventory = []
    for role in roles:
        item = plan.get('images', {}).get(role, {})
        path = root / (item.get('path') or '__missing__')
        if not path.is_file():
            issues.append('missing ELF: ' + role)
            continue
        try:
            image = elf_inventory(path)
        except (ValueError, OSError, struct.error) as error:
            issues.append(role + ': ' + str(error))
            continue
        inventory.append(dict(role=role, **image))
        if item.get('sha256') != image['sha256'] or item.get('entry') != image['entry']:
            issues.append('ELF hash/entry mismatch or unresolved: ' + role)
        if image['machine'] != (189 if role == 'pmufw' else 183):
            issues.append('ELF architecture mismatch: ' + role)
        if item.get('translation_verified') is not True or item.get('translation_evidence') not in verified_evidence:
            issues.append('system address translation unverified: ' + role)
        bias = item.get('system_address_bias')
        if type(bias) is not int:
            issues.append('system address bias unresolved: ' + role)
            continue
        owned = [r for r in regions if r.get('owner') == role and type(r.get('base')) is int and type(r.get('end')) is int]
        for segment in image['loads']:
            if not any(r['base'] <= segment['base'] + bias and segment['end'] + bias <= r['end'] for r in owned):
                issues.append('ELF including BSS escapes reservation: ' + role)
        for kind in ('stack', 'heap', 'dynamic'):
            policy = item.get('runtime', {}).get(kind, {})
            if policy.get('verified') is not True or policy.get('mode') not in ('reserved', 'unused'):
                issues.append('runtime ' + kind + ' unresolved: ' + role)
            elif policy['mode'] == 'reserved' and not any(r.get('name') == policy.get('allocation') and r.get('owner') == role for r in regions):
                issues.append('runtime allocation missing: ' + role + '/' + kind)
            elif policy.get('evidence') not in verified_evidence:
                issues.append('runtime evidence missing: ' + role + '/' + kind)
    if plan.get('configuration') == 'B2' and not any(r.get('purpose') == 'shared' and r.get('security') == 'nonsecure' for r in regions):
        issues.append('B2 nonsecure shared reservation is missing')
    for key in ('jtag_boot_mode', 'debug_authorization', 'initial_el_security', 'handoff', 'uart_clock_route', 'fsbl_pmufw_lifetime', 'mmio_allowlist', 'master_route', 'gic_routing'):
        record = plan.get('reviews', {}).get(key, {})
        if record.get('verified') is not True or record.get('evidence') not in verified_evidence:
            issues.append('review unresolved: ' + key)
    mapping = json.loads((root / 'platform/zcu106/memory_map.json').read_text())
    try:
        platform_dir = root / 'platform/zcu106'
        validate(mapping, (platform_dir / 'memory_map.h').read_text(),
                 {kind:(platform_dir / ('linker_' + kind + '.ld')).read_text() for kind in ('el3', 'secure', 'nonsecure')},
                 require_protection=True)
    except ValueError as error:
        issues.append(str(error))
    aliases = {'bl31':'tfa_reserved', 'bl32':'optee_reserved', 'bl33':'nonsecure'}
    b2_regions = None
    if plan.get('configuration') == 'B2':
        from zcu106_b2_layout import load_layout
        proposal, b2_regions = load_layout(root)
        if proposal.get('hardware_confirmed') is not True:
            issues.append('B2-specific reservations are a software proposal, not verified hardware reservations')
    for image in inventory:
        role = image['role']
        if b2_regions is not None:
            expected = [r for r in b2_regions.values() if r.get('owner') == role]
        else:
            expected = [r for r in mapping['regions'] if r.get('name') == aliases.get(role) or r.get('owner') == aliases.get(role, role)]
        if role not in ('fsbl', 'pmufw'):
            bias = plan['images'][role].get('system_address_bias')
            if type(bias) is int and any(not any(r['base'] <= segment['base'] + bias and segment['end'] + bias <= r['end'] for r in expected) for segment in image['loads']):
                issues.append('ELF conflicts with configured map/header reservation: ' + role)
    profile = json.loads((root / 'platform/zcu106/board_profile.json').read_text())
    issues += profile_issues(profile, mapping)
    catalog = json.loads((root / 'platform/zcu106/mmio_catalog.json').read_text())['blocks']
    if not plan.get('mmio'):
        issues.append('MMIO bindings are missing')
    for binding in plan.get('mmio', []):
        block = catalog.get(binding.get('block'), {})
        base, end = binding.get('base'), binding.get('end')
        if type(base) is not int or type(end) is not int or base not in block.get('bases', []) or end != base + block.get('window_size', 0):
            issues.append('MMIO address outside official catalog')
        if binding.get('evidence') not in verified_evidence or binding.get('security') not in ('secure', 'nonsecure'):
            issues.append('MMIO security/snapshot evidence unresolved')
    masters = plan.get('masters', {})
    for role, key in (('apu', 'apu_master_id'), ('dma', 'dma_master_id')):
        value = masters.get(role)
        if type(value) is not int or not 0 <= value <= 0x3ff or value != profile.get(key):
            issues.append('unresolved/mismatched access master: ' + role)
    if masters.get('nonsecure_verified') is not True or masters.get('evidence') not in verified_evidence:
        issues.append('master security attribute/route evidence unresolved')
    return dict(status='BLOCKED' if issues else 'READY', issues=issues, elf_inventory=inventory,
                hardware_execution='NOT_RUN', scope='pre-load validation; no hardware execution')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--plan', type=Path, default=ROOT / 'platform/zcu106/boot_plan.json')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--inventory', type=Path, nargs='*')
    args = parser.parse_args()
    try:
        if args.inventory is not None:
            result = [elf_inventory(path) for path in args.inventory]
        else:
            result = check_plan(json.loads(args.plan.read_text()))
    except (OSError, ValueError, KeyError, TypeError, AttributeError, struct.error) as error:
        result = dict(status='BLOCKED', issues=['Invalid/incomplete input: ' + str(error)], hardware_execution='NOT_RUN')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=4) + '\n')
    print(args.output)
    return 2 if isinstance(result, dict) and result['status'] == 'BLOCKED' else 0


if __name__ == '__main__':
    raise SystemExit(main())
