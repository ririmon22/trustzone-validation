import argparse
import json
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path
from unittest.mock import patch

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
from zip_sources import REQUIRED, inspect_zip_source, tree_manifest
from source_snapshot import verify_source
from build_external import configured_plan


class ZipSourceTests(unittest.TestCase):
    def fixture(self,root,name):
        for relative in REQUIRED[name]:
            path=root/relative;path.parent.mkdir(parents=True,exist_ok=True);path.write_text('source fixture\n')
        if name=='tfa-amd':
            (root/'Makefile').write_text('VERSION_MAJOR := 2\nVERSION_MINOR := 12\n')
            (root/'services/spd/opteed/opteed_main.c').write_text('opteed source fixture')
        if name=='optee':
            (root/'mk/config.mk').write_text('CFG_EARLY_TA CFG_ARM64_core CFG_USER_TA_TARGETS')
            (root/'core/arch/arm/plat-zynqmp/conf.mk').write_text('zcu106 CFG_WITH_ARM_TRUSTED_FW')
        if name=='pyelftools':(root/'elftools/__init__.py').write_text("__version__ = '0.32'\n")

    def test_directory_only_zip_mode_never_invokes_git_or_claims_commit(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);self.fixture(root,'optee')
            with patch('subprocess.check_output',side_effect=AssertionError('Git must not run')):
                result=verify_source(root,{'repository':'https://example/optee_os.git','commit':'1'*40},'zip')
            self.assertEqual(result['provenance_status'],'UNVERIFIED')
            self.assertIsNone(result['verified_commit'])
            self.assertIsNone(result['observed_version'])

    def test_archive_hash_is_recorded_without_claiming_binding(self):
        with tempfile.TemporaryDirectory() as directory:
            base=Path(directory);root=base/'source';root.mkdir();self.fixture(root,'pyelftools')
            archive=base/'input.zip'
            with zipfile.ZipFile(archive,'w') as stream:stream.writestr('fixture.txt','fixture')
            result=inspect_zip_source(root,'pyelftools','0.32',archive)
            self.assertEqual(len(result['archive_sha256']),64)
            self.assertEqual(result['archive_directory_binding'],'USER_SUPPLIED_NOT_VERIFIED')
            self.assertIsNone(result['verified_commit'])

    def test_missing_source_and_unsupported_version_block(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);self.fixture(root,'tfa-amd')
            with self.assertRaisesRegex(ValueError,'unsupported'):
                inspect_zip_source(root,'tfa-amd','99.0')
            (root/'Makefile').unlink()
            with self.assertRaisesRegex(ValueError,'required source'):
                inspect_zip_source(root,'tfa-amd')

    def test_escaping_link_and_prebuilt_objects_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory)/'source';root.mkdir();(root/'escape').symlink_to(Path(directory)/'outside')
            with self.assertRaisesRegex(ValueError,'escapes'):
                tree_manifest(root)
            (root/'escape').unlink();(root/'cached.o').write_bytes(b'fixture')
            with self.assertRaisesRegex(ValueError,'Generated file'):
                tree_manifest(root)

    def test_copy_change_is_detected(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);self.fixture(root,'pyelftools')
            record=inspect_zip_source(root,'pyelftools')
            (root/'.zip_input.json').write_text(json.dumps(record))
            (root/'elftools/__init__.py').write_text("__version__ = '0.32'\n# modified\n")
            with self.assertRaisesRegex(ValueError,'changed after staging'):
                verify_source(root,{'repository':'https://example/pyelftools.git'},'zip')

    def test_zip_build_banner_and_ta_prefixes_do_not_claim_fixed_commit(self):
        args=argparse.Namespace(component='tfa',cross_compile='reviewed-aarch64-',compiler='clang',llvm_dir=Path('/actual/llvm'),platform='zynqmp',configuration='b2',early_ta=[],jobs=1,host_cc=None,source_mode='zip')
        _,command=configured_plan(args)
        self.assertIn('BUILD_STRING=ZIP-UNVERIFIED',command)
        self.assertNotIn('BUILD_STRING=dd3512287a17eea866d5c82fa7b86ad58de59163',command)

    def test_locked_mode_does_not_accept_arbitrary_directory(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);self.fixture(root,'pyelftools')
            import subprocess
            with self.assertRaises((OSError,subprocess.CalledProcessError,ValueError)):
                verify_source(root,{'repository':'https://example/pyelftools.git','commit':'1'*40})
