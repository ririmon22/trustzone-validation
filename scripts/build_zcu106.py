import argparse
import datetime
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path
from check_build_environment import inspect as inspect_environment
from build_workspace import copy_sources
from zip_sources import inspect_zip_source, tree_manifest, input_fingerprint

ROOT = Path(__file__).resolve().parents[1]
UUIDS = ('6dc13418-534f-4642-9138-215876304910', '6dc13419-534f-4642-9138-215876304910')


def preflight(config, config_dir):
    if config.get('source_mode') != 'zip':
        raise ValueError('This entry requires explicit source_mode=zip; locked source verification is available in source_snapshot.py')
    default_index=17 if config.get('review_suite',False) else 16 if config.get('signed_ta',False) else 14
    if config.get('review_suite',False) and not config.get('signed_ta',False):raise ValueError('review suite requires signed_ta=true')
    if config.get('b2_test_index',default_index) != default_index:
        raise ValueError('Use select_b2_test.py for all/single functional selection')
    if config.get('platform','zcu106')!='zcu106' or config.get('qemu',False) or config.get('host_tests',False) or config.get('secure_sgi',False):
        raise ValueError('This entry is ZCU106-only; host/QEMU/SGI development profiles are not installed')
    run_id=int(str(config.get('run_id',1)),0)
    if not 0 < run_id < (1 << 64):raise ValueError('run_id must be a nonzero u64 integer')
    if config.get('physical_dedicated',False) and (config.get('platform','zcu106')!='zcu106' or not config.get('physical_probe',False)):
        raise ValueError('dedicated physical map requires ZCU physical_probe profile')
    platform = config.get('platform', 'zcu106')
    compiler = config.get('compiler','gcc')
    if compiler not in ('gcc','clang','auto'):
        raise ValueError('Invalid compiler')
    sources = {}
    records = {}
    for name, record in config.get('sources', {}).items():
        if name not in ('tfa-amd','optee','pyelftools','compiler-rt'):
            raise ValueError('Unknown ZIP component: ' + name)
        path = (config_dir / record['directory']).resolve()
        archive = (config_dir / record['archive']).resolve() if record.get('archive') else None
        records[name] = inspect_zip_source(path, name, record.get('declared_version'), archive)
        sources[name] = path
    environment = inspect_environment('optee', compiler, config.get('cross_compile'), config.get('llvm_dir'), False, False, source_mode='zip', source_directories=sources)
    needed = ['tfa-amd','optee','pyelftools'] + (['compiler-rt'] if environment['selected']['compiler']=='clang' else [])
    for name in needed:
        if name not in records:
            environment['issues'].append('Missing ZIP source directory configuration: ' + name)
    if environment['issues']:
        environment['status'] = 'BLOCKED'
    return environment, {name:records[name] for name in needed if name in records}


def run(config, environment, records, output):
    workspace = output/'workspace'
    source_hashes=copy_sources(ROOT,workspace)
    (output/'source-hashes.json').write_text(json.dumps(source_hashes,indent=4)+'\n')
    for name,record in records.items():
        source = Path(record['directory'])
        if workspace.resolve().is_relative_to(source):
            raise ValueError('Workspace may not be inside source input')
        destination = workspace/'third_party'/name
        shutil.copytree(source,destination,symlinks=True,ignore=shutil.ignore_patterns('.git','__pycache__','*.pyc','.source_manifest.json','.zip_input.json'))
        if tree_manifest(destination)['tree_sha256'] != record['tree_sha256']:
            raise ValueError('Source copy mismatch: ' + name)
        (destination/'.zip_input.json').write_text(json.dumps(record,indent=4)+'\n')
    env = os.environ.copy()
    env.update(PYTHONDONTWRITEBYTECODE='1', PYTHONNOUSERSITE='1', GIT_ALLOW_PROTOCOL='', GIT_NO_LAZY_FETCH='1', GIT_CEILING_DIRECTORIES=str(workspace), LC_ALL='C', TZ='UTC', SOURCE_DATE_EPOCH='1767225600')
    for key in ('MAKEFLAGS','MFLAGS','PYTHONPATH','BASH_ENV','ENV','CFG_OPTEE_CONFIG','TA_SIGN_KEY','TA_ENC_KEY','TA_DEV_KIT_DIR','EARLY_TA_PATHS'):
        env.pop(key,None)
    selected = environment['selected']
    if selected['llvm_dir']:
        env['PATH'] = selected['llvm_dir'] + os.pathsep + env.get('PATH','')
    python_bin=workspace/'build/python-bin';python_bin.mkdir(parents=True)
    (python_bin/'python3').symlink_to(Path(sys.executable).resolve())
    env['PATH']=str(python_bin)+os.pathsep+env.get('PATH','')
    env['PYTHON3']=sys.executable
    logs=workspace/'results/build';logs.mkdir(parents=True)
    commands=[]

    def execute(command, filename, allowed=(0,), host_fixture=False):
        commands.append(command)
        with (logs/filename).open('w') as stream:
            stream.write('COMMAND: '+repr(command)+'\n');stream.flush()
            child_env=env.copy()
            if host_fixture:child_env['GIT_ALLOW_PROTOCOL']='file'
            result=subprocess.run(command,cwd=workspace,env=child_env,stdout=stream,stderr=subprocess.STDOUT)
            stream.write('\nEXIT_CODE='+str(result.returncode)+'\n')
        if result.returncode not in allowed:
            raise RuntimeError('Build/check failed: '+str(logs/filename))
        return result.returncode

    def make(targets,filename):
        variables=['PLATFORM='+config.get('platform','zcu106'),'TOOLCHAIN='+selected['compiler'],'CROSS_COMPILE='+selected['cross_compile'],'SOURCE_MODE=zip','B2_TEST_INDEX='+str(config.get('b2_test_index',17 if config.get('review_suite',False) else 16 if config.get('signed_ta',False) else 14)),'B2_PHYSICAL_DEDICATED='+str(int(bool(config.get('physical_dedicated',False)))),'B2_VALIDATION_PTA='+str(int(bool(config.get('validation_pta',False)))),'B2_SECURE_SGI='+str(int(bool(config.get('secure_sgi',False)))),'B2_SIGNED_TA_PROFILE='+str(1 if config.get('signed_ta',False) else 0),'DEBUG_RUN_ID='+str(int(str(config.get('run_id',1)),0)),'PYTHON='+sys.executable]
        if selected['llvm_dir']:
            variables+=['LLVM_DIR='+selected['llvm_dir'],'CC='+selected['tools']['clang']]
        execute(['make',*variables,*targets],filename)

    def external(component,extra=()):
        command=[sys.executable,'scripts/build_external.py',component,'--source-mode','zip','--compiler',selected['compiler'],'--cross-compile',selected['cross_compile'],'--platform','zynqmp','--log-dir',str(logs)]
        if config.get('signed_ta',False):
            if component in ('optee','optee-sdk'):command+=['--signed-ta-profile','--ta-public-key',str(workspace/'build/signed-test-keys/trusted.pub.pem')]
            if component.startswith('ta-'):command+=['--ta-sign-key',str(workspace/'build/signed-test-keys/trusted.pem')]
        if selected['llvm_dir']:command+=['--llvm-dir',selected['llvm_dir']]
        execute(command+list(extra),'invoke-'+component+'.log')

    result=dict(run_id=int(str(config.get('run_id',1)),0),source_provenance='ZIP_UNVERIFIED',verified_commit=None,build_status='NOT_RUN',static_status='NOT_RUN',hardware_execution='NOT_RUN',hardware_load='BLOCKED',workspace=str(workspace))
    try:
        if config.get('signed_ta',False):
            from make_signed_bundle import generate_keys
            generate_keys(workspace/'build/signed-test-keys')
        if selected['compiler']=='clang':make(['llvm-runtime'],'build-runtime.log')
        external('optee-sdk')
        sdk=workspace/'build/external'/'optee-zynqmp'/'export-ta_arm64'
        for variant in ('a','b'):external('ta-'+variant,['--ta-sdk',str(sdk)])
        if config.get('signed_ta',False):
            external('ta-external',['--ta-sdk',str(sdk)])
            from make_signed_bundle import build_bundle
            build_bundle(workspace)
        early=[]
        for variant,uuid in zip(('a','b'),UUIDS):early+=['--early-ta',str(workspace/'build/external'/('ta-'+variant)/(uuid+'.stripped.elf'))]
        if config.get('validation_pta',False):early+=['--validation-pta']
        if config.get('physical_probe',False):early+=['--physical-pta']
        if config.get('physical_dedicated',False):early+=['--physical-dedicated']
        if config.get('physical_dedicated',False):
            execute([sys.executable,'scripts/check_b2_physical_reservation.py','--output',str(logs/'physical-reservation.json')],'physical-reservation.log')
        external('optee',early)
        external('tfa',['--configuration','b2'])
        make(['standalone-optee'],'build-bl33.log')
        result['build_status']='PASS'
        execute([sys.executable,'scripts/check_b2_integration.py','--platform','zynqmp','--output',str(logs/'b2-integration.json')],'integration.log')
        audit=json.loads((logs/'b2-integration.json').read_text())
        if audit.get('software_contract_status')!='PASS':raise RuntimeError('B2 software contract failed')
        if config.get('signed_ta',False):
            execute([sys.executable,'scripts/check_signed_profile.py','--platform','zynqmp','--output',str(logs/'signed-profile.json')],'signed-profile.log')
            execute([sys.executable,'scripts/check_signed_layout.py','--platform',config.get('platform','zcu106'),'--elf',str(workspace/'build/zcu106/optee_bl33.elf'),'--output',str(logs/'signed-layout.json')],'signed-layout.log')
        execute([sys.executable,'scripts/check_b2_review_profile.py','--root',str(workspace),'--platform',config.get('platform','zcu106'),'--elf',str(workspace/'build/zcu106/optee_bl33.elf'),'--output',str(logs/'review-profile.json')],'review-profile.log')
        result['static_status']='PASS'
        result['artifacts']=audit['images']
        execute([sys.executable,'scripts/check_hardware_profile.py'],'hardware-gate.log',allowed=(2,))
    except (OSError,ValueError,RuntimeError) as error:
        result['error']=str(error)
        if result['build_status']=='NOT_RUN':result['build_status']='FAIL'
    finally:
        unchanged={name:input_fingerprint(Path(record['directory']))==record['input_state_sha256'] for name,record in records.items()}
        result['original_inputs_unchanged']=unchanged
        result['commands']=commands
        if not all(unchanged.values()):result['error']='Original source input changed during build'
        (output/'result.json').write_text(json.dumps(result,indent=4)+'\n')
        if 'error' not in result:
            (ROOT/'results/zcu106-build'/('latest-'+config.get('platform','zcu106')+'.json')).write_text(json.dumps(result,indent=4)+'\n')
    print(json.dumps(result,indent=2))
    return 1 if 'error' in result else 0


if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('--config',type=Path,required=True)
    parser.add_argument('--check',action='store_true')
    args=parser.parse_args()
    output=ROOT/'results/zcu106-build'/datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S%f')
    output.mkdir(parents=True)
    try:
        config=json.loads(args.config.read_text())
        environment,records=preflight(config,args.config.resolve().parent)
        (output/'preflight.json').write_text(json.dumps(environment,indent=4)+'\n')
        (output/'source-inputs.json').write_text(json.dumps(records,indent=4)+'\n')
        print(output,flush=True)
        if environment['status']!='READY':
            print(json.dumps(environment,indent=2));raise SystemExit(2)
        if args.check:
            print('DEPENDENCIES READY; ZIP PROVENANCE UNVERIFIED; HARDWARE BLOCKED');raise SystemExit(0)
        raise SystemExit(run(config,environment,records,output))
    except (OSError,ValueError,KeyError,TypeError,subprocess.CalledProcessError) as error:
        (output/'blocked.json').write_text(json.dumps(dict(status='BLOCKED',reason=str(error),source_provenance='ZIP_UNVERIFIED',hardware_execution='NOT_RUN'),indent=4)+'\n')
        parser.exit(2,'ZIP BUILD BLOCKED: '+str(error)+'\n')
