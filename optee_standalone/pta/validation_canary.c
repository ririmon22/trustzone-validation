#include <kernel/pseudo_ta.h>
#include <stdint.h>
#include <malloc.h>
#include <mm/phys_mem.h>
#include <pta_stats.h>
#include <tee_api_defines.h>
#include "validation_canary.h"
#define CANARY_INITIAL 0x749a52c1U
/* Core BSSではなく初期化済みprivate dataに置き、session終了でも保持する。 */
static volatile uint32_t core_canary = CANARY_INITIAL;
static TEE_Result invoke_canary(void *context, uint32_t command, uint32_t types, TEE_Param params[4])
{
    (void)context;
    if (types != TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INOUT, TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE, TEE_PARAM_TYPE_NONE)) { return TEE_ERROR_BAD_PARAMETERS; }
    if (command == CANARY_COMMAND_ADDRESS) {
        /* 攻撃試験用のpointer公開であり、製品Coreには組み込まない。 */
        if ((uintptr_t)&core_canary > UINT32_MAX) { return TEE_ERROR_NOT_SUPPORTED; }
        params[0].value.a = (uint32_t)(uintptr_t)&core_canary;
        return TEE_SUCCESS;
    }
    if (command == CANARY_COMMAND_CHECK) {
        if (core_canary != CANARY_INITIAL) { return TEE_ERROR_SECURITY; }
        params[0].value.a = CANARY_CHECK_TAG;
        return TEE_SUCCESS;
    }
#ifdef CFG_WITH_STATS
    if (command == CANARY_COMMAND_HEAP || command == CANARY_COMMAND_TA_RAM || command == CANARY_COMMAND_ALLOC_FAILURES) {
        struct pta_stats_alloc stats = { 0 };
        if (command == CANARY_COMMAND_TA_RAM) { phys_mem_stats(&stats, false); }
        else { malloc_get_stats(&stats); }
        params[0].value.a = command == CANARY_COMMAND_ALLOC_FAILURES ? stats.num_alloc_fail : stats.allocated;
        return TEE_SUCCESS;
    }
#endif
    return TEE_ERROR_BAD_PARAMETERS;
}
pseudo_ta_register(.uuid = VALIDATION_CANARY_UUID, .name = "validation-canary.pta",
    .flags = PTA_DEFAULT_FLAGS, .invoke_command_entry_point = invoke_canary);
