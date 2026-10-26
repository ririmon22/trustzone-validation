#include "runtime.h"
#define ESR_EC_DATA_ABORT 0x25UL
#define ESR_EC_SHIFT 26U
#define ESR_EC_MASK 0x3fUL
#define ESR_WRITE_BIT (1UL << 6)
#define ESR_DFSC_MASK 0x3fUL
#define DFSC_SYNC_EXTERNAL 0x10UL
#define DFSC_PERMISSION_BASE 0x0cUL
#define DFSC_PERMISSION_LAST 0x0fUL
extern char probe_read_fault[], probe_read_resume[], probe_write_fault[], probe_write_resume[];
volatile uint64_t expected_fault;
volatile uint64_t expected_fault_address;
volatile uint64_t fault_count;
volatile uint64_t last_fault_esr;
uint64_t handle_el1_sync(uint64_t esr, uint64_t pc)
{
    uint64_t far, sctlr;
    __asm__ volatile("mrs %0, far_el1; mrs %1, sctlr_el1" : "=r"(far), "=r"(sctlr));
    if ((esr & (1UL << 10)) != 0UL || far != expected_fault_address) { return 0UL; }
    if ((sctlr & 1UL) == 0UL && (esr & ESR_DFSC_MASK) != DFSC_SYNC_EXTERNAL) { return 0UL; }
    uint64_t dfsc = esr & ESR_DFSC_MASK;
    bool data_abort = ((esr >> ESR_EC_SHIFT) & ESR_EC_MASK) == ESR_EC_DATA_ABORT;
    bool denied = dfsc == DFSC_SYNC_EXTERNAL || (dfsc >= DFSC_PERMISSION_BASE && dfsc <= DFSC_PERMISSION_LAST);
    if (expected_fault == 0UL || !data_abort || !denied) { return 0UL; }
    last_fault_esr = esr;
    console_write("MEM_ABORT address="); console_hex(expected_fault_address);
    console_write(" far="); console_hex(far); console_write(" esr="); console_hex(esr);
    console_write(" pc="); console_hex(pc); console_write(" sctlr="); console_hex(sctlr); console_write("\n");
    if (expected_fault == 1UL && pc == (uint64_t)probe_read_fault && (esr & ESR_WRITE_BIT) == 0UL) {
        fault_count++; return (uint64_t)probe_read_resume;
    }
    if (expected_fault == 2UL && pc == (uint64_t)probe_write_fault && (esr & ESR_WRITE_BIT) != 0UL) {
        fault_count++; return (uint64_t)probe_write_resume;
    }
    return 0UL;
}
