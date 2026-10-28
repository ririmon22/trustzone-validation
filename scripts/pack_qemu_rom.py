import argparse
import json
import struct
from pathlib import Path
from check_memory_map import validate

ROOT = Path(__file__).resolve().parents[1]
MANIFEST_OFFSET = 0x1000
FLASH_CAPACITY = 64 * 1024 * 1024
MAX_SEGMENTS = 32


def pack(stub, paths, unified=False):
    mapping = json.loads((ROOT / "platform/qemu/memory_map.json").read_text())
    if unified:
        from check_a_unified import inspect
        if len(paths) != 1:
            raise ValueError("one unified ELF required")
        inspect('qemu', paths[0])
    else:
        validate(mapping, elf_paths=paths)
    segments = []
    entries = {}
    for path in paths:
        data = path.read_bytes()
        entries[path.name] = struct.unpack_from("<Q", data, 24)[0]
        offset = struct.unpack_from("<Q", data, 32)[0]
        stride, count = struct.unpack_from("<HH", data, 54)
        for index in range(count):
            kind, flags, source, virtual, physical, filesz, memsz, alignment = struct.unpack_from("<IIQQQQQQ", data, offset + index * stride)
            if kind == 1 and memsz:
                segments.append((physical, memsz, data[source:source + filesz]))
    if len(stub) > MANIFEST_OFFSET or not 0 < len(segments) <= MAX_SEGMENTS:
        raise ValueError("bootstrap/segment count exceeds limits")
    if not unified and set(entries) != {"el3.elf", "secure.elf", "nonsecure.elf"}:
        raise ValueError("all three baremetal payloads required exactly once")
    payload_offset = MANIFEST_OFFSET + 16 + 32 * len(segments)
    records, payload = bytearray(), bytearray()
    for destination, size, data in segments:
        records.extend(struct.pack("<QQQQ", payload_offset + len(payload), destination, len(data), size))
        payload.extend(data)
    rom = stub.ljust(MANIFEST_OFFSET, b"\0") + struct.pack("<QQ", len(segments), (next(iter(entries.values())) if unified else entries["el3.elf"])) + records + payload
    if len(rom) > FLASH_CAPACITY:
        raise ValueError("ROM exceeds QEMU flash capacity")
    return rom


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--stub", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--unified", action="store_true")
    parser.add_argument("elf", nargs="+", type=Path)
    args = parser.parse_args()
    args.output.write_bytes(pack(args.stub.read_bytes(), args.elf, args.unified))
    print("ROM PACKED: Secure EL3 bootstrap copies validated ELF segments")


if __name__ == "__main__":
    main()
