import csv
import io
import struct
from analyze_benchmarks import summarize
from decode_b2_results import text, STATUSES, PHASES
PARENTS=['TZ-SYS-01-V01','TZ-SYS-02-V01','TZ-SYS-02-V02','TZ-SYS-03-V01','TZ-SYS-03-V02',
         'TZ-SYS-04-V01','TZ-SYS-05-V01','TZ-SYS-06-V01','TZ-SYS-07-V01','EXT-AUTH-V01','EXT-KEY-V01',
         'EXT-TA-V01','EXT-TA-V02','EXT-TA-V03','EXT-BOOT-V01','EXT-STORE-V01',
         'PERF-01','PERF-02','PERF-03','PERF-04','PERF-05']
SERIES=['PERF-01','PERF-02','PERF-05','PERF-06','PERF-08','PERF-09','PERF-10','PERF-11']

def review(data,run_id,parent=0,performance_only=False):
    if len(data)!=19016:raise ValueError('missing/truncated review RAM')
    magic,version,size,state,actual,current,cleanup=struct.unpack_from('<4IQ2I',data)
    if version not in (1,2) or (magic,size,actual)!=(0x42325256,len(data),run_id) or state not in (1,2,3) or current not in (*range(21),0xffffffff):
        raise ValueError('review version/run/header mismatch')
    selected=({5,6,8,14,15,16,17,18,19,20} if performance_only else set(range(21)) if parent==0 else {parent-1})
    rows=[]
    for i,ident in enumerate(PARENTS):
        offset=32+i*904
        name=text(data[offset:offset+24]);status,phase,count,overflow=struct.unpack_from('<4I',data,offset+24)
        if name!=ident or status>=len(STATUSES) or phase>=len(PHASES) or count>16:raise ValueError('review item fields invalid')
        if i not in selected and (status!=4 or phase!=0 or count or overflow):raise ValueError('unselected review item has evidence')
        checks=[]
        for j in range(count):
            at=offset+136+j*48
            expected,actual=struct.unpack_from('<QQ',data,at+32)
            checks.append(dict(name=text(data[at:at+32]),expected=expected,actual=actual,matched=expected==actual))
        if status==0 and (phase!=7 or not checks or overflow or any(not c['matched'] for c in checks)):
            raise ValueError('review PASS contradicts evidence: '+ident)
        if status==0 and ident in ('TZ-SYS-02-V02','EXT-AUTH-V01'):
            required=({'iterations':256,'per-session-counter-steps':128,'input-seed':0x59e1a273,
                       'x19-x29-SP-errors':0,'last-counter':128,'ECHO-transport-result':0,
                       'counter-transport-result':0,'TA-origins':(4<<32)|4}
                      if ident=='TZ-SYS-02-V02' else
                      {'before-token':0xffff0001,'wrong-token':0xffff0001,'other-session-denied':0xffff0001,
                       'revoke':0xffff0001,'after-revoke':0xffff0001,'reopen-without-token':0xffff0001,
                       'TA-identity-subtest':0})
            if version==1 and ident=='TZ-SYS-02-V02':
                required={k:v for k,v in required.items() if k in ('iterations','per-session-counter-steps','input-seed','x19-x29-SP-errors')}
            for label,value in required.items():
                found=[c for c in checks if c['name']==label]
                if len(found)!=1 or found[0]['expected']!=value or found[0]['actual']!=value:
                    raise ValueError('review PASS lacks specified input/iteration/refusal: '+label)
        label='MEASURED'  if status==0 and ident.startswith('PERF-') else STATUSES[status]
        rows.append(dict(test_id=ident,status=label,phase=PHASES[phase],reason=text(data[offset+40:offset+136]),checks=checks))
    return dict(version=version,run_id=run_id,state=state,cleanup_errors=cleanup,
                collection_status='COMPLETE' if state==2 and not cleanup and all(rows[i]['phase']=='DONE' and rows[i]['status']!='NOT_RUN' for i in selected) else 'INCOMPLETE',items=rows)

def benchmarks(data,run_id):
    if len(data)!=17600:raise ValueError('missing/truncated benchmark RAM')
    magic,version,size,state,actual,freq,sctlr,daif,count,errors,warmup,samples=struct.unpack_from('<4I4Q4I',data)
    if (magic,version,size,actual)!=(0x4232424d,1,len(data),run_id) or not freq or count!=8 or warmup!=16 or samples!=128:
        raise ValueError('benchmark header/run/conditions mismatch')
    rows=[];stream=io.StringIO();writer=csv.writer(stream)
    writer.writerow(['parent_id','series_id','kind','index','ticks','timer_hz','run_id'])
    for i,ident in enumerate(SERIES):
        at=64+i*2192;name=text(data[at:at+16]);interval=text(data[at+16:at+128])
        n,over,complete,reserved=struct.unpack_from('<4I',data,at+128)
        if name!=ident or (n,over,complete,reserved)!=(128,128,1,0):raise ValueError('benchmark series incomplete')
        values=list(struct.unpack_from('<128Q',data,at+144));overhead=list(struct.unpack_from('<128Q',data,at+1168))
        parent=ident if ident in ('PERF-01','PERF-02') else 'PERF-05'
        rows.append(dict(parent_id=parent,series_id=ident,interval=interval,unit='CNTPCT_EL0 ticks',
                         statistics=summarize(values),overhead=summarize(overhead),overhead_subtracted=False))
        for kind,sequence in [('sample',values),('overhead',overhead)]:
            for index,value in enumerate(sequence):writer.writerow([parent,ident,kind,index,value,freq,run_id])
    return dict(state=state,errors=errors,run_id=run_id,timer_hz=freq,cpu_hz='UNKNOWN',sctlr_el1=sctlr,daif=daif,
                warmup=warmup,samples=samples,collection_status='COMPLETE' if state==2 and not errors else 'INCOMPLETE',
                interpretation='measurement infrastructure only; no speed threshold; QEMU is not hardware performance',series=rows),stream.getvalue()


def sgi(data,run_id):
    if len(data)!=2112:raise ValueError('missing/truncated SGI samples')
    magic,version,size,state,actual,freq,warmup,count,errors,intid,handler_mask,entry_mask=struct.unpack_from('<4I2Q4I2Q',data)
    if (magic,version,size,actual)!=(0x42325347,1,len(data),run_id) or not freq or warmup!=16 or count!=128 or intid!=11:
        raise ValueError('SGI sample header/run/ID/count mismatch')
    values=list(struct.unpack_from('<128Q',data,64));overhead=list(struct.unpack_from('<128Q',data,1088))
    return dict(collection_status='COMPLETE' if state==2 and not errors else 'INCOMPLETE',run_id=run_id,
                counter='CNTPCT_EL0',timer_hz=freq,cpu_hz='UNKNOWN',unit='ticks',received_id=intid,
                handler_mask=handler_mask,entry_mask=entry_mask,warmup=warmup,sample_count=count,
                interval='Secure SGI issue to first C handler timestamp; includes unmask/handler prologue; excludes SMC/printf',
                scope='Secure-self only; not Normal-running delivery; not vector entry latency',
                statistics=summarize(values),overhead=summarize(overhead),samples=values,overhead_samples=overhead,
                interpretation='measurement complete only; no speed threshold')
