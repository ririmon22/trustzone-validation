"""追加PTAをビルド用コピーへ配置する。JSON設定は読まない。"""
import argparse
import shutil
from pathlib import Path
from stage_signed_loader import apply

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--source', type=Path, required=True)
parser.add_argument('--destination', type=Path, required=True)
parser.add_argument('--profile', choices=('signed', 'physical'), required=True)
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
# 既存コピーを黙って削除しない。設定を変更する場合は利用者が退避して作り直す。
if args.destination.exists():
    parser.exit(2, 'Core copy exists: move it aside before prepare-core/bl32\n')
shutil.copytree(args.source, args.destination, ignore=shutil.ignore_patterns('.git', '__pycache__'))
names = ['validation_canary']
if args.profile == 'physical':
    names.append('physical_probe')
for name in names:
    for suffix in ('.c', '.h'):
        shutil.copyfile(root / 'secure_world/pta' / (name + suffix), args.destination / 'core/pta' / (name + suffix))
with (args.destination / 'core/pta/sub.mk').open('a') as output:
    output.write('\nsrcs-$(CFG_TZ_VALIDATION_PTA) += validation_canary.c\n')
    if args.profile == 'physical':
        output.write('srcs-$(CFG_TZ_PHYSICAL_PTA) += physical_probe.c\n')
if args.profile == 'signed':
    apply(root, args.destination)
