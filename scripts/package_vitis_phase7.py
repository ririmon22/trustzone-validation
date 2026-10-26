import hashlib
import json
import shlex
import shutil
import tarfile
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
COMMON=['common/memory.c','common/arch.c','common/result_report/console.c','platform/zcu106/uart.c']
EL1=['common/el1_vectors.S','common/el1_fault.c','common/probe.S']
RUNNER=['common/test_runner/runner.c']
BENCH=['common/benchmark/benchmark.c']


def main():
    destination=ROOT/'build/vitis-phase7'
    if destination.exists():
        raise SystemExit('Package destination exists; preserve it or choose a fresh build directory')
    clean=json.loads((ROOT/'results/phase7/clean-workspace.json').read_text())
    if clean.get('status')!='PASS': raise SystemExit('Clean build has not passed; package BLOCKED')
    workspace=Path(clean['workspace'])
    destination.mkdir(parents=True)
    components={
        'A-el3':dict(sources=['baremetal/el3_monitor/start.S','baremetal/el3_monitor/monitor.c']+COMMON,linker='platform/zcu106/linker_el3.ld',startup='custom _start at EL3; platform initialization/handoff unverified'),
        'A-secure':dict(sources=['baremetal/secure_app/start.S','baremetal/secure_app/main.c']+COMMON+EL1,linker='platform/zcu106/linker_secure.ld',startup='EL3 ERET to Secure EL1'),
        'A-nonsecure':dict(sources=['baremetal/nonsecure_app/start.S','baremetal/nonsecure_app/main.c','baremetal/tests/register_context.S']+COMMON+EL1+RUNNER+BENCH,linker='platform/zcu106/linker_nonsecure.ld',startup='EL3 ERET to NS EL1'),
        'B1-bl33':dict(sources=['tfa_standalone/integration/start.S','tfa_standalone/integration/main.c']+COMMON+EL1+RUNNER,linker='platform/zcu106/linker_nonsecure.ld',startup='FSBL -> AMD TF-A BL31 -> NS EL1 BL33'),
        'B2-bl33':dict(sources=['tfa_standalone/integration/start.S','optee_standalone/tests/main.c','optee_standalone/tests/security_tests.c','optee_standalone/tests/stress_tests.c','optee_standalone/client/client.c']+COMMON+EL1+RUNNER+BENCH,linker='platform/zcu106/linker_nonsecure.ld',startup='FSBL -> BL31 opteed -> BL32/BL33; real SHM/MMU platform adapter required')}
    cflags=next(line.split(':=',1)[1].strip() for line in (ROOT/'Makefile').read_text().splitlines() if line.startswith('CFLAGS :='))
    for record in components.values():
        record.update(include_paths=['common/include','platform/zcu106','optee_standalone/client'],
                      compiler_options=shlex.split(cflags),
                      validated_toolchain='Clang/LLD 21.1.8; Vitis GNU settings require verification',
                      clang_driver_options=['--target=aarch64-none-elf','-fuse-ld=lld'],
                      linker_options=['-nostdlib','-static','-Wl,--build-id=none'],status='BUILD ONLY; hardware NOT_RUN; load BLOCKED')
    manifest=dict(platform_component=dict(hardware='Vitis builtin ZCU106 if installed; record selected XSA and hash',processor='psu_cortexa53_0',os='standalone',architecture='aarch64',status='NOT_RUN'),
                  application_components=components,
                  external_firmware=dict(bl31='pinned AMD TF-A PLAT=zynqmp RESET_TO_BL31=1; separate external build',bl32='ZCU106 OP-TEE port NOT_BUILT; QEMU BL32 is excluded',early_tas='optee_standalone/ta; SDK and Early TA build instructions',fsbl_pmufw='Vitis platform boot artifacts, generated maps required'),
                  boot_configuration='platform/zcu106/boot_plan.json; unresolved values stay BLOCKED',
                  separation='compiler, linker and startup fields are separate; do not link BSP startup twice')
    for name in ('baremetal','common','tfa_standalone','optee_standalone','platform/zcu106','scripts','docs','tests','container'):
        shutil.copytree(ROOT/name,destination/name,ignore=shutil.ignore_patterns('__pycache__','*.pyc'))
    for name in ('Makefile','README.md','third_party/dependencies.lock.json','third_party/README.md'):
        output=destination/name; output.parent.mkdir(parents=True,exist_ok=True); shutil.copy2(ROOT/name,output)
    # QEMU専用MMU/bootstrapや生成BL32を実機用packageに混ぜない。
    (destination/'README_VITIS.md').write_text('# Vitis import package\n\nUse component_manifest.json and docs/phase7_vitis_package.md. This is a ZCU106 source package, not the complete QEMU workspace. Use PLATFORM=zcu106 for Make builds. Every hardware execution is NOT_RUN and load is BLOCKED. QEMU BL32 is intentionally excluded. External fixed sources must be installed separately.\n')
    artifact_paths=['build/zcu106/'+name+'.elf' for name in ('el3','secure','nonsecure','tfa_bl33','optee_bl33')]
    artifact_paths+=['build/external/tfa-zynqmp/zynqmp/release/bl31/bl31.elf','build/external/tfa-zynqmp/zynqmp/release/bl31.bin']
    for item in artifact_paths:
        source=workspace/item
        target=destination/'artifacts'/Path(item).relative_to('build')
        target.parent.mkdir(parents=True,exist_ok=True)
        shutil.copy2(source,target)
    for name in ('ta-a','ta-b'):
        target=destination/'artifacts'/name
        target.mkdir(parents=True)
        for source in (workspace/'build/external'/name).glob('*.stripped.elf'):
            shutil.copy2(source,target/source.name)
        for source in (workspace/'build/external'/name).glob('*.ta'):
            shutil.copy2(source,target/source.name)
    for name in ('tfa-amd','optee'):
        source=ROOT/'third_party'/name
        for item in source.rglob('*'):
            if not item.is_file() or item.name.lower().split('.')[0] not in ('license','copying'):
                continue
            target=destination/'third_party-notices'/name/item.relative_to(source)
            target.parent.mkdir(parents=True,exist_ok=True)
            shutil.copy2(item,target)
    manifest['platform_backend_sources']=[str(p.relative_to(ROOT)) for p in (ROOT/'platform/zcu106').glob('*') if p.suffix in ('.c','.S')]
    manifest['backend_integration']='Archive compilation only; world/exception/DMA bindings remain unverified'
    manifest['clean_build_workspace']=str(workspace)
    manifest['files']={str(p.relative_to(destination)):hashlib.sha256(p.read_bytes()).hexdigest() for p in destination.rglob('*') if p.is_file()}
    for record in components.values():
        for relative in record['sources']+[record['linker']]:
            if relative not in manifest['files']: raise ValueError('missing component input: '+relative)
            if manifest['files'][relative]!=clean['own_source_sha256'][relative]: raise ValueError('Source differs from clean build: '+relative)
    (destination/'component_manifest.json').write_text(json.dumps(manifest,indent=4)+'\n')
    output=ROOT/'results/phase7/vitis-source-package.tar.gz'
    with tarfile.open(output,'w:gz') as archive:
        archive.add(destination,arcname='trustzone-validation-vitis')
    (output.parent/'vitis-package.json').write_text(json.dumps(dict(archive=str(output.relative_to(ROOT)),sha256=hashlib.sha256(output.read_bytes()).hexdigest(),status='source package only; hardware NOT_RUN; load BLOCKED',components=manifest),indent=4)+'\n')
    print(output)


if __name__=='__main__': main()
