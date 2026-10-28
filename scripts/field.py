import argparse
import datetime
import hashlib
import json
import subprocess
import sys
from pathlib import Path
from check_build_environment import inspect as environment

ROOT=Path(__file__).resolve().parents[1]


def main():
    parser=argparse.ArgumentParser(description='Local build/check/plan/decode only; hardware operations are separate gated Tcl procedures')
    parser.add_argument('action',choices=('a-export','a-build','a-check','b2-check','b2-build','b2-plan','decode'))
    parser.add_argument('config',type=Path)
    parser.add_argument('--raw',type=Path)
    args=parser.parse_args()
    config=json.loads(args.config.read_text());base=args.config.resolve().parent
    out=ROOT/'results/field'/datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S%f');out.mkdir(parents=True)
    print('PC log directory:',out,flush=True)
    def execute(command,allowed=(0,)):
        with (out/'commands.log').open('a') as log:
            log.write('COMMAND: '+repr(command)+'\n');log.flush()
            result=subprocess.run(command,cwd=ROOT,stdout=log,stderr=subprocess.STDOUT)
            log.write('EXIT='+str(result.returncode)+'\n')
        if result.returncode:
            print('Nonzero status',result.returncode,'; details:',out/'commands.log')
            print('\n'.join((out/'commands.log').read_text().splitlines()[-15:]))
        if result.returncode not in allowed:raise ValueError('Command failed; see '+str(out/'commands.log'))
        return result.returncode
    if args.action.startswith('b2-') and args.action!='b2-plan':
        code=execute([sys.executable,'scripts/build_zip_b2.py','--config',str(args.config.resolve()),*(['--check'] if args.action=='b2-check' else [])],allowed=(0,2))
        if code==0:
            print('DEPENDENCIES READY; SOURCE ZIP_UNVERIFIED; HARDWARE BLOCKED' if args.action=='b2-check' else 'BUILD PASS; STATIC PASS; SOURCE ZIP_UNVERIFIED; HARDWARE BLOCKED')
        return code
    if args.action=='b2-plan':
        if config.get('platform','zcu106')!='zcu106':raise ValueError('B2 JTAG planning is ZCU106-only; do not use QEMU addresses')
        input_path=(base/config['b2_inputs']).resolve() if config.get('b2_inputs') else ROOT/'platform/zcu106/jtag_b2_inputs.json'
        inputs=json.loads(input_path.read_text())
        latest=ROOT/'results/zip-b2'/('latest-'+config.get('platform','zcu106')+'.json')
        bl33=None
        if latest.is_file():
            receipt=json.loads(latest.read_text())
            for role,image in receipt.get('artifacts',{}).items():
                path=Path(image['path'])
                if hashlib.sha256(path.read_bytes()).hexdigest()!=image['sha256']:raise ValueError('Built artifact hash changed: '+role)
                inputs['images'][role].update(path=str(path),sha256=image['sha256'],expected_entry=image['entry'])
                if role=='bl33':bl33=path
            inputs['artifact_binding']='STATIC_BUILD_OUTPUT_ONLY_NOT_HARDWARE_CONFIRMED'
        if config.get('b2_elf'):bl33=(base/config['b2_elf']).resolve()
        staged=out/'b2-inputs.json';staged.write_text(json.dumps(inputs,indent=4)+'\n')
        code=execute([sys.executable,'scripts/prepare_b2_jtag.py','--inputs',str(staged),'--output',str(out/'b2-plan.json'),'--emit-tcl',str(out/'b2-plan.tcl')],allowed=(2,))
        if bl33:
            capture=out/'b2-capture.tcl'
            execute([sys.executable,'scripts/prepare_jtag_capture.py',str(bl33),'--output',str(capture)])
            with (out/'b2-plan.tcl').open('a') as stream:stream.write(capture.read_text())
        else:print('BL33 capture plan unavailable: build B2 or set b2_elf in local config')
        return code
    if args.action=='decode':
        if args.raw is None or not args.raw.is_dir():raise ValueError('--raw must name the PC-side dump directory')
        statuses={}
        for path in sorted(args.raw.glob('*.bin')):
            if path.stem.endswith('debug_ram_log'):
                command=[sys.executable,'scripts/decode_ram_log.py',str(path.resolve()),'--output',str(out/(path.stem+'.txt'))]
                if config.get('run_id'):command+=['--expected-run-id',str(config['run_id'])]
            elif path.stem.endswith(('debug_exception','debug_sync_exception')):
                command=[sys.executable,'scripts/decode_exception.py',str(path.resolve())]
            else:continue
            result=subprocess.run(command,cwd=ROOT,capture_output=True,text=True)
            (out/(path.stem+'.decode.txt')).write_text(result.stdout+result.stderr)
            statuses[path.name]=result.returncode
        if not statuses:raise ValueError('No named RAM-log/exception dumps found')
        (out/'decode-status.json').write_text(json.dumps(statuses,indent=4)+'\n')
        return 0 if all(value==0 for value in statuses.values()) else 2
    export=(base/config['a_export']).resolve()
    if args.action=='a-export':
        code=execute([sys.executable,'scripts/export_a_vitis.py','--output',str(export)])
        print('A source export:',export)
        return code
    if args.action=='a-build':
        checked=environment('baremetal',config.get('compiler','gcc'),config.get('cross_compile'),config.get('llvm_dir'))
        (out/'environment.json').write_text(json.dumps(checked,indent=4)+'\n')
        if checked['status']!='READY':raise ValueError('Missing compiler/tool: '+ '; '.join(checked['issues']))
        selected=checked['selected']
        execute(['cmake','-S',str(export/'src'),'-B',str(export/'cmake-build'),'-DTZ_COMPILER='+selected['compiler'],'-DTZ_CROSS_COMPILE='+selected['cross_compile'],'-DTZ_LLVM_DIR='+(selected['llvm_dir'] or '/usr/bin'),'-DTZ_RUN_ID='+str(config.get('run_id',1))])
        execute(['cmake','--build',str(export/'cmake-build'),'--verbose'])
    elf=export/'src/build/zcu106/unified/a_unified.elf'
    code=execute([sys.executable,'scripts/prepare_a_unified_jtag.py','--elf',str(elf),'--output',str(out/'a-plan.json'),'--emit-tcl',str(out/'a-plan.tcl')],allowed=(2,))
    (out/'result.json').write_text(json.dumps(dict(build_status='PASS' if args.action=='a-build' else 'NOT_RUN',static_status='PASS',hardware_load='BLOCKED',hardware_execution='NOT_RUN',plan=str(out/'a-plan.tcl')),indent=4)+'\n')
    print('A STATIC PASS; HARDWARE BLOCKED; plan:',out/'a-plan.tcl')
    return code


if __name__=='__main__':
    try:raise SystemExit(main())
    except (OSError,ValueError,KeyError,TypeError) as error:raise SystemExit('FIELD BLOCKED: '+str(error))
