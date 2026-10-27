import argparse
import datetime
import hashlib
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path
from check_build_environment import inspect
from source_snapshot import verify_source

ROOT = Path(__file__).resolve().parents[1]


def selected_sources(configuration, compiler):
    names = []
    if configuration in ('tfa','optee','all'):
        names += ['tfa-amd']
    if configuration in ('optee','early-ta','all'):
        names += ['optee','pyelftools']
        if compiler == 'clang':
            names += ['compiler-rt']
    return names


def copy_source_workspace(destination, names):
    shutil.copytree(ROOT, destination, symlinks=True,
                    ignore=shutil.ignore_patterns('build','results','third_party','.git','__pycache__','*.pyc','*.elf','*.bin','*.o','*.a','*.ta'))
    (destination/'third_party').mkdir()
    for name in ('dependencies.lock.json','README.md'):
        shutil.copy2(ROOT/'third_party'/name, destination/'third_party'/name)
    lock = json.loads((ROOT/'third_party/dependencies.lock.json').read_text())
    proof = {}
    for name in names:
        source, target = ROOT/'third_party'/name, destination/'third_party'/name
        proof[name] = verify_source(source, lock[name])
        if (source/'.git').exists():
            git_head = subprocess.run(['git','-C',str(source),'rev-parse','HEAD'],text=True,capture_output=True)
            proof[name]['input_git_head'] = git_head.stdout.strip() if git_head.returncode == 0 else None
        manifest = source/'.source_manifest.json'
        if manifest.exists():
            target.mkdir()
            shutil.copy2(manifest, target/manifest.name)
            for item in json.loads(manifest.read_text())['files']:
                origin, output = source/item['path'], target/item['path']
                output.parent.mkdir(parents=True, exist_ok=True)
                if origin.is_symlink():
                    output.symlink_to(os.readlink(origin))
                else:
                    shutil.copy2(origin, output)
        else:
            subprocess.run(['git','clone','--no-hardlinks','--no-checkout',str(source),str(target)],check=True)
            subprocess.run(['git','-C',str(target),'checkout','--detach',lock[name]['commit']],check=True)
        verify_source(target, lock[name])
    return proof


def run_build(args, preflight):
    logdir = ROOT/'results/phase8'
    logdir.mkdir(parents=True, exist_ok=True)
    if any((ROOT/'build'/name).exists() for name in ('qemu','zcu106','external','toolchain-runtime')):
        raise ValueError('Fresh workspace required; do not use --in-place in an existing build tree')
    selected = preflight['selected']
    env = os.environ.copy()
    env.update(GIT_ALLOW_PROTOCOL='file', PYTHONNOUSERSITE='1', PYTHONDONTWRITEBYTECODE='1',
               SOURCE_DATE_EPOCH='1767225600', LC_ALL='C', TZ='UTC')
    for key in ('MAKEFLAGS','MFLAGS','PYTHONPATH','BASH_ENV','ENV','CFG_OPTEE_CONFIG','TA_SIGN_KEY','TA_ENC_KEY','TA_DEV_KIT_DIR','EARLY_TA_PATHS'):
        env.pop(key, None)
    env['GIT_CEILING_DIRECTORIES'] = str(ROOT)
    env['TOOLCHAIN'] = selected['compiler']
    env['CROSS_COMPILE'] = selected['cross_compile']
    if selected['llvm_dir']:
        env['PATH'] = selected['llvm_dir'] + os.pathsep + env.get('PATH','')
        env['LLVM_DIR'] = selected['llvm_dir']
    python_tools = ROOT/'build/offline-python-bin'
    python_tools.mkdir(parents=True,exist_ok=True)
    (python_tools/'python3').symlink_to(Path(sys.executable).resolve())
    env['PATH'] = str(python_tools) + os.pathsep + env.get('PATH','')
    env['PYTHON3'] = sys.executable
    env['QEMU_OBJCOPY'] = selected['tools'].get('llvm-objcopy', selected['tools'].get('objcopy',''))
    (logdir/'environment.json').write_text(json.dumps(preflight,indent=4)+'\n')
    commands = []
    def execute(command, filename):
        commands.append(command)
        with (logdir/filename).open('w') as log:
            log.write('COMMAND: '+repr(command)+'\n'); log.flush()
            completed = subprocess.run(command, cwd=ROOT, env=env, stdout=log, stderr=subprocess.STDOUT)
            log.write(f'\nEXIT_CODE={completed.returncode}\n')
        if completed.returncode:
            raise RuntimeError('Build/test failed: '+filename)
    def make(targets, platform_name=args.platform, filename='build.log'):
        variables = ['TOOLCHAIN='+selected['compiler'],'CROSS_COMPILE='+selected['cross_compile'],
                     'PLATFORM='+platform_name,'PYTHON='+sys.executable+(' -S' if args.configuration=='baremetal' else '')]
        if selected['llvm_dir']:
            variables += ['LLVM_DIR='+selected['llvm_dir'],'CC='+selected['tools']['clang']]
        execute(['make',*variables,*targets],filename)
    def external(component, extra=()):
        execute([sys.executable,'scripts/build_external.py',component,'--compiler',selected['compiler'],
                 '--cross-compile',selected['cross_compile'],'--platform',('qemu' if args.platform=='qemu' else 'zynqmp'),'--log-dir',str(logdir),
                 *(['--llvm-dir',selected['llvm_dir']] if selected['llvm_dir'] else []),*extra], 'invoke-'+component+'-'+str(len(commands))+'.log')
    cfg = args.configuration
    if cfg=='baremetal':
        make(['baremetal'],filename='build-baremetal.log')
    elif cfg=='tfa':
        make(['standalone-tfa'],filename='build-bl33.log')
    elif cfg=='early-ta':
        pass
    elif cfg=='optee':
        make(['standalone-optee'],filename='build-bl33.log')
    else:
        make(['all'],filename='build-qemu.log')
        if args.platform=='qemu':
            make(['all'],platform_name='zcu106',filename='build-zcu106.log')
    if cfg in ('optee','early-ta','all'):
        if selected['compiler']=='clang':
            make(['llvm-runtime'],filename='build-compiler-runtime.log')
        external('optee-sdk')
        sdk = ROOT/'build/external'/('optee' if args.platform=='qemu' else 'optee-zynqmp')/'export-ta_arm64'
        # 外部projectのplatform名はzcu106ではなくzynqmpで指定する。
        for variant in ('a','b'):
            external('ta-'+variant, ['--ta-sdk',str(sdk)])
        if cfg!='early-ta':
            paths = [str(ROOT/'build/external'/('ta-'+v)/(uuid+'.stripped.elf')) for v,uuid in
                     (('a','6dc13418-534f-4642-9138-215876304910'),('b','6dc13419-534f-4642-9138-215876304910'))]
            extra = ['--early-ta',paths[0],'--early-ta',paths[1]]
            if args.platform=='qemu': extra+=['--validation-pta']
            external('optee',extra)
    if cfg in ('tfa','optee','all'):
        if args.platform=='qemu':
            targets = ['build/qemu/external/tfa_bl33.bin'] if cfg=='tfa' else ['qemu-external']
            make(targets,filename='build-qemu-bl33.log')
        if cfg in ('tfa','all'):
            external('tfa',['--configuration','b1'])
        if cfg in ('optee','all'):
            external('tfa',['--configuration','b2'])
            # 接続済み実体とメモリ契約を調べる。実機の未確定事項は許可に変換しない。
            execute([sys.executable,'scripts/check_b2_integration.py','--platform',
                     'qemu' if args.platform=='qemu' else 'zynqmp','--output',str(logdir/'b2-integration.json')],
                    'b2-integration-audit.log')
    if args.host_tests:
        execute([sys.executable,'-S','-m','unittest','discover','-s','tests','-v'],'host-tests.log')
    if args.qemu:
        if cfg in ('baremetal','all'):
            make(['qemu-rom'],filename='build-rom.log')
        configurations = {'baremetal':['baremetal'],'tfa':['tfa'],'optee':['optee'],'all':['baremetal','tfa','optee']}[cfg]
        env['RESULT_DIR'] = 'results/phase8/qemu'
        for configuration in configurations:
            execute(['bash','scripts/run_qemu.sh',configuration],'qemu-'+configuration+'-console.log')
    gate_command = [sys.executable,'scripts/check_hardware_profile.py']
    with (logdir/'load-gate.log').open('w') as log:
        gate = subprocess.run(gate_command,cwd=ROOT,env=env,stdout=log,stderr=subprocess.STDOUT)
    if gate.returncode not in (0,2):
        raise RuntimeError('Hardware safety gate returned an unexpected error')
    status = dict(load_gate='BLOCKED' if gate.returncode==2 else 'READY',load_gate_exit_code=gate.returncode,build_status='PASS',qemu_execution='PASS' if args.qemu else 'NOT_RUN',
                  vitis_ide_build='NOT_RUN', hardware_execution='NOT_RUN',
                  offline_policy='no download/install/acquisition commands; Git remote protocols disabled during build',
                  commands=commands,artifacts={})
    for image in (ROOT/'build').rglob('*.elf'):
        status['artifacts'][str(image.relative_to(ROOT))] = hashlib.sha256(image.read_bytes()).hexdigest()
    (logdir/'build-result.json').write_text(json.dumps(status,indent=4)+'\n')
    return 0


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--configuration',choices=('baremetal','tfa','optee','early-ta','all'),default='baremetal')
    parser.add_argument('--platform',choices=('zcu106','qemu'))
    parser.add_argument('--compiler',choices=('auto','gcc','clang'),default='auto')
    parser.add_argument('--cross-compile')
    parser.add_argument('--llvm-dir')
    parser.add_argument('--allow-snapshots',action='store_true')
    parser.add_argument('--qemu',action='store_true')
    parser.add_argument('--host-tests',action='store_true')
    parser.add_argument('--in-place',action='store_true',help=argparse.SUPPRESS)
    args = parser.parse_args()
    os.environ['GIT_ALLOW_PROTOCOL'] = 'file'
    os.environ['GIT_NO_LAZY_FETCH'] = '1'
    args.platform = args.platform or ('qemu' if args.qemu else 'zcu106')
    if args.qemu and (args.platform!='qemu' or args.configuration=='early-ta'):
        parser.error('QEMU requires qemu platform and runnable configuration')
    # checker/TF-A/OP-TEEはplatform識別子を分けて扱う。
    preflight = inspect(args.configuration,args.compiler,args.cross_compile,args.llvm_dir,args.qemu,args.allow_snapshots)
    output = ROOT/'results/phase8'; output.mkdir(parents=True,exist_ok=True)
    (output/'preflight.json').write_text(json.dumps(preflight,indent=4)+'\n')
    if preflight['status']!='READY':
        print(json.dumps(preflight,indent=2)); return 2
    if args.in_place:
        # external()だけzynqmpを受け取るため、呼出し側で変換する。
        return run_build(args,preflight)
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S%f')
    destination = ROOT/'build'/('offline-'+stamp)
    proof = copy_source_workspace(destination,selected_sources(args.configuration,preflight['selected']['compiler']))
    env = os.environ.copy(); env['GIT_ALLOW_PROTOCOL']='file'
    forwarded = sys.argv[1:]+['--in-place']
    pipeline_log = output/('pipeline-'+stamp+'.log')
    with pipeline_log.open('w') as log:
        result = subprocess.run([sys.executable,*(['-S'] if args.configuration=='baremetal' else []),str(destination/'scripts/offline_build.py'),*forwarded],cwd=destination,env=env,stdout=log,stderr=subprocess.STDOUT)
    exported = output/'runs'/stamp
    shutil.copytree(destination/'results/phase8',exported)
    record = dict(status='PASS' if result.returncode==0 else 'FAIL',exit_code=result.returncode,configuration=args.configuration,platform=args.platform,compiler=preflight['selected']['compiler'],qemu_requested=args.qemu,pipeline_log=str(pipeline_log.relative_to(ROOT)),workspace=str(destination),
                  evidence=str(exported.relative_to(ROOT)),source_provenance=proof,
                  prebuilt_artifacts_copied=False,ubuntu_24_04_validation=preflight['ubuntu_24_04_validation'],
                  vitis_validation='NOT_RUN',hardware_execution='NOT_RUN')
    (output/'latest.json').write_text(json.dumps(record,indent=4)+'\n')
    (output/('latest-'+args.configuration+'-'+args.platform+'.json')).write_text(json.dumps(record,indent=4)+'\n')
    print(json.dumps(record,indent=2))
    return result.returncode


if __name__=='__main__':
    try:
        raise SystemExit(main())
    except (OSError,ValueError,RuntimeError,subprocess.CalledProcessError) as error:
        raise SystemExit('OFFLINE BUILD BLOCKED/FAIL: '+str(error))
