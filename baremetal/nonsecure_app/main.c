#include "test_runner.h"
#include "benchmark.h"
extern uint64_t test_register_context(void);
extern void probe_read(uint64_t address);
extern void probe_write(uint64_t address, uint64_t value);
extern volatile uint64_t expected_fault, expected_fault_address, fault_count;
static uint64_t normal_counter;
static uint64_t secure_request(uint64_t operation, uint64_t argument)
{
    return smc_call(SMC_RUN_SERVICE, operation, argument, 0UL).x0;
}
static bool test_separation(void)
{
    uint64_t secure = secure_request(SERVICE_GET_COUNTER, 0UL);
    normal_counter = SHARED_TOKEN;
    for (unsigned int i = 1U; i <= TEST_ITERATIONS; ++i) {
        uint64_t saved = normal_counter;
        secure += i;
        if (secure_request(SERVICE_ACCUMULATE, i) != secure || normal_counter != saved) { return false; }
        normal_counter += 2UL;
        if (secure_request(SERVICE_GET_COUNTER, 0UL) != secure) { return false; }
    }
    return normal_counter == SHARED_TOKEN + 2UL * TEST_ITERATIONS;
}
static bool test_switching(void)
{
    if (read_current_el() != EXPECTED_EL1 || secure_request(SERVICE_GET_EL, 0UL) != EXPECTED_EL1) { return false; }
    uint64_t expected = secure_request(SERVICE_GET_COUNTER, 0UL);
    for (unsigned int i = 0U; i < TEST_ITERATIONS; ++i) {
        uint64_t argument = (uint64_t)i * 7UL;
        expected += argument;
        if (secure_request(SERVICE_ACCUMULATE, argument) != expected) { return false; }
    }
    if (test_register_context() != 1UL) { return false; }
    return smc_call(UINT64_MAX, 0UL, 0UL, 0UL).x0 == UINT64_MAX;
}
static bool test_memory_protection(void)
{
    volatile uint64_t *shared = (volatile uint64_t *)SHARED_MEMORY_BASE;
    *shared = SHARED_TOKEN;
    if (*shared != SHARED_TOKEN || secure_request(SERVICE_CHECK_BUFFER, 0UL) != 1UL) { return false; }
    uint64_t sctlr;
    __asm__ volatile("mrs %0, sctlr_el1" : "=r"(sctlr));
    report_memory_state("nonsecure");
    if ((sctlr & 1UL) != 0UL) { return false; }
    fault_count = 0UL;
    const uint64_t addresses[] = { PROTECTED_BUFFER_BASE, PROTECTED_BUFFER_END - sizeof(uint64_t) };
    for (unsigned int i = 0U; i < ARRAY_SIZE(addresses); ++i) {
        expected_fault_address = addresses[i];
        expected_fault = 1UL;
        probe_read(addresses[i]);
        expected_fault = 2UL;
        probe_write(addresses[i], 0UL);
        expected_fault = 0UL;
        if (fault_count != (uint64_t)(i + 1U) * 2UL) { return false; }
        if (secure_request(SERVICE_CHECK_BUFFER, 0UL) != 1UL) { return false; }
    }
    return *shared == SHARED_TOKEN;
}
static void benchmark_switch(void) { (void)secure_request(SERVICE_GET_COUNTER, 0UL); }
static void benchmark_memory(void)
{
    volatile uint64_t *shared = (volatile uint64_t *)SHARED_MEMORY_BASE;
    for (unsigned int i = 0U; i < 64U; ++i) { shared[i] = i; }
    __asm__ volatile("dsb sy" ::: "memory");
}
static void run_boolean(const char *id, bool (*operation)(void), const char *detail)
{
    uint64_t start = counter_read();
    bool passed = operation();
    test_report(id, passed ? TEST_PASS : TEST_FAIL, detail, counter_read() - start);
}
void nonsecure_main(void)
{
    (void)log_printf("%s", "BOOT Non-secure EL1\n");
#ifdef TZ_FAULT_INJECTION
    /* QEMU専用のsnapshot検査。通常buildと実機向けでは定義しない。 */
    __asm__ volatile("mov x0, #0x1234; mov x1, #0x5678; udf #0" ::: "x0", "x1", "memory");
#endif
    test_begin("baremetal");
    run_boolean("TZ-SYS-01", test_separation, "independent-counters");
    run_boolean("TZ-SYS-02", test_switching, "repeated-world-roundtrip");
    run_boolean("TZ-SYS-03", test_memory_protection, "allowed-access+denied-read-write+secure-recheck+boundaries");
    test_report("TZ-SYS-04", TEST_BLOCKED, "secure-peripheral-test-not-implemented", 0UL);
    test_report("TZ-SYS-05", TEST_SKIP, "Zynq-DMA-not-on-virt", 0UL);
    test_report("TZ-SYS-06", TEST_BLOCKED, "GIC-driver-and-interrupt-test-not-implemented", 0UL);
    test_report("TZ-SYS-07", TEST_BLOCKED, "requires-hardware-debug-policy", 0UL);
    benchmark_run("PERF-01", benchmark_switch, "SMC-secure-service-roundtrip-including-dispatch");
    benchmark_run("PERF-02", benchmark_memory, "64-u64-stores-and-dsb");
    test_finish();
    platform_halt();
}
