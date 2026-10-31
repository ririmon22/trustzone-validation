#include "physical_memory.h"
#define PHYS_PAGE_MASK (~(uint64_t)(PHYSICAL_PAGE_BYTES - 1U))
#define TABLE_ADDRESS_MASK 0x000000fffffff000UL
#define DESC_TABLE 3UL
#define DESC_TEST_PAGE (3UL | (1UL << 10) | (3UL << 8) | (2UL << 2) | (1UL << 53) | (1UL << 54))
#define MAIR_TEST_SLOT (0x44UL << 16)
#define HCR_STAGE2 1UL
#define PHYS_REPLACEMENT 0xdeadc0de52831749UL
#define PHYS_ECHO 0xa35bU
const uint64_t b2_physical_reservation[2] = {B2_PHYS_ALLOWED_BASE, B2_PHYS_ALLOWED_END};
volatile struct b2_physical_record b2_physical_record __attribute__((aligned(64)));
static uint64_t alias_l2[512] __attribute__((aligned(PHYSICAL_PAGE_BYTES)));
static uint64_t alias_l3[512] __attribute__((aligned(PHYSICAL_PAGE_BYTES)));
static volatile struct b2_physical_attempt *active_attempt;
extern volatile uint64_t expected_fault;
extern uint64_t b2_physical_read(uint64_t address);
extern void b2_physical_write(uint64_t address, uint64_t value);
extern char b2_physical_read_fault[], b2_physical_read_resume[], b2_physical_write_fault[],
    b2_physical_write_resume[];
static const uint8_t physical_uuid[16] = {0x6d, 0xc1, 0x34, 0x1b, 0x53, 0x4f, 0x46, 0x42,
                                          0x91, 0x38, 0x21, 0x58, 0x76, 0x30, 0x49, 0x10};
static bool success(struct tee_reply reply)
{
    return reply.transport == TEE_CLIENT_OK && reply.result == TEE_SUCCESS;
}
static void publish_physical(void)
{
    uint64_t sctlr;
    __asm__ volatile("mrs %0, sctlr_el1" : "=r"(sctlr));
    if (sctlr & 4UL)
    {
        uintptr_t end = (uintptr_t)&b2_physical_record + sizeof(b2_physical_record);
        for (uintptr_t line = (uintptr_t)&b2_physical_record; line < end; line += 64UL)
        {
            __asm__ volatile("dc cvac, %0" ::"r"(line) : "memory");
        }
    }
    __asm__ volatile("dsb sy" ::: "memory");
}
uint64_t b2_handle_phys_fault(uint64_t esr, uint64_t far, uint64_t pc, uint64_t sctlr)
{
    volatile struct b2_physical_attempt *attempt = active_attempt;
    if (!attempt || far != attempt->va || (esr & (1UL << 10U)) != 0UL || ((esr >> 26U) & 0x3fUL) != 0x25UL ||
        ((esr >> 6U) & 1UL) != attempt->write ||
        pc != (uint64_t)(attempt->write ? b2_physical_write_fault : b2_physical_read_fault))
    {
        return 0UL;
    }
    if (sctlr != b2_physical_record.sctlr)
    {
        return 0UL;
    }
    attempt->esr = esr;
    attempt->far = far;
    attempt->elr = pc;
    attempt->classification = b2_classify_physical_abort(esr);
    attempt->completed = 2UL;
    publish_physical();
    return (uint64_t)(attempt->write ? b2_physical_write_resume : b2_physical_read_resume);
}
static uint64_t translation(uint64_t address, bool write)
{
    uint64_t par;
    if (write)
    {
        __asm__ volatile("at s1e1w, %0" ::"r"(address) : "memory");
    }
    else
    {
        __asm__ volatile("at s1e1r, %0" ::"r"(address) : "memory");
    }
    __asm__ volatile("isb; mrs %0, par_el1" : "=r"(par));
    return par;
}
static bool translated_to(uint64_t par, uint64_t va, uint64_t physical)
{
    return (par & 1UL) == 0UL && ((par & TABLE_ADDRESS_MASK) | (va & ~PHYS_PAGE_MASK)) == physical;
}
static bool install_alias(uint64_t physical, uint64_t **root_out)
{
    uint64_t root_address = b2_physical_record.ttbr0 & TABLE_ADDRESS_MASK;
    if ((b2_physical_record.sctlr & 5UL) != 5UL || (b2_physical_record.tcr & 0x3fUL) != 25UL ||
        (b2_physical_record.tcr & (3UL << 14)) != 0UL || ((b2_physical_record.tcr >> 32U) & 7UL) != 2UL ||
        (b2_physical_record.ttbr0 & 0x0000ff0000000000UL) != 0UL ||
        (b2_physical_record.mair & (0xffUL << 16)) != 0UL || root_address < NONSECURE_DATA_BASE ||
        root_address > NONSECURE_DATA_END - PHYSICAL_PAGE_BYTES)
    {
        return false;
    }
    if (!translated_to(translation(root_address, false), root_address, root_address) ||
        !translated_to(translation(root_address, true), root_address, root_address))
    {
        return false;
    }
    uint64_t *root = (uint64_t *)root_address;
    b2_physical_record.root_descriptor = root[B2_ALIAS_ROOT_INDEX];
    if (root[B2_ALIAS_ROOT_INDEX] != 0UL)
    {
        return false;
    }
    for (unsigned int i = 0U; i < 512U; ++i)
    {
        alias_l2[i] = 0UL;
        alias_l3[i] = 0UL;
    }
    alias_l2[0] = (uint64_t)alias_l3 | DESC_TABLE;
    alias_l3[0] = physical | DESC_TEST_PAGE;
    uint64_t mair = b2_physical_record.mair | MAIR_TEST_SLOT;
    b2_physical_record.probe_mair = mair;
    __asm__ volatile("dsb ishst; msr mair_el1, %0; isb" ::"r"(mair) : "memory");
    root[B2_ALIAS_ROOT_INDEX] = (uint64_t)alias_l2 | DESC_TABLE;
    __asm__ volatile("dsb ishst; tlbi vmalle1; dsb ish; isb" ::: "memory");
    b2_physical_record.root_descriptor = root[B2_ALIAS_ROOT_INDEX];
    *root_out = root;
    return true;
}
static void remove_alias(uint64_t *root)
{
    root[B2_ALIAS_ROOT_INDEX] = 0UL;
    __asm__ volatile(
        "dsb ishst; tlbi vmalle1; dsb ish; isb; msr mair_el1, %0; isb" ::"r"(b2_physical_record.mair)
        : "memory");
}
void b2_run_physical_memory(void)
{
    struct tee_session service = {0U, false}, ta = {0U, false};
    enum test_status status = TEST_BLOCKED;
    const char *reason = "physical-review-or-EL2-state-unconfirmed";
    uint64_t *root = NULL;
    volatile unsigned char *bytes = (volatile unsigned char *)&b2_physical_record;
    for (unsigned int i = 0U; i < sizeof(b2_physical_record); ++i)
    {
        bytes[i] = 0U;
    }
    b2_physical_record.version = B2_PHYSICAL_RECORD_VERSION;
    b2_physical_record.run_id = debug_boot.run_id;
    b2_physical_record.state = B2_RUNNING;
    b2_physical_record.alias_va = B2_PHYS_ALIAS_BASE;
    uint64_t el, sctlr, tcr, ttbr, mair;
    __asm__ volatile(
        "mrs %0, CurrentEL; mrs %1, sctlr_el1; mrs %2, tcr_el1; mrs %3, ttbr0_el1; mrs %4, mair_el1"
        : "=r"(el), "=r"(sctlr), "=r"(tcr), "=r"(ttbr), "=r"(mair));
    b2_physical_record.entry_el = el >> 2;
    b2_physical_record.sctlr = sctlr;
    b2_physical_record.tcr = tcr;
    b2_physical_record.ttbr0 = ttbr;
    b2_physical_record.mair = mair;
    b2_physical_record.el2_known = debug_handoff.el2_known;
    b2_physical_record.hcr = debug_handoff.hcr;
    b2_physical_record.vttbr = debug_handoff.vttbr;
    b2_physical_record.vtcr = debug_handoff.vtcr;
    publish_physical();
    if (B2_PHYS_REVIEWED != 1U || el != 4UL || debug_handoff.el2_known != 1UL ||
        (debug_handoff.hcr & HCR_STAGE2))
    {
        goto cleanup;
    }
    struct tee_reply opened = tee_session_open(&service, physical_uuid);
    b2_note_reply("physical-PTA-open", opened);
    if (!success(opened))
    {
        reason = "physical-PTA-not-built-or-open-failed";
        goto cleanup;
    }
    struct tee_reply prepared = tee_invoke(&service, PHYSICAL_PREPARE, 0U);
    struct tee_reply size = tee_invoke(&service, PHYSICAL_SIZE, 0U);
    b2_note_reply("secure-prepare", prepared);
    if (!success(prepared) || !success(size) || size.value != PHYSICAL_PAGE_BYTES)
    {
        reason = "secure-page-allocation-or-control-failed";
        goto cleanup;
    }
    (void)b2_check_value("Secure-allocation-bytes", PHYSICAL_PAGE_BYTES, size.value);
    uint64_t physical = prepared.value;
    b2_physical_record.page_pa = physical;
    b2_physical_record.page_bytes = size.value;
    if (physical < B2_PHYS_ALLOWED_BASE || physical > B2_PHYS_ALLOWED_END - PHYSICAL_PAGE_BYTES ||
        (physical & ~PHYS_PAGE_MASK))
    {
        reason = "probe-page-outside-profile-Secure-reservation";
        goto cleanup;
    }
    if (!install_alias(physical, &root))
    {
        reason = "NS-alias-or-memory-attributes-incompatible";
        goto cleanup;
    }
    volatile uint64_t *control = (volatile uint64_t *)(SHARED_MEMORY_END - 32UL);
    *control = PHYS_ECHO;
    if (*control != PHYS_ECHO)
    {
        status = TEST_FAIL;
        reason = "allowed-Normal-memory-control-failed";
        goto cleanup;
    }
    status = TEST_PASS;
    reason = "stage1-PA-confirmed+external-aborts+Secure-page-unchanged+continuity";
    for (unsigned int i = 0U; i < B2_PHYSICAL_ATTEMPTS; ++i)
    {
        volatile struct b2_physical_attempt *attempt = &b2_physical_record.attempts[i];
        uint64_t offset = i < 2U ? 0UL : PHYSICAL_PAGE_BYTES - sizeof(uint64_t);
        attempt->va = B2_PHYS_ALIAS_BASE + offset;
        attempt->pa = physical + offset;
        attempt->write = i & 1U;
        attempt->par_read = translation(attempt->va, false);
        attempt->par_write = translation(attempt->va, true);
        if (!translated_to(attempt->par_read, attempt->va, attempt->pa) ||
            !translated_to(attempt->par_write, attempt->va, attempt->pa))
        {
            status = TEST_BLOCKED;
            reason = "NS-stage1-translation-not-established";
            break;
        }
        b2_stage(B2_EXPECTED_FAULT);
        active_attempt = attempt;
        expected_fault = attempt->write ? 4UL : 3UL;
        if (attempt->write)
        {
            b2_physical_write(attempt->va, PHYS_REPLACEMENT);
        }
        else
        {
            attempt->value = b2_physical_read(attempt->va);
        }
        expected_fault = 0UL;
        active_attempt = NULL;
        if (attempt->completed != 2UL)
        {
            attempt->completed = 1UL;
            bool disclosed =
                !attempt->write && attempt->value == (PHYSICAL_PATTERN ^ (offset / sizeof(uint64_t)));
            status = disclosed ? TEST_FAIL : TEST_BLOCKED;
            reason = disclosed ? "Normal-read-returned-Secure-pattern"
                               : "completed-access-poison-or-denial-mechanism-unclassified";
        }
        else if (attempt->classification != 3UL)
        {
            status = TEST_BLOCKED;
            reason = "MMU-or-unclassified-denial-is-not-physical-proof";
        }
        b2_stage(B2_COMPARE);
        struct tee_reply checked = tee_invoke(&service, PHYSICAL_VERIFY, 0U);
        b2_note_reply("secure-nonmodification", checked);
        if (!success(checked) || checked.value != PHYSICAL_CHECK_TAG)
        {
            status = TEST_FAIL;
            reason = "Secure-test-page-changed-or-service-lost";
        }
        (void)b2_check_value("precise-external-abort", 3UL, attempt->classification);
        (void)b2_check_value("Secure-page-check", PHYSICAL_CHECK_TAG, checked.value);
        publish_physical();
        if (status != TEST_PASS)
        {
            break;
        }
    }
    if (!success(tee_session_open(&ta, validation_ta_uuid)))
    {
        status = TEST_FAIL;
        reason = "TA-continuity-open-failed";
    }
    else
    {
        struct tee_reply echo = tee_invoke(&ta, TA_COMMAND_ECHO, PHYS_ECHO);
        b2_note_reply("post-probe-TA-ECHO", echo);
        (void)b2_check_value("post-probe-TA-ECHO", PHYS_ECHO, echo.value);
        (void)b2_check_value("Normal-control", PHYS_ECHO, *control);
        if (!success(echo) || echo.value != PHYS_ECHO || *control != PHYS_ECHO)
        {
            status = TEST_FAIL;
            reason = "post-probe-TA-or-Normal-continuity-failed";
        }
    }
cleanup:
    expected_fault = 0UL;
    active_attempt = NULL;
    b2_stage(B2_CLEANUP);
    if (root)
    {
        remove_alias(root);
    }
    if (service.open)
    {
        struct tee_reply released = tee_invoke(&service, PHYSICAL_RELEASE, 0U);
        if (!success(released))
        {
            b2_results.cleanup_errors++;
            status = TEST_FAIL;
            reason = "Secure-page-release-failed";
        }
    }
    b2_close_session(&ta);
    b2_close_session(&service);
    b2_physical_record.state = B2_COMPLETE;
    publish_physical();
    test_report("TZ-SYS-03", status, reason, 0UL);
}
