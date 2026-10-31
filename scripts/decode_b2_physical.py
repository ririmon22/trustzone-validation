import struct

def decode_physical(data, run_id):
    if len(data) != 488:
        raise ValueError('truncated physical evidence')
    values=struct.unpack('<61Q',data)
    names=['version','run_id','state','page_pa','page_bytes','entry_el','sctlr','tcr','ttbr0','mair','el2_known','hcr','vttbr','vtcr','alias_va','probe_mair','root_descriptor']
    report=dict(zip(names,values[:17]))
    if report['version']!=1 or report['run_id']!=run_id or report['state'] not in (1,2,3):
        raise ValueError('physical evidence version/run mismatch')
    report['attempts']=[]
    fields=['va','pa','write','completed','value','esr','far','elr','classification','par_read','par_write']
    classes={0:'NO_ABORT',1:'NS_TRANSLATION_FAULT',2:'NS_PERMISSION_FAULT',3:'PRECISE_EXTERNAL_ABORT',4:'OTHER_OR_WALK_ABORT'}
    for i in range(4):
        row=dict(zip(fields,values[17+i*11:28+i*11]))
        if row['write'] not in (0,1) or row['completed'] not in (0,1,2) or row['classification'] not in range(5):
            raise ValueError('corrupt physical attempt')
        row['classification_name']=classes.get(row['classification'],'INVALID')
        if row['completed']==0:row['classification_name']='NOT_ATTEMPTED'
        row['read_value_valid']=row['write']==0 and row['completed']==1
        report['attempts'].append(row)
    report['xmpu_attribution']='NOT_ESTABLISHED_BY_ESR_ALONE'
    return report


def validate_physical_pass(report, read_pc, write_pc):
    mask=0x000000fffffff000
    if report['state']!=2 or report['entry_el']!=1 or report['el2_known']!=1 or report['hcr'] & 1:
        raise ValueError('physical PASS lacks completed EL1/stage2 evidence')
    if report['page_bytes']!=4096 or report['page_pa'] & 4095 or report['sctlr'] & 5 != 5 or (report['probe_mair'] >> 16) & 255 != 0x44:
        raise ValueError('physical PASS has incompatible memory attributes')
    for index,row in enumerate(report['attempts']):
        offset=0 if index<2 else 4088
        if row['completed']!=2 or row['classification']!=3 or row['write']!=index%2 or row['pa']!=report['page_pa']+offset or row['va']!=report['alias_va']+offset:
            raise ValueError('physical PASS lacks four matched read/write attempts')
        if row['esr']>>26 & 63 != 0x25 or row['esr'] & 63 != 16 or row['esr'] & ((1<<10)|(1<<7)) or row['esr']>>6 & 1 != row['write']:
            raise ValueError('physical PASS has MMU/invalid external-abort evidence')
        if row['far']!=row['va'] or row['elr']!=(write_pc if row['write'] else read_pc):
            raise ValueError('physical PASS FAR/ELR mismatch')
        for par in [row['par_read'],row['par_write']]:
            if par & 1 or (par & mask | row['va'] & 4095)!=row['pa']:
                raise ValueError('physical PASS lacks same-PA translation')
