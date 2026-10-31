#include "physical_memory.h"
/* 同期外部abort以外を物理保護成立の根拠にしない。S1PTW/FnVも除外する。 */
unsigned int b2_classify_physical_abort(uint64_t esr)
{
    unsigned int code = (unsigned int)(esr & 0x3fUL);
    if (((esr >> 26U) & 0x3fUL) != 0x25UL || (esr & ((1UL << 10U) | (1UL << 7U))) != 0UL)
    {
        return 4U;
    }
    if (code >= 4U && code <= 7U)
    {
        return 1U;
    }
    if (code >= 12U && code <= 15U)
    {
        return 2U;
    }
    return code == 16U ? 3U : 4U;
}
