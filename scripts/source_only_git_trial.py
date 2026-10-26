import datetime
import json
import os
import subprocess
from pathlib import Path
from offline_build import ROOT, copy_source_workspace


def git(*arguments):
    return subprocess.check_output(['git','-c','core.hooksPath=/dev/null','-c','commit.gpgsign=false',*map(str,arguments)],text=True,stderr=subprocess.STDOUT).strip()


def commit(root):
    git('init',root)
    git('-C',root,'add','.')
    git('-C',root,'-c','user.name=Offline source trial','-c','user.email=offline-trial@example.invalid','commit','-m','Verified source-only development mirror')
    return git('-C',root,'rev-parse','HEAD')


def main():
    stamp=datetime.datetime.now(datetime.timezone.utc).strftime('%Y%m%dT%H%M%S%f')
    work=ROOT/'build'/('git-source-trial-'+stamp)
    source,received=work/'source',work/'received'
    names=['tfa-amd','optee','pyelftools','compiler-rt']
    proof=copy_source_workspace(source,names)
    mirrors={}
    for name in names:
        mirrors[name]=dict(development_mirror_commit=commit(source/'third_party'/name),upstream=proof[name])
    project_commit=commit(source)
    git('clone','--no-hardlinks',source,received)
    for name in names:
        git('clone','--no-hardlinks',source/'third_party'/name,received/'third_party'/name)
    env=os.environ.copy()
    env['GIT_ALLOW_PROTOCOL']='file'
    output=ROOT/'results/phase8'; output.mkdir(parents=True,exist_ok=True)
    record=dict(project_commit=project_commit,mirrors=mirrors,received=str(received),
                source_mode='local Git transport of verified fixed-upstream source subsets; not full upstream checkouts',
                compiler='clang',ubuntu24_vitis='NOT_RUN',hardware='NOT_RUN')
    (output/'source-only-git-trial.json').write_text(json.dumps(record,indent=4)+'\n')
    with (output/'source-only-git-trial.log').open('w') as log:
        result=subprocess.run(['/usr/bin/python3',str(received/'scripts/offline_build.py'),'--configuration','all','--platform','qemu','--compiler','clang','--qemu','--allow-snapshots','--host-tests'],cwd=received,env=env,stdout=log,stderr=subprocess.STDOUT)
    record['exit_code']=result.returncode
    record['status']='PASS' if result.returncode==0 else 'FAIL'
    record['build_receipt']=json.loads((received/'results/phase8/latest.json').read_text()) if (received/'results/phase8/latest.json').exists() else None
    (output/'source-only-git-trial.json').write_text(json.dumps(record,indent=4)+'\n')
    print(output/'source-only-git-trial.json')
    return result.returncode


if __name__=='__main__':
    raise SystemExit(main())
