import argparse
import hashlib
import json
import shutil
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
COMMANDS = {'cc':['clang','--version'], 'assembler':['clang','--version'], 'linker':['ld.lld','--version'],
            'llvm-ar':['/usr/lib/llvm-21/bin/llvm-ar','--version'], 'llvm-objcopy':['/usr/lib/llvm-21/bin/llvm-objcopy','--version'],
            'qemu':['qemu-system-aarch64','--version'], 'python':['python3','--version'], 'make':['make','--version'], 'host-cc':['gcc','--version']}


def capture():
    tools = {}
    for name,command in COMMANDS.items():
        path = Path(shutil.which(command[0]) or command[0]).resolve()
        result = subprocess.run(command,capture_output=True,text=True)
        tools[name] = dict(path=str(path),sha256=hashlib.sha256(path.read_bytes()).hexdigest(),
                           version=(result.stdout+result.stderr).strip().splitlines()[0],exit_code=result.returncode)
    return dict(tools=tools,dependencies=json.loads((ROOT/'third_party/dependencies.lock.json').read_text()),
                assembler='Clang integrated AArch64 assembler; same compiler binary',host_os=Path('/etc/os-release').read_text())


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--verify',type=Path)
    args=parser.parse_args()
    current=capture()
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(current,indent=4)+'\n')
    if args.verify:
        expected=json.loads(args.verify.read_text())
        for name,record in current['tools'].items():
            if record['sha256']!=expected['tools'][name]['sha256'] or record['version']!=expected['tools'][name]['version']:
                parser.exit(1,'TOOLCHAIN MISMATCH: '+name+'\n')
        for name,record in current['dependencies'].items():
            if record['commit']!=expected['dependencies'][name]['commit']:
                parser.exit(1,'DEPENDENCY MISMATCH: '+name+'\n')
    print(args.output)


if __name__=='__main__':
    main()
