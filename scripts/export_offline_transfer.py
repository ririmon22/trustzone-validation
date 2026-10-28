import argparse
import hashlib
import json
import tarfile
from pathlib import Path
from check_offline_transfer import check

ROOT=Path(__file__).resolve().parents[1]


if __name__=='__main__':
    parser=argparse.ArgumentParser()
    parser.add_argument('--output',type=Path,required=True,help='new output directory; no existing exports are overwritten')
    args=parser.parse_args()
    report=check(ROOT)
    if report['status']!='PASS':parser.error('Transfer checks failed: '+repr(report['issues']))
    if args.output.exists():parser.error('Output exists; choose a new export directory')
    args.output.mkdir(parents=True)
    files=json.loads((ROOT/'offline-transfer.files.json').read_text())
    archive=args.output/'trustzone-validation.tar'
    with tarfile.open(archive,'w') as stream:
        for name in sorted(files):
            path=ROOT/name
            if path.is_symlink() or Path(name).is_absolute() or '..' in Path(name).parts:
                parser.error('Unsafe transfer path: '+name)
            info=stream.gettarinfo(str(path),arcname='trustzone-validation/'+name)
            info.uid=info.gid=0;info.uname=info.gname='';info.mtime=0
            with path.open('rb') as data:stream.addfile(info,data)
    digest=hashlib.sha256(archive.read_bytes()).hexdigest()
    (args.output/'trustzone-validation.tar.sha256').write_text(digest+'  trustzone-validation.tar\n')
    (args.output/'export-metadata.json').write_text(json.dumps(dict(snapshot='UNCOMMITTED_WORKING_TREE_NOT_A_COMMIT',file_count=len(files),sha256=digest,hardware_load='BLOCKED',hardware_execution='NOT_RUN'),indent=4)+'\n')
    print(archive)
