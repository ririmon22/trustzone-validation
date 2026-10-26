import datetime
import json
import hashlib
import os
import shutil
import subprocess
from pathlib import Path
from source_snapshot import verify_source

ROOT = Path(__file__).resolve().parents[1]


def stage_sources(destination):
    shutil.copytree(ROOT,destination,symlinks=True,ignore=shutil.ignore_patterns('build','results','third_party','.git','__pycache__','*.pyc'))
    lock=json.loads((ROOT/'third_party/dependencies.lock.json').read_text())
    (destination/'third_party').mkdir()
    for path in (ROOT/'third_party').iterdir():
        if path.is_file():
            shutil.copy2(path,destination/'third_party'/path.name)
    provenance={}
    for name in ('tfa-amd','optee','compiler-rt','pyelftools'):
        source=ROOT/'third_party'/name
        provenance[name]=verify_source(source,lock[name])
        target=destination/'third_party'/name
        target.mkdir()
        manifest=source/'.source_manifest.json'
        if manifest.exists():
            paths=[r['path'] for r in json.loads(manifest.read_text())['files']]
            shutil.copy2(manifest,target/manifest.name)
        else:
            paths=subprocess.check_output(['git','-C',str(source),'ls-tree','-r','--name-only','HEAD'],text=True).splitlines()
        for relative in paths:
            origin,output=source/relative,target/relative
            output.parent.mkdir(parents=True,exist_ok=True)
            if origin.is_symlink():
                output.symlink_to(os.readlink(origin))
            else:
                shutil.copy2(origin,output)
        if not manifest.exists():
            subprocess.run(['git','init',str(target)],check=True,stdout=subprocess.DEVNULL)
            subprocess.run(['git','-C',str(target),'fetch','--depth','1',str(source),lock[name]['commit']],check=True,stdout=subprocess.DEVNULL)
            subprocess.run(['git','-C',str(target),'reset','--mixed',lock[name]['commit']],check=True,stdout=subprocess.DEVNULL)
            subprocess.run(['git','-C',str(target),'remote','add','origin',lock[name]['repository']],check=True)
    return provenance


def main():
    stamp=datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S%f')
    destination=ROOT/'build'/('phase7-clean-'+stamp)
    output=ROOT/'results/phase7'
    output.mkdir(parents=True,exist_ok=True)
    provenance=stage_sources(destination)
    own_hashes={str(p.relative_to(destination)):hashlib.sha256(p.read_bytes()).hexdigest() for p in destination.rglob('*') if p.is_file() and 'third_party' not in p.relative_to(destination).parts and (p.suffix in ('.c','.h','.S','.ld','.py','.sh','.json') or p.name in ('Makefile','Containerfile'))}
    record=dict(own_source_sha256=own_hashes,workspace=str(destination),cached_build_artifacts_copied=False,sources=provenance,
                environment='fresh source/build workspace on existing host; not a verified container')
    (output/'clean-workspace.json').write_text(json.dumps(record,indent=4)+'\n')
    env=os.environ.copy()
    env.update(SOURCE_DATE_EPOCH='1767225600',PYTHONHASHSEED='0',TZ='UTC',LC_ALL='C')
    with (output/'clean-pipeline.log').open('w') as log:
        result=subprocess.run(['bash','scripts/run_phase7_local.sh'],cwd=destination,env=env,stdout=log,stderr=subprocess.STDOUT)
    generated=destination/'results/phase7'
    if generated.exists():
        target=output/'clean'
        if target.exists():
            target=output/('clean-'+stamp)
        shutil.copytree(generated,target)
        prefix=str(target.relative_to(ROOT))
        def rebase(value):
            if isinstance(value,str) and value.startswith('results/phase7/'):
                return prefix+'/'+value[len('results/phase7/'):]
            if isinstance(value,list): return [rebase(item) for item in value]
            if isinstance(value,dict): return {key:rebase(item) for key,item in value.items()}
            return value
        for path in target.glob('*.json'):
            path.write_text(json.dumps(rebase(json.loads(path.read_text())),indent=4)+'\n')
        report=target/'report.md'
        if report.exists():
            content=report.read_text().replace('results/phase7/',prefix+'/')
            report.write_text(content)
            (output/'latest-report.md').write_text(content)
        record['evidence']=prefix
    record['exit_code']=result.returncode
    record['status']=('BUILD_PASS_EXECUTION_NOT_RUN' if env.get('TZ_BUILD_ONLY')=='1' else 'PASS') if result.returncode==0 else 'FAIL'
    (output/'clean-workspace.json').write_text(json.dumps(record,indent=4)+'\n')
    print(json.dumps(record,indent=2))
    return result.returncode


if __name__=='__main__':
    raise SystemExit(main())
