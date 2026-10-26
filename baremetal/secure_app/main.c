#include "runtime.h"
static uint64_t secure_counter;
void secure_main(void)
{
    volatile uint64_t *protected = (volatile uint64_t *)PROTECTED_BUFFER_BASE;
    volatile uint64_t *boundary = (volatile uint64_t *)(PROTECTED_BUFFER_END - sizeof(uint64_t));
    if (read_current_el() != EXPECTED_EL1) { console_write("FATAL secure expected EL1\n"); platform_halt(); }
    *protected = TEST_SENTINEL;
    *boundary = TEST_SENTINEL;
    console_write("BOOT Secure EL1\n");
    report_memory_state("secure");
    struct smc_result request = smc_call(SMC_SECURE_READY, 0UL, 0UL, 0UL);
    for (;;) {
        uint64_t result = UINT64_MAX;
        if (request.x0 == SERVICE_ACCUMULATE) {
            secure_counter += request.x1;
            result = secure_counter;
        } else if (request.x0 == SERVICE_GET_COUNTER) {
            result = secure_counter;
        } else if (request.x0 == SERVICE_CHECK_BUFFER) {
            result = (*protected == TEST_SENTINEL && *boundary == TEST_SENTINEL) ? 1UL : 0UL;
            console_write("MEM_RECHECK intact="); console_uint(result); console_write("\n");
        } else if (request.x0 == SERVICE_GET_EL) {
            result = read_current_el();
        }
        request = smc_call(SMC_SERVICE_RETURN, result, 0UL, 0UL);
    }
}
