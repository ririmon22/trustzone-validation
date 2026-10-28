import hashlib
import json
import os
import subprocess
from pathlib import Path

SOURCE_SUFFIXES = {'.c', '.h', '.S', '.s', '.mk', '.py', '.ld', '.lds', '.dts', '.dtsi'}


def verify_source(root, entry, source_mode="locked"):
    if source_mode == "zip":
        from zip_sources import verify_zip_source
        return verify_zip_source(root, entry)
    if source_mode != "locked":
        raise ValueError("Unknown source mode")
    root = Path(root)
    manifest_path = root / '.source_manifest.json'
    if not manifest_path.exists():
        commit = subprocess.check_output(['git', '-C', str(root), 'rev-parse', 'HEAD'], text=True).strip()
        dirty = subprocess.check_output(['git', '-C', str(root), 'status', '--porcelain'], text=True)
        if commit != entry['commit'] or dirty:
            raise ValueError('source commit mismatch or local changes')
        return {'commit': commit, 'source_mode': 'git-checkout'}
    manifest = json.loads(manifest_path.read_text())
    if manifest['commit'] != entry['commit'] or manifest['repository'] != entry['repository']:
        raise ValueError('snapshot provenance mismatch')
    if manifest.get('source_mode') != 'verified-build-subset' or not manifest['files']:
        raise ValueError('invalid snapshot mode/files')
    known = set()
    for record in manifest['files']:
        relative = Path(record['path'])
        if relative.is_absolute() or '..' in relative.parts or record['path'] in known:
            raise ValueError('unsafe or duplicate snapshot path')
        known.add(record['path'])
        path = root / relative
        if not path.resolve().is_relative_to(root.resolve()):
            raise ValueError('snapshot link escapes source root')
        if record['mode'] == '120000':
            if not path.is_symlink():
                raise ValueError('snapshot symlink mode mismatch')
            data = os.readlink(path).encode()
        else:
            if path.is_symlink():
                raise ValueError('unexpected source symlink')
            data = path.read_bytes()
            executable = bool(path.stat().st_mode & 0o111)
            if executable != (record['mode'] == '100755'):
                raise ValueError('snapshot executable mode mismatch: ' + record['path'])
        digest = hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest()
        if digest != record['sha'] or len(data) != record['size']:
            raise ValueError('snapshot blob mismatch: ' + record['path'])
    for path in root.rglob('*'):
        relative = path.relative_to(root)
        if '.git' in relative.parts or '__pycache__' in relative.parts or not path.is_file():
            continue
        if (path.suffix in SOURCE_SUFFIXES or path.name == 'Makefile') and str(relative) not in known:
            raise ValueError('unmanifested source file: ' + str(relative))
    return {'commit': manifest['commit'], 'source_mode': manifest['source_mode'], 'verified_files': len(known)}


def main():
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument('--source-mode', choices=('locked','zip'), default='locked')
    parser.add_argument('components', nargs='+')
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    lock = json.loads((root / 'third_party/dependencies.lock.json').read_text())
    try:
        results = {name: verify_source(root / 'third_party' / name, lock[name], args.source_mode) for name in args.components}
    except (ValueError, OSError, KeyError, subprocess.CalledProcessError) as error:
        parser.exit(1, 'SOURCE VERIFICATION FAIL: ' + str(error) + '\n')
    content = json.dumps(results, indent=4) + '\n'
    if args.output:
        args.output.write_text(content)
    print(content, end='')


if __name__ == '__main__':
    main()
