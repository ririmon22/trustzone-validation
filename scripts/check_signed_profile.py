import argparse
import hashlib
import json
import subprocess
import sys
from pathlib import Path
from elf_symbols import elf_symbols,symbol_payload,read_config
ROOT=Path(__file__).resolve().parents[1]
UUID='6dc1341c-534f-4642-9138-215876304910'

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
    manifest=json.loads((bundle/'manifest.json').read_text())
    if manifest['trusted_public_sha256']!=hashlib.sha256(public.read_bytes()).hexdigest():raise ValueError('public key manifest mismatch')
    results=[]
    for image,public_name,expected in [('normal.ta','trusted.pub.pem',0),('tampered.ta','trusted.pub.pem',1),('wrong-key.ta','trusted.pub.pem',1),('wrong-key.ta','wrong.pub.pem',0)]:
        command=[sys.executable,str(optee/'scripts/sign_encrypt.py'),'verify','--uuid',UUID,'--key',str(keys/public_name),'--in',str(bundle/image)]
        run=subprocess.run(command,capture_output=True,text=True)
        if (run.returncode==0)!=(expected==0):raise ValueError('official host verification unexpected: '+image+'/'+public_name)
        results.append(dict(image=image,key=public_name,exit=run.returncode))
    return dict(static_status='PASS',uuid=UUID,external_ta_not_early=True,core_public_key_matches=True,official_host_checks=results,scope='host checks do not replace runtime loader execution',hardware_execution='NOT_RUN')

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--platform',choices=['zynqmp'],required=True);p.add_argument('--core',type=Path,required=True);p.add_argument('--conf',type=Path,required=True);p.add_argument('--bundle',type=Path,required=True);p.add_argument('--keys',type=Path,required=True);p.add_argument('--optee',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();d=check(a.platform,a.core,a.conf,a.bundle,a.keys,a.optee);a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(d,indent=4)+'\n');print('SIGNED PROFILE STATIC PASS')
