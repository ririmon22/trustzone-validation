import ast
import json
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def check(root):
    files = json.loads((root / 'offline-transfer.files.json').read_text())
    known = set(files)
    issues = []
    forbidden = ('build/', 'results/', '__pycache__/', 'third_party/tfa-amd/',
                 'third_party/optee/', 'third_party/pyelftools/',
                 'third_party/compiler-rt/', 'third_party/embeddedsw/')
    for name in files:
        path = root / name
        if name.startswith(forbidden) or path.suffix in ('.elf', '.bin', '.log', '.pyc', '.o', '.a', '.ta'):
            issues.append('Forbidden transfer file: ' + name)
        if not path.is_file():
            issues.append('Missing transfer file: ' + name)
            continue
        if path.suffix == '.py':
            try:
                ast.parse(path.read_text(), filename=name)
            except SyntaxError as error:
                issues.append(str(error))
        if path.suffix == '.sh':
            result = subprocess.run(['bash', '-n', str(path)], capture_output=True, text=True)
            if result.returncode:
                issues.append(name + ': ' + result.stderr)
        if path.suffix != '.md':
            continue
        for target in re.findall(r'\]\(([^)\n]+)\)', path.read_text()):
            if '://' in target or target.startswith(('#', 'mailto:')):
                continue
            target = target.split('#')[0]
            try:
                relative = (path.parent / target).resolve().relative_to(root.resolve()).as_posix()
            except ValueError:
                issues.append(name + ': link escapes transfer root: ' + target)
                continue
            if relative not in known and not any(item.startswith(relative.rstrip('/') + '/') for item in known):
                issues.append(name + ': missing packaged link: ' + target)
    header = (root / 'platform/zcu106/b2/memory_map.h').read_text()
    if '#define B2_RUNTIME_READY 0' not in header:
        issues.append('B2 runtime guard changed')
    plan = json.loads((root / 'platform/zcu106/boot_plan.json').read_text())
    if plan['integration']['hardware_load_authorized'] is not False or plan['integration']['runtime_ready'] is not False:
        issues.append('B2 boot safety flags changed')
    return dict(status='FAIL' if issues else 'PASS', files=len(files), issues=issues,
                scope='package files, local document links, Python/shell syntax and safety flags only',
                external_dependencies='CHECK_SEPARATELY', hardware_execution='NOT_RUN')


if __name__ == '__main__':
    report = check(ROOT)
    print(json.dumps(report, indent=4))
    raise SystemExit(report['status'] != 'PASS')
