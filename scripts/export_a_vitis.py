import argparse
import json
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        parser.error('Use a new output directory; existing files are preserved')
    source = args.output / 'src'
    source.mkdir(parents=True)
    for name in ('baremetal', 'common', 'platform', 'scripts', 'tests'):
        shutil.copytree(ROOT / name, source / name, ignore=shutil.ignore_patterns('__pycache__', '*.pyc'))
    (source / 'third_party').mkdir()
    shutil.copy2(ROOT / 'third_party/dependencies.lock.json', source / 'third_party/dependencies.lock.json')
    shutil.copy2(ROOT / 'Makefile', source / 'Makefile')
    cmake = '''cmake_minimum_required(VERSION 3.20)
project(tz_a_unified NONE)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(TZ_COMPILER "gcc" CACHE STRING "gcc or clang")
set(TZ_CROSS_COMPILE "aarch64-none-elf-" CACHE STRING "Reviewed AArch64 GNU prefix")
set(TZ_LLVM_DIR "/usr/bin" CACHE PATH "LLVM bin directory for clang route")
set(TZ_RUN_ID "1" CACHE STRING "Nonzero run identifier")
add_custom_target(a_unified ALL
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/build_a_unified.py
        --platform zcu106 --compiler ${TZ_COMPILER} --cross-compile ${TZ_CROSS_COMPILE}
        --llvm-dir ${TZ_LLVM_DIR} --run-id ${TZ_RUN_ID}
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/check_a_unified.py
        --platform zcu106 --elf ${CMAKE_CURRENT_SOURCE_DIR}/build/zcu106/unified/a_unified.elf
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    VERBATIM)
'''
    (source / 'CMakeLists.txt').write_text(cmake)
    (args.output / 'import.json').write_text(json.dumps(dict(configuration='A-unified', startup='baremetal/el3_monitor/start.S only as ELF entry; role-local starts are symbol-prefixed', libc=False, standard_bsp_startup=False, linker='platform/zcu106/linker_a_unified.ld', output='src/build/zcu106/unified/a_unified.elf', vitis_ide_validation='NOT_RUN', hardware_load='BLOCKED'), indent=4)+'\n')
    print(args.output)
