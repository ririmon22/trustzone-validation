import hashlib
import json
import shutil
from pathlib import Path


def stage(root, physical=False, canary=True, signed=False, sgi=False):
    original = root / 'third_party/optee'
    destination = root / ('build/source/optee-signed-loader' if signed else 'build/source/optee-validation-physical' if physical else 'build/source/optee-validation')
    if destination.exists():
        shutil.rmtree(destination)
    shutil.copytree(original, destination, symlinks=True, ignore=shutil.ignore_patterns('__pycache__', '.git'))
    if (destination / '.source_manifest.json').exists():
        (destination / '.source_manifest.json').rename(destination / '.base_source_manifest.json')
    if (destination / '.zip_input.json').exists():
        (destination / '.zip_input.json').rename(destination / '.base_zip_input.json')
    changes = []
    names = (['validation_canary.c','validation_canary.h'] if canary else []) + (['physical_probe.c','physical_probe.h'] if physical else [])
    for name in names:
        source = root / 'optee_standalone/pta' / name
        target = destination / 'core/pta' / name
        target.write_bytes(source.read_bytes())
        changes.append(dict(path=str(target.relative_to(destination)), original_sha256=None,
                            source=str(source.relative_to(root)), sha256=hashlib.sha256(target.read_bytes()).hexdigest()))
    makefile = destination / 'core/pta/sub.mk'
    original_bytes = makefile.read_bytes()
    makefile.write_bytes(original_bytes + (b'\nsrcs-$(CFG_TZ_VALIDATION_PTA) += validation_canary.c\n' if canary else b'') + (b'\nsrcs-$(CFG_TZ_PHYSICAL_PTA) += physical_probe.c\n' if physical else b''))
    changes.append(dict(path='core/pta/sub.mk', original_sha256=hashlib.sha256(original_bytes).hexdigest(),
                        sha256=hashlib.sha256(makefile.read_bytes()).hexdigest()))
    if signed:
        from stage_signed_loader import apply
        changes.extend(apply(root,destination))
    if sgi:raise ValueError('Secure SGI overlay is not available for ZCU106')
    for item in changes:
        item['sha256']=hashlib.sha256((destination/item['path']).read_bytes()).hexdigest()
    manifest = dict(mode='explicit-validation-overlay-on-build-copy', base_commit=json.loads((original / '.source_manifest.json').read_text())['commit'] if (original / '.source_manifest.json').exists() else json.loads((root / 'third_party/dependencies.lock.json').read_text())['optee']['commit'],
                    original_source_unchanged=True, changes=changes)
    if (original / '.zip_input.json').exists():
        manifest['base_commit'] = None
        manifest['source_provenance'] = 'ZIP_UNVERIFIED'
    (destination / 'validation-overlay.json').write_text(json.dumps(manifest, indent=4) + '\n')
    return destination, manifest
