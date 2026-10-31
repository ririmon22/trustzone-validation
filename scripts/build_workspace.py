import hashlib
import shutil
from pathlib import Path

# 実際のbuild/検査が使うsourceだけを新しい作業領域へ複製する。
SOURCE_DIRECTORIES = ('common', 'optee_standalone', 'platform', 'scripts', 'tfa_standalone')

def copy_sources(root, destination):
    root=Path(root).resolve();destination=Path(destination).resolve()
    if destination.exists():raise ValueError('build workspace already exists; preserve previous run')
    destination.mkdir(parents=True)
    for name in SOURCE_DIRECTORIES:
        shutil.copytree(root/name,destination/name,symlinks=False,
                        ignore=shutil.ignore_patterns('__pycache__','*.pyc','*.local.json'))
    shutil.copy2(root/'Makefile',destination/'Makefile')
    (destination/'third_party').mkdir()
    shutil.copy2(root/'third_party/dependencies.lock.json',destination/'third_party/dependencies.lock.json')
    return {str(path.relative_to(destination)):hashlib.sha256(path.read_bytes()).hexdigest()
            for path in sorted(destination.rglob('*')) if path.is_file()}
