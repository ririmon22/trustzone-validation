import argparse
import json
import re
from pathlib import Path


def audit(text):
    states = re.findall(r'MEM_STATE world=(secure|nonsecure) sctlr=(0x[0-9a-f]+) ttbr0=(0x[0-9a-f]+) tcr=(0x[0-9a-f]+) base=(0x[0-9a-f]+) end=(0x[0-9a-f]+)', text)
    if len(states) != 2 or {s[0] for s in states} != {'secure', 'nonsecure'} or any(int(s[1],16)&1 for s in states):
        raise ValueError('both EL1 MMUs must be verified disabled')
    base, end = int(states[0][4],16), int(states[0][5],16)
    if any((int(s[4],16),int(s[5],16)) != (base,end) for s in states):
        raise ValueError('Secure/NS target addresses differ')
    worlds = re.findall(r'MEM_WORLD world=(secure|nonsecure) scr=(0x[0-9a-f]+)', text)
    if len(worlds)!=2 or {name for name,scr in worlds}!={'secure','nonsecure'} or any(bool(int(scr,16)&1)!=(name=='nonsecure') for name,scr in worlds):
        raise ValueError('SCR security-state evidence mismatch')
    hcr = re.findall(r'MEM_HCR value=(0x[0-9a-f]+)', text)
    if len(hcr)!=1 or int(hcr[0],16)&1:
        raise ValueError('stage-2 translation enabled or unknown')
    aborts = re.findall(r'MEM_ABORT address=(0x[0-9a-f]+) far=(0x[0-9a-f]+) esr=(0x[0-9a-f]+) pc=(0x[0-9a-f]+) sctlr=(0x[0-9a-f]+)', text)
    expected = [(base,False),(base,True),(end-8,False),(end-8,True)]
    if len(aborts)!=4:
        raise ValueError('four read/write boundary aborts required')
    for row,(address,write) in zip(aborts,expected):
        addr,far,esr,pc,sctlr = (int(value,16) for value in row)
        if addr!=address or far!=address or esr>>26!=0x25 or esr&0x3f!=0x10 or bool(esr&(1<<6))!=write or esr&(1<<10) or sctlr&1:
            raise ValueError('not an MMU-off synchronous external abort at the expected address')
    if re.findall(r'MEM_RECHECK intact=(\d+)', text)!=['1','1','1']:
        raise ValueError('secure pre/post data checks missing or failed')
    return dict(status='PASS', mechanism='QEMU Secure-only physical address-space decode; not a page-table Translation Fault',
                va_equals_pa=True, mmu=False, stage2=False, target_base=base,target_end=end,
                aborts=[dict(address=int(a,16),far=int(f,16),esr=int(e,16),pc=int(p,16)) for a,f,e,p,s in aborts],
                scope='QEMU virt Secure RAM only; not ZCU106 XMPU')


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('log',type=Path)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    try:
        result=audit(args.log.read_text())
    except (ValueError,OSError) as error:
        parser.exit(1,'TZ AUDIT FAIL: '+str(error)+'\n')
    args.output.write_text(json.dumps(result,indent=4)+'\n')
    print('TZ AUDIT PASS: MMU-off external abort and Secure data rechecks verified')


if __name__=='__main__':
    main()
