import argparse
import ast
import hashlib
import json
import re
import struct
import zlib
from pathlib import Path
from check_boot_plan import B2_ARTIFACT_PATHS, b2_binding_issues, elf_inventory

ROOT = Path(__file__).resolve().parents[1]
TA_UUIDS = ('6dc13418-534f-4642-9138-215876304910', '6dc13419-534f-4642-9138-215876304910')
ELF_SECTION_SIZE = 64
ELF_SYMBOL_SIZE = 24
ELF_SYMTAB = 2
ELF_STRTAB = 3
ELF_NOBITS = 8


def require(condition, message):
    if not condition:
        raise ValueError(message)


def elf_symbols(path):
    # 固定buildのELF64 LEを対象とし、未知の拡張形式は受け入れない。
    elf_inventory(path)
    data = Path(path).read_bytes()
    require(data[4:6] == b'\x02\x01', 'AArch64 ELF64 LE required')
    offset = struct.unpack_from('<Q', data, 40)[0]
    size, count = struct.unpack_from('<HH', data, 58)
    require(size == ELF_SECTION_SIZE and count > 0 and offset + size * count <= len(data), 'invalid ELF sections')
    sections = [struct.unpack_from('<IIQQQQIIQQ', data, offset + size * index) for index in range(count)]
    symbols = {}
    for section in sections:
        if section[1] != ELF_SYMTAB:
            continue
        begin, length, link, stride = section[4], section[5], section[6], section[9]
        require(stride == ELF_SYMBOL_SIZE and length % stride == 0 and begin + length <= len(data) and link < count, 'invalid symbol table')
        strings = sections[link]
        require(strings[1] == ELF_STRTAB and strings[4] + strings[5] <= len(data), 'invalid symbol strings')
        names = data[strings[4]:strings[4] + strings[5]]
        for cursor in range(begin, begin + length, stride):
            name, info, other, index, address, extent = struct.unpack_from('<IBBHQQ', data, cursor)
            if not index or not name:
                continue
            require(name < len(names) and b'\0' in names[name:], 'unterminated symbol name')
            label = names[name:names.index(b'\0', name)].decode('utf-8')
            symbols[label] = dict(address=address, size=extent, section=index)
    require(symbols, 'ELF symbols are required to verify dispatcher and Early TAs')
    return data, sections, symbols


def symbol_payload(parsed, name):
    data, sections, symbols = parsed
    require(name in symbols, 'missing linked symbol: ' + name)
    symbol = symbols[name]
    require(symbol['section'] < len(sections), 'symbol outside section table')
    section = sections[symbol['section']]
    displacement = symbol['address'] - section[3]
    require(section[1] != ELF_NOBITS and 0 <= displacement and symbol['size'] > 0 and displacement + symbol['size'] <= section[5], 'symbol escapes file-backed section')
    start = section[4] + displacement
    require(start + symbol['size'] <= len(data), 'symbol payload truncated')
    return data[start:start + symbol['size']]


def read_command(path):
    first = path.read_text().splitlines()[0]
    require(first.startswith('COMMAND: '), 'missing recorded build command: ' + str(path))
    command = ast.literal_eval(first[len('COMMAND: '):])
    require(isinstance(command, list) and all(isinstance(arg, str) for arg in command), 'invalid command record')
    return command


def read_config(path):
    return dict(re.findall(r'^([A-Za-z0-9_]+)=(.*)$', path.read_text(), re.M))


def check_contract(core_shm, client_shm, cached_client):
    issues = []
    if not client_shm[0] <= core_shm[0] < core_shm[1] <= client_shm[1]:
        issues.append('OP-TEE reserved SHM is outside BL33 client allowed range')
    if not cached_client:
        issues.append('fixed OP-TEE MEM_AREA_NSEC_SHM is cached, but BL33 selects uncached transport')
    return issues


def inspect_b2(root, platform, log_dir):
    require(platform == 'zynqmp', 'ZCU106-only integration checker')
    board = True
    core_dir = root / 'build/external' / ('optee-zynqmp' if board else 'optee')
    release = root / 'build/external' / ('tfa-zynqmp-b2/zynqmp/release' if board else 'tfa-qemu-b2/qemu/release')
    bl31 = release / 'bl31/bl31.elf'
    bl32 = core_dir / 'core/tee.elf'
    bl33 = root / ('build/zcu106/optee_bl33.elf' if board else 'build/qemu/external/optee_bl33.elf')
    tfa_command = read_command(log_dir / ('external-tfa-zynqmp-b2.log' if board else 'external-tfa-qemu-b2.log'))
    core_command = read_command(log_dir / ('external-optee-zynqmp.log' if board else 'external-optee.log'))
    require('SPD=opteed' in tfa_command, 'B2 TF-A command must specify SPD=opteed')
    dispatcher = elf_symbols(bl31)
    require(all(name in dispatcher[2] for name in ('opteed_setup', 'opteed_smc_handler')), 'BL31 does not contain linked OP-TEE dispatcher')
    config = read_config(core_dir / 'conf.mk')
    require(config.get('CFG_EARLY_TA') == 'y' and config.get('CFG_WITH_ARM_TRUSTED_FW') == 'y', 'Core Early TA/TF-A integration disabled')
    require(config.get('CFG_CORE_RESERVED_SHM') == 'y', 'reserved SHM transport required by client')
    core = elf_symbols(bl32)
    embedded = []
    for variant, uuid in zip(('a', 'b'), TA_UUIDS):
        path = root / 'build/external' / ('ta-' + variant) / (uuid + '.stripped.elf')
        blob = symbol_payload(core, 'ts_bin_' + uuid.replace('-', ''))
        require(zlib.decompress(blob) == path.read_bytes(), 'Early TA differs from linked Core payload: ' + uuid)
        embedded.append(dict(uuid=uuid, ta_sha256=hashlib.sha256(path.read_bytes()).hexdigest(), linked_payload_sha256=hashlib.sha256(blob).hexdigest()))
    images = {role: elf_inventory(path) for role, path in (('bl31', bl31), ('bl32', bl32), ('bl33', bl33))}
    shm = (int(config['CFG_SHMEM_START'], 0), int(config['CFG_SHMEM_START'], 0) + int(config['CFG_SHMEM_SIZE'], 0))
    header = (root / ('platform/zcu106/b2/memory_map.h' if board else 'platform/qemu/external/memory_map.h')).read_text()
    def macro(name):
        match = re.search(r'^#define ' + name + r' (0x[0-9a-fA-F]+)$', header, re.M)
        require(match is not None, 'missing client memory macro: ' + name)
        return int(match[1], 0)
    client = (macro('SHARED_MEMORY_BASE'), macro('SHARED_MEMORY_END'))
    build_path = log_dir / ('build-bl33.log' if board else 'build-qemu-bl33.log')
    if board and not build_path.exists():
        build_path = log_dir / 'build-qemu.log'
    build_log = build_path.read_text()
    cached_client = any('-DTEE_SHARED_CACHED=1' in line and 'optee_bl33.elf' in line for line in build_log.splitlines())
    issues = check_contract(shm, client, cached_client)
    hardware_blockers = []
    board_plan = None
    firmware_comparison = []
    require(images['bl32']['entry'] == int(config['CFG_TZDRAM_START'], 0), 'Core entry differs from effective TZDRAM base')
    if board:
        binding = json.loads((root / 'platform/zcu106/boot_plan.json').read_text())
        require(not b2_binding_issues(binding), 'B2 boot plan selects wrong artifacts or dispatcher')
        require('PRELOADED_BL33_BASE=' + hex(images['bl33']['entry']) in tfa_command, 'JTAG fallback BL33 entry differs from ELF entry')
        from zcu106_b2_layout import load_layout
        board_plan, reservations = load_layout(root)
        require(binding.get('software_memory_plan') == 'platform/zcu106/b2/memory_map.json' and binding['allocations'] == board_plan['regions'], 'boot plan differs from B2 software reservations')
        for role in ('bl31','bl32','bl33'):
            owned = [region for region in reservations.values() if region['owner'] == role]
            if any(not any(region['base'] <= load['base'] < load['end'] <= region['end'] for region in owned) for load in images[role]['loads']):
                issues.append(role + ' ELF including BSS escapes B2-specific reservations')
        for prefix, region in (('ZYNQMP_ATF_MEM', reservations['bl31_reserved']), ('ZYNQMP_BL32_MEM', reservations['bl32_reserved'])):
            require(prefix+'_BASE='+hex(region['base']) in tfa_command and prefix+'_SIZE='+hex(region['size']) in tfa_command, 'TF-A reservation arguments differ from B2 plan')
        require(images['bl33']['entry'] == reservations['bl33_code']['base'], 'BL33 entry differs from B2 plan')
        require(shm == (reservations['shared_memory']['base'],reservations['shared_memory']['end']), 'Core SHM differs from B2 plan')
        require(int(config['CFG_TZDRAM_START'],0) == reservations['bl32_reserved']['base'] and int(config['CFG_TZDRAM_SIZE'],0) == reservations['bl32_reserved']['size'], 'Core TZDRAM differs from B2 plan')
        client_image = elf_symbols(bl33)
        require(all(name in client_image[2] for name in ('platform_prepare_tee_client','zcu106_b2_build_page_tables','client_tables','__b2_vectors_start','__b2_vectors_end')), 'BL33 lacks B2 MMU or vector reservation')
        require(symbol_payload(client_image,'zcu106_b2_runtime_ready') == bytes(4), 'B2 runtime gate must remain blocked before hardware review')
        table = client_image[2]['client_tables']
        signed_profile = 'b2_signed_profile' in client_image[2] and symbol_payload(client_image,'b2_signed_profile') == bytes([1,0,0,0])
        if signed_profile:
            require(config.get('CFG_REE_FS_TA')=='y' and config.get('CFG_PREALLOC_RPC_CACHE')=='n', 'signed BL33 requires matching runtime-loader Core profile')
            from check_signed_layout import check as check_signed_layout
            check_signed_layout(root,'zcu106',bl33)
        require(table['address'] % 4096 == 0 and table['size'] == (7 if signed_profile else 6) * 4096 and reservations['bl33_data']['base'] <= table['address'] < table['address']+table['size'] <= reservations['bl33_data']['end'], 'BL33 page tables escape data reservation')
        vector_base = client_image[2]['__b2_vectors_start']['address']
        vector_end = client_image[2]['__b2_vectors_end']['address']
        require(vector_base % 4096 == 0 and vector_end > vector_base and vector_end % 4096 == 0 and any(load['base'] == vector_base and load['end'] == vector_end and load['flags'] == 5 for load in images['bl33']['loads']), 'BL33 vectors must have isolated RX pages')
        observations = json.loads((root / 'platform/zcu106/vitis_observations.json').read_text())
        ddr = next(bank for bank in observations['memory_banks'] if bank['name'] == 'DDR0')
        require(board_plan['reported_ddr0']['base'] == ddr['base'] and board_plan['reported_ddr0']['end'] == ddr['end'] and int(config['CFG_DDR_SIZE'],0) == ddr['end']-ddr['base'], 'DDR registration differs from reported DDR0 bounds')
        for owner, firmware in observations['firmware'].items():
            for load in firmware['segments']:
                for region in reservations.values():
                    require(max(load['physical'],region['base']) >= min(load['end'],region['end']), 'B2 reservation overlaps reported firmware: '+owner+'/'+region['name'])
                firmware_comparison.append(dict(owner=owner, base=load['physical'],end=load['end'],reported_load_overlap=False,address_aliases_verified=False))
        hardware_blockers += ['FSBL/PMUFW runtime reservations and processor/system address aliases unverified',
                              'B2 software reservations are not confirmed board reservations; XMPU/master/locks unverified',
                              'Live boot mode, EL2/stage2 and incoming EL1 MMU/cache/dirty state unverified',
                              'UART clock/MIO and GIC/other-core state unverified',
                              'B2_RUNTIME_READY remains 0; hardware load gates remain blocked']
    require(not issues, '; '.join(issues))
    return dict(configuration='B2', platform=platform, artifact_status='PASS',
                software_contract_status='PASS', integration_status='BLOCKED' if hardware_blockers else 'PASS', hardware_load_status='BLOCKED', hardware_execution='NOT_RUN',
                qemu_execution='NOT_APPLICABLE_TO_THIS_PACKAGE',
                linked_dispatcher='opteed', linked_early_tas=embedded, images=images,
                core_reserved_shm=list(shm), client_allowed_shm=list(client), client_cached_transport=cached_client,
                tfa_command=tfa_command, core_command=core_command, software_issues=issues, hardware_blockers=hardware_blockers, integration_issues=hardware_blockers,
                software_memory_plan=board_plan, reported_firmware_comparison=firmware_comparison,
                core_canary_validation_overlay='--validation-pta' in core_command or 'CFG_TZ_VALIDATION_PTA=y' in core_command,
                deferred_tests=['GPIO', 'DMA'], scope='linked artifact identity, software memory/cache contracts and build binding; not hardware protection evidence')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--platform', choices=('zynqmp',), required=True)
    parser.add_argument('--root', type=Path, default=ROOT)
    parser.add_argument('--log-dir', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--require-integrated', action='store_true')
    args = parser.parse_args()
    try:
        report = inspect_b2(args.root, args.platform, args.log_dir or args.root / 'results/build')
    except FileNotFoundError as error:
        report = dict(configuration='B2', artifact_status='BLOCKED', integration_status='BLOCKED', hardware_load_status='BLOCKED', hardware_execution='NOT_RUN', error=str(error))
    except (ValueError, KeyError, TypeError, IndexError, UnicodeError, struct.error, zlib.error, OSError) as error:
        report = dict(configuration='B2', artifact_status='FAIL', integration_status='BLOCKED', hardware_load_status='BLOCKED', hardware_execution='NOT_RUN', error=str(error))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=4) + '\n')
    print(json.dumps(report, indent=2))
    if report['artifact_status'] == 'FAIL':
        return 1
    return 2 if report['artifact_status'] == 'BLOCKED' or args.require_integrated and report['integration_status'] != 'PASS' else 0


if __name__ == '__main__':
    raise SystemExit(main())
