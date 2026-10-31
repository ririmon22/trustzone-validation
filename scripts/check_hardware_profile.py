import argparse
import json
from pathlib import Path
from zcu106_b2_layout import load_layout
ROOT=Path(__file__).resolve().parents[1]
FLAGS=('hardware_map_verified','master_route_verified','secure_el3_verified','configuration_unlocked',
       'other_masters_quiesced','gic_routing_verified','xmpu_lock_verified','xmpu_poison_verified',
       'ddr_pa_aliases_verified','firmware_runtime_reserved_verified')

def profile_issues(profile,mapping):
    issues=[key+' is not verified' for key in FLAGS if profile.get(key) is not True]
    if mapping.get('hardware_confirmed') is not True:
        issues.append('B2 hardware reservation map is unconfirmed')
    if mapping.get('load_authorized') is not True or mapping.get('runtime_ready') is not True:
        issues.append('B2 hardware load/runtime authorization remains blocked')
    protected=mapping.get('physical_probe',{}).get('dedicated_region',{})
    start,end=profile.get('protected_start'),profile.get('protected_end')
    if type(start) is not int or type(end) is not int or start%0x100000 or end%0x100000 or start>=end:
        issues.append('DDR XMPU requires known exclusive 1 MiB aligned bounds')
    elif (start,end)!=(protected.get('base'),protected.get('end')):
        issues.append('protection bounds differ from dedicated B2 proposal')
    banks=profile.get('xmpu_bank_bases',[])
    if not banks or any(type(bank) is not int or bank not in {0xfd000000+i*0x10000 for i in range(6)} for bank in banks):
        issues.append('DDR XMPU bank route unresolved or invalid')
    for key in ('apu_master_id','apu_master_mask'):
        value=profile.get(key)
        if type(value) is not int or not 0<=value<=0x3ff:issues.append(key+' unresolved/outside 10-bit field')
    return issues

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--profile',type=Path,default=ROOT/'platform/zcu106/board_profile.json');a=p.parse_args()
    mapping,_=load_layout(ROOT)
    issues=profile_issues(json.loads(a.profile.read_text()),mapping)
    print('HARDWARE_PROFILE BLOCKED\n'+'\n'.join(issues) if issues else 'HARDWARE_PROFILE READY')
    raise SystemExit(2 if issues else 0)
