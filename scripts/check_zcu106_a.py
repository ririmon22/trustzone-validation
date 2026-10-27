import argparse
import hashlib
import json
import re
import struct
from pathlib import Path
from check_boot_plan import elf_inventory
from check_hardware_profile import profile_issues
from check_memory_map import validate, validate_a_observations

ROOT = Path(__file__).resolve().parents[1]
IMAGES = ('el3', 'secure', 'nonsecure')


def validate_blocked_entry_definitions(header):
    # 静的準備の段階で保護停止や入口検査を解除しない。
    for name, value in (('PLATFORM_PROTECTION_CONFIRMED', 0), ('PLATFORM_REQUIRE_EL3_ENTRY_GUARD', 1)):
        if not re.search(r'^#define\s+' + name + r'\s+' + str(value) + r'\s*$', header, re.M):
            raise ValueError('A preparation requires ' + name + '=' + str(value))


def inspect_a(build_dir, root=ROOT):
    directory = root / 'platform/zcu106'
    mapping = json.loads((directory / 'memory_map.json').read_text())
    observations = json.loads((directory / mapping['hardware_observations']).read_text())
    plan = json.loads((directory / 'jtag_a_plan.json').read_text())
    profile = json.loads((directory / 'board_profile.json').read_text())
    header = (directory / 'memory_map.h').read_text()
    validate_blocked_entry_definitions(header)
    if plan.get('configuration') != 'A' or plan.get('load_authorized') is not False:
        raise ValueError('A plan must remain a blocked preparation plan')
    if plan.get('entry_contract') != dict(exception_level=3, spsel=1, sctlr_el3_m=False, sctlr_el3_c=False, sctlr_el3_i=False):
        raise ValueError('unexpected A entry contract')
    elfs = [Path(build_dir) / (image + '.elf') for image in IMAGES]
    validate(mapping, header,
             {image: (directory / ('linker_' + image + '.ld')).read_text() for image in IMAGES},
             elfs, observations=observations)
    inventory = []
    by_name = {region['name']: region for region in mapping['regions']}
    for image, path in zip(IMAGES, elfs):
        record = elf_inventory(path)
        code = by_name[{'el3': 'el3_firmware', 'secure': 'secure_code', 'nonsecure': 'nonsecure_code'}[image]]
        if record['entry'] != code['base'] or plan['images'][image]['proposed_entry'] != record['entry']:
            raise ValueError('A entry mismatch: ' + image)
        stack = by_name[{'el3': 'el3_stack', 'secure': 'secure_stack', 'nonsecure': 'nonsecure_stack'}[image]]
        stack_loads = [segment for segment in record['loads'] if (segment['base'], segment['end']) == (stack['base'], stack['end'])]
        if len(stack_loads) != 1 or stack_loads[0]['file_bytes'] != 0 or stack_loads[0]['flags'] != 6:
            raise ValueError('A stack must be a complete non-executable zero-fill runtime reservation')
        inventory.append(dict(role=image, **record))
    static = validate_a_observations(mapping, observations)
    blocked = profile_issues(profile, mapping)
    blocked += [
        'Original XSA/BSP/linker, launch.json and FSBL/PMUFW ELFs/hashes have not been supplied',
        'Reported PhysAddr views/aliases, firmware stack/heap/scratch and post-handoff lifetimes remain unverified',
        'DDR0 is a reported static bound, not an executed DDR initialization test; DDR1 is excluded',
        'Reset Entire System with FSBL selected does not establish EL3, cache state, handoff or MPU policy',
        'Live XMPU registers/lock/master-route and Secure versus Non-secure access policy are unknown',
        'Generated XMPU initialization returning 1 does not prove register configuration',
        'PLATFORM_PROTECTION_CONFIRMED remains 0; no hardware load or execution authorized',
    ]
    return dict(configuration='A', static_status='PASS', elf_validation='PASS',
                load_status='BLOCKED', hardware_execution='NOT_RUN', qemu_execution='NOT_RUN',
                evidence_scope='built A ELFs versus supplied numeric Vitis observations; no board access',
                observations_sha256=hashlib.sha256((directory / mapping['hardware_observations']).read_bytes()).hexdigest(),
                static_checks=static, elf_inventory=inventory,
                deferred_tests=observations['deferred_tests'], load_blockers=blocked)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--build-dir', type=Path, default=ROOT / 'build/zcu106')
    parser.add_argument('--output', type=Path, default=ROOT / 'results/zcu106-a/static-report.json')
    parser.add_argument('--require-load-ready', action='store_true')
    args = parser.parse_args()
    try:
        report = inspect_a(args.build_dir)
    except FileNotFoundError as error:
        report = dict(configuration='A', static_status='BLOCKED', load_status='BLOCKED',
                      hardware_execution='NOT_RUN', error=str(error))
    except (OSError, ValueError, KeyError, TypeError, AttributeError, struct.error) as error:
        report = dict(configuration='A', static_status='FAIL', load_status='BLOCKED',
                      hardware_execution='NOT_RUN', error=str(error))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=4) + '\n')
    print(json.dumps(report, indent=2))
    if report['static_status'] == 'BLOCKED':
        return 2
    if report['static_status'] != 'PASS':
        return 1
    return 2 if args.require_load_ready and report['load_status'] != 'READY' else 0


if __name__ == '__main__':
    raise SystemExit(main())
