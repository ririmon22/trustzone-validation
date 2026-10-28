import ctypes
import json
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
from decode_ram_log import decode
from decode_exception import decode as decode_exception


class RamLogTests(unittest.TestCase):
    def record(self, text=b'END\n', state=2, dropped=0, used=None):
        return struct.pack('<6IQ', 0x545a4c47, 1, 16, len(text) if used is None else used, dropped, state, 77) + text.ljust(16, b'\0')

    def test_complete_and_run_id(self):
        result = decode(self.record())
        self.assertEqual((result['collection_status'], result['run_id'], result['text']), ('COMPLETE', 77, 'END\n'))

    def test_partial_overflow_and_fatal_are_not_complete(self):
        for data in (self.record(state=1), self.record(state=3), self.record(dropped=1)):
            self.assertEqual(decode(data)['collection_status'], 'PARTIAL')

    def test_short_torn_or_wrong_version_rejected(self):
        for data in (b'', self.record()[:-1], self.record(used=17), bytes(len(self.record()))):
            with self.assertRaises(ValueError):
                decode(data)

    def test_fixed_exception_record(self):
        words = [2, 3, 8, 0x96000010, 0x1234, 0x5678, 0x3c5, 0x8000, 0, 0x9000] + list(range(31))
        record = decode_exception(struct.pack('<41Q', *words))
        self.assertEqual(record['status'], 'CAPTURED')
        self.assertEqual(record['x'], list(range(31)))
        self.assertEqual(record['far'], 0x1234)
        with self.assertRaises(ValueError):
            decode_exception(b'')

    def test_actual_formatter_and_overflow(self):
        source = r'''
#include "runtime.h"
#include <string.h>
char __bss_start[1], __bss_end[1];
volatile struct boot_debug_state debug_boot;
volatile struct exception_record debug_exception, debug_sync_exception;
static unsigned int uart_calls;
void platform_uart_putc(char value) { (void)value; uart_calls++; }
void debug_stage(uint64_t stage, uint64_t reason) { (void)stage; (void)reason; }
_Noreturn void platform_halt(void) { __builtin_trap(); }
int main(void) {
    debug_boot.run_id = 99;
    int count = log_printf("%s %u %d %016llx %%", "ok", 42U, -7, 255ULL);
    const char *expected = "ok 42 -7 00000000000000ff %";
    if (count != (int)strlen(expected) || memcmp((const void *)debug_ram_log.data, expected, strlen(expected))) return 1;
    if (debug_ram_log.run_id != 99 || uart_calls != 0) return 2;
    for (unsigned int i=0; i<80; i++) console_putc('x');
    if (debug_ram_log.used != 64 || debug_ram_log.dropped == 0) return 3;
    log_set_state(RAM_LOG_COMPLETE);
    return debug_ram_log.state == RAM_LOG_COMPLETE ? 0 : 4;
}
'''
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            (path / 'test.c').write_text(source)
            subprocess.run(['clang', '-std=c11', '-Wall', '-Wextra', '-Werror', '-DRAM_LOG_CAPACITY=64', '-DLOG_UART_ENABLED=0', '-I'+str(ROOT/'common/include'), '-I'+str(ROOT/'platform/qemu'), str(path/'test.c'), str(ROOT/'common/result_report/console.c'), '-o', str(path/'test')], check=True)
            subprocess.run([str(path/'test')], check=True)

    def test_tcl_capture_only_defines_procedure(self):
        source = ROOT / 'scripts/jtag/capture.tcl'
        probe = 'source {'+str(source)+'}\nputs [info procs tz_dump]\n'
        run = subprocess.run(['tclsh'], input=probe, text=True, capture_output=True, check=True)
        self.assertIn('tz_dump', run.stdout)
        self.assertNotIn('invalid command', run.stderr)

class JtagSafetyTests(unittest.TestCase):
    def test_boot_template_rejects_before_any_hardware_command(self):
        script = 'source {' + str(ROOT/'scripts/jtag/b2_boot.tcl') + '}\nset tz_boot_authorized 0\nif {![catch {tz_b2_execute} reason]} {exit 1}\nputs $reason\n'
        run = subprocess.run(['tclsh'], input=script, text=True, capture_output=True, check=True)
        self.assertIn('BLOCKED', run.stdout)
        self.assertNotIn('invalid command', run.stdout + run.stderr)

    def test_actual_boot_inventory_stays_blocked(self):
        from prepare_b2_jtag import inspect
        inputs = json.loads((ROOT/'platform/zcu106/jtag_b2_inputs.json').read_text())
        report = inspect(inputs)
        self.assertEqual(report['status'], 'BLOCKED')
        self.assertFalse(report['execution_authorized'])
        self.assertTrue(any('provenance' in issue for issue in report['issues']))
        self.assertTrue(any('PMUFW ready' in issue for issue in report['issues']))

    def test_capture_requires_stopped_target_without_stopping_it(self):
        script = '''
proc targets {args} { return }
proc state {} { return Running }
source {''' + str(ROOT/'scripts/jtag/capture.tcl') + '''}
if {![catch {tz_dump {name == "CPU"} {{test 4096 4}} /tmp/unused-tz-capture} reason]} {exit 1}
puts $reason
'''
        run = subprocess.run(['tclsh'], input=script, text=True, capture_output=True, check=True)
        self.assertIn('BLOCKED', run.stdout)

class ExternalPrefixTests(unittest.TestCase):
    def test_ta_uses_all_aarch64_prefix_variables(self):
        from build_external import build_plan
        command = build_plan('ta-a', 'reviewed-aarch64-', Path('/tmp/test-ta-output'))
        for variable in ('CROSS_COMPILE', 'CROSS_COMPILE64', 'CROSS_COMPILE_ta_arm64'):
            self.assertIn(variable + '=reviewed-aarch64-', command)
