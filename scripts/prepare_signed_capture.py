import argparse
import hashlib
import json
from pathlib import Path
from prepare_jtag_capture import plan
from check_b2_integration import elf_symbols
from check_boot_plan import elf_inventory

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--elf',type=Path,required=True);p.add_argument('--core',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    inputs={a.elf.resolve(),a.core.resolve()}
    if a.output.resolve() in inputs or a.output.with_suffix('.json').resolve() in inputs:
        p.error('output must preserve input ELFs')
    records=plan(a.elf)
    _,_,symbols=elf_symbols(a.core)
    symbol=symbols.get('signed_loader_audit')
    if not symbol or symbol['size']!=1040:p.error('Core lacks signed-loader audit profile')
    # Core ASLR/secure address spaceを推測せず、Core PTAからNormalへコピーしたsnapshotを回収する。
    a.output.parent.mkdir(parents=True,exist_ok=True)
    a.output.write_text('# Definition only; no target operations\nset tz_capture_plan {\n'+''.join('    {'+name+' '+hex(address)+' '+str(size)+'}\n' for name,address,size in records)+'}\n')
    early_inputs=[]
    for role in ('ta-a','ta-b'):
        for image in (a.core.parent.parent.parent/role).glob('*.stripped.elf'):
            early_inputs.append(dict(role=role,path=str(image),sha256=hashlib.sha256(image.read_bytes()).hexdigest()))
    metadata=dict(ta_inputs=early_inputs,ta_input_binding='co-located build inputs; compare build receipt, not device attestation',elf=str(a.elf),sha256=hashlib.sha256(a.elf.read_bytes()).hexdigest(),core=str(a.core),core_sha256=hashlib.sha256(a.core.read_bytes()).hexdigest(),records=records,hardware_execution='NOT_RUN')
    a.output.with_suffix('.json').write_text(json.dumps(metadata,indent=4)+'\n')
