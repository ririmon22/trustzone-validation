#include "runtime.h"
uint64_t read_current_el(void)
{
    uint64_t value;
    __asm__ volatile("mrs %0, CurrentEL" : "=r"(value));
    return (value >> 2U) & 3U;
}
uint64_t counter_read(void)
{
    uint64_t value;
    __asm__ volatile("isb; mrs %0, cntpct_el0" : "=r"(value) :: "memory");
    return value;
}
uint64_t counter_frequency(void)
{
    uint64_t value;
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(value));
    return value;
}
struct smc_result smc_call(uint64_t fid, uint64_t a1, uint64_t a2, uint64_t a3)
{
    register uint64_t x0 __asm__("x0") = fid;
    register uint64_t x1 __asm__("x1") = a1;
    register uint64_t x2 __asm__("x2") = a2;
    register uint64_t x3 __asm__("x3") = a3;
    register uint64_t x4 __asm__("x4") = 0UL;
    register uint64_t x5 __asm__("x5") = 0UL;
    register uint64_t x6 __asm__("x6") = 0UL;
    register uint64_t x7 __asm__("x7") = 0UL;
    /* SMCCCで保存を保証しない一時レジスタを明示する。 */
    __asm__ volatile("smc #0" : "+r"(x0), "+r"(x1), "+r"(x2), "+r"(x3), "+r"(x4), "+r"(x5), "+r"(x6), "+r"(x7) ::
         "x8", "x9", "x10", "x11", "x12", "x13", "x14", "x15", "x16", "x17", "memory", "cc");
    return (struct smc_result){ x0, x1, x2, x3 };
}
_Noreturn void platform_halt(void)
{
    for (;;) { __asm__ volatile("wfe"); }
}

/* MMU無効時のVA=PAと使用状態を再監査用に記録する。 */
void report_memory_state(const char *world)
{
    uint64_t sctlr, ttbr0, tcr;
    __asm__ volatile("mrs %0, sctlr_el1; mrs %1, ttbr0_el1; mrs %2, tcr_el1" : "=r"(sctlr), "=r"(ttbr0), "=r"(tcr));
    console_write("MEM_STATE world="); console_write(world);
    console_write(" sctlr="); console_hex(sctlr); console_write(" ttbr0="); console_hex(ttbr0);
    console_write(" tcr="); console_hex(tcr); console_write(" base="); console_hex(PROTECTED_BUFFER_BASE);
    console_write(" end="); console_hex(PROTECTED_BUFFER_END); console_write("\n");
}
