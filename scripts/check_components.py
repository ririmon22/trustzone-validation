"""ZCU106の実生成物だけを検査する。ビルド・ロード・実行許可は行わない。
旧integrationの検査を、現行Makeの明示ELF/TA/conf/log引数へ移したもの。
ログとの照合はbuild引数の整合検査であり、実機にロードされたimageの認証ではない。
"""
import argparse
import hashlib
import json
import shlex
import struct
import uuid
import zlib
from pathlib import Path
from elftools.elf.elffile import ELFFile
from elftools.common.exceptions import ELFError
from check_boot_plan import elf_inventory
from elf_symbols import elf_symbols, symbol_payload, read_config, require
from zcu106_b2_layout import load_layout
from check_b2_review_profile import check as review_check
from check_signed_layout import check as signed_layout_check

ROOT = Path(__file__).resolve().parents[1]
TA_UUIDS = ('6dc13418-534f-4642-9138-215876304910', '6dc13419-534f-4642-9138-215876304910')


def inspect_elf(path, role):
    image = elf_inventory(path)
    require(image['machine'] == 183 and image['elf_class'] == 64, role + ': AArch64 ELF64 required')
    with path.open('rb') as stream:
        elf = ELFFile(stream)
        require(elf.little_endian, role + ': little endian required')
        require(elf.header['e_type'] == ('ET_DYN' if role.startswith('ta-') else 'ET_EXEC'), role + ': unexpected ELF type')
        sections = []
        for section in elf.iter_sections():
            if not section['sh_flags'] & 2 or not section['sh_size']:
                continue
            address, size, alignment = section['sh_addr'], section['sh_size'], section['sh_addralign']
            require(not (section['sh_flags'] & 1 and section['sh_flags'] & 4), role + ': writable executable section')
            require(alignment == 0 or (alignment & (alignment - 1) == 0 and address % alignment == 0), role + ': section alignment')
            require(any(s['virtual'] <= address and address + size <= s['virtual'] + s['runtime_bytes'] for s in image['loads']), role + ': section escapes LOAD')
            sections.append(dict(name=section.name, address=address, size=size, flags=section['sh_flags']))
        for segment in elf.iter_segments():
            if segment['p_type'] != 'PT_LOAD':
                continue
            require(segment['p_align'] <= 1 or (segment['p_paddr'] - segment['p_offset']) % segment['p_align'] == 0, role + ': physical alignment')
            flags = segment['p_flags']
            # この版のOP-TEE ELFは単一RWX LOAD。CPUのMMU権限とは別にsection属性も検査する。
            require(flags in ((4, 5, 6, 7) if role == 'bl32' else (4, 5, 6)), role + ': unexpected LOAD permissions')
        image['sections'] = sections
        if role.startswith('ta-'):
            head = elf.get_section_by_name('.ta_head')
            require(head is not None and head['sh_size'] == 32, role + ': TA header missing')
            image['uuid'] = str(uuid.UUID(bytes_le=head.data()[:16]))
    return image


def log_command(path, predicate):
    for line in path.read_text().splitlines():
        try:
            words = shlex.split(line)
        except ValueError:
            continue
        if predicate(words):
            return words
    raise ValueError('matching actual build command missing: ' + str(path))


def check(root, profile, bl31, bl32, bl33, conf, ta_a, ta_b, bl31_log, bl33_log):
    plan, regions = load_layout(root)
    config = read_config(conf)
    images = {role: inspect_elf(path, role) for role, path in [('bl31', bl31), ('bl32', bl32), ('bl33', bl33)]}
    for role, image in images.items():
        owned = [r for r in regions.values() if r['owner'] == role]
        require(all(s['virtual'] == s['base'] and any(r['base'] <= s['base'] < s['end'] <= r['end'] for r in owned) for s in image['loads']), role + ': LOAD/BSS escapes reservation or is not identity mapped')
        expected_entry = regions[{'bl31': 'bl31_reserved', 'bl32': 'bl32_reserved', 'bl33': 'bl33_code'}[role]]['base']
        require(image['entry'] == expected_entry, role + ': entry mismatch')
    dispatcher = elf_symbols(bl31)
    require(all(name in dispatcher[2] for name in ('opteed_setup', 'opteed_smc_handler')), 'linked OP-TEE dispatcher missing')
    for key, value in {'CFG_EARLY_TA':'y', 'CFG_ARM64_core':'y', 'CFG_WITH_PAGER':'n', 'CFG_DT':'n', 'CFG_WITH_ARM_TRUSTED_FW':'y', 'CFG_CORE_RESERVED_SHM':'y', 'CFG_CORE_ASLR':'n', 'CFG_CORE_ASYNC_NOTIF':'n', 'CFG_REE_FS':'n', 'CFG_RPMB_FS':'n'}.items():
        require(config.get(key) == value, 'Core config mismatch: ' + key)
    for key, value in {'CFG_TZDRAM_START':0x60000000, 'CFG_TZDRAM_SIZE':0x10000000, 'CFG_SHMEM_START':0x10500000, 'CFG_SHMEM_SIZE':0x100000, 'CFG_DDR_SIZE':plan['reported_ddr0']['end']-plan['reported_ddr0']['base']}.items():
        require(int(config[key], 0) == value, 'Core memory config mismatch: ' + key)
    tfa = log_command(bl31_log, lambda words: 'SPD=opteed' in words and 'bl31' in words)
    for setting in ['PLAT=zynqmp', 'RESET_TO_BL31=1', 'PRELOADED_BL33_BASE=0x10200000', 'ZYNQMP_ATF_MEM_BASE=0x1000', 'ZYNQMP_ATF_MEM_SIZE=0x7f000', 'ZYNQMP_BL32_MEM_BASE=0x60000000', 'ZYNQMP_BL32_MEM_SIZE=0x10000000']:
        require(setting in tfa, 'TF-A handoff build argument missing: ' + setting)
    base = next((x.split('=', 1)[1] for x in tfa if x.startswith('BUILD_BASE=')), '')
    require(base and Path(base).resolve() == bl31.resolve().parents[3], 'TF-A log selects another build')
    def selects_bl33(words):
        return '-o' in words and words.index('-o') + 1 < len(words) and Path(words[words.index('-o') + 1]).resolve() == bl33.resolve()
    client_command = log_command(bl33_log, selects_bl33)
    require('-DTEE_SHARED_CACHED=1' in client_command, 'BL33 cached SHM transport is required')
    core = elf_symbols(bl32)
    version = symbol_payload(core,'core_v_str').decode('ascii')
    expected_version = '4.5.0-0919de0f'
    if profile != 'basic':
        expected_version += '-validation-' + profile
    require(version.startswith(expected_version + ' '), 'Core source version label mismatch')
    early = []
    for role, expected, path in [('ta-a', TA_UUIDS[0], ta_a), ('ta-b', TA_UUIDS[1], ta_b)]:
        image = inspect_elf(path, role)
        require(image['uuid'] == expected, role + ': UUID mismatch')
        blob = symbol_payload(core, 'ts_bin_' + expected.replace('-', ''))
        expected_data = path.read_bytes()
        decoder = zlib.decompressobj()
        embedded = decoder.decompress(blob, len(expected_data) + 1)
        require(decoder.eof and not decoder.unused_data and not decoder.unconsumed_tail and embedded == expected_data, role + ': Early TA differs from Core')
        early.append(dict(role=role, uuid=expected, sha256=image['sha256']))
    client = elf_symbols(bl33)
    require(symbol_payload(client, 'zcu106_b2_runtime_ready') == bytes(4), 'runtime-ready must stay zero')
    require(struct.unpack('<3I', symbol_payload(client, 'b2_review_features')) == (1, int(profile != 'basic'), 0), 'BL33 profile features mismatch')
    signed = profile == 'signed'
    require(int.from_bytes(symbol_payload(client, 'b2_signed_profile'), 'little') == int(signed), 'BL33 signed profile mismatch')
    selection = int.from_bytes(symbol_payload(client, 'b2_test_selection'), 'little')
    require(selection == {'basic':14, 'signed':17, 'physical':15}[profile], 'BL33 default selection mismatch')
    require(config.get('CFG_REE_FS_TA') == ('y' if signed else 'n'), 'Core runtime TA loader mismatch')
    require(('CFG_TZ_PHYSICAL_FIXED_BASE' in config) == (profile == 'physical'), 'Core physical profile mismatch')
    table = client[2]['client_tables']
    require(table['address'] % 4096 == 0 and table['size'] == (7 if signed else 6) * 4096, 'page table ABI/alignment mismatch')
    start, end = [client[2][n]['address'] for n in ('__b2_vectors_start', '__b2_vectors_end')]
    require(start % 4096 == 0 and end > start and end % 4096 == 0 and any(s['base'] == start and s['end'] == end and s['flags'] == 5 for s in images['bl33']['loads']), 'vector RX isolation mismatch')
    review = review_check(root, 'zcu106', bl33, bl32, conf)
    if profile == 'physical':
        require(b'physical-probe.pta' in bl32.read_bytes(), 'physical PTA registration missing')
    if signed:
        signed_layout_check(root, 'zcu106', bl33)
        require(core[2].get('signed_loader_audit', {}).get('size') == 1040 and 'signed-loader-audit.pta' in bl32.read_bytes().decode('latin1'), 'signed loader audit/PTA missing')
    return dict(static_status='PASS', profile=profile, images=images, early_tas=early, review=review,
                core_load_permissions_note='OP-TEE single RWX ELF container; allocated sections have no W+X; live MMU permissions require hardware review',
                bl31_log_sha256=hashlib.sha256(bl31_log.read_bytes()).hexdigest(), bl33_log_sha256=hashlib.sha256(bl33_log.read_bytes()).hexdigest(),
                hardware_load='BLOCKED', hardware_execution='NOT_RUN', scope='linked files and build logs only; not device attestation, live memory protection or runtime TA execution')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--profile', choices=('basic','signed','physical'), required=True)
    for name in ('bl31','bl32','bl33','conf','ta-a','ta-b','bl31-log','bl33-log','output'):
        parser.add_argument('--' + name, type=Path, required=True)
    args = parser.parse_args()
    inputs = [args.bl31,args.bl32,args.bl33,args.conf,args.ta_a,args.ta_b,args.bl31_log,args.bl33_log]
    require(args.output.resolve() not in {p.resolve() for p in inputs}, 'output must preserve inputs')
    try:
        result = check(ROOT,args.profile,args.bl31,args.bl32,args.bl33,args.conf,args.ta_a,args.ta_b,args.bl31_log,args.bl33_log)
    except (OSError,ValueError,KeyError,TypeError,IndexError,struct.error,zlib.error,ELFError) as error:
        result = dict(static_status='FAIL', error=str(error), hardware_load='BLOCKED', hardware_execution='NOT_RUN')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2) + '\n')
    print(result['static_status'] + (': ' + result['error'] if 'error' in result else '; HARDWARE BLOCKED'))
    return 0 if result['static_status'] == 'PASS' else 1


if __name__ == '__main__':
    raise SystemExit(main())
