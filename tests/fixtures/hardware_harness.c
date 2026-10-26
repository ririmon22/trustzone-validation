#include "hardware_tests.h"
#include <stdlib.h>
#include <string.h>
#define MOCK_WORDS 2048U
#define TEST_REGION 2U
#define TEST_DDR_START UINT64_C(0x70000000)
#define TEST_DDR_END UINT64_C(0x70100000)
#define TEST_PERIPHERAL UINT64_C(0xff020000)
struct mock {
    uint32_t words[MOCK_WORDS];
    unsigned int reads, writes, barriers;
    bool io_failure, corrupt_readback, permit_access, modify_secure, wrong_irq, missing_irq;
    uint64_t secure;
    uint64_t last_write;
};
static bool read_register(void *context, uint64_t address, uint32_t *value)
{
    struct mock *mock = context;
    mock->reads++;
    if (mock->io_failure) { return false; }
    size_t index = (size_t)((address & UINT64_C(0xffff)) / sizeof(uint32_t));
    if (index >= MOCK_WORDS) { return false; }
    *value = mock->words[index];
    if (mock->corrupt_readback && mock->writes != 0U) { *value ^= 1U; }
    return true;
}
static bool write_register(void *context, uint64_t address, uint32_t value)
{
    struct mock *mock = context;
    if (mock->io_failure) { return false; }
    size_t index = (size_t)((address & UINT64_C(0xffff)) / sizeof(uint32_t));
    if (index >= MOCK_WORDS) { return false; }
    mock->writes++;
    mock->last_write = address;
    mock->words[index] = value;
    return true;
}
static void barrier(void *context) { ((struct mock *)context)->barriers++; }
static bool control(void *context) { (void)context; return true; }
static bool seed(void *context, uint64_t value) { ((struct mock *)context)->secure = value; return true; }
static bool read_secure(void *context, uint64_t *value) { *value = ((struct mock *)context)->secure; return true; }
static enum access_result forbidden_read(void *context, uint64_t *value)
{
    *value = 0U;
    return ((struct mock *)context)->permit_access ? ACCESS_SUCCEEDED : ACCESS_DENIED;
}
static enum access_result forbidden_write(void *context, uint64_t value)
{
    struct mock *mock = context;
    if (mock->modify_secure) { mock->secure = value; }
    return mock->permit_access ? ACCESS_SUCCEEDED : ACCESS_DENIED;
}
static bool violation(void *context, bool write) { (void)context; (void)write; return true; }
static bool dma_seed(void *context) { return seed(context, HARDWARE_TEST_SENTINEL); }
static bool dma_unchanged(void *context) { return ((struct mock *)context)->secure == HARDWARE_TEST_SENTINEL; }
static enum access_result dma_read(void *context)
{
    return ((struct mock *)context)->permit_access ? ACCESS_SUCCEEDED : ACCESS_DENIED;
}
static enum access_result dma_write(void *context)
{
    uint64_t unused = 0U;
    enum access_result result = forbidden_read(context, &unused);
    (void)forbidden_write(context, HARDWARE_TEST_WRITE_VALUE);
    return result;
}
static bool deliver(void *context, bool secure_irq, bool secure_world, struct interrupt_observation *observation)
{
    struct mock *mock = context;
    (void)secure_world;
    bool route = mock->wrong_irq ? !secure_irq : secure_irq;
    observation->secure_handlers = route ? 1U : 0U;
    observation->nonsecure_handlers = route ? 0U : 1U;
    if (mock->missing_irq) { observation->secure_handlers = 0U; observation->nonsecure_handlers = 0U; }
    observation->resumed = true;
    observation->context_preserved = true;
    return true;
}
int main(int argc, char **argv)
{
    if (argc != 2) { return 2; }
    const char *scenario = argv[1];
    struct mock mock = { 0 };
    struct protection_io io = { &mock, read_register, write_register, barrier };
    struct protection_authority authority = { true, true, true, true, true };
    struct xmpu_region_policy xmpu = {
        authority, XMPU_DDR_FIRST_BASE, TEST_REGION, TEST_DDR_START, TEST_DDR_END,
        0U, 0U, XMPU_ALIGN_1MB, XMPU_POISON_ATTRIBUTE, XMPU_REGION_NONSECURE
    };
    const unsigned int config = (XMPU_REGION_FIRST + TEST_REGION * XMPU_REGION_STRIDE + XMPU_CONFIG_OFFSET) / sizeof(uint32_t);
    mock.words[XMPU_CTRL_OFFSET / sizeof(uint32_t)] = xmpu.expected_ctrl;
    mock.words[XMPU_POISON_OFFSET / sizeof(uint32_t)] = xmpu.expected_poison;
    mock.words[config] = xmpu.expected_disabled_config;
    if (strcmp(scenario, "xmpu-postcheck") == 0) {
        if (xmpu_program_secure_region(&io, &xmpu) != PROTECTION_OK || xmpu_verify_secure_region(&io, &xmpu) != PROTECTION_OK) { return 1; }
        mock.words[config] ^= XMPU_REGION_NONSECURE;
        return !(xmpu_verify_secure_region(&io, &xmpu) == PROTECTION_STATE_MISMATCH && mock.writes == 4U);
    }
    if (strcmp(scenario, "xmpu-blocked") == 0) {
        xmpu.authority.hardware_map_verified = false;
        return !(xmpu_program_secure_region(&io, &xmpu) == PROTECTION_BLOCKED && mock.reads == 0U && mock.writes == 0U);
    }
    if (strcmp(scenario, "xmpu-align") == 0) {
        xmpu.end = xmpu.start + XPPU_APERTURE_ALIGNMENT;
        return !(xmpu_program_secure_region(&io, &xmpu) == PROTECTION_INVALID && mock.reads == 0U);
    }
    if (strcmp(scenario, "xmpu-enable-last") == 0) {
        enum protection_result result = xmpu_program_secure_region(&io, &xmpu);
        return !(result == PROTECTION_OK && mock.writes == 4U && mock.barriers == 4U &&
            mock.last_write == XMPU_DDR_FIRST_BASE + config * sizeof(uint32_t) &&
            mock.words[config] == (XMPU_REGION_ENABLE | XMPU_REGION_READ | XMPU_REGION_WRITE));
    }
    if (strcmp(scenario, "xmpu-snapshot") == 0) {
        mock.words[0] = 0U;
        return !(xmpu_program_secure_region(&io, &xmpu) == PROTECTION_STATE_MISMATCH && mock.writes == 0U);
    }
    if (strcmp(scenario, "xmpu-io") == 0) {
        mock.io_failure = true;
        return !(xmpu_program_secure_region(&io, &xmpu) == PROTECTION_IO_ERROR && mock.writes == 0U);
    }
    if (strcmp(scenario, "xmpu-readback") == 0) {
        mock.corrupt_readback = true;
        return !(xmpu_program_secure_region(&io, &xmpu) == PROTECTION_STATE_MISMATCH && mock.writes == 1U &&
            mock.words[config] == XMPU_REGION_NONSECURE);
    }
    if (strncmp(scenario, "xppu-", 5U) == 0) {
        memset(&mock, 0, sizeof(mock));
        struct xppu_aperture_policy xppu = { 0 };
        xppu.authority = authority;
        xppu.peripheral_base = TEST_PERIPHERAL;
        xppu.aperture = TEST_REGION;
        xppu.expected_ctrl = XPPU_ENABLE;
        xppu.expected_permission = XPPU_NONSECURE | 1U;
        xppu.secure_permission = 1U;
        xppu.parity_encoding_verified = true;
        mock.words[0] = XPPU_ENABLE;
        mock.words[(XPPU_APERTURE_FIRST + TEST_REGION * XPPU_APERTURE_STRIDE) / sizeof(uint32_t)] = xppu.expected_permission;
        if (strcmp(scenario, "xppu-blocked") == 0) {
            xppu.parity_encoding_verified = false;
            return !(xppu_program_secure_aperture(&io, &xppu) == PROTECTION_BLOCKED && mock.reads == 0U);
        }
        if (strcmp(scenario, "xppu-expand") == 0) {
            xppu.secure_permission = 3U;
            return !(xppu_program_secure_aperture(&io, &xppu) == PROTECTION_INVALID && mock.writes == 0U);
        }
        if (strcmp(scenario, "xppu-profile") == 0) {
            xppu.expected_master_profiles[0] = 1U;
            return !(xppu_program_secure_aperture(&io, &xppu) == PROTECTION_STATE_MISMATCH && mock.writes == 0U);
        }
        if (strcmp(scenario, "xppu-postcheck") == 0) {
            if (xppu_program_secure_aperture(&io, &xppu) != PROTECTION_OK || xppu_verify_secure_aperture(&io, &xppu) != PROTECTION_OK) { return 1; }
            mock.words[XPPU_MASTER_FIRST / sizeof(uint32_t)] ^= 1U;
            return !(xppu_verify_secure_aperture(&io, &xppu) == PROTECTION_STATE_MISMATCH && mock.writes == 1U);
        }
        return !(xppu_program_secure_aperture(&io, &xppu) == PROTECTION_OK && mock.writes == 1U);
    }
    struct access_test_ops access = { &mock, true, control, seed, read_secure, forbidden_read, forbidden_write, violation };
    if (strcmp(scenario, "access-blocked") == 0) {
        access.configuration_verified = false;
        return run_access_denial_test(&access) != HARDWARE_TEST_BLOCKED;
    }
    if (strcmp(scenario, "access-permitted") == 0) {
        mock.permit_access = true;
        return run_access_denial_test(&access) != HARDWARE_TEST_FAIL;
    }
    if (strcmp(scenario, "access-modified") == 0) {
        mock.modify_secure = true;
        return run_access_denial_test(&access) != HARDWARE_TEST_FAIL;
    }
    if (strcmp(scenario, "access-valid") == 0) { return run_access_denial_test(&access) != HARDWARE_TEST_PASS; }
    struct dma_test_ops dma = { &mock, true, control, dma_seed, dma_unchanged, dma_read, control, dma_write, violation };
    if (strcmp(scenario, "dma-valid") == 0) { return run_dma_protection_test(&dma) != HARDWARE_TEST_PASS; }
    if (strcmp(scenario, "dma-modified") == 0) {
        mock.modify_secure = true;
        return run_dma_protection_test(&dma) != HARDWARE_TEST_FAIL;
    }
    if (strcmp(scenario, "dma-blocked") == 0) {
        dma.route_and_nonsecure_master_verified = false;
        return run_dma_protection_test(&dma) != HARDWARE_TEST_BLOCKED;
    }
    struct interrupt_test_ops irq = { &mock, true, deliver };
    if (strcmp(scenario, "irq-valid") == 0) { return run_interrupt_isolation_test(&irq) != HARDWARE_TEST_PASS; }
    if (strcmp(scenario, "irq-wrong") == 0) {
        mock.wrong_irq = true;
        return run_interrupt_isolation_test(&irq) != HARDWARE_TEST_FAIL;
    }
    if (strcmp(scenario, "irq-missing") == 0) {
        mock.missing_irq = true;
        return run_interrupt_isolation_test(&irq) != HARDWARE_TEST_FAIL;
    }
    if (strcmp(scenario, "irq-blocked") == 0) {
        irq.gic_and_routing_verified = false;
        return run_interrupt_isolation_test(&irq) != HARDWARE_TEST_BLOCKED;
    }
    return 2;
}
