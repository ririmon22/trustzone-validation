import argparse
import json
import re
from pathlib import Path
from parse_results import parse_log

ROOT = Path(__file__).resolve().parents[1]


def validate_faults(normal, secure):
    requests = re.findall(r'^PROBE_FAULT domain=(ta|core) address=(0x[0-9a-f]+) write=([01])$', normal, re.M)
    faults = re.findall(r'User mode data-abort at address (0x[0-9a-f]+) \(translation fault\)\s*\n[^\n]*esr (0x[0-9a-f]+)', secure)
    if len(requests) != len(faults) or not requests:
        raise ValueError('missing or unexpected Secure data aborts')
    records = []
    for (domain, address, write), (far, syndrome) in zip(requests, faults):
        esr = int(syndrome, 16)
        if int(address, 16) != int(far, 16) or (esr >> 6) & 1 != int(write) or esr >> 26 != 0x24 or esr & 0x3F not in (4, 5, 6, 7) or esr & (1 << 10):
            raise ValueError('fault address/direction/class mismatch')
        records.append(dict(domain=domain, address=address, write=bool(int(write)), esr=syndrome))
    for test_id, domain in (('TEE-ISO-TA', 'ta'), ('TEE-ISO-CORE', 'core')):
        if re.search(r'^RESULT ' + test_id + r' PASS ', normal, re.M):
            if [(row['domain'], row['write']) for row in records if row['domain'] == domain] != [(domain, False), (domain, True)]:
                raise ValueError(domain + ' PASS requires read and write evidence')
    if re.search(r'^RESULT TEE-03 PASS ', normal, re.M) and not all(re.search(r'^RESULT ' + test_id + r' PASS ', normal, re.M) for test_id in ('TEE-ISO-TA', 'TEE-ISO-CORE')):
        raise ValueError('combined isolation PASS lacks component results')
    return records


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('normal', type=Path)
    parser.add_argument('secure', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    try:
        records = validate_faults(args.normal.read_text(), args.secure.read_text())
    except (OSError, ValueError) as error:
        parser.exit(1, 'SECURITY EVIDENCE FAIL: ' + str(error) + '\n')
    args.output.write_text(json.dumps(records, indent=4) + '\n')
    print('SECURITY EVIDENCE PASS: Secure FAR/ESR match requested probes')


if __name__ == '__main__':
    main()
