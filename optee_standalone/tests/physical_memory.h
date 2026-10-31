#ifndef B2_PHYSICAL_MEMORY_H
#define B2_PHYSICAL_MEMORY_H
#include "../pta/physical_probe.h"
#include "functional.h"
#ifndef B2_PHYSICAL_DEDICATED
#define B2_PHYSICAL_DEDICATED 0U
#endif
#if B2_PHYSICAL_DEDICATED
#define B2_PHYS_ALLOWED_BASE B2_PROBE_REGION_BASE
#define B2_PHYS_ALLOWED_END B2_PROBE_REGION_END
#else
#define B2_PHYS_ALLOWED_BASE OPTEE_RESERVED_BASE
#define B2_PHYS_ALLOWED_END OPTEE_RESERVED_END
#endif
#define B2_PHYSICAL_RECORD_VERSION 1U
#define B2_PHYSICAL_ATTEMPTS 4U
#define B2_PHYSICAL_SELECTION 15U
#define B2_ALIAS_ROOT_INDEX 2U
#define B2_PHYS_ALIAS_BASE 0x80000000UL
#ifndef B2_PHYS_REVIEWED
#define B2_PHYS_REVIEWED 0U
#endif
struct b2_physical_attempt
{
    uint64_t va, pa, write, completed, value, esr, far, elr, classification, par_read, par_write;
};
struct b2_physical_record
{
    uint64_t version, run_id, state, page_pa, page_bytes, entry_el, sctlr, tcr, ttbr0, mair;
    uint64_t el2_known, hcr, vttbr, vtcr, alias_va, probe_mair, root_descriptor;
    struct b2_physical_attempt attempts[B2_PHYSICAL_ATTEMPTS];
};
extern volatile struct b2_physical_record b2_physical_record;
void b2_run_physical_memory(void);
uint64_t b2_handle_phys_fault(uint64_t esr, uint64_t far, uint64_t pc, uint64_t sctlr);
unsigned int b2_classify_physical_abort(uint64_t esr);
#endif
