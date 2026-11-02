"""実生成物の破損・構成混在を拒否するPC試験。SMC/実機ローダの代替ではない。
先にdocs/pc_validation.mdのbuildを実行する。TZ_BUILD_ROOT/TZ_LOG_ROOTは試験対象の選択だけ。
"""
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import sys
import tempfile
import unittest
import zlib

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'scripts'))
from check_components import check, inspect_elf
from check_signed_profile import check_bundle
from elf_symbols import elf_symbols

BUILD = Path(os.environ.get('TZ_BUILD_ROOT', str(ROOT/'build'))).resolve()
LOGS = Path(os.environ.get('TZ_LOG_ROOT', str(ROOT/'results/pc-preflight/run-a'))).resolve()


class ArtifactChecks(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not (BUILD/'components/physical/optee/core/tee.elf').is_file():
            raise unittest.SkipTest('全profileの実生成物が必要。docs/pc_validation.mdを先に実行')

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='tz-artifact-')
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)

    def inputs(self, profile='signed', compiler='clang'):
        p = BUILD/'components'/profile
        return dict(root=ROOT, profile=profile,
                    bl31=p/'tfa/zynqmp/release/bl31/bl31.elf', bl32=p/'optee/core/tee.elf',
                    bl33=BUILD/'bl33'/compiler/profile/'optee_bl33.elf', conf=p/'optee/conf.mk',
                    ta_a=p/'ta-a/6dc13418-534f-4642-9138-215876304910.stripped.elf',
                    ta_b=p/'ta-b/6dc13419-534f-4642-9138-215876304910.stripped.elf',
                    bl31_log=LOGS/(profile+'-bl31.log'), bl33_log=LOGS/(profile+'-bl33-'+compiler+'.log'))

    def corrupted(self, source, change):
        data = bytearray(source.read_bytes())
        change(data)
        target = self.directory/source.name
        target.write_bytes(data)
        return target

    def test_all_profiles_and_compilers(self):
        for profile in ('basic','signed','physical'):
            for compiler in ('clang','gcc'):
                with self.subTest(profile=profile, compiler=compiler):
                    self.assertEqual(check(**self.inputs(profile,compiler))['static_status'], 'PASS')

    def test_ta_uuid_and_profile_mix(self):
        inputs = self.inputs()
        inputs['ta_b'] = inputs['ta_a']
        with self.assertRaisesRegex(ValueError, 'UUID mismatch'):
            check(**inputs)
        inputs = self.inputs('basic')
        inputs['bl33'] = self.inputs('signed')['bl33']
        inputs['bl33_log'] = self.inputs('signed')['bl33_log']
        with self.assertRaisesRegex(ValueError, 'features mismatch'):
            check(**inputs)

    def test_shm_and_handoff_mismatch(self):
        inputs = self.inputs()
        conf = self.directory/'conf.mk'
        conf.write_text(inputs['conf'].read_text().replace('CFG_SHMEM_START=0x10500000','CFG_SHMEM_START=0x10600000'))
        inputs['conf'] = conf
        with self.assertRaisesRegex(ValueError, 'Core memory config mismatch'):
            check(**inputs)
        inputs = self.inputs()
        log = self.directory/'bl31.log'
        log.write_text(inputs['bl31_log'].read_text().replace('PRELOADED_BL33_BASE=0x10200000','PRELOADED_BL33_BASE=0x10201000'))
        inputs['bl31_log'] = log
        with self.assertRaisesRegex(ValueError, 'handoff build argument missing'):
            check(**inputs)

    def test_elf_arch_entry_permissions_alignment_and_bounds(self):
        original = self.inputs()['bl33']
        ph = struct.unpack_from('<Q', original.read_bytes(), 32)[0]
        mutations = {
            'architecture': lambda d: struct.pack_into('<H',d,18,62),
            'entry': lambda d: struct.pack_into('<Q',d,24,0xdead),
            'permissions': lambda d: struct.pack_into('<I',d,ph+4,7),
            'alignment': lambda d: struct.pack_into('<Q',d,ph+48,3),
            'truncated': lambda d: d.__delitem__(slice(64,None)),
        }
        for name, mutate in mutations.items():
            with self.subTest(name=name), self.assertRaises((ValueError,struct.error)):
                inspect_elf(self.corrupted(original,mutate),'bl33')
        inputs = self.inputs()
        inputs['bl33'] = self.corrupted(original, lambda d: struct.pack_into('<Q',d,ph+40,0x1000000))
        with self.assertRaisesRegex(ValueError, 'escapes reservation'):
            check(**inputs)

    def test_early_ta_payload_is_compared(self):
        inputs = self.inputs()
        data,sections,symbols = elf_symbols(inputs['bl32'])
        symbol = symbols['ts_bin_6dc13418534f46429138215876304910']
        section = sections[symbol['section']]
        offset = section[4]+symbol['address']-section[3]+symbol['size']//2
        inputs['bl32'] = self.corrupted(inputs['bl32'],lambda d: d.__setitem__(offset,d[offset]^1))
        with self.assertRaises((ValueError,zlib.error)):
            check(**inputs)

    def copy_bundle(self):
        target = self.directory/'bundle'
        shutil.copytree(BUILD/'components/signed/bundle',target)
        return target

    def test_bundle_valid_and_corrupt(self):
        target = self.copy_bundle()
        self.assertEqual(check_bundle(target)['bytes'],0x100000)
        data = bytearray((target/'bundle.bin').read_bytes())
        data[4096] ^= 1
        (target/'bundle.bin').write_bytes(data)
        with self.assertRaisesRegex(ValueError, 'manifest hash mismatch'):
            check_bundle(target)
        manifest = json.loads((target/'manifest.json').read_text())
        manifest['bundle_sha256'] = hashlib.sha256(data).hexdigest()
        (target/'manifest.json').write_text(json.dumps(manifest))
        with self.assertRaisesRegex(ValueError, 'differs from verified TA'):
            check_bundle(target)

    def test_bundle_overlapping_slots(self):
        target = self.copy_bundle()
        data = bytearray((target/'bundle.bin').read_bytes())
        struct.pack_into('<Q',data,32+48,4096)
        (target/'bundle.bin').write_bytes(data)
        manifest = json.loads((target/'manifest.json').read_text())
        manifest['bundle_sha256'] = hashlib.sha256(data).hexdigest()
        (target/'manifest.json').write_text(json.dumps(manifest))
        with self.assertRaisesRegex(ValueError, 'slot bounds mismatch'):
            check_bundle(target)

    def test_decoder_rejects_stale_or_torn_log(self):
        import subprocess
        from decode_ram_log import decode
        from decode_b2_results import decode as decode_results
        good = struct.pack('<6IQ',0x545a4c47,1,4,2,0,2,9)+b'ok\0\0'
        self.assertEqual(decode(good)['collection_status'],'COMPLETE')
        dropped = bytearray(good)
        struct.pack_into('<I',dropped,16,1)
        self.assertEqual(decode(dropped)['collection_status'],'PARTIAL')
        torn = bytearray(good)
        struct.pack_into('<I',torn,12,5)
        with self.assertRaises(ValueError):
            decode(torn)
        with self.assertRaises(ValueError):
            decode_results(b'',9)
        dump = self.directory/'log.bin'
        dump.write_bytes(good)
        result = subprocess.run([sys.executable,str(ROOT/'scripts/decode_ram_log.py'),str(dump),'--expected-run-id','10','--output',str(self.directory/'out.txt')],capture_output=True,text=True)
        self.assertNotEqual(result.returncode,0)
        self.assertIn('run ID mismatch',result.stderr)

    def test_malformed_jtag_input_and_input_preservation(self):
        import subprocess
        invalid = self.directory/'invalid.json'
        invalid.write_text('[]')
        output = self.directory/'inspection.json'
        tcl = self.directory/'inspection.tcl'
        result = subprocess.run([sys.executable,str(ROOT/'scripts/prepare_b2_jtag.py'),'--inputs',str(invalid),'--output',str(output),'--emit-tcl',str(tcl)],capture_output=True,text=True)
        self.assertEqual(result.returncode,2)
        self.assertFalse(json.loads(output.read_text())['execution_authorized'])
        self.assertIn('set tz_boot_authorized 0',tcl.read_text())
        elf = self.inputs()['bl33']
        before = hashlib.sha256(elf.read_bytes()).hexdigest()
        result = subprocess.run([sys.executable,str(ROOT/'scripts/prepare_jtag_capture.py'),str(elf),'--output',str(elf)],capture_output=True,text=True)
        self.assertNotEqual(result.returncode,0)
        self.assertEqual(hashlib.sha256(elf.read_bytes()).hexdigest(),before)


if __name__ == '__main__':
    unittest.main()
