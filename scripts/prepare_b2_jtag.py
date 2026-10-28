import argparse
import hashlib
import json
from pathlib import Path
from check_boot_plan import elf_inventory, check_plan

ROOT = Path(__file__).resolve().parents[1]


def inspect(inputs):
    issues, inventory = [], {}
    for role, record in inputs['images'].items():
        name = record.get('path')
        if not name or not (ROOT / name).is_file():
            issues.append(role + ': original ELF missing')
            continue
        try:
            image = elf_inventory(ROOT / name)
            inventory[role] = image
            if image['sha256'] != record.get('sha256') or image['entry'] != record.get('expected_entry'):
                issues.append(role + ': expected hash/entry missing or mismatched')
            if image['machine'] != (189 if role == 'pmufw' else 183):
                issues.append(role + ': wrong ELF architecture')
        except (OSError, ValueError) as error:
            issues.append(role + ': ' + str(error))
    xsa = inputs['xsa']
    path = ROOT / (xsa.get('path') or '__missing__')
    if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest() != xsa.get('sha256'):
        issues.append('original XSA hash unavailable/mismatched')
    binding = inputs['firmware_xsa_bindings']
    if not xsa.get('sha256') or any(binding.get(role + '_xsa_sha256') != xsa['sha256'] for role in ('fsbl', 'pmufw')) or not binding.get('review_evidence'):
        issues.append('FSBL/PMUFW to XSA provenance unresolved')
    provenance = inputs['source_provenance']
    if any(provenance.get(name) is not True for name in ('tfa_fixed_commit_verified', 'optee_fixed_commit_verified')) or not provenance.get('evidence'):
        issues.append('ZIP-built TF-A/OP-TEE fixed-commit provenance unresolved')
    if any(not value for value in inputs['targets'].values()):
        issues.append('unique APU/PMU target filters unresolved')
    if any(type(value) is not int for value in inputs['stops'].values()):
        issues.append('FSBL Exit / OP-TEE initialization / BL33 stop addresses unresolved')
    if any(type(inputs['pmu_ready'].get(key)) is not int for key in ('address', 'mask', 'expected')) or not inputs['pmu_ready'].get('evidence'):
        issues.append('PMUFW ready predicate requires documented firmware-specific evidence')
    if type(inputs.get('timeout_ms')) is not int or not 1 <= inputs['timeout_ms'] <= 300000:
        issues.append('bounded timeout must be 1..300000 milliseconds')
    if not inputs['reset_initialization_reviewed']:
        issues.append('reset/initialization order not reviewed')
    gate = check_plan(json.loads((ROOT / 'platform/zcu106/boot_plan.json').read_text()))
    issues += gate['issues']
    if '#define B2_RUNTIME_READY 0' in (ROOT / 'platform/zcu106/b2/memory_map.h').read_text():
        issues.append('B2_RUNTIME_READY=0; not authorized to execute')
    return dict(status='BLOCKED', issues=issues, elf_inventory=inventory, hardware_execution='NOT_RUN', execution_authorized=False)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--inputs', type=Path, default=ROOT / 'platform/zcu106/jtag_b2_inputs.json')
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--emit-tcl', type=Path)
    args = parser.parse_args()
    inputs = {}
    try:
        inputs = json.loads(args.inputs.read_text())
        report = inspect(inputs)
    except (OSError, ValueError, KeyError, TypeError) as error:
        report = dict(status='BLOCKED', issues=[str(error)], hardware_execution='NOT_RUN', execution_authorized=False)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=4) + '\n')
    if args.emit_tcl:
        args.emit_tcl.parent.mkdir(parents=True, exist_ok=True)
        def word(value):
            text = '' if value is None else str(value)
            escaped = ''.join('\\' + char if char in '\\"$[]' else '\\n' if char == '\n' else '\\r' if char == '\r' else char for char in text)
            return '"' + escaped + '"'
        images = inputs.get('images', {})
        targets = inputs.get('targets', {})
        stops = inputs.get('stops', {})
        ready = inputs.get('pmu_ready', {})
        script = '# Inspection only. No authorization is emitted.\nset tz_boot_authorized 0\n'
        script += 'set tz_images [dict create ' + ' '.join(word(role) + ' ' + word((ROOT / rec['path']).resolve() if rec.get('path') else None) for role, rec in images.items()) + ']\n'
        script += 'set tz_apu_filter ' + word(targets.get('apu_filter')) + '\nset tz_pmu_filter ' + word(targets.get('pmu_filter')) + '\n'
        script += 'set tz_pmu_ready [list ' + ' '.join(word(ready.get(key)) for key in ('address','mask','expected')) + ']\n'
        script += 'set tz_stops [dict create ' + ' '.join(key + ' ' + word(stops.get(key + '_address')) for key in ('fsbl_exit','optee_init','bl33_main')) + ']\n'
        script += 'dict set tz_stops bl31_entry ' + word(images.get('bl31', {}).get('expected_entry')) + '\n'
        script += 'set tz_timeout ' + word(inputs.get('timeout_ms')) + '\n'
        args.emit_tcl.write_text(script)
    print(args.output)
    raise SystemExit(2)
