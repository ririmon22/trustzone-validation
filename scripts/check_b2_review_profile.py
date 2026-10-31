import argparse
import json
import struct
from pathlib import Path
from check_b2_integration import elf_symbols, symbol_payload
from check_b2_physical_reservation import check as physical_check

def check(root,platform,elf):
    if platform!='zcu106':raise ValueError('ZCU106-only software profile checker')
    directory=root/'build/external'/('optee' if platform=='qemu' else 'optee-zynqmp')
    values={}
    for line in (directory/'conf.mk').read_text().splitlines():
        if line.startswith('CFG_') and '=' in line:
            key,value=line.split('=',1);values[key]=value.strip()
    version,canary,sgi=struct.unpack('<3I',symbol_payload(elf_symbols(elf),'b2_review_features'))
    if version!=1 or canary not in (0,1) or sgi not in (0,1):raise ValueError('invalid linked profile')
    _,_,core=elf_symbols(directory/'core/tee.elf')
    if canary and (values.get('CFG_TZ_VALIDATION_PTA')!='y' or values.get('CFG_WITH_STATS')!='y' or 'core_canary' not in core):
        raise ValueError('BL33 requires canary/stats not built into selected Core')
    if sgi and (platform!='qemu' or values.get('CFG_TZ_SGI_PTA')!='y' or 'b2_secure_sgi_boot_init' not in core):
        raise ValueError('SGI profile/platform mismatch')
    if sgi and (values.get('CFG_TEE_CORE_NB_CORE')!='1' or values.get('CFG_CORE_ASYNC_NOTIF')!='n' or values.get('CFG_ARM_GICV3')=='y' or values.get('CFG_CORE_IRQ_IS_NATIVE_INTR')=='y' or values.get('CFG_CORE_WORKAROUND_ARM_NMFI')!='n'):
        raise ValueError('SGI proof requires single-core GICv2 native FIQ profile without async notification/NMFI')
    physical=None
    if 'CFG_TZ_PHYSICAL_FIXED_BASE' in values:
        physical=physical_check(root,[elf,directory/'core/tee.elf'])
        allowed=struct.unpack('<2Q',symbol_payload(elf_symbols(elf),'b2_physical_reservation'))
        if allowed!=(physical['region']['base'],physical['region']['end']):raise ValueError('BL33 PA acceptance differs from dedicated Core reservation')
        if int(values['CFG_TZ_PHYSICAL_FIXED_BASE'],0)!=physical['region']['base'] or values.get('CFG_TZ_PHYSICAL_PTA')!='y':
            raise ValueError('dedicated Secure map differs from static reservation')
    return dict(static_status='PASS',canary=canary,secure_sgi=sgi,physical=physical,
                hardware_execution='NOT_RUN',hardware_load='BLOCKED',scope='linked software profile only; not live GIC/XMPU or loaded-image attestation')

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--root',type=Path,required=True);p.add_argument('--platform',choices=['zcu106'],required=True);p.add_argument('--elf',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    report=check(a.root,a.platform,a.elf);a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(report,indent=4)+'\n');print('STATIC PROFILE PASS; HARDWARE BLOCKED')
