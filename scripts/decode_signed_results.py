import hashlib
import struct
from pathlib import Path
UUID=bytes.fromhex('6dc1341c534f46429138215876304910')
EXPECTED=[0,0xffff000f,0,0xffff000f,0,0xffff0008,0xffff0006,0]

def decode_signed(directory,run_id):
    directory=Path(directory);data=(directory/'b2_signed_audit.bin').read_bytes()
    if len(data)!=1032:raise ValueError('signed RPC result size mismatch')
    header=struct.unpack_from('<4IQ12I',data)
    if header[0]!=2 or header[1]!=1032 or header[2]!=2 or header[3]!=8 or header[4]!=run_id:raise ValueError('signed RPC results incomplete/stale/version mismatch')
    cumulative=dict(zip(['rpc_count','queries','copies','allocations','frees','live','unsupported','local_refusals','current_scenario','rpc_function','rpc_command','poisoned'],header[5:]))
    if cumulative['live'] or cumulative['unsupported'] or cumulative['poisoned'] or cumulative['allocations']!=cumulative['frees']:raise ValueError('RPC resource leak/unsupported suspension')
    bundle=(directory/'signed_ta_bundle.bin').read_bytes()
    if len(bundle)!=0x100000 or struct.unpack_from('<4I',bundle)!=(0x5441424e,1,0x100000,3) or bundle[16:32]!=UUID:raise ValueError('invalid bundle/UUID')
    images=[]
    for i in range(3):
        offset,size,digest=struct.unpack_from('<QQ32s',bundle,32+i*48)
        if not 4096<=offset<=len(bundle) or not 0<size<=0x40000 or size>len(bundle)-offset:raise ValueError('bundle range overflow')
        actual=hashlib.sha256(bundle[offset:offset+size]).digest()
        if actual!=digest:raise ValueError('RAM image hash differs from bundle manifest')
        images.append(dict(offset=offset,size=size,sha256=actual.hex()))
    status=(directory/'b2_signed_snapshot_status.bin').read_bytes()
    if len(status)!=16 or struct.unpack('<4I',status)[:3]!=(0,0,4):raise ValueError('Core loader audit copy did not succeed')
    core=(directory/'b2_signed_loader_snapshot.bin').read_bytes()
    if len(core)!=1040:raise ValueError('missing/truncated Core loader audit')
    version,used,overflow,_=struct.unpack_from('<4I',core)
    if version!=1 or used!=8 or overflow:raise ValueError('Core loader audit not one fresh run with 8 opens')
    rows=[]
    for i in range(8):
        values=struct.unpack_from('<28I',data,72+i*120)
        scenario,expected,origin,invokes=values[:4];opened=values[4:8];echo=values[8:12];count=values[12:16];closed=values[16:20];rpc=values[20:]
        declared_size=struct.unpack_from('<Q',data,72+i*120+112)[0]
        positive=EXPECTED[i]==0
        if (scenario,expected,origin)!=(i,EXPECTED[i],4 if positive else 3) or opened[:3]!=(0,expected,origin):raise ValueError('runtime Open result/origin mismatch')
        if rpc[5]!=0 or rpc[3]!=rpc[4] or rpc[6]!=0:raise ValueError('per-case RPC cleanup failed')
        if positive:
            if invokes!=2 or echo!=(0,0,4,0x7e168ac1) or count!=(0,0,4,1) or closed[:3]!=(0,0,3):raise ValueError('ECHO/fresh instance/Close failed')
        elif invokes!=0:raise ValueError('invalid image reached TA Invoke')
        if i not in (5,6) and (rpc[1]<1 or rpc[2]<1):raise ValueError('TA was not actually fetched by runtime loader')
        if i in (5,6) and (rpc[2]!=0 or rpc[7]<1):raise ValueError('invalid transfer not refused locally')
        at=16+i*32;uuid=core[at:at+16];flags,sig,digest,_=struct.unpack_from('<4I',core,at+16)
        wanted=bytearray(UUID)
        if i==5:wanted[15]^=1
        if uuid!=wanted:raise ValueError('Core loader UUID sequence mismatch')
        if positive and (flags!=127 or sig!=0 or digest!=0):raise ValueError('normal TA did not pass real signature and digest stages')
        if i==1 and (flags!=95 or sig!=0 or digest!=0xffff000f):raise ValueError('tampered payload was not rejected at digest stage')
        if i==3 and (flags!=71 or sig!=0xffff000f or digest!=0xffffffff):raise ValueError('wrong key was not rejected at signature stage')
        if i in (5,6) and (flags!=1 or sig!=0xffffffff or digest!=0xffffffff):raise ValueError('local refusal confused with signature rejection')
        if i==6 and declared_size!=0x40001:raise ValueError('oversize case did not exercise the size bound')
        rows.append(dict(scenario=i,uuid=uuid.hex(),declared_size=declared_size,image_variant=['normal','tampered','normal','wrong-key','normal','absent','oversized','normal'][i],open=dict(zip(['transport','result','origin','value'],opened)),invokes=invokes,rpc_count=rpc[0],queries=rpc[1],copies=rpc[2],allocations=rpc[3],frees=rpc[4],live=rpc[5],core_flags=flags,core_signature=sig,core_digest=digest))
    return dict(test_id='EXT-TA-V03',aliases=['T-16','TEE-04'],uuid=UUID.hex(),images=images,rpc=cumulative,subcases=rows,status='PASS',scope='runtime signature/digest loader via RAM RPC; not Secure Boot or rollback protection')
