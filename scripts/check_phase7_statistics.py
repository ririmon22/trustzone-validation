import argparse
import csv
import json
import math
import re
from pathlib import Path


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('directory',type=Path)
    args=parser.parse_args()
    checks=[]
    for config in ('baremetal','optee'):
        log=(args.directory/'qemu'/(config+'.log')).read_text()
        rows=list(csv.DictReader((args.directory/'qemu'/(config+'-benchmarks.csv')).open()))
        for row in rows:
            values=[int(value) for index,value in re.findall(r'^SAMPLE '+row['test_id']+r' (\d+) (\d+)$',log,re.M)]
            if len(values)!=128: raise ValueError('sample count mismatch')
            ordered=sorted(values)
            expected=dict(minimum=ordered[0],maximum=ordered[-1],mean=sum(values)/128,median=(ordered[63]+ordered[64])/2,p95=ordered[121],p99=ordered[126])
            if any(not math.isfinite(float(row[key])) or not math.isclose(float(row[key]),value,rel_tol=1e-12,abs_tol=1e-9) for key,value in expected.items()):
                raise ValueError('statistics mismatch: '+row['test_id'])
            checks.append(dict(configuration=config,test_id=row['test_id'],status='PASS',independent_nearest_rank_check=True))
    from report_performance_phase7 import validate_series_keys
    validate_series_keys((row['configuration'],row['test_id']) for row in checks)
    (args.directory/'statistics-check.json').write_text(json.dumps(checks,indent=4)+'\n')
    print('STATISTICS PASS:',len(checks),'configuration-specific series recomputed from raw samples')


if __name__=='__main__': main()
