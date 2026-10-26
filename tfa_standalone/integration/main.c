#include "test_runner.h"
#define SMCCC_VERSION_FID 0x80000000UL
#define PSCI_VERSION_FID 0x84000000UL
#define SMCCC_VERSION_1_0 0x00010000UL
#define SMCCC_VERSION_1_7 0x00010007UL
#define PSCI_VERSION_0_2 0x00000002UL
#define PSCI_VERSION_1_3 0x00010003UL
void standalone_main(void)
{
    test_begin("tfa");
    if (read_current_el() != EXPECTED_EL1) {
        console_write("FATAL BL33 expected NS EL1, verify TF-A handoff configuration\n"); platform_halt();
    }
    uint64_t smccc = smc_call(SMCCC_VERSION_FID, 0UL, 0UL, 0UL).x0;
    uint64_t psci = smc_call(PSCI_VERSION_FID, 0UL, 0UL, 0UL).x0;
    bool valid = smccc >= SMCCC_VERSION_1_0 && smccc <= SMCCC_VERSION_1_7 &&
        psci >= PSCI_VERSION_0_2 && psci <= PSCI_VERSION_1_3;
    console_write("SMCCC_VERSION="); console_hex(smccc); console_write(" PSCI_VERSION="); console_hex(psci); console_write("\n");
    test_report("B1-SMCCC-01", valid ? TEST_PASS : TEST_FAIL, "standard-interface-smoke-only", 0UL);
    test_report("TZ-SYS-01", TEST_BLOCKED, "secure-EL1-payload-not-integrated", 0UL);
    test_report("TZ-SYS-02", TEST_BLOCKED, "SMCCC-smoke-does-not-cover-world-context-test", 0UL);
    test_report("TZ-SYS-03", TEST_BLOCKED, "platform-protection-test-not-integrated", 0UL);
    test_report("TZ-SYS-04", TEST_BLOCKED, "peripheral-service-missing", 0UL);
    test_report("TZ-SYS-05", TEST_SKIP, "Zynq-DMA-not-on-virt", 0UL);
    test_report("TZ-SYS-06", TEST_BLOCKED, "interrupt-test-missing", 0UL);
    test_report("TZ-SYS-07", TEST_BLOCKED, "requires-hardware", 0UL);
    test_finish(); platform_halt();
}
