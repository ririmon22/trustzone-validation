#include "backends.h"
#include "access_probe.h"
#include <stdlib.h>
#include <string.h>
#define MOCK_DWORDS 1024U
#define MOCK_DMA_ATTR ZDMA_BURST_INCREMENT
#define MOCK_XMPU_MASTER 0x123U
#define MOCK_ADDRESS UINT64_C(0x62010000)
#define MOCK_SECURE_SGI 8U
#define MOCK_NORMAL_SGI 9U
struct mock {
    uint32_t dma[MOCK_DWORDS], distributor[MOCK_DWORDS], cpu[MOCK_DWORDS], xpu[8];
    unsigned int reads, writes;
    bool transfer_started, dma_error, timeout, corrupt, stale, wrong_master, modify_secure;
    unsigned int transfers;
    uint64_t secure_value;
    uint32_t eoi;
    unsigned char source[64], destination[64];
};
void console_write(const char *text) { (void)text; }
_Noreturn void platform_halt(void) { exit(3); }
char zcu106_probe_load_fault[1], zcu106_probe_load_resume[1];
char zcu106_probe_store_fault[1], zcu106_probe_store_resume[1];
uint64_t zcu106_probe_load(uint64_t address) { (void)address; return 0U; }
void zcu106_probe_store(uint64_t address, uint64_t value) { (void)address; (void)value; }
char zcu106_probe_load32_fault[1], zcu106_probe_load32_resume[1];
char zcu106_probe_store32_fault[1], zcu106_probe_store32_resume[1];
uint32_t zcu106_probe_load32(uint64_t address) { (void)address; return 0U; }
void zcu106_probe_store32(uint64_t address, uint32_t value) { (void)address; (void)value; }
static uint32_t *word(struct mock *mock, uint64_t address)
{
    if (address >= ZDMA_ADMA_BASE && address < ZDMA_ADMA_BASE + sizeof(mock->dma)) { return &mock->dma[(address - ZDMA_ADMA_BASE) / 4U]; }
    if (address >= GICD_BASE && address < GICD_BASE + sizeof(mock->distributor)) { return &mock->distributor[(address - GICD_BASE) / 4U]; }
    if (address >= GICC_BASE && address < GICC_BASE + sizeof(mock->cpu)) { return &mock->cpu[(address - GICC_BASE) / 4U]; }
    if (address >= XMPU_DDR_FIRST_BASE && address < XMPU_DDR_FIRST_BASE + sizeof(mock->xpu)) { return &mock->xpu[(address - XMPU_DDR_FIRST_BASE) / 4U]; }
    return NULL;
}
static bool read32(void *context, uint64_t address, uint32_t *value)
{
    struct mock *mock = context;
    uint32_t *reg = word(mock, address);
    mock->reads++;
    if (reg == NULL) { return false; }
    *value = *reg;
    if (mock->corrupt && address == ZDMA_ADMA_BASE + ZDMA_SRC_WORD0) { *value ^= 1U; }
    return true;
}
static bool write32(void *context, uint64_t address, uint32_t value)
{
    struct mock *mock = context;
    uint32_t *reg = word(mock, address);
    mock->writes++;
    if (reg == NULL) { return false; }
    if (address == ZDMA_ADMA_BASE + ZDMA_ISR || address == XMPU_DDR_FIRST_BASE + XPU_ISR) {
        if (!mock->stale) { *reg &= ~value; }
        return true;
    }
    *reg = value;
    if (address == GICC_BASE + GICC_EOIR) { mock->eoi = value; }
    if (address == ZDMA_ADMA_BASE + ZDMA_CTRL2 && value == ZDMA_ENABLE) {
        mock->transfer_started = true;
        mock->transfers++;
        uint64_t source = mock->dma[ZDMA_SRC_WORD0 / 4U] | ((uint64_t)mock->dma[(ZDMA_SRC_WORD0 + 4U) / 4U] << 32U);
        uint64_t destination = mock->dma[ZDMA_DST_WORD0 / 4U] | ((uint64_t)mock->dma[(ZDMA_DST_WORD0 + 4U) / 4U] << 32U);
        if (source == MOCK_ADDRESS || destination == MOCK_ADDRESS) {
            mock->xpu[XPU_ISR / 4U] = XMPU_SECURITY_VIOLATION;
            mock->xpu[XPU_ERR_ADDRESS / 4U] = (uint32_t)(MOCK_ADDRESS >> 12U);
            mock->xpu[XPU_ERR_MASTER / 4U] = MOCK_XMPU_MASTER + (mock->wrong_master ? 1U : 0U);
            mock->dma[ZDMA_ISR / 4U] = source == MOCK_ADDRESS ? ZDMA_READ_ERROR : ZDMA_WRITE_ERROR;
            mock->dma[ZDMA_STATUS / 4U] = ZDMA_STATE_ERROR;
            mock->dma[ZDMA_CTRL2 / 4U] = 0U;
            if (mock->modify_secure && destination == MOCK_ADDRESS) { mock->secure_value = 0U; }
            return true;
        }
        if (mock->timeout) { mock->dma[ZDMA_STATUS / 4U] = 2U; return true; }
        mock->dma[ZDMA_ISR / 4U] = mock->dma_error ? ZDMA_DATA_ERRORS : ZDMA_DONE;
        mock->dma[ZDMA_STATUS / 4U] = mock->dma_error ? ZDMA_STATE_ERROR : ZDMA_STATE_IDLE;
        if (!mock->dma_error) { memcpy(mock->destination, mock->source, sizeof(mock->source)); }
        mock->dma[ZDMA_TOTAL_BYTE / 4U] = (uint32_t)sizeof(mock->source);
        mock->dma[ZDMA_CTRL2 / 4U] = 0U;
    }
    return true;
}
static void barrier(void *context) { (void)context; }
static bool prepare_normal(void *context)
{
    struct mock *mock = context;
    memset(mock->source, 0xa5, sizeof(mock->source));
    memset(mock->destination, 0, sizeof(mock->destination));
    return true;
}
static bool normal_matches(void *context)
{
    struct mock *mock = context;
    return memcmp(mock->source, mock->destination, sizeof(mock->source)) == 0;
}
static bool seed_secure(void *context) { ((struct mock *)context)->secure_value = HARDWARE_TEST_SENTINEL; return true; }
static bool secure_unchanged(void *context) { return ((struct mock *)context)->secure_value == HARDWARE_TEST_SENTINEL; }
int main(int argc, char **argv)
{
    if (argc != 2) { return 2; }
    const char *scenario = argv[1];
    struct mock mock = { 0 };
    struct protection_io io = { &mock, read32, write32, barrier };
    struct zdma_policy dma = { &io, ZDMA_ADMA_BASE, true, true, true, true, MOCK_DMA_ATTR,
        (uint64_t)mock.source, (uint64_t)mock.source + sizeof(mock.source),
        (uint64_t)mock.destination, (uint64_t)mock.destination + sizeof(mock.destination), 4U };
    mock.dma[ZDMA_DATA_ATTR / 4U] = MOCK_DMA_ATTR;
    struct zdma_completion completion;
    memset(mock.source, 0xa5, sizeof(mock.source));
    if (strncmp(scenario, "adapter-", 8U) == 0) {
        struct dma_validation_backend backend = { 0 };
        backend.channel = dma;
        backend.protection = (struct xpu_probe){ &io, XMPU_DDR_FIRST_BASE, true, false };
        backend.master_id = MOCK_XMPU_MASTER;
        backend.normal_source = (uint64_t)mock.source;
        backend.normal_destination = (uint64_t)mock.destination;
        backend.secure_buffer = MOCK_ADDRESS;
        backend.size = sizeof(mock.source);
        backend.memory_context = &mock;
        backend.prepare_normal = prepare_normal;
        backend.normal_copy_matches = normal_matches;
        backend.seed_secure = seed_secure;
        backend.secure_unchanged = secure_unchanged;
        backend.read_destination_unchanged = normal_matches;
        struct dma_test_ops ops;
        if (strcmp(scenario, "adapter-blocked") == 0) {
            backend.channel.route_verified = false;
            return !(zcu106_make_dma_test_ops(&backend, &ops) == PROTECTION_BLOCKED && mock.writes == 0U);
        }
        if (zcu106_make_dma_test_ops(&backend, &ops) != PROTECTION_OK) { return 1; }
        if (strcmp(scenario, "adapter-master") == 0) { mock.wrong_master = true; }
        if (strcmp(scenario, "adapter-modified") == 0) { mock.modify_secure = true; }
        enum hardware_test_result expected = mock.wrong_master ? HARDWARE_TEST_BLOCKED : (mock.modify_secure ? HARDWARE_TEST_FAIL : HARDWARE_TEST_PASS);
        enum hardware_test_result actual = run_dma_protection_test(&ops);
        return !(actual == expected && (expected != HARDWARE_TEST_PASS || mock.transfers == 4U));
    }
    if (strncmp(scenario, "dma-", 4U) == 0) {
        enum zdma_result expected = ZDMA_COMPLETE;
        if (strcmp(scenario, "dma-blocked") == 0) { dma.route_verified = false; expected = ZDMA_BLOCKED; }
        if (strcmp(scenario, "dma-error") == 0) { mock.dma_error = true; expected = ZDMA_TRANSFER_ERROR; }
        if (strcmp(scenario, "dma-timeout") == 0) { mock.timeout = true; expected = ZDMA_TIMEOUT; }
        if (strcmp(scenario, "dma-readback") == 0) { mock.corrupt = true; expected = ZDMA_IO_ERROR; }
        if (strcmp(scenario, "dma-stale") == 0) { mock.stale = true; mock.dma[ZDMA_ISR / 4U] = ZDMA_DONE; expected = ZDMA_TRANSFER_ERROR; }
        if (strcmp(scenario, "dma-range") == 0) { dma.source_end--; expected = ZDMA_INVALID; }
        enum zdma_result actual = zdma_copy(&dma, (uint64_t)mock.source, (uint64_t)mock.destination, (uint32_t)sizeof(mock.source), &completion);
        if (actual != expected) { return 1; }
        if (expected == ZDMA_COMPLETE) {
            return !(completion.started && completion.bytes_observed == sizeof(mock.source) && memcmp(mock.source, mock.destination, sizeof(mock.source)) == 0 &&
                mock.dma[ZDMA_SRC_WORD0 / 4U] == (uint32_t)(uintptr_t)mock.source && mock.dma[(ZDMA_DST_WORD0 + 8U) / 4U] == sizeof(mock.source));
        }
        if (expected == ZDMA_BLOCKED) { return !(mock.reads == 0U && mock.writes == 0U); }
        if (expected == ZDMA_IO_ERROR || expected == ZDMA_INVALID || mock.stale) { return mock.transfer_started ? 1 : 0; }
        return completion.started ? 0 : 1;
    }
    struct gic_sgi_policy gic = { &io, true, true, true, 0x0200143bU, 0U, 0U, 0U, MOCK_SECURE_SGI, MOCK_NORMAL_SGI, 0x40U, 0x80U };
    mock.distributor[GICD_TYPER / 4U] = GIC_SECURITY_EXTENSIONS;
    mock.distributor[GICD_IIDR / 4U] = gic.expected_iidr;
    if (strncmp(scenario, "gic-", 4U) == 0) {
        if (strcmp(scenario, "gic-blocked") == 0) {
            gic.vectors_and_routing_verified = false;
            return !(gic_prepare_sgis(&gic) == PROTECTION_BLOCKED && mock.reads == 0U && mock.writes == 0U);
        }
        if (strcmp(scenario, "gic-pending") == 0) {
            mock.distributor[GICD_ISPENDR0 / 4U] = 1U << MOCK_SECURE_SGI;
            return !(gic_prepare_sgis(&gic) == PROTECTION_STATE_MISMATCH && mock.writes == 0U);
        }
        if (gic_prepare_sgis(&gic) != PROTECTION_OK) { return 1; }
        if (strcmp(scenario, "gic-dispatch") == 0 || strcmp(scenario, "gic-wrong-handler") == 0) {
            volatile struct interrupt_observation observation = { 0U, 0U, 0U, false, false };
            if (zcu106_bind_interrupt_handler(&gic, &observation, true) != PROTECTION_OK) { return 1; }
            bool wrong = strcmp(scenario, "gic-wrong-handler") == 0;
            mock.cpu[GICC_IAR / 4U] = wrong ? MOCK_NORMAL_SGI : MOCK_SECURE_SGI;
            zcu106_irq_dispatch();
            return !(observation.secure_handlers == (wrong ? 0U : 1U) &&
                observation.unexpected_handlers == (wrong ? 1U : 0U) &&
                !observation.resumed && !observation.context_preserved && mock.eoi == mock.cpu[GICC_IAR / 4U]);
        }
        if (strcmp(scenario, "gic-eoi") == 0) {
            uint32_t token;
            mock.cpu[GICC_IAR / 4U] = (2U << 10) | MOCK_SECURE_SGI;
            return !(gic_acknowledge(&gic, &token) == PROTECTION_OK && gic_end_interrupt(&gic, token) == PROTECTION_OK && mock.eoi == token);
        }
        if (strcmp(scenario, "gic-spurious") == 0) {
            uint32_t token;
            mock.cpu[GICC_IAR / 4U] = 1023U;
            return !(gic_acknowledge(&gic, &token) == PROTECTION_STATE_MISMATCH && gic_end_interrupt(&gic, token) == PROTECTION_INVALID && mock.eoi == 0U);
        }
        if (gic_send_sgi(&gic, true) != PROTECTION_OK || mock.distributor[GICD_SGIR / 4U] != (GIC_SGI_SELF | MOCK_SECURE_SGI)) { return 1; }
        return !(gic_send_sgi(&gic, false) == PROTECTION_OK && mock.distributor[GICD_SGIR / 4U] == (GIC_SGI_SELF | GIC_SGI_NONSECURE | MOCK_NORMAL_SGI));
    }
    struct xpu_probe probe = { &io, XMPU_DDR_FIRST_BASE, true, false };
    struct xpu_capture capture;
    if (strncmp(scenario, "xpu-", 4U) == 0) {
        if (strcmp(scenario, "xpu-blocked") == 0) {
            probe.exclusive_verified = false;
            return !(xpu_probe_begin(&probe) == PROTECTION_BLOCKED && mock.reads == 0U && mock.writes == 0U);
        }
        mock.xpu[XPU_ISR / 4U] = XMPU_SECURITY_VIOLATION;
        if (xpu_probe_begin(&probe) != PROTECTION_OK) { return 1; }
        mock.xpu[XPU_ISR / 4U] = XMPU_SECURITY_VIOLATION;
        mock.xpu[XPU_ERR_ADDRESS / 4U] = (uint32_t)(MOCK_ADDRESS >> 12U);
        mock.xpu[XPU_ERR_MASTER / 4U] = MOCK_XMPU_MASTER;
        if (xpu_probe_capture(&probe, &capture) != PROTECTION_OK) { return 1; }
        if (strcmp(scenario, "xpu-master") == 0) { capture.master++; return xpu_capture_matches(&capture, MOCK_ADDRESS, MOCK_XMPU_MASTER, XMPU_SECURITY_VIOLATION) ? 1 : 0; }
        if (strcmp(scenario, "xpu-page") == 0) { capture.address_page++; return xpu_capture_matches(&capture, MOCK_ADDRESS, MOCK_XMPU_MASTER, XMPU_SECURITY_VIOLATION) ? 1 : 0; }
        return !(xpu_capture_matches(&capture, MOCK_ADDRESS, MOCK_XMPU_MASTER, XMPU_SECURITY_VIOLATION) && xpu_probe_capture(&probe, &capture) == PROTECTION_BLOCKED);
    }
    if (strcmp(scenario, "abort32-valid") == 0) {
        zcu106_fault_probe.armed = true;
        zcu106_fault_probe.width = sizeof(uint32_t);
        zcu106_fault_probe.address = MOCK_ADDRESS;
        zcu106_fault_probe.write = false;
        return zcu106_handle_probe_abort((UINT64_C(0x25) << 26U) | 0x10U, MOCK_ADDRESS,
            (uint64_t)zcu106_probe_load32_fault) == (uint64_t)zcu106_probe_load32_resume ? 0 : 1;
    }
    if (strcmp(scenario, "abort-address") == 0 || strcmp(scenario, "abort-valid") == 0) {
        zcu106_fault_probe.armed = true;
        zcu106_fault_probe.width = sizeof(uint64_t);
        zcu106_fault_probe.address = MOCK_ADDRESS;
        zcu106_fault_probe.write = false;
        uint64_t far = strcmp(scenario, "abort-address") == 0 ? MOCK_ADDRESS + 8U : MOCK_ADDRESS;
        uint64_t resume = zcu106_handle_probe_abort((UINT64_C(0x25) << 26U) | 0x10U, far, (uint64_t)zcu106_probe_load_fault);
        return strcmp(scenario, "abort-address") == 0 ? (resume != 0U) : !(resume == (uint64_t)zcu106_probe_load_resume && zcu106_fault_probe.fault_seen);
    }
    return 2;
}
