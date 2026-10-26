import argparse
import hashlib
import json
import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SHDR_FORMAT = '<IIIIHH'
SHDR_SIZE = struct.calcsize(SHDR_FORMAT)
SHDR_MAGIC = 0x4F545348
SHDR_BOOTSTRAP = 1


def mutated_images(data):
    if len(data) < SHDR_SIZE:
        raise ValueError('short signed header')
    magic, kind, image_size, algorithm, hash_size, signature_size = struct.unpack_from(SHDR_FORMAT, data)
    payload_offset = SHDR_SIZE + hash_size + signature_size
    if magic != SHDR_MAGIC or kind != SHDR_BOOTSTRAP or payload_offset + 20 + image_size != len(data):
        raise ValueError('unsupported bootstrap TA layout')
    images = {}
    for name, offset in [('signature', SHDR_SIZE + hash_size), ('payload', len(data) - 1)]:
        changed = bytearray(data)
        changed[offset] ^= 1
        images[name] = bytes(changed)
    return images


def main():
    try:
        from cryptography.hazmat.primitives import serialization
        from cryptography.hazmat.primitives.asymmetric import rsa
    except ImportError as error:
        raise SystemExit("SIGNATURE CHECK BLOCKED: Python cryptography is required: " + str(error))
    parser = argparse.ArgumentParser()
    parser.add_argument('--ta', type=Path, required=True)
    parser.add_argument('--uuid', required=True)
    parser.add_argument('--output', type=Path, default=ROOT / 'results/phase5/signed-ta')
    args = parser.parse_args()
    from source_snapshot import verify_source
    lock = json.loads((ROOT / 'third_party/dependencies.lock.json').read_text())
    verify_source(ROOT / 'third_party/optee', lock['optee'])
    args.output.mkdir(parents=True, exist_ok=True)
    key = serialization.load_pem_private_key((ROOT / 'third_party/optee/keys/default_ta.pem').read_bytes(), password=None)
    public_path = args.output / 'development-public.pem'
    public_path.write_bytes(key.public_key().public_bytes(serialization.Encoding.PEM, serialization.PublicFormat.SubjectPublicKeyInfo))
    wrong = rsa.generate_private_key(public_exponent=65537, key_size=2048)
    wrong_path = args.output / 'wrong-public.pem'
    wrong_path.write_bytes(wrong.public_key().public_bytes(serialization.Encoding.PEM, serialization.PublicFormat.SubjectPublicKeyInfo))
    original = args.ta.read_bytes()
    cases = [('valid', args.ta, public_path, args.uuid, True)]
    for name, data in mutated_images(original).items():
        path = args.output / ('tampered-' + name + '.ta')
        path.write_bytes(data)
        cases.append((name, path, public_path, args.uuid, False))
    cases += [('wrong-key', args.ta, wrong_path, args.uuid, False),
              ('wrong-uuid', args.ta, public_path, '00000000-0000-0000-0000-000000000000', False)]
    results = []
    for name, path, public, uuid, accepted in cases:
        command = [sys.executable, str(ROOT / 'third_party/optee/scripts/sign_encrypt.py'), 'verify',
                   '--uuid', uuid, '--in', str(path), '--key', str(public)]
        completed = subprocess.run(command, capture_output=True, text=True)
        log = args.output / (name + '.log')
        log.write_text('COMMAND: ' + repr(command) + '\n' + completed.stdout + completed.stderr + f'\nexit={completed.returncode}\n')
        results.append(dict(case=name, status='PASS' if (completed.returncode == 0) == accepted else 'FAIL',
                            scope='HOST TOOL ONLY; not OP-TEE runtime acceptance', expected_accepted=accepted,
                            exit_code=completed.returncode, evidence=str(log), ta_sha256=hashlib.sha256(path.read_bytes()).hexdigest()))
    (args.output / 'results.json').write_text(json.dumps(results, indent=4) + '\n')
    print(json.dumps(results, indent=2))
    return int(any(row['status'] != 'PASS' for row in results))


if __name__ == '__main__':
    raise SystemExit(main())
