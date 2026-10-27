import json
import re
from pathlib import Path


def load_layout(root):
    directory = Path(root) / 'platform/zcu106/b2'
    plan = json.loads((directory / 'memory_map.json').read_text())
    if plan.get('configuration') != 'B2' or plan.get('load_authorized') is not False or plan.get('hardware_confirmed') is not False or plan.get('runtime_ready') is not False:
        raise ValueError('B2 layout is an unverified static proposal; load must remain blocked')
    regions = {item['name']: item for item in plan['regions']}
    expected = {'bl31_reserved','bl32_reserved','bl33_code','bl33_data','bl33_stack','shared_memory'}
    if set(regions) != expected or len(regions) != len(plan['regions']):
        raise ValueError('invalid B2 layout regions')
    ordered = sorted(regions.values(),key=lambda item:item['base'])
    for item in ordered:
        if not all(type(item.get(key)) is int for key in ('base','end','size','alignment')) or item['alignment'] != 4096 or item['base'] % 4096 or item['size'] <= 0 or item['size'] % 4096 or item['end'] != item['base'] + item['size']:
            raise ValueError('invalid B2 allocation: '+item['name'])
        if not plan['reported_ddr0']['base'] <= item['base'] < item['end'] <= plan['reported_ddr0']['end']:
            raise ValueError('B2 allocation outside reported DDR0: '+item['name'])
    for left,right in zip(ordered,ordered[1:]):
        if left['end'] > right['base']:
            raise ValueError('B2 reservation overlap: '+left['name']+'/'+right['name'])
    header = (directory / 'memory_map.h').read_text()
    def macro(name):
        match = re.search(r'^#define '+name+r' (0x[0-9a-fA-F]+|[0-9]+)$',header,re.M)
        if match is None:
            raise ValueError('missing B2 header macro: '+name)
        return int(match[1],0)
    for role,prefix in (('bl31_reserved','B2_BL31'),('bl32_reserved','B2_BL32'),('shared_memory','SHARED_MEMORY')):
        for suffix,key in (('BASE','base'),('END','end')):
            if macro(prefix+'_'+suffix) != regions[role][key]:
                raise ValueError('B2 header/JSON mismatch: '+prefix+'_'+suffix)
    if macro('SHARED_MEMORY_SIZE') != regions['shared_memory']['size'] or macro('B2_RUNTIME_READY') != 0:
        raise ValueError('B2 SHM size mismatch or runtime gate disabled')
    common_header = (directory.parent / 'memory_map.h').read_text()
    for name, prefix in (('bl33_code','NONSECURE_CODE'),('bl33_data','NONSECURE_DATA'),('bl33_stack','NONSECURE_STACK')):
        for suffix,key in (('BASE','base'),('END','end')):
            match = re.search(r'^#define '+prefix+'_'+suffix+r' (0x[0-9a-fA-F]+)$', common_header, re.M)
            if match is None or int(match[1],0) != regions[name][key]:
                raise ValueError('B2 BL33 layout differs from imported NS header')
    uart = plan['mmio'][0]
    if macro('B2_UART_BASE') != uart['base'] or macro('B2_UART_END') != uart['end'] or uart['size'] != uart['end'] - uart['base']:
        raise ValueError('B2 UART proposal mismatch')
    return plan,regions
