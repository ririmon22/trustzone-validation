import argparse
import json
import re
from pathlib import Path
from check_boot_plan import elf_inventory
from check_b2_integration import elf_symbols,symbol_payload
ROOT=Path(__file__).resolve().parents[1]

def check(root,platform,elf):
    if platform!='zcu106':raise ValueError('ZCU106-only layout checker')
    source=(0x10600000,0x10700000)
    shared=(0x10500000,0x10600000)
    reserved=[]
    if platform=='zcu106':
        from zcu106_b2_layout import load_layout
        plan,regions=load_layout(root)
        reserved=[(item['base'],item['end'],name) for name,item in regions.items()]
        observations=json.loads((root/'platform/zcu106/vitis_observations.json').read_text())
        reserved += [(s['physical'],s['end'],role+' reported LOAD') for role,item in observations['firmware'].items() for s in item['segments']]
        if source[1]>plan['reported_ddr0']['end']:raise ValueError('TA input outside reported DDR0')
    for base,end,name in reserved:
        if source[0]<end and base<source[1]:raise ValueError('TA input overlaps '+name)
    parsed=elf_symbols(elf)
    declared=symbol_payload(parsed,'b2_signed_source')
    if len(declared)!=16 or int.from_bytes(declared[:8],'little')!=source[0] or int.from_bytes(declared[8:],'little')!=source[1]-source[0]:raise ValueError('ELF TA source geometry differs from reserved map')
    image=elf_inventory(elf)
    for segment in image['loads']:
        if source[0]<segment['end'] and segment['base']<source[1]:raise ValueError('TA source overlaps ELF BSS/runtime')
    arena=(shared[0]+0x10000,shared[0]+0x10000+3*0x40000)
    if arena[1]>shared[1] or arena[0]<shared[0]+4096:raise ValueError('RPC arena overflows/resides in client message')
    return dict(static_status='PASS',ta_source=source,rpc_arena=arena,hardware_load='BLOCKED',hardware_execution='NOT_RUN',limitations='reported FSBL/PMUFW LOAD only; full runtime reservations and aliases remain unconfirmed')

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--platform',choices=['zcu106'],required=True);p.add_argument('--elf',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    report=check(ROOT,a.platform,a.elf);a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(report,indent=4)+'\n');print(report['static_status'])
