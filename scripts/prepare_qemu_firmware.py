import argparse
import hashlib
import json
import re
import os
import shutil
import struct
import subprocess
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MAP_PATH = ROOT / 'platform/qemu/external/memory_map.json'
MAP = json.loads(MAP_PATH.read_text())
FIP_OFFSET = int(MAP['fip_offset'], 0)
FLASH_SIZE = int(MAP['flash_size'], 0)
FIP_MAGIC = 0xAA640001
OPTEE_MAGIC = 0x4554504F
REGIONS = {name: tuple(int(value, 0) for value in region) for name, region in MAP['regions'].items()}


def require(condition, message):
    if not condition:
        raise ValueError(message)


def elf_segments(path):
    data = path.read_bytes()
    require(len(data) >= 64 and data[:6] == b'\x7fELF\x02\x01', 'invalid ELF64 little endian')
    require(struct.unpack_from('<H', data, 18)[0] == 183, 'ELF must be AArch64')
    entry, offset = struct.unpack_from('<QQ', data, 24)
    stride, count = struct.unpack_from('<HH', data, 54)
    require(stride == 56 and offset + stride * count <= len(data), 'invalid ELF program headers')
    segments = []
    for index in range(count):
        kind, flags, source, virtual, physical, filesz, memsz, align = struct.unpack_from('<IIQQQQQQ', data, offset + stride * index)
        if kind != 1 or not memsz:
            continue
        require(filesz <= memsz and source + filesz <= len(data), 'invalid ELF load size')
        require(virtual + memsz < 1 << 64 and physical + memsz < 1 << 64, 'ELF overflow')
        require(align > 0 and align & (align - 1) == 0 and (virtual - source) % align == 0, 'ELF alignment')
        segments.append(dict(virtual=virtual, physical=physical, filesz=filesz, memsz=memsz, flags=flags))
    require(any(s['flags'] & 1 and s['virtual'] <= entry < s['virtual'] + s['memsz'] for s in segments), 'entry outside executable load')
    return entry, segments


def inside(base, size, region):
    return region[0] <= base and base + size <= region[1]


def validate_elf(path, owner):
    entry, segments = elf_segments(path)
    for segment in segments:
        virtual, physical = segment['virtual'], segment['physical']
        if owner == 'bl1':
            require(inside(virtual, segment['memsz'], (0, FIP_OFFSET)) or inside(virtual, segment['memsz'], REGIONS['tfa']), 'BL1 VMA outside ROM/Secure RAM')
            if virtual != physical:
                require(inside(virtual, segment['memsz'], REGIONS['tfa']) and inside(physical, segment['filesz'], (0, FIP_OFFSET)), 'invalid BL1 initialized-data LMA')
            else:
                require(inside(physical, segment['memsz'], (0, FIP_OFFSET)) or inside(physical, segment['memsz'], REGIONS['tfa']), 'BL1 LMA outside reservation')
        else:
            require(virtual == physical and inside(virtual, segment['memsz'], REGIONS[owner]), owner + ' load outside reservation')
    return dict(entry=entry, segments=segments)


def fip_entries(data):
    require(len(data) >= 56 and struct.unpack_from('<I', data)[0] == FIP_MAGIC, 'invalid FIP magic')
    entries, spans = {}, []
    cursor = 16
    while True:
        require(cursor + 40 <= len(data), 'truncated FIP table')
        uuid, offset, size, flags = struct.unpack_from('<16sQQQ', data, cursor)
        cursor += 40
        if uuid == bytes(16):
            break
        require(uuid not in entries and size > 0 and offset + size <= len(data), 'duplicate/invalid FIP entry')
        entries[uuid] = data[offset:offset + size]
        spans.append((offset, offset + size))
    spans.sort()
    require(all(start >= cursor for start, _ in spans), 'FIP overlaps table')
    require(all(first[1] <= second[0] for first, second in zip(spans, spans[1:])), 'overlapping FIP entries')
    return entries


def prepare(configuration):
    version = 'b1' if configuration == 'tfa' else 'b2'
    release = ROOT / ('build/external/tfa-qemu-' + version + '/qemu/release')
    inputs, elf_records = {}, {}
    def read(path):
        data = path.read_bytes()
        inputs[str(path.relative_to(ROOT))] = hashlib.sha256(data).hexdigest()
        return data
    read(MAP_PATH)
    spans = sorted(REGIONS.values())
    require(all(base < end and base % 4096 == 0 and end % 4096 == 0 for base, end in spans), 'reservation alignment')
    require(all(left[1] <= right[0] for left, right in zip(spans, spans[1:])), 'reservation overlap')
    header_text = read(ROOT / 'platform/qemu/external/memory_map.h').decode()
    for macro, expected in (('NONSECURE_CODE_BASE', REGIONS['bl33'][0]), ('NONSECURE_DATA_END', REGIONS['bl33'][1]),
                            ('NONSECURE_STACK_BASE', REGIONS['stack'][0]), ('NONSECURE_STACK_END', REGIONS['stack'][1]),
                            ('SHARED_MEMORY_BASE', REGIONS['shared'][0]), ('SHARED_MEMORY_END', REGIONS['shared'][1])):
        match = re.search(r'^#define ' + macro + r' (0x[0-9a-fA-F]+)$', header_text, re.M)
        require(match is not None and int(match[1], 0) == expected, 'external header mismatch: ' + macro)
    linker = read(ROOT / 'platform/qemu/external/linker_bl33.ld').decode()
    require('0x60000000' in linker and '0x60200000' in linker and '0x60300000' in linker, 'BL33 linker mismatch')
    def compare_binary(elf, binary):
        with tempfile.TemporaryDirectory() as directory:
            converted = Path(directory) / 'image.bin'
            subprocess.run([os.environ.get('QEMU_OBJCOPY') or str(Path(shutil.which('clang') or '/usr/bin/clang').resolve().parent / 'llvm-objcopy'), '-O', 'binary', str(elf), str(converted)], check=True, capture_output=True)
            require(converted.read_bytes() == read(binary), 'ELF/binary mismatch: ' + str(binary))
    for name in ('bl1', 'bl2', 'bl31'):
        path = release / name / (name + '.elf')
        read(path)
        compare_binary(path, release / (name + '.bin'))
        elf_records[name] = validate_elf(path, 'bl1' if name == 'bl1' else 'tfa')
    bl33 = ROOT / 'build/qemu/external' / (configuration + '_bl33.elf')
    read(bl33)
    compare_binary(bl33, bl33.with_suffix('.bin'))
    elf_records['bl33'] = validate_elf(bl33, 'bl33')
    require(elf_records['bl33']['entry'] == REGIONS['bl33'][0], 'TF-A NS_IMAGE_OFFSET mismatch')
    header_path = ROOT / 'third_party/tfa-amd/include/tools_share/firmware_image_package.h'
    definitions = read(header_path).decode()
    def uuid(name):
        match = re.search(r'#define ' + name + r'\s+\\\n([^\n]+)', definitions)
        require(match is not None, 'missing fixed-source UUID')
        value = bytes(int(x, 16) for x in re.findall(r'0x([0-9a-fA-F]+)', match[1]))
        require(len(value) == 16, 'invalid source UUID')
        return value
    expected = {
        uuid('UUID_TRUSTED_BOOT_FIRMWARE_BL2'): read(release / 'bl2.bin'),
        uuid('UUID_EL3_RUNTIME_FIRMWARE_BL31'): read(release / 'bl31.bin'),
        uuid('UUID_NON_TRUSTED_FIRMWARE_BL33'): read(bl33.with_suffix('.bin')),
    }
    if configuration == 'optee':
        core = ROOT / 'build/external/optee/core'
        path = core / 'tee.elf'
        read(path)
        elf_records['optee'] = validate_elf(path, 'optee')
        require(elf_records['optee']['entry'] == REGIONS['optee'][0], 'Core probe target differs from ELF entry')
        header = read(core / 'tee-header_v2.bin')
        pager = read(core / 'tee-pager_v2.bin')
        pageable = read(core / 'tee-pageable_v2.bin')
        require(len(header) >= 28, 'short OP-TEE header')
        magic, version, arch, flags, count = struct.unpack_from('<IBBHI', header)
        require((magic, version, arch, count) == (OPTEE_MAGIC, 2, 1, 1) and len(header) == 28, 'unsupported OP-TEE image format')
        high, low, image_id, size = struct.unpack_from('<IIII', header, 12)
        address = (high << 32) | low
        require(image_id == 0 and size == len(pager) and address == elf_records['optee']['entry'] and inside(address, size, REGIONS['optee']), 'OP-TEE pager mismatch')
        require(not pageable, 'paged image layout requires separate validation')
        expected.update({uuid('UUID_SECURE_PAYLOAD_BL32'): header, uuid('UUID_SECURE_PAYLOAD_BL32_EXTRA1'): pager})
    fip = read(release / 'fip.bin')
    require(fip_entries(fip) == expected, 'FIP payload differs from verified binaries')
    bl1 = read(release / 'bl1.bin')
    require(len(bl1) <= FIP_OFFSET and FIP_OFFSET + len(fip) <= FLASH_SIZE, 'flash image overflow')
    flash = bl1.ljust(FIP_OFFSET, b'\0') + fip
    output = ROOT / 'build/qemu' / (configuration + '_flash.bin')
    output.write_bytes(flash)
    manifest = dict(configuration=configuration, platform='qemu-virt-9.2', inputs=inputs, elf=elf_records,
                    reservations=REGIONS, flash_sha256=hashlib.sha256(flash).hexdigest(),
                    scope='build artifact placement and FIP identity; not hardware protection evidence')
    (output.parent / (configuration + '_map_verified.json')).write_text(json.dumps(manifest, indent=4) + '\n')
    return manifest


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('configuration', choices=('tfa', 'optee'))
    args = parser.parse_args()
    try:
        manifest = prepare(args.configuration)
    except (ValueError, OSError, struct.error, subprocess.CalledProcessError) as error:
        parser.exit(1, 'FIRMWARE PREPARATION FAIL: ' + str(error) + '\n')
    print('FIRMWARE PREPARATION PASS: ' + manifest['flash_sha256'])


if __name__ == '__main__':
    main()
