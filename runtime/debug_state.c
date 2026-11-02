#include "runtime.h"
#include "debug_log.h"
/* 各ELFまたは単一ELF内の各Worldが別の実体を持つ。 */
#ifndef DEBUG_RUN_ID
#define DEBUG_RUN_ID 1UL
#endif
volatile const uint64_t debug_run_id = DEBUG_RUN_ID;
volatile struct boot_debug_state debug_boot;
volatile struct exception_record debug_exception;
volatile struct exception_record debug_sync_exception;
void debug_stage(uint64_t stage, uint64_t reason)
{
    debug_boot.version = 1U;
    debug_boot.run_id = debug_run_id;
    debug_boot.stage = stage;
    debug_boot.stop_reason = reason;
}

volatile struct handoff_record debug_handoff;
