import hashlib
import json
import re
from pathlib import Path
from parse_results import parse_log
from check_tz_audit import audit
from check_security_evidence import validate_faults

ROOT=Path(__file__).resolve().parents[1]


def main():
    directory=ROOT/'results/phase7'
    expectations=json.loads((ROOT/'tests/expectations.json').read_text())
    records=[]
    counts={}
    for configuration in ('baremetal','tfa','optee'):
        log=directory/'qemu'/(configuration+'.log')
        required=[f'TEE-{i:02}' for i in range(1,9)]+['TEE-AUTH-TA','TEE-AUTH-NS','TEE-SHM-01','TEE-KEY-API','TEE-KEY-MEM','TEE-ISO-TA','TEE-ISO-CORE','TEE-STRESS-01'] if configuration=='optee' else [f'TZ-SYS-{i:02}' for i in range(1,8)]
        if configuration=='tfa': required+=['B1-SMCCC-01']
        rows=parse_log(log.read_text(),expectations,required)
        if any(r['execution_status']=='FAIL' or r['configuration']!=configuration or r['target_platform']!='qemu' for r in rows):
            raise ValueError('functional run failed or wrong header')
        counts[configuration]={}
        for row in rows:
            counts[configuration][row['execution_status']]=counts[configuration].get(row['execution_status'],0)+1
            row.update(build_status='BUILD PASS',evidence=[str(log.relative_to(ROOT))],limitation='QEMU only; hardware NOT_RUN')
            records.append(row)
    atext=(directory/'qemu/baremetal.log').read_text()
    (directory/'tz-audit.json').write_text(json.dumps(audit(atext),indent=4)+'\n')
    normal=(directory/'qemu/optee.log').read_text()
    secure=(directory/'qemu/optee-secure.log').read_text()
    validate_faults(normal,secure)
    meta=re.search(r'^STRESS_META .+$',normal,re.M)
    resource=re.search(r'^STRESS_RESOURCE heap_before=(\d+) heap_after=(\d+) ta_before=(\d+) ta_after=(\d+) alloc_fail_before=(\d+) alloc_fail_after=(\d+) completed=(\d+)$',normal,re.M)
    if not meta or not resource: raise ValueError('missing stress evidence')
    hb,ha,tb,ta,fb,fa,n=map(int,resource.groups())
    if n!=512 or ha>hb or ta>tb or fa!=fb: raise ValueError('stress resource growth or incomplete iterations')
    (directory/'stress.json').write_text(json.dumps(dict(meta=meta[0],heap_before=hb,heap_after=ha,ta_before=tb,ta_after=ta,alloc_fail_before=fb,alloc_fail_after=fa,completed=n,status='PASS',scope='bounded execution, not a proof of unbounded leak freedom'),indent=4)+'\n')
    (directory/'results.json').write_text(json.dumps(records,indent=4)+'\n')
    (directory/'qemu-summary.json').write_text(json.dumps(counts,indent=4)+'\n')
    artifacts={}
    for pattern in ('build/qemu/*.elf','build/zcu106/*.elf','build/qemu/external/*.elf','build/external/ta-*/*.ta','build/external/optee/core/tee.elf','build/external/tfa-*/**/bl31.elf'):
        for path in ROOT.glob(pattern):
            artifacts[str(path.relative_to(ROOT))]=dict(size=path.stat().st_size,sha256=hashlib.sha256(path.read_bytes()).hexdigest())
    (directory/'artifacts.json').write_text(json.dumps(artifacts,indent=4)+'\n')
    lines=['# QEMU検証レポート Phase 7','','クリーンなsource/build workspaceで再構築。コンテナ動作確認とは区別する。実機は全NOT_RUN。','','|構成|PASS|FAIL|SKIP|BLOCKED|','|---|---|---|---|---|']
    for config,c in counts.items(): lines.append('|'+config+'|'+'|'.join(str(c.get(s,0)) for s in ('PASS','FAIL','SKIP','BLOCKED'))+'|')
    lines+=['','|Test ID|構成|状態|実結果|証跡|','|---|---|---|---|---|']
    for row in records: lines.append('|'+row['test_id']+'|'+row['configuration']+'|'+row['execution_status']+'|'+row['actual_result']+'|'+','.join(row['evidence'])+'|')
    lines+=['','個別の期待値・ticksはresults.json。MMU-off保護監査はtz-audit.json、TEE反復はstress.json、性能はperformance.md/overhead.json。ZCU106安全検査はboot-plan-check.json、hardware executionはNOT_RUN。']
    (directory/'report.md').write_text('\n'.join(lines)+'\n')


if __name__=='__main__': main()
