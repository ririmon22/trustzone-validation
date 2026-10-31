import argparse
import datetime
import json
import shutil
import hashlib
import sys
import subprocess
from pathlib import Path
from decode_b2_results import IDS
from decode_b2_review import PARENTS
from build_workspace import copy_sources
ROOT = Path(__file__).resolve().parents[1]

if __name__ == '__main__':
    parser=argparse.ArgumentParser(description='Build only own BL33 in a new workspace; never load hardware or change ready flags')
    parser.add_argument('--config',type=Path,required=True)
    parser.add_argument('--test',choices=['all','legacy','review','performance','EXT-TA-V03']+IDS+PARENTS,default='all')
    args=parser.parse_args()
    config=json.loads(args.config.read_text())
    if config.get('platform','zcu106') != 'zcu106' or config.get('compiler','gcc') not in ('gcc','clang'):
        parser.error('platform must be zcu106/qemu and compiler gcc/clang')
    if config.get('physical_dedicated',False) and (config.get('platform')!='zcu106' or not config.get('physical_probe',False)):
        parser.error('dedicated physical region requires ZCU physical_probe profile')
    if config.get('secure_sgi',False) and config.get('platform','zcu106')!='qemu':
        parser.error('Secure SGI profile is QEMU-only pending platform review')
    run_id=int(str(config.get('run_id',1)),0)
    if not 0 < run_id < (1 << 64):parser.error('run_id must be a nonzero u64')
    parent=PARENTS.index(args.test)+1 if args.test in PARENTS and args.test!='EXT-TA-V03' else 0
    index=17 if parent else 17 if args.test=='review' else 18 if args.test=='performance' else 0 if args.test=='all' else 14 if args.test=='legacy' else 16 if args.test in ('EXT-TA-V03','TEE-04') else 15 if args.test=='TZ-SYS-03' else IDS.index(args.test)+1
    out=ROOT/'results/b2-functional'/datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S%f')
    workspace=out/'workspace'
    out.mkdir(parents=True)
    sources=copy_sources(ROOT,workspace)
    (out/'source-hashes.json').write_text(json.dumps(sources,indent=4)+'\n')
    command=['make','PLATFORM='+config.get('platform','zcu106'),'TOOLCHAIN='+config.get('compiler','gcc'),
             'CROSS_COMPILE='+config.get('cross_compile','aarch64-none-elf-'),'B2_TEST_INDEX='+str(index),'B2_PHYSICAL_DEDICATED='+str(int(bool(config.get('physical_dedicated',False)))),'B2_VALIDATION_PTA='+str(int(bool(config.get('validation_pta',False)))),'B2_SECURE_SGI='+str(int(bool(config.get('secure_sgi',False)))),'B2_REVIEW_PARENT='+str(parent),'B2_SIGNED_TA_PROFILE='+str(1 if index in (16,17) or config.get('signed_ta',False) else 0),
             'DEBUG_RUN_ID='+str(int(str(config.get('run_id',1)),0)),
             'standalone-optee']
    if config.get('llvm_dir'):command.append('LLVM_DIR='+config['llvm_dir'])
    print(out,flush=True)
    with (out/'build.log').open('w') as log:
        log.write('COMMAND: '+repr(command)+'\n');log.flush()
        code=subprocess.run(command,cwd=workspace,stdout=log,stderr=subprocess.STDOUT).returncode
    elf=workspace/'build/zcu106/optee_bl33.elf'
    if code==0:
        subprocess.run([sys.executable,str(workspace/'scripts/prepare_jtag_capture.py'),str(elf),'--output',str(out/'capture.tcl')],check=True)
    elf_hash=hashlib.sha256(elf.read_bytes()).hexdigest() if elf.is_file() else None
    if code==0 and index==16:
        subprocess.run([sys.executable,str(workspace/'scripts/check_signed_layout.py'),'--platform',config.get('platform','zcu106'),'--elf',str(elf),'--output',str(out/'signed-layout.json')],check=True)
    (out/'build-summary.json').write_text(json.dumps(dict(build='PASS' if code==0 else 'FAIL',test=args.test,index=index,elf=str(elf),elf_sha256=elf_hash,run_id=run_id,hardware='NOT_RUN',load_gate='BLOCKED'),indent=4)+'\n')
    raise SystemExit(code)
