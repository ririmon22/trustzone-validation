import hashlib
import json
import os
import re
import stat
import zipfile
from pathlib import Path

REQUIRED = {
    'tfa-amd': ('Makefile','bl31/bl31_main.c','services/spd/opteed/opteed_main.c','plat/xilinx/zynqmp/platform.mk','plat/xilinx/zynqmp/include/platform_def.h','plat/qemu/qemu/platform.mk'),
    'optee': ('Makefile','mk/config.mk','ta/mk/ta_dev_kit.mk','core/arch/arm/plat-zynqmp/conf.mk','core/arch/arm/plat-zynqmp/main.c','core/arch/arm/plat-vexpress/conf.mk','lib/libutee/include/utee_syscalls.h','scripts/ts_bin_to_c.py'),
    'pyelftools': ('elftools/__init__.py','elftools/elf/elffile.py'),
    'compiler-rt': ('compiler-rt/lib/builtins/udivti3.c','compiler-rt/lib/builtins/udivmodti4.c','compiler-rt/lib/builtins/int_lib.h'),
}
SUPPORTED = {'tfa-amd': ('2.12','2.12.0','xlnx_rebase_v2.12_2025.1'), 'optee': ('4.5.0',), 'pyelftools': ('0.32',), 'compiler-rt': ('21.1.8',)}


def sha256(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            digest.update(block)
    return digest.hexdigest()


def input_fingerprint(root):
    records=[]
    root=Path(root)
    for path in sorted(root.rglob('*')):
        name=str(path.relative_to(root))
        if path.is_symlink():records.append((name,'link',os.readlink(path)))
        elif path.is_file():records.append((name,sha256(path),path.stat().st_mode & 0o777))
    return hashlib.sha256(json.dumps(records,separators=(',',':')).encode()).hexdigest()


def tree_manifest(root):
    root = Path(root).resolve()
    if not root.is_dir():
        raise ValueError('Source directory missing: ' + str(root))
    records = []
    for path in sorted(root.rglob('*')):
        relative = path.relative_to(root)
        if '.git' in relative.parts or '__pycache__' in relative.parts or path.name in ('.source_manifest.json','.zip_input.json') or path.suffix == '.pyc':
            continue
        if path.is_symlink():
            if not path.resolve().is_relative_to(root):
                raise ValueError('Source symlink escapes root: ' + str(relative))
            records.append(dict(path=str(relative), kind='symlink', target=os.readlink(path)))
        elif path.is_file():
            if path.suffix in ('.o','.a','.elf','.bin','.ta','.pyc'):
                raise ValueError('Generated file in ZIP source input: ' + str(relative))
            records.append(dict(path=str(relative), kind='file', sha256=sha256(path), size=path.stat().st_size, executable=bool(path.stat().st_mode & stat.S_IXUSR)))
    if not records:
        raise ValueError('Empty source directory')
    digest = hashlib.sha256(json.dumps(records, sort_keys=True, separators=(',',':')).encode()).hexdigest()
    return dict(tree_sha256=digest, files=records)


def inspect_zip_source(root, name, declared_version=None, archive=None):
    root = Path(root).resolve()
    for relative in REQUIRED[name]:
        if not (root/relative).is_file():
            raise ValueError(name + ': required source missing: ' + relative)
    if declared_version and declared_version not in SUPPORTED[name]:
        raise ValueError(name + ': unsupported declared version ' + declared_version + '; supported/tested labels: ' + ','.join(SUPPORTED[name]))
    version = None
    if name == 'tfa-amd':
        makefile = (root/'Makefile').read_text()
        values = [re.search(r'^VERSION_'+key+r'\s*:?=\s*(\d+)',makefile,re.M) for key in ('MAJOR','MINOR')]
        version = '.'.join(match[1] for match in values) if all(values) else None
        if version != '2.12':
            raise ValueError('TF-A Makefile version must be 2.12; observed ' + str(version))
        if 'opteed' not in (root/'services/spd/opteed/opteed_main.c').read_text():
            raise ValueError('TF-A opteed implementation missing')
    if name == 'optee':
        config = (root/'core/arch/arm/plat-zynqmp/conf.mk').read_text()
        if 'zcu106' not in config or 'CFG_WITH_ARM_TRUSTED_FW' not in config:
            raise ValueError('OP-TEE zcu106/TF-A platform interface missing')
        for token in ('CFG_EARLY_TA','CFG_ARM64_core','CFG_USER_TA_TARGETS'):
            if token not in (root/'mk/config.mk').read_text():
                raise ValueError('OP-TEE configuration interface missing: ' + token)
    if name == 'pyelftools':
        match = re.search(r"__version__\s*=\s*['\"]([^'\"]+)", (root/'elftools/__init__.py').read_text())
        version = match[1] if match else None
        if version != '0.32':
            raise ValueError('pyelftools version must be 0.32; observed ' + str(version))
    state = tree_manifest(root)
    archive_hash = None
    if archive:
        archive = Path(archive).resolve()
        if not archive.is_file() or not zipfile.is_zipfile(archive):
            raise ValueError('Input archive is missing/not a ZIP: ' + str(archive))
        archive_hash = sha256(archive)
    return dict(input_state_sha256=input_fingerprint(root), input_kind='EXPANDED_ZIP_WITH_ARCHIVE' if archive else 'EXPANDED_DIRECTORY_ONLY',
                git_metadata_present=(root/'.git').exists(), snapshot_metadata_present=(root/'.source_manifest.json').exists(),
                checked_required_files=list(REQUIRED[name]), source_mode='zip-unverified', provenance_status='UNVERIFIED', verified_commit=None,
                component=name, directory=str(root), declared_version=declared_version,
                observed_version=version, version_status='STRUCTURAL_CHECK_ONLY_NOT_COMMIT_VERIFICATION',
                archive=str(archive) if archive else None, archive_sha256=archive_hash,
                archive_directory_binding='USER_SUPPLIED_NOT_VERIFIED' if archive else 'DIRECTORY_ONLY',
                compatibility='REQUIRED_INTERFACES_PRESENT; complete compatibility requires build/static tests', **state)


def verify_zip_source(root, entry):
    urls = {'arm-trusted-firmware':'tfa-amd','optee_os':'optee','pyelftools':'pyelftools','llvm-project':'compiler-rt'}
    name = next((value for key,value in urls.items() if key in entry['repository']), None)
    if name is None:
        raise ValueError('Unsupported ZIP dependency')
    report = inspect_zip_source(root,name)
    marker = Path(root)/'.zip_input.json'
    if marker.exists():
        expected = json.loads(marker.read_text())
        if expected['tree_sha256'] != report['tree_sha256']:
            raise ValueError('ZIP working-copy source changed after staging: ' + name)
    return {key:report[key] for key in ('source_mode','provenance_status','verified_commit','tree_sha256','observed_version')}
