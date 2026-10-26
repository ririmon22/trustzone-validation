import argparse
import ast
import json
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
NAMES = (
    '00_overview', '01_environment_setup', '02_vitis_platform_creation',
    '03_vitis_application_creation', '04_baremetal_build', '05_tfa_build',
    '06_optee_build', '07_jtag_baremetal', '08_jtag_tfa', '09_jtag_optee',
    '10_uart_logging', '11_test_execution', '12_performance_measurement',
    '13_troubleshooting', '14_hardware_safety_check', '15_offline_source_transfer',
)
SECTIONS = ('目的', '前提条件・事前確認事項', '必要なファイル・入力ファイル',
            'GUIのメニュー名', '操作手順・実行コマンド',
            '出力ファイル・実行ログの保存先', '正常時の確認方法', 'エラー時の対応',
            'PASS／FAIL／BLOCKED／NOT_RUN判定・実環境での確認状況',
            '公式資料の文書番号と章節')


def check():
    issues, files = [], []
    for name in NAMES:
        path = ROOT / 'docs/manual' / (name + '.md')
        if not path.is_file():
            issues.append('Missing manual: ' + name)
            continue
        content = path.read_text()
        for section in SECTIONS:
            if '## ' + section not in content:
                issues.append(name + ': missing section ' + section)
        if not re.search(r'^1\. ', content, re.M):
            issues.append(name + ': missing numbered operations')
        for link in re.findall(r'\]\(([^)]+)\)', content):
            if '://' in link or link.startswith('#'):
                continue
            if not (path.parent / link.split('#')[0]).exists():
                issues.append(name + ': missing local link ' + link)
        blocks = re.findall(r'```bash\n(.*?)\n```', content, re.S)
        for index, block in enumerate(blocks):
            syntax = subprocess.run(['bash', '-n'], input=block, capture_output=True, text=True)
            if syntax.returncode:
                issues.append(f'{name}: shell block {index}: {syntax.stderr}')
            for python in re.findall(r"python3 - <<'EOF'\n(.*?)\nEOF", block, re.S):
                try:
                    ast.parse(python)
                except SyntaxError as error:
                    issues.append(name + ': Python snippet syntax: ' + str(error))
        files.append(dict(path=str(path.relative_to(ROOT)), shell_blocks=len(blocks)))
    return dict(status='FAIL' if issues else 'PASS', files=files, issues=issues,
                scope='document completeness, local links and snippet syntax only; no GUI, hardware or command execution',
                hardware_execution='NOT_RUN')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--output', type=Path, default=ROOT / 'results/manual/manual-check.json')
    args = parser.parse_args()
    result = check()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=4, ensure_ascii=False) + '\n')
    print(json.dumps(result, indent=2, ensure_ascii=False))
    return int(result['status'] != 'PASS')


if __name__ == '__main__':
    raise SystemExit(main())
