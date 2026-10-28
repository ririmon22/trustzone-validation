import argparse
import json
import struct
from pathlib import Path

MAGIC = 0x545A4C47
HEADER_SIZE = 32
MAX_CAPACITY = 16 * 1024 * 1024


def decode(data):
    if len(data) < HEADER_SIZE:
        raise ValueError('short RAM log header')
    magic, version, capacity, used, dropped, state, run_id = struct.unpack_from('<6IQ', data)
    if magic != MAGIC or version != 1 or capacity == 0 or capacity > MAX_CAPACITY:
        raise ValueError('unsupported/uninitialized RAM log')
    if used > capacity or len(data) != HEADER_SIZE + capacity or state not in (1, 2, 3):
        raise ValueError('invalid or torn RAM log')
    return dict(version=version, run_id=run_id, used=used, capacity=capacity, dropped=dropped,
                state={1: 'RUNNING', 2: 'COMPLETE', 3: 'FATAL'}[state],
                collection_status='COMPLETE' if state == 2 and dropped == 0 else 'PARTIAL',
                text=data[HEADER_SIZE:HEADER_SIZE + used].decode('utf-8', errors='replace'))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('dump', type=Path)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--expected-run-id', type=lambda value: int(value, 0))
    args = parser.parse_args()
    if args.output.resolve() == args.dump.resolve():
        parser.error("Output must preserve the raw dump")
    try:
        record = decode(args.dump.read_bytes())
        if args.expected_run_id is not None and record['run_id'] != args.expected_run_id:
            raise ValueError('run ID mismatch; preserve raw dump and do not treat a stale log as current')
    except (OSError, ValueError) as error:
        parser.exit(1, 'RAM LOG FAIL: ' + str(error) + '\n')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(record.pop('text'))
    args.output.with_suffix(args.output.suffix + '.json').write_text(json.dumps(record, indent=4) + '\n')
    print(json.dumps(record, indent=4))
    raise SystemExit(0 if record['collection_status'] == 'COMPLETE' else 2)
