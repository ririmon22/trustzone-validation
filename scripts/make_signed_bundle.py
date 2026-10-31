import hashlib
import json
import struct
import subprocess
import sys
from pathlib import Path
UUID='6dc1341c-534f-4642-9138-215876304910'
BUNDLE_BYTES=0x100000
MAX_BYTES=0x40000

def generate_keys(directory):
    from cryptography.hazmat.primitives.asymmetric import rsa
    from cryptography.hazmat.primitives import serialization
    directory.mkdir(parents=True)
    for name in ['trusted','wrong']:
        key=rsa.generate_private_key(public_exponent=65537,key_size=2048)
        private=directory/(name+'.pem')
        private.write_bytes(key.private_bytes(serialization.Encoding.PEM,serialization.PrivateFormat.PKCS8,serialization.NoEncryption()));private.chmod(0o600)
        (directory/(name+'.pub.pem')).write_bytes(key.public_key().public_bytes(serialization.Encoding.PEM,serialization.PublicFormat.SubjectPublicKeyInfo))


def build_bundle(workspace):
    import uuid
    base=workspace/'build/external/ta-external';normal=base/(UUID+'.ta');elf=base/(UUID+'.stripped.elf')
    keys=workspace/'build/signed-test-keys';out=workspace/'build/external/signed';out.mkdir()
    command=[sys.executable,str(workspace/'third_party/optee/scripts/sign_encrypt.py'),'sign-enc','--uuid',UUID,'--key',str(keys/'wrong.pem'),'--in',str(elf),'--out',str(out/'wrong.ta')]
    subprocess.run(command,check=True)
    good=normal.read_bytes();header=struct.unpack_from('<4I2H',good)
    if header[0]!=0x4f545348 or header[1]!=1 or header[4]!=32 or len(good)>MAX_BYTES:raise ValueError('Unsupported signed TA format/size')
    signed_header=20+header[4]+header[5]
    if good[signed_header:signed_header+16]!=uuid.UUID(UUID).bytes:raise ValueError('Bootstrap UUID mismatch')
    altered=bytearray(good);altered[signed_header+16]^=1
    images=[good,bytes(altered),(out/'wrong.ta').read_bytes()]
    if struct.unpack_from('<4I2H',images[2])!=header or images[0][signed_header:]!=images[2][signed_header:]:raise ValueError('Wrong-key TA changed payload or format')
    bundle=bytearray(BUNDLE_BYTES);struct.pack_into('<4I',bundle,0,0x5441424e,1,BUNDLE_BYTES,3);bundle[16:32]=uuid.UUID(UUID).bytes
    records=[]
    for index,(name,image) in enumerate(zip(['normal','tampered','wrong-key'],images)):
        offset=4096+index*MAX_BYTES
        if len(image)>MAX_BYTES:raise ValueError('TA exceeds reserved slot')
        digest=hashlib.sha256(image).digest();struct.pack_into('<QQ32s',bundle,32+index*48,offset,len(image),digest);bundle[offset:offset+len(image)]=image
        (out/(name+'.ta')).write_bytes(image);records.append(dict(variant=name,offset=offset,size=len(image),sha256=digest.hex()))
    (out/'bundle.bin').write_bytes(bundle)
    report=dict(test_id='EXT-TA-V03',alias='T-16 / TEE-04',uuid=UUID,images=records,bundle_sha256=hashlib.sha256(bundle).hexdigest(),tamper='bootstrap ta_version byte changed; UUID/ELF/signed header lengths unchanged',trusted_public_sha256=hashlib.sha256((keys/'trusted.pub.pem').read_bytes()).hexdigest(),private_keys='generated test only; never transfer/commit')
    (out/'manifest.json').write_text(json.dumps(report,indent=4)+'\n')
    return out
