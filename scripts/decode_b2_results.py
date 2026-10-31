import argparse
import hashlib
import json
import struct
from pathlib import Path
from check_b2_integration import elf_symbols, symbol_payload
from decode_ram_log import decode as decode_log

IDS = ['TEE-01', 'TEE-07', 'TEE-AUTH-TA', 'TEE-SHM-01', 'TEE-KEY-API', 'TEE-ISO-TA',
       'TEE-SESSION-01', 'TEE-ABI-01', 'TEE-BOUNDARY-01', 'B2-SMCCC-01',
       'TEE-SESSION-CORE-01', 'TEE-PARAM-01', 'TEE-KEY-ATTR-01', 'TZ-SYS-03', 'TEE-04']
PHASES = ['IDLE', 'PREPARE', 'OPERATE', 'EXPECTED_FAULT', 'COMPARE', 'SAVE', 'CLEANUP', 'DONE']
STATUSES = ['PASS', 'FAIL', 'SKIP', 'BLOCKED', 'NOT_RUN']
HEADER = struct.Struct('<4IQ4I')
RECORD_BYTES = 1952
TOTAL_BYTES = HEADER.size + len(IDS) * RECORD_BYTES


def text(data):
    if b'\0' not in data:
        raise ValueError('unterminated result field')
    return data.split(b'\0', 1)[0].decode('ascii')


def decode(data, run_id, parent=0):
    if len(data) not in (TOTAL_BYTES, HEADER.size+13*RECORD_BYTES,HEADER.size+14*RECORD_BYTES):
        raise ValueError('missing/truncated RAM results')
    magic, version, size, state, actual_run, selection, current, phase, cleanup = HEADER.unpack_from(data)
    if magic != 0x42325253 or version not in (1,2,3) or size != len(data) or len(data) != HEADER.size + (12+version)*RECORD_BYTES:
        raise ValueError('unsupported/uninitialized result format')
    if actual_run != run_id:
        raise ValueError('stale run ID')
    if state not in (1, 2, 3) or selection not in (*range(14),15,16,17,18) or phase >= len(PHASES) or current not in (*range(15), 0xffffffff):
        raise ValueError('invalid result header')
    if version == 1 and selection > 13:
        raise ValueError('physical selection requires result version 2')
    if selection == 16 and version < 3:raise ValueError('signed loader selection requires v3')
    rows = []
    for index, ident in enumerate(IDS[:12+version]):
        offset = HEADER.size + index * RECORD_BYTES
        name = text(data[offset:offset+24])
        selected, status, case_phase, reported, count, overflow = struct.unpack_from('<6I', data, offset+24)
        required={0:set(range(13))|{14},1:{0},2:{0},5:{3,8,11},10:{2},11:{4,12},12:{0,6,7,10,11},13:{1,5},14:{14}}
        expected_selection = (selection == 0 and index < 13) or (selection == 17 and index in required.get(parent,set())) or selection == (index+2 if index>=13 else index+1)
        if name != ident or selected != expected_selection or status >= len(STATUSES) or case_phase >= len(PHASES) or count > 32:
            raise ValueError('invalid case fields: ' + ident)
        if not selected and (status != 4 or reported or count):
            raise ValueError('unselected case has execution evidence: ' + ident)
        checks = []
        for number in range(count):
            start = offset + 160 + number * 56
            label = text(data[start:start+40])
            expected, actual = struct.unpack_from('<QQ', data, start+40)
            checks.append(dict(name=label, expected=expected, actual=actual, matched=expected == actual))
        rows.append(dict(test_id=name, selected=bool(selected), status=STATUSES[status], phase=PHASES[case_phase],
                         reported=reported, overflow=overflow, reason=text(data[offset+48:offset+160]), checks=checks))
    complete = state == 2 and phase == 7 and cleanup == 0 and all(
        not row['selected'] or (row['reported'] == 1 and row['status'] in ('PASS','FAIL','BLOCKED') and row['phase'] == 'DONE' and row['overflow'] == 0 and (row['status'] != 'PASS' or len(row['checks']) > 0))
        for row in rows)
    if any(row['status'] == 'PASS' and any(not check['matched'] for check in row['checks']) for row in rows):
        raise ValueError('PASS contradicts recorded comparisons')
    return dict(version=version, run_id=actual_run, selection=selection, state={1:'RUNNING',2:'COMPLETE',3:'ERROR'}[state],
                current_case=current, phase=PHASES[phase], cleanup_errors=cleanup,
                collection_status='COMPLETE' if complete else 'INCOMPLETE', cases=rows)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('dump', type=Path)
    parser.add_argument('--ram-log',type=Path,required=True)
    parser.add_argument('--elf', type=Path, required=True)
    parser.add_argument('--core-elf',type=Path)
    parser.add_argument('--capture-plan',type=Path,required=True)
    parser.add_argument('--run-id', type=lambda value:int(value,0), required=True)
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    try:
        inputs={p.resolve() for p in (args.dump,args.elf,args.ram_log,args.capture_plan)}
        inputs.update((args.dump.parent/(name+'.bin')).resolve() for name,address,size in json.loads(args.capture_plan.read_text()).get('records',[]))
        if args.core_elf is not None:
            inputs.add(args.core_elf.resolve())
        if any(args.output.with_suffix(suffix).resolve() in inputs for suffix in ('.txt','.json','.csv')) or args.output.resolve() in inputs:
            raise ValueError('output must preserve all inputs')
        parsed = elf_symbols(args.elf)
        _, _, symbols = parsed
        capture = json.loads(args.capture_plan.read_text())
        elf_hash = hashlib.sha256(args.elf.read_bytes()).hexdigest()
        if capture.get('sha256') != elf_hash:
            raise ValueError('capture plan and ELF hash mismatch')
        expected_run = struct.unpack('<Q',symbol_payload(parsed,'debug_run_id'))[0]
        expected_selection = struct.unpack('<I',symbol_payload(parsed,'b2_test_selection'))[0]
        if expected_run != args.run_id:
            raise ValueError('run ID does not match the selected ELF')
        if not any(name == 'b2_results' and address == symbols['b2_results']['address'] and size == symbols['b2_results']['size'] for name,address,size in capture.get('records',[])):
            raise ValueError('missing/mismatched capture region')
        if symbols.get('b2_results',{}).get('size') not in (TOTAL_BYTES,HEADER.size+13*RECORD_BYTES,HEADER.size+14*RECORD_BYTES):
            raise ValueError('ELF has missing/incompatible b2_results symbol')
        parent=int.from_bytes(symbol_payload(parsed,'b2_review_parent'),'little') if 'b2_review_parent' in symbols else 0
        report = decode(args.dump.read_bytes(), args.run_id,parent)
        if report['selection'] != expected_selection:
            raise ValueError('selection does not match the ELF')
        log = decode_log(args.ram_log.read_bytes())
        if log['run_id'] != args.run_id:
            raise ValueError('RAM log run ID mismatch')
        if log['collection_status'] != 'COMPLETE':
            report['collection_status'] = 'INCOMPLETE'
        report['log_metadata'] = {key:value for key,value in log.items() if key != 'text'}
        if report['selection'] == 15:
            from decode_b2_physical import decode_physical, validate_physical_pass
            physical_file = args.dump.with_name('b2_physical_record.bin')
            physical_symbol = symbols.get('b2_physical_record')
            if physical_symbol is None or physical_symbol['size']!=488 or not any(name=='b2_physical_record' and address==physical_symbol['address'] and size==488 for name,address,size in capture.get('records',[])):
                raise ValueError('physical capture region does not match ELF')
            if any(name not in symbols for name in ['b2_physical_read_fault','b2_physical_write_fault']):
                raise ValueError('ELF missing physical probe instructions')
            physical_data=physical_file.read_bytes()
            if report['collection_status']!='COMPLETE' and len(physical_data)==488 and not any(physical_data):
                report['physical']={'state':'NOT_ENTERED','reason':'global preparation incomplete; inspect BL33 log and debug_boot'}
            else:
                report['physical'] = decode_physical(physical_data, args.run_id)
            if report['physical']['state'] != 2:
                report['collection_status'] = 'INCOMPLETE'
            physical_case = next(row for row in report['cases'] if row['test_id']=='TZ-SYS-03')
            if physical_case['status']=='PASS':
                begin,end=struct.unpack('<2Q',symbol_payload(parsed,'b2_physical_reservation'))
                if not begin<=report['physical']['page_pa']<=end-4096:raise ValueError('physical PASS outside linked reservation')
                report['physical']['linked_reservation']=[begin,end]
                validate_physical_pass(report['physical'],symbols['b2_physical_read_fault']['address'],symbols['b2_physical_write_fault']['address'])
                requirements={'Secure-page-check':(0x50485953,4),'precise-external-abort':(3,4),'Secure-allocation-bytes':(4096,1),'post-probe-TA-ECHO':(0xa35b,1),'Normal-control':(0xa35b,1)}
                for name,(value,count) in requirements.items():
                    checks=[check for check in physical_case['checks'] if check['name']==name]
                    if len(checks)!=count or any(check['expected']!=value or check['actual']!=value for check in checks):
                        raise ValueError('physical PASS lacks Secure nonmodification/continuity: '+name)
        if report['selection']==16 or (report['selection']==17 and parent in (0,14)):
            if args.core_elf is None or capture.get('core_sha256')!=hashlib.sha256(args.core_elf.read_bytes()).hexdigest():
                raise ValueError('Core ELF/hash required for signed loader proof')
            from decode_signed_results import decode_signed
            try:
                report['signed_loader']=decode_signed(args.dump.parent,args.run_id)
            except (OSError, ValueError, struct.error) as error:
                report['signed_loader']={'verification':'UNVERIFIED','reason':str(error)}
                report['collection_status']='INCOMPLETE'
                for row in report['cases']:
                    if row['selected'] and row['status']=='PASS':
                        row['status']='BLOCKED'
                        row['reason']='signed loader evidence incomplete: '+str(error)
        if report['selection'] in (17,18):
            from decode_b2_review import review, benchmarks
            decoders=[('review','b2_review_results',review)]
            if report['selection']==18 or parent in (0,17,18,21):decoders.append(('benchmarks','b2_bench_results',benchmarks))
            for key,name,decoder in decoders:
                try:
                    value=decoder(args.dump.with_name(name+'.bin').read_bytes(),args.run_id,parent,report['selection']==18) if key=='review' else decoder(args.dump.with_name(name+'.bin').read_bytes(),args.run_id)
                    if key=='benchmarks':
                        report[key],samples_csv=value
                        args.output.parent.mkdir(parents=True,exist_ok=True)
                        args.output.with_suffix('.csv').write_text(samples_csv)
                    else:report[key]=value
                    if report[key]['collection_status']!='COMPLETE':report['collection_status']='INCOMPLETE'
                except (OSError,ValueError,struct.error) as error:
                    report[key]={'collection_status':'INCOMPLETE','reason':str(error)}
                    report['collection_status']='INCOMPLETE'
            irq_row=next((r for r in report.get('review',{}).get('items',[]) if r['test_id']=='PERF-03'),None)
            if irq_row and irq_row['status']=='MEASURED':
                from decode_b2_review import sgi
                try:
                    report['secure_sgi']=sgi(args.dump.with_name('b2_sgi_results.bin').read_bytes(),args.run_id)
                    if report['secure_sgi']['collection_status']!='COMPLETE':raise ValueError('SGI samples incomplete')
                    import csv,io
                    csv_path=args.output.with_suffix('.csv')
                    stream=io.StringIO();writer=csv.writer(stream)
                    current_bench=report.get('benchmarks',{}).get('collection_status')=='COMPLETE'
                    if not current_bench:writer.writerow(['parent_id','series_id','kind','index','ticks','timer_hz','run_id'])
                    for kind,key in [('sample','samples'),('overhead','overhead_samples')]:
                        for i,value in enumerate(report['secure_sgi'][key]):writer.writerow(['PERF-03','Secure-self-SGI',kind,i,value,report['secure_sgi']['timer_hz'],args.run_id])
                    csv_path.parent.mkdir(parents=True,exist_ok=True)
                    csv_path.write_text((csv_path.read_text() if current_bench else '')+stream.getvalue())
                except (OSError,ValueError,struct.error) as error:
                    irq_row['status']='BLOCKED';report['collection_status']='INCOMPLETE'
                    report['secure_sgi']={'collection_status':'INCOMPLETE','reason':str(error)}
            if report['selection']==17 and report.get('signed_loader',{}).get('verification')=='UNVERIFIED':
                for row in report['review'].get('items',[]):
                    if row['test_id']=='EXT-TA-V03':row['status']='BLOCKED'
        report['elf_sha256'] = hashlib.sha256(args.elf.read_bytes()).hexdigest()
        report['review_parent']=parent
        report['core_sha256']=capture.get('core_sha256')
        report['ta_build_inputs']=capture.get('ta_inputs',[])
        report['binding'] = 'USER_SELECTED_ELF; compare capture-plan hash with the ELF actually loaded'
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.with_suffix('.json').write_text(json.dumps(report,indent=4)+'\n')
        lines = ['B2 results version={} run={} ELF={}'.format(report['version'], report['run_id'],report['elf_sha256']),
                 'Collection={} state={} phase={} cleanup_errors={}'.format(report['collection_status'],report['state'],report['phase'],report['cleanup_errors'])]
        for row in report['cases']:
            lines.append('{} {} phase={} selected={} {}'.format(row['test_id'],row['status'],row['phase'],row['selected'],row['reason']))
            for check in row['checks']:
                lines.append('  {} expected=0x{:x} actual=0x{:x} matched={}'.format(check['name'],check['expected'],check['actual'],check['matched']))
        if 'physical' in report:
            lines.extend(['', 'PHYSICAL EVIDENCE (XMPU attribution requires separate snapshots)', json.dumps(report['physical'],indent=2)])
        if 'signed_loader' in report:
            lines.extend(['', 'SIGNED LOADER (EXT-TA-V03 / T-16)',json.dumps(report['signed_loader'],indent=2)])
        for key in ('review','benchmarks','secure_sgi'):
            if key in report:lines.extend(['',key.upper(),json.dumps(report[key],indent=2)])
        lines.extend(['', 'BL33 RAM LOG (not TF-A/Core/TA trace)', log['text']])
        args.output.write_text('\n'.join(lines)+'\n')
        if report['collection_status'] != 'COMPLETE':
            parser.exit(2,'B2 RESULTS INCOMPLETE; partial evidence preserved\n')
        failed = any(row['status']=='FAIL' for row in report.get('review',{}).get('items',[])) or any(row['selected'] and row['status']=='FAIL' for row in report['cases'])
        blocked = any(row['selected'] and row['status']=='BLOCKED' for row in report['cases'])
        raise SystemExit(1 if failed else 2 if blocked else 0)
    except (OSError,ValueError,UnicodeError,struct.error) as error:
        parser.exit(1,'B2 RESULTS ERROR: '+str(error)+'\n')
