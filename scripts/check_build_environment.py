import argparse
import importlib.util
import json
import os
import platform
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path
from source_snapshot import verify_source

ROOT = Path(__file__).resolve().parents[1]
GNU_TOOLS = ('gcc', 'as', 'ld', 'ar', 'objcopy', 'objdump', 'nm', 'readelf')
LLVM_TOOLS = ('clang', 'ld.lld', 'llvm-ar', 'llvm-objcopy', 'llvm-objdump', 'llvm-nm', 'llvm-readelf')


def probe(command, purpose):
    result = subprocess.run(command, capture_output=True, text=True)
    return dict(command=command, purpose=purpose, exit_code=result.returncode,
                output=(result.stdout + result.stderr).strip()[:2000])


def choose_toolchain(compiler, prefix, llvm_dir):
    if prefix and not re.fullmatch(r'[A-Za-z0-9_./+:-]+', prefix):
        raise ValueError('Cross prefix must be a path/name without shell syntax or spaces')
    if compiler == 'auto':
        compiler = 'gcc' if prefix or shutil.which('aarch64-none-elf-gcc') or shutil.which('aarch64-xilinx-elf-gcc') or shutil.which('aarch64-linux-gnu-gcc') else 'clang'
    if compiler == 'gcc' and not prefix:
        for candidate in ('aarch64-none-elf-', 'aarch64-xilinx-elf-', 'aarch64-linux-gnu-'):
            if shutil.which(candidate + 'gcc'):
                prefix = candidate
                break
    if compiler == 'clang':
        binary = shutil.which('clang')
        directory = Path(llvm_dir).resolve() if llvm_dir else Path(binary or '/missing/clang').resolve().parent
        tools = {name: str(directory / name) for name in LLVM_TOOLS}
    else:
        prefix = prefix or 'aarch64-none-elf-'
        tools = {name: prefix + name for name in GNU_TOOLS}
        directory = None
    return dict(compiler=compiler, cross_compile=prefix or 'aarch64-linux-gnu-',
                llvm_dir=str(directory) if directory else None, tools=tools)


def inspect(configuration, compiler='auto', prefix=None, llvm_dir=None, qemu=False, allow_snapshots=False):
    selection = choose_toolchain(compiler, prefix, llvm_dir)
    if configuration == 'performance':
        selection = dict(compiler='not-required',tools={},llvm_dir=None,cross_compile=None)
    issues, observations = [], []
    observations.append(dict(host_os=platform.platform(), python=sys.version, python_executable=sys.executable, site_packages_disabled=bool(sys.flags.no_site),
                             vitis=shutil.which('vitis'), vitis_root_environment=os.environ.get('XILINX_VITIS'), vitis_status='NOT_RUN; tool presence is not a Vitis build result'))
    if sys.version_info < (3, 9):
        issues.append('Python >= 3.9 required by source/path validation; Ubuntu 24.04 Python 3.12 is a candidate, not tested here')
    base_tools = () if configuration == 'performance' else ('make', 'bash', 'echo', 'mkdir', 'rm') + (() if configuration == 'baremetal' else ('git',))
    if configuration in ('optee','early-ta','all'):
        base_tools += ('cmp',)
    for name in base_tools:
        path = shutil.which(name)
        if not path:
            issues.append('Missing tool: ' + name)
        else:
            observations.append(probe([path, '--version'], 'build orchestration / local Git provenance'))
    selected = selection['tools']
    # Baremetal ELFはcompiler driverがassembler/linkerを呼び出せれば生成できる。
    required = ('clang', 'ld.lld') if selection['compiler'] == 'clang' else ('gcc', 'as', 'ld')
    if configuration == 'performance':
        required = ()
    elif configuration != 'baremetal':
        required = tuple(selected)
    for name in required:
        path = shutil.which(selected[name])
        if not path:
            issues.append('Missing target tool: ' + selected[name])
        else:
            observations.append(probe([path, '--version'], 'target build: ' + name))
    cc = selected.get('clang', selected.get('gcc'))
    if required and cc and shutil.which(cc) and all(shutil.which(selected[name]) for name in required):
        with tempfile.TemporaryDirectory() as directory:
            source, elf = Path(directory) / 'probe.S', Path(directory) / 'probe.elf'
            source.write_text('.text\n.global _start\n_start:\n    wfe\n    b _start\n')
            command = [cc] + (['--target=aarch64-none-elf', '-fuse-ld=lld'] if selection['compiler']=='clang' else [])
            command += ['-mcpu=cortex-a53', '-nostdlib', '-static', '-Wl,-e,_start', str(source), '-o', str(elf)]
            checked = probe(command, 'actual AArch64 assemble/link capability')
            observations.append(checked)
            if checked['exit_code'] or not elf.exists() or elf.read_bytes()[4:6] != b'\x02\x01' or int.from_bytes(elf.read_bytes()[18:20], 'little') != 183:
                issues.append('Target compiler did not generate AArch64 ELF64')
    if selection['compiler'] == 'gcc' and configuration in ('optee','early-ta','all') and shutil.which(selected['gcc']):
        runtime = probe([selected['gcc'],'-print-libgcc-file-name'],'compiler-supplied AArch64 runtime; avoids LLVM/compiler-rt source')
        observations.append(runtime)
        if runtime['exit_code'] or not Path(runtime['output']).is_file():
            issues.append('Target GCC libgcc runtime is unavailable')
    dependencies = []
    if configuration in ('tfa', 'optee', 'all'):
        dependencies.append('tfa-amd')
    if configuration in ('optee', 'early-ta', 'all'):
        dependencies += ['optee', 'pyelftools']
        if selection['compiler'] == 'clang':
            dependencies += ['compiler-rt']
        try:
            from cryptography.hazmat.primitives import serialization
            from cryptography.hazmat.primitives.asymmetric import rsa
            observations.append(dict(python_module='cryptography', path=serialization.__file__, status='AVAILABLE', purpose='official OP-TEE TA signing; not replaced by handwritten cryptography'))
        except ImportError as error:
            issues.append('Missing Python cryptography for official TA signing: ' + str(error))
    lock = json.loads((ROOT/'third_party/dependencies.lock.json').read_text())
    for name in dependencies:
        source = ROOT/'third_party'/name
        try:
            proof = verify_source(source, lock[name])
            observations.append(dict(source=name, **proof))
            if proof['source_mode'] != 'git-checkout' and not allow_snapshots:
                issues.append(name + ': full pinned Git checkout required; verified snapshot only allowed for explicitly labeled development tests')
        except (OSError, ValueError, subprocess.CalledProcessError) as error:
            issues.append('Source unavailable/unverified: ' + name + ': ' + str(error))
    if configuration in ('optee','early-ta','all') and (ROOT/'third_party/pyelftools/elftools/__init__.py').is_file():
        environment = os.environ.copy()
        environment['PYTHONPATH'] = str(ROOT/'third_party/pyelftools')
        checked = subprocess.run([sys.executable,'-c','from elftools.elf.elffile import ELFFile'],env=environment,capture_output=True,text=True)
        observations.append(dict(python_module='elftools',exit_code=checked.returncode,purpose='official OP-TEE ELF processing from fixed Git source, no pip install'))
        if checked.returncode: issues.append('Fixed pyelftools source is not importable')
    if qemu and configuration in ('tfa','optee','all'):
        openssl = Path('/usr/bin/openssl')
        if not openssl.is_file():
            issues.append('Official fiptool default requires /usr/bin/openssl (OpenSSL version detection)')
        else:
            observations.append(probe([str(openssl),'version'],'official fiptool OpenSSL API selection'))
        host_cc = shutil.which('cc') or shutil.which('gcc') or shutil.which('clang')
        if not host_cc:
            issues.append('QEMU FIP requires native C compiler')
        else:
            with tempfile.TemporaryDirectory() as directory:
                source, output = Path(directory)/'crypto.c',Path(directory)/'crypto'
                source.write_text('#include <openssl/evp.h>\nint main(void) { return EVP_sha256() == 0; }\n')
                result = probe([host_cc,str(source),'-lcrypto','-o',str(output)],'TF-A official fiptool OpenSSL headers and libcrypto (libssl-dev if missing)')
                observations.append(result)
                if result['exit_code']: issues.append('Native fiptool requires OpenSSL development headers/library')
    if qemu:
        path = shutil.which(os.environ.get('QEMU_SYSTEM_AARCH64','qemu-system-aarch64'))
        if not path: issues.append('QEMU execution requested; qemu-system-aarch64 is missing')
        else: observations.append(probe([path,'--version'],'QEMU functional validation only; not required to build hardware ELF'))
    return dict(status='BLOCKED' if issues else 'READY', configuration=configuration, selected=selection,
                observations=observations, issues=issues, network_operations_performed=False,
                ubuntu_24_04_validation='NOT_RUN' if 'VERSION_ID="24.04"' not in Path('/etc/os-release').read_text() else 'host OS only; Vitis NOT_RUN',
                hardware_execution='NOT_RUN', source_policy='verified snapshots explicitly allowed' if allow_snapshots else 'pinned clean Git checkout')


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--configuration',choices=('baremetal','tfa','optee','early-ta','performance','all'),default='baremetal')
    parser.add_argument('--compiler',choices=('auto','gcc','clang'),default='auto')
    parser.add_argument('--cross-compile')
    parser.add_argument('--llvm-dir')
    parser.add_argument('--qemu',action='store_true')
    parser.add_argument('--allow-snapshots',action='store_true')
    parser.add_argument('--output',type=Path,default=ROOT/'results/phase8/environment.json')
    args=parser.parse_args()
    try:
        result=inspect(args.configuration,args.compiler,args.cross_compile,args.llvm_dir,args.qemu,args.allow_snapshots)
    except (OSError,ValueError,subprocess.CalledProcessError) as error:
        result=dict(status='BLOCKED',issues=[str(error)],hardware_execution='NOT_RUN')
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(result,indent=4)+'\n')
    print(json.dumps(result,indent=2))
    return 2 if result['status']=='BLOCKED' else 0


if __name__=='__main__':
    raise SystemExit(main())
