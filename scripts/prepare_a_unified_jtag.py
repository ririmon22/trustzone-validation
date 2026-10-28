import argparse
import hashlib
import json
from pathlib import Path
from check_a_unified import inspect
from check_b2_integration import elf_symbols
from check_hardware_profile import profile_issues
from prepare_jtag_capture import plan

ROOT = Path(__file__).resolve().parents[1]


def inspect_plan(elf):
    layout = inspect('zcu106', elf)
    _, _, symbols = elf_symbols(elf)
    wanted = {'entry':'_start', 'monitor':'el3_monitor_main', 'secure':'secure_entry', 'normal':'nonsecure_entry', 'finish':'nonsecure_platform_halt', 'entry_blocked':'el3_entry_state_blocked'}
    stops = {key:symbols[name]['address'] for key,name in wanted.items()}
    profile = json.loads((ROOT/'platform/zcu106/board_profile.json').read_text())
    mapping = json.loads((ROOT/'platform/zcu106/memory_map.json').read_text())
    blockers = profile_issues(profile, mapping)
    blockers += ['PLATFORM_PROTECTION_CONFIRMED=0; no execution permission', 'actual FSBL/PMUFW/XSA, reset/DDR/EL3 handoff and debug access evidence incomplete']
    return dict(static_status=layout['static_status'], hardware_load='BLOCKED', load_authorized=False,
                hardware_execution='NOT_RUN', elf=str(elf.resolve()), sha256=hashlib.sha256(elf.read_bytes()).hexdigest(),
                entry=layout['image']['entry'], loads=layout['image']['loads'], stops=stops,
                capture=plan(elf), blockers=blockers)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--elf', type=Path, default=ROOT/'build/zcu106/unified/a_unified.elf')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--emit-tcl', type=Path)
    args = parser.parse_args()
    try:
        report = inspect_plan(args.elf)
    except (OSError, ValueError, KeyError) as error:
        report = dict(static_status='FAIL',hardware_load='BLOCKED',load_authorized=False,hardware_execution='NOT_RUN',blockers=[str(error)])
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report,indent=4)+'\n')
    if args.emit_tcl:
        args.emit_tcl.parent.mkdir(parents=True, exist_ok=True)
        script = '# 検査専用。現状のplanは実機操作を許可しない。\nset tz_a_load_authorized 0\n'
        if report['static_status'] == 'PASS':
            quoted = json.dumps(report['elf']).replace('$','\\$').replace('[','\\[').replace(']','\\]')
            script += 'set tz_a_elf ' + quoted + '\n'
            script += 'set tz_a_sha256 '+report['sha256']+'\n'
            script += 'set tz_a_stops [dict create '+' '.join(key+' '+hex(value) for key,value in report['stops'].items())+']\n'
            script += 'set tz_capture_plan {\n'+''.join('    {'+name+' '+hex(address)+' '+str(size)+'}\n' for name,address,size in report['capture'])+'}\n'
        args.emit_tcl.write_text(script)
    print(args.output)
    raise SystemExit(2 if report['static_status'] == 'PASS' else 1)
