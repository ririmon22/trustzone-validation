import argparse
import hashlib
import json
from pathlib import Path
from check_boot_plan import elf_inventory

if __name__=='__main__':
    parser=argparse.ArgumentParser(description='Read-only inventory; never assume hardware-used files equal repository builds')
    parser.add_argument('--input',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    config=json.loads(args.input.read_text())
    report=dict(scope='user-supplied hardware-used inputs; no board access',roles={},hardware_execution='NOT_RUN')
    for name in ['bl31','bl32','bl33','fsbl','pmufw','xsa','settings','source_archive']:
        value=config.get(name)
        record={'status':'NOT_PROVIDED','same_as_repository':'UNKNOWN'}
        if value:
            p=(args.input.resolve().parent/value).resolve()
            record={'path':str(p),'status':'MISSING','same_as_repository':'UNKNOWN'}
            if p.is_file():
                record.update(status='USER_SUPPLIED_NOT_HARDWARE_VERIFIED',sha256=hashlib.sha256(p.read_bytes()).hexdigest(),bytes=p.stat().st_size)
                if name in ('bl31','bl32','bl33','fsbl','pmufw'):record['elf']=elf_inventory(p)
        report['roles'][name]=record
    input_paths={args.input.resolve()} | {(args.input.resolve().parent/value).resolve() for value in config.values() if isinstance(value,str)}
    if args.output.resolve() in input_paths:parser.error('output must preserve supplied originals')
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(report,indent=4)+'\n')
    print(args.output)
