import argparse
import json
import shlex
import shutil
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
COMMON = ['common/debug_state.c', 'common/memory.c', 'common/arch.c', 'common/result_report/console.c']
EL1 = ['common/el1_vectors.S', 'common/el1_fault.c', 'common/probe.S']
ROLES = {
    'el3': ['baremetal/el3_monitor/start.S', 'baremetal/el3_monitor/monitor.c'],
    'secure': ['baremetal/secure_app/start.S', 'baremetal/secure_app/main.c'] + EL1,
    'nonsecure': ['baremetal/nonsecure_app/start.S', 'baremetal/nonsecure_app/main.c', 'baremetal/tests/register_context.S', 'common/test_runner/runner.c', 'common/benchmark/benchmark.c'] + EL1,
}


def build(platform, compiler, prefix, llvm_dir, run_id=1, fault_injection=False, ram_only=False):
    output = ROOT / 'build' / platform / ('unified-fault' if fault_injection else 'unified-ram' if ram_only else 'unified')
    output.mkdir(parents=True, exist_ok=True)
    cc = str(llvm_dir / 'clang') if compiler == 'clang' else prefix + 'gcc'
    objcopy = str(llvm_dir / 'llvm-objcopy') if compiler == 'clang' else prefix + 'objcopy'
    common = (['-DLOG_UART_ENABLED=0'] if ram_only else []) + ['-DDEBUG_RUN_ID=' + str(run_id) + 'UL', '-mcpu=cortex-a53', '-mgeneral-regs-only', '-mstrict-align', '-ffreestanding', '-fno-builtin', '-fno-stack-protector', '-fno-pic', '-fno-pie', '-fno-asynchronous-unwind-tables', '-fno-unwind-tables', '-fstack-usage', '-O2', '-g', '-Wall', '-Wextra', '-Werror', '-Wconversion', '-Wshadow', '-std=c11', '-Icommon/include', '-Iplatform/' + platform]
    target = ['--target=aarch64-none-elf'] if compiler == 'clang' else []
    link = ['-nostdlib', '-nostartfiles', '-nodefaultlibs', '-static', '-Wl,--build-id=none']
    linker = ['-fuse-ld=lld'] if compiler == 'clang' else []
    commands = []

    def run(command):
        commands.append(command)
        print(shlex.join(command), flush=True)
        subprocess.run(command, cwd=ROOT, check=True)

    try:
        for role, sources in ROLES.items():
            objects = []
            for index, source in enumerate(sources + COMMON + ['platform/' + platform + '/uart.c']):
                obj = output / (role + '-' + str(index) + '.o')
                run([cc, *target, *common, *(['-DTZ_FAULT_INJECTION=1'] if fault_injection and role == 'nonsecure' else []), '-c', source, '-o', str(obj)])
                objects.append(str(obj))
            partial = output / (role + '.partial.o')
            run([cc, *target, *linker, *link, '-Wl,-r', *objects, '-o', str(partial)])
            run([objcopy, '--prefix-symbols=' + role + '_', str(partial), str(output / (role + '.world.o'))])
        image = output / 'a_unified.elf'
        run([cc, *target, *linker, *link, '-Wl,-T,platform/' + platform + '/linker_a_unified.ld', '-Wl,-Map,' + str(image) + '.map', *[str(output / (role + '.world.o')) for role in ROLES], '-o', str(image)])
    finally:
        (output / 'commands.json').write_text(json.dumps(commands, indent=4) + '\n')
    return image


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--platform', choices=('qemu', 'zcu106'), required=True)
    parser.add_argument('--compiler', choices=('clang', 'gcc'), default='clang')
    parser.add_argument('--cross-compile', default='aarch64-none-elf-')
    parser.add_argument('--llvm-dir', type=Path, default=Path(shutil.which('clang') or '/usr/bin/clang').resolve().parent)
    parser.add_argument('--run-id', type=lambda value: int(value, 0), default=1)
    parser.add_argument('--ram-only', action='store_true')
    parser.add_argument('--fault-injection', action='store_true', help='QEMU-only undefined-instruction snapshot test')
    args = parser.parse_args()
    if not 0 < args.run_id < (1 << 64) or (args.fault_injection and args.platform != 'qemu'):
        parser.error('invalid run ID or non-QEMU fault injection')
    try:
        print(build(args.platform, args.compiler, args.cross_compile, args.llvm_dir, args.run_id, args.fault_injection, args.ram_only))
    except (OSError, subprocess.CalledProcessError) as error:
        parser.exit(1, 'UNIFIED BUILD FAIL: ' + str(error) + '\n')
