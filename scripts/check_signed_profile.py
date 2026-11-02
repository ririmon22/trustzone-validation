import argparse
import hashlib
import json
import subprocess
import sys
import struct
import uuid
import io
from pathlib import Path
from elf_symbols import elf_symbols,symbol_payload,read_config
ROOT=Path(__file__).resolve().parents[1]
UUID='6dc1341c-534f-4642-9138-215876304910'

def check_bundle(directory):
    """RAMへ置くbundleとPC検証する3つのTAが同一であることを確かめる。
    署名の正否はここで判断せず、下の公式sign_encrypt.pyで別途検証する。
    """
    data = (directory/'bundle.bin').read_bytes()
    if len(data) != 0x100000 or struct.unpack_from('<4I',data) != (0x5441424e,1,0x100000,3):
        raise ValueError('bundle header/size mismatch')
    if data[16:32] != uuid.UUID(UUID).bytes:
        raise ValueError('bundle UUID mismatch')
    manifest = json.loads((directory/'manifest.json').read_text())
    if manifest.get('uuid') != UUID or len(manifest.get('images', [])) != 3:
        raise ValueError('bundle manifest UUID/image count mismatch')
    if manifest['bundle_sha256'] != hashlib.sha256(data).hexdigest():
        raise ValueError('bundle manifest hash mismatch')
    for index,name in enumerate(('normal','tampered','wrong-key')):
        offset,size,digest = struct.unpack_from('<QQ32s',data,32+index*48)
        image = (directory/(name+'.ta')).read_bytes()
        if offset != 4096+index*0x40000 or not 0 < size <= 0x40000 or offset+size > len(data):
            raise ValueError('bundle slot bounds mismatch: '+name)
        if size != len(image) or data[offset:offset+size] != image or digest != hashlib.sha256(image).digest():
            raise ValueError('bundle slot differs from verified TA: '+name)
        record = manifest['images'][index]
        if record != dict(variant=name,offset=offset,size=size,sha256=digest.hex()):
            raise ValueError('bundle slot manifest mismatch: '+name)
    return dict(path=str((directory/'bundle.bin').resolve()),sha256=manifest['bundle_sha256'],bytes=len(data))


def check(platform,core,conf,bundle,keys,optee):
    if platform!='zynqmp':raise ValueError('ZynqMP-only signed profile checker')
    from cryptography.hazmat.primitives.serialization import load_pem_public_key
    config=read_config(conf)
    for key,value in {'CFG_REE_FS_TA':'y','CFG_REE_FS_TA_BUFFERED':'n','CFG_PREALLOC_RPC_CACHE':'n','CFG_REE_FS':'n','CFG_RPMB_FS':'n','CFG_TZ_SIGNED_AUDIT_PTA':'y'}.items():
        if config.get(key)!=value:raise ValueError('signed profile config mismatch: '+key)
    parsed=elf_symbols(core)
    if 'ts_bin_'+UUID.replace('-','') in parsed[2]:raise ValueError('external TA incorrectly embedded as Early TA')
    public=keys/'trusted.pub.pem'
    key=load_pem_public_key(public.read_bytes()).public_numbers()
    if symbol_payload(parsed,'ta_pub_key_modulus')!=key.n.to_bytes(256,'big'):raise ValueError('Core public key differs from signing profile')
    if parsed[2].get('signed_loader_audit',{}).get('size')!=1040:raise ValueError('loader stage audit missing')
    bundle_record=check_bundle(bundle)
    # 正式署名で守られたbootstrap UUIDだけでなく、実行するELFのTAヘッダUUIDも一致させる。
    from elftools.elf.elffile import ELFFile
    normal=(bundle/'normal.ta').read_bytes()
    magic,kind,_,_,hash_size,signature_size=struct.unpack_from('<4I2H',normal)
    if magic!=0x4f545348 or kind!=1:
        raise ValueError('unsupported signed TA format')
    offset=20+hash_size+signature_size
    if normal[offset:offset+16]!=uuid.UUID(UUID).bytes:
        raise ValueError('signed bootstrap UUID mismatch')
    ta=ELFFile(io.BytesIO(normal[offset+20:]))
    head=ta.get_section_by_name('.ta_head')
    if head is None or head['sh_size']!=32 or str(uuid.UUID(bytes_le=head.data()[:16]))!=UUID:
        raise ValueError('signed TA ELF header UUID mismatch')
    manifest=json.loads((bundle/'manifest.json').read_text())
    if manifest['trusted_public_sha256']!=hashlib.sha256(public.read_bytes()).hexdigest():raise ValueError('public key manifest mismatch')
    results=[]
    for image,public_name,expected in [('normal.ta','trusted.pub.pem',0),('tampered.ta','trusted.pub.pem',1),('wrong-key.ta','trusted.pub.pem',1),('wrong-key.ta','wrong.pub.pem',0)]:
        command=[sys.executable,str(optee/'scripts/sign_encrypt.py'),'verify','--uuid',UUID,'--key',str(keys/public_name),'--in',str(bundle/image)]
        run=subprocess.run(command,capture_output=True,text=True)
        if (run.returncode==0)!=(expected==0):raise ValueError('official host verification unexpected: '+image+'/'+public_name)
        results.append(dict(image=image,key=public_name,exit=run.returncode))
    return dict(static_status='PASS',uuid=UUID,external_ta_not_early=True,core_public_key_matches=True,official_host_checks=results,bundle=bundle_record,scope='host checks do not replace runtime loader execution',hardware_execution='NOT_RUN')

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--platform',choices=['zynqmp'],required=True);p.add_argument('--core',type=Path,required=True);p.add_argument('--conf',type=Path,required=True);p.add_argument('--bundle',type=Path,required=True);p.add_argument('--keys',type=Path,required=True);p.add_argument('--optee',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();d=check(a.platform,a.core,a.conf,a.bundle,a.keys,a.optee);a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(d,indent=4)+'\n');print('SIGNED PROFILE STATIC PASS')
