import argparse
import json
import struct
from pathlib import Path


def decode(data):
    if len(data) != 328:
        raise ValueError('exception record size mismatch')
    values = struct.unpack('<41Q', data)
    record = dict(zip(('valid', 'level', 'vector', 'esr', 'far', 'elr', 'spsr', 'sp', 'sp_el0', 'sp_el1'), values[:10]))
    record['x'] = list(values[10:])
    record['status'] = {0: 'NOT_CAPTURED', 1: 'PARTIAL', 2: 'CAPTURED'}.get(record['valid'], 'INVALID')
    record['ec'] = (record['esr'] >> 26) & 63
    record['iss'] = record['esr'] & ((1 << 25) - 1)
    record['scope'] = 'register evidence only; fault cause and protection require separate review'
    return record


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('dump', type=Path)
    args = parser.parse_args()
    record = decode(args.dump.read_bytes())
    print(json.dumps(record, indent=4))
    raise SystemExit(0 if record['status'] == 'CAPTURED' else 2)
