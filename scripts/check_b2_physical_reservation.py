import argparse
import json
from pathlib import Path
from zcu106_b2_layout import load_layout
from check_boot_plan import elf_inventory
ROOT=Path(__file__).resolve().parents[1]

def check(root, elfs=()):
    plan, regions=load_layout(root)
    candidate=plan['physical_probe']['dedicated_region']
    start,end=candidate['base'],candidate['end']
    if start%0x100000 or end-start!=0x100000 or candidate['test_buffer_size']!=0x10000:
        raise ValueError('dedicated region needs isolated 1 MiB and 64 KiB buffer')
    reserved=[(x['base'],x['end'],name) for name,x in regions.items()]
    signed=plan['signed_ta_profile']['source'] if 'source' in plan.get('signed_ta_profile',{}) else None
    reserved.append((0x10600000,0x10700000,'signed TA bundle'))
    observations=json.loads((root/'platform/zcu106/vitis_observations.json').read_text())
    reserved += [(s['physical'],s['end'],role+' LOAD') for role,v in observations['firmware'].items() for s in v['segments']]
    for elf in elfs:
        reserved += [(s['base'],s['end'],str(elf)) for s in elf_inventory(elf)['loads']]
    if not plan['reported_ddr0']['base']<=start<end<=plan['reported_ddr0']['end']:
        raise ValueError('outside reported DDR0')
    for base,limit,name in reserved:
        if start<limit and base<end:raise ValueError('physical probe overlaps '+name)
    return dict(static_status='PASS',region=candidate,hardware_load='BLOCKED',hardware_execution='NOT_RUN',
                reason='path/master/lock/poison/PA aliases/full firmware runtime reservations unresolved',
                proposal='after review only: disabled region -> START/END/MASTER readback -> secure read/write CONFIG -> readback; no CTRL-wide change',
                writes_performed=False)

if __name__=='__main__':
    p=argparse.ArgumentParser(description='Static proposal only; no MMIO or XSDB operations')
    p.add_argument('--elf',type=Path,action='append',default=[]);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    result=check(ROOT,a.elf);a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(result,indent=4)+'\n');print('STATIC PASS; HARDWARE BLOCKED')
