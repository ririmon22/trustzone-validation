import sys
import unittest
from pathlib import Path
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'scripts'))
import check_build_environment as environment


class EnvironmentTests(unittest.TestCase):
    def test_log_analysis_requires_no_target_toolchain(self):
        with patch.object(environment.shutil, 'which', return_value=None):
            result = environment.inspect('performance')
        self.assertEqual(result['status'], 'READY')
        self.assertEqual(result['selected']['compiler'], 'not-required')

    def test_baremetal_missing_tools_is_blocked(self):
        with patch.object(environment.shutil, 'which', return_value=None):
            result = environment.inspect('baremetal', 'gcc', '/missing/aarch64-none-elf-')
        self.assertEqual(result['status'], 'BLOCKED')
        self.assertTrue(any('Missing target tool' in issue for issue in result['issues']))

    def test_explicit_gnu_prefix_does_not_fall_back_to_clang(self):
        result = environment.choose_toolchain('auto', '/missing/aarch64-none-elf-', None)
        self.assertEqual(result['compiler'], 'gcc')
        self.assertEqual(result['tools']['gcc'], '/missing/aarch64-none-elf-gcc')

    def test_shell_syntax_in_prefix_rejected(self):
        with self.assertRaises(ValueError):
            environment.choose_toolchain('gcc', 'aarch64-;false', None)


if __name__ == '__main__':
    unittest.main()
