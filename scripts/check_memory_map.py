import argparse
import json
import re
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def load_segments(path):
    data = Path(path).read_bytes()
    if len(data) < 64 or data[:6] != b"\x7fELF\x02\x01":
        raise ValueError(f"{path}: little-endian ELF64 required")
    machine = struct.unpack_from("<H", data, 18)[0]
    if machine != 183:
        raise ValueError("AArch64 ELF required")
    offset = struct.unpack_from("<Q", data, 32)[0]
    size, count = struct.unpack_from("<HH", data, 54)
    if size != 56 or offset + size * count > len(data):
        raise ValueError("invalid program header table")
    segments = []
    for index in range(count):
        kind, flags, file_offset, virtual, physical, file_size, mem_size, alignment = struct.unpack_from("<IIQQQQQQ", data, offset + size * index)
        if kind == 1 and mem_size:
            if file_size > mem_size or file_offset + file_size > len(data) or virtual != physical:
                raise ValueError("invalid or non-identity load segment")
            if alignment and (alignment & (alignment - 1) or physical % alignment != file_offset % alignment):
                raise ValueError("ELF segment alignment mismatch")
            segments.append((physical, physical + mem_size, flags))
    if not segments:
        raise ValueError("ELF has no load segments")
    entry = struct.unpack_from("<Q", data, 24)[0]
    if not any(start <= entry < end and flags & 1 for start, end, flags in segments):
        raise ValueError("ELF entry outside executable segment")
    return segments


def validate_protection_domains(mapping, header=None, require_protection=False):
    domains = mapping.get("protection_domains", [])
    if mapping["platform"] == "zcu106" and len(domains) != 1:
        raise ValueError("one dedicated DDR XMPU protection domain required")
    for domain in domains:
        start, end = domain["base"], domain["end"]
        if start < 0 or start >= end or end - start != domain["size"] or end > 2 ** 40:
            raise ValueError("invalid protection domain bounds")
        if domain["alignment"] != 0x100000 or start % 0x100000 or end % 0x100000:
            raise ValueError("DDR XMPU protection domain requires 1 MiB alignment")
        if domain["allowed_regions"] != ["protected_buffer"] or domain["security"] != "secure":
            raise ValueError("protection domain may contain only the Secure test buffer")
        for region in mapping["regions"]:
            if region["name"] == "protected_buffer":
                if region["security"] != "secure" or not start <= region["base"] < region["end"] <= end:
                    raise ValueError("test buffer outside protection domain")
            elif max(start, region["base"]) < min(end, region["end"]):
                raise ValueError("protection domain overlaps allocation: " + region["name"])
        for reservation in mapping.get("external_reservations", []):
            if reservation.get("confirmed") is not True:
                if require_protection:
                    raise ValueError("BLOCKED: unresolved firmware reservation: " + reservation["owner"])
                continue
            if not 0 <= reservation["base"] < reservation["end"] <= 2 ** 40:
                raise ValueError("invalid firmware reservation bounds")
            if max(start, reservation["base"]) < min(end, reservation["end"]):
                raise ValueError("protection domain overlaps firmware: " + reservation["owner"])
        if header is not None:
            for suffix, value in (("BASE", start), ("END", end), ("SIZE", end - start)):
                match = re.search(r"^#define XMPU_PROTECTED_" + suffix + r" (0x[0-9a-f]+)$", header, re.M)
                if not match or int(match[1], 16) != value:
                    raise ValueError("protection domain header mismatch")
        if require_protection and (domain["protection"] == "unconfirmed" or mapping.get("external_reservations_complete") is not True):
            raise ValueError("BLOCKED: protection/firmware reservations not confirmed")


def validate_a_observations(mapping, observations):
    """報告された範囲の静的整合だけを確認し、実機保護成立と区別する。"""
    if mapping.get('platform') != 'zcu106' or observations.get('schema_version') != 1:
        raise ValueError('unsupported ZCU106 observation schema')
    banks = observations.get('memory_banks', [])
    for bank in banks:
        if any(type(bank.get(key)) is not int for key in ('base', 'end', 'size')):
            raise ValueError('invalid reported bank range')
        if not 0 <= bank['base'] < bank['end'] <= 2 ** 64 or bank['end'] != bank['base'] + bank['size']:
            raise ValueError('invalid reported bank end')
    ddr = [bank for bank in banks if bank['name'] == 'DDR0' and bank.get('use_for_a_static_layout') is True]
    if len(ddr) != 1:
        raise ValueError('reported DDR0 allocation range required')
    if any(bank.get('use_for_a_static_layout') for bank in banks if bank['name'] != 'DDR0'):
        raise ValueError('A plan must not use unresolved DDR1 or firmware OCM')
    config = mapping.get('configuration_plans', {}).get('A', {})
    if config.get('active_owners') != ['el3', 'secure', 'nonsecure', 'client']:
        raise ValueError('invalid A allocation owners')
    allocations = [region for region in mapping['regions'] if region['owner'] in config['active_owners']]
    allocations += mapping.get('protection_domains', [])
    for region in allocations:
        if not ddr[0]['base'] <= region['base'] < region['end'] <= ddr[0]['end']:
            raise ValueError('A allocation outside reported DDR0: ' + region['name'])
    known_loads = []
    for role in ('FSBL', 'PMUFW'):
        firmware = observations.get('firmware', {}).get(role, {})
        segments = firmware.get('segments', [])
        entry = firmware.get('entry')
        if type(entry) is not int or not segments:
            raise ValueError('missing reported firmware loads/entry: ' + role)
        for segment in segments:
            if any(type(segment.get(key)) is not int for key in ('physical', 'mem_size', 'end')):
                raise ValueError('invalid reported firmware segment')
            start, end = segment['physical'], segment['end']
            if not 0 <= start < end <= 2 ** 64 or end != start + segment['mem_size']:
                raise ValueError('invalid reported firmware segment end')
            if role == 'FSBL' and not any(bank['name'] == 'OCM' and bank['base'] <= start < end <= bank['end'] for bank in banks):
                raise ValueError('reported FSBL load outside OCM')
            for region in allocations:
                if max(start, region['base']) < min(end, region['end']):
                    raise ValueError('A allocation overlaps reported firmware: ' + region['name'] + '/' + role)
            for previous in known_loads:
                if max(start, previous['base']) < min(end, previous['end']):
                    raise ValueError('reported firmware load overlap: ' + previous['owner'] + '/' + role)
            known_loads.append(dict(owner=role, base=start, end=end))
        if not any(segment['physical'] <= entry < segment['end'] for segment in segments):
            raise ValueError('reported firmware entry outside loads: ' + role)
    return dict(static_status='PASS', scope='reported numeric ranges only; firmware address aliases/runtime unknown',
                allocation_names=[region['name'] for region in allocations], reported_firmware_loads=known_loads,
                load_status='BLOCKED', hardware_execution='NOT_RUN')


def validate(mapping, header=None, linkers=None, elf_paths=(), require_protection=False, observations=None):
    regions = mapping["regions"]
    names = [region["name"] for region in regions]
    if len(set(names)) != len(names):
        raise ValueError("duplicate region name")
    required = {"el3_firmware", "exception_vector", "secure_code", "secure_data", "secure_stack", "el3_stack", "nonsecure_code", "nonsecure_data", "nonsecure_stack", "shared_memory", "protected_buffer", "tfa_reserved", "optee_reserved"}
    if set(names) != required:
        raise ValueError("missing or unexpected region")
    for region in regions:
        start, end, size, alignment = (region[key] for key in ("base", "end", "size", "alignment"))
        if start < 0 or size <= 0 or end != start + size or end > 2 ** 64:
            raise ValueError(f"{region['name']}: invalid end or size")
        if alignment <= 0 or alignment & (alignment - 1) or start % alignment or size % alignment:
            raise ValueError(f"{region['name']}: invalid alignment")
        if region["name"] == "exception_vector" and start % 2048:
            raise ValueError("vector alignment")
        if region["security"] not in ("secure", "nonsecure", "shared") or region["protection"] not in ("qemu-secure-ram", "none", "unconfirmed"):
            raise ValueError("unknown security/protection attribute")
        if region["protection"] == "qemu-secure-ram" and (mapping["platform"] != "qemu" or region["security"] != "secure"):
            raise ValueError("protection/platform/security mismatch")
        if region["security"] == "secure" and region["protection"] == "qemu-secure-ram":
            window = mapping["protection_window"]
            if not window or not (window[0] <= start < end <= window[1]):
                raise ValueError("secure region outside protection window")
        if region["name"] == "shared_memory" and region["security"] != "shared":
            raise ValueError("shared memory security mismatch")
        if header is not None:
            for suffix, value in (("BASE", start), ("END", end), ("SIZE", size)):
                match = re.search(r"^#define " + region["name"].upper() + "_" + suffix + r" (0x[0-9a-f]+)$", header, re.M)
                if not match or int(match[1], 16) != value:
                    raise ValueError("header mismatch: " + region["name"])
    ordered = sorted(regions, key=lambda region: region["base"])
    for left, right in zip(ordered, ordered[1:]):
        if left["end"] > right["base"]:
            raise ValueError(f"overlap: {left['name']} / {right['name']}")
    validate_protection_domains(mapping, header, require_protection)
    if require_protection and (not mapping["confirmed"] or any(region["security"] == "secure" and region["protection"] in ("unconfirmed", "none") for region in regions)):
        raise ValueError("BLOCKED: hardware map/protection not confirmed")
    by_name = {region["name"]: region for region in regions}
    if linkers:
        for image, script in linkers.items():
            code = {"el3": "el3_firmware", "secure": "secure_code", "nonsecure": "nonsecure_code"}[image]
            locations = [int(value, 16) for value in re.findall(r"\. = (0x[0-9a-f]+);", script)]
            if not locations or locations[0] != by_name[code]["base"]:
                raise ValueError("linker origin mismatch: " + image)
            stack = by_name[{"el3": "el3_stack", "secure": "secure_stack", "nonsecure": "nonsecure_stack"}[image]]["end"]
            if f"__stack_top = 0x{stack:x};" not in script:
                raise ValueError("linker stack mismatch")
            if mapping.get('runtime_stack_policy') == 'elf-noload':
                stack_region = by_name[{'el3': 'el3_stack', 'secure': 'secure_stack', 'nonsecure': 'nonsecure_stack'}[image]]
                if '.stack (NOLOAD)' not in script or f'. = 0x{stack_region["base"]:x};' not in script or f'. += 0x{stack_region["size"]:x};' not in script:
                    raise ValueError('linker stack reservation mismatch: ' + image)
    all_segments = []
    for elf_path in elf_paths:
        filename = Path(elf_path).name
        image = {"el3.elf": "el3", "secure.elf": "secure", "nonsecure.elf": "nonsecure", "tfa_bl33.elf": "nonsecure", "optee_bl33.elf": "nonsecure"}.get(filename)
        if image is None:
            raise ValueError("unknown image role: " + filename)
        allowed = {"el3": {"el3_firmware", "exception_vector"}, "secure": {"secure_code", "secure_data"}, "nonsecure": {"nonsecure_code", "nonsecure_data"}}[image]
        segments = load_segments(elf_path)
        if mapping.get('runtime_stack_policy') == 'elf-noload':
            stack_name = {'el3': 'el3_stack', 'secure': 'secure_stack', 'nonsecure': 'nonsecure_stack'}[image]
            stack_region = by_name[stack_name]
            if not any((start, end, flags) == (stack_region['base'], stack_region['end'], 6) for start, end, flags in segments):
                raise ValueError('ELF runtime stack reservation missing or invalid: ' + image)
            allowed = allowed | {stack_name}
        for start, end, flags in segments:
            if not any(region["name"] in allowed and region["base"] <= start < end <= region["end"] for region in regions):
                raise ValueError(f"{elf_path}: segment outside assigned region {start:#x}..{end:#x}")
            if flags & 3 == 3:
                raise ValueError("writable executable load segment")
            for previous_start, previous_end in all_segments:
                if max(start, previous_start) < min(end, previous_end):
                    raise ValueError("ELF load segments overlap")
            all_segments.append((start, end))
    if observations is not None:
        validate_a_observations(mapping, observations)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--platform", choices=("qemu", "zcu106"), required=True)
    parser.add_argument("--elf", action="append", default=[])
    parser.add_argument("--require-protection", action="store_true")
    args = parser.parse_args()
    directory = ROOT / "platform" / args.platform
    try:
        mapping = json.loads((directory / "memory_map.json").read_text())
        observations = json.loads((directory / mapping['hardware_observations']).read_text()) if mapping.get('hardware_observations') else None
        validate(mapping, (directory / "memory_map.h").read_text(), {image: (directory / f"linker_{image}.ld").read_text() for image in ("el3", "secure", "nonsecure")}, args.elf, args.require_protection, observations)
    except (ValueError, KeyError, TypeError, AttributeError, OSError, struct.error) as error:
        parser.exit(1, f"MEMORY_MAP FAIL: {error}\n")
    print("MEMORY_MAP PASS: layout/ELF checks only; protection execution is separate")
    if not mapping["confirmed"]:
        print("HARDWARE_PREPARATION BLOCKED: map and protection remain provisional")


if __name__ == "__main__":
    main()
