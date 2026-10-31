#include "functional.h"
#include "signed_rpc.h"
volatile uint8_t b2_signed_loader_snapshot[1040] __attribute__((aligned(64)));
volatile struct tee_reply b2_signed_snapshot_status __attribute__((aligned(64)));
#define SECURITY_ERROR 0xffff000fU
#define NOT_FOUND_ERROR 0xffff0008U
#define EXTERNAL_ECHO 0x7e168ac1U
static bool success(struct tee_reply reply)
{
    return reply.transport == TEE_CLIENT_OK && reply.result == TEE_SUCCESS;
}
static void publish_signed(void)
{
    uintptr_t end = (uintptr_t)&b2_signed_audit + sizeof(b2_signed_audit);
    for (uintptr_t p = (uintptr_t)&b2_signed_audit; p < end; p += 64UL)
    {
        __asm__ volatile("dc cvac, %0" ::"r"(p) : "memory");
    }
    for (uintptr_t p = (uintptr_t)b2_signed_loader_snapshot;
         p < (uintptr_t)b2_signed_loader_snapshot + sizeof(b2_signed_loader_snapshot); p += 64UL)
    {
        __asm__ volatile("dc cvac, %0" ::"r"(p) : "memory");
    }
    __asm__ volatile("dc cvac, %0" ::"r"((uintptr_t)&b2_signed_snapshot_status) : "memory");
    __asm__ volatile("dsb sy" ::: "memory");
}
void b2_run_signed_ta(void)
{
    enum test_status status = TEST_BLOCKED;
    const char *reason = "signed-profile-or-RAM-bundle-unavailable";
    if (!signed_rpc_begin())
    {
        test_report("TEE-04", status, reason, 0UL);
        return;
    }
    const unsigned int variants[SIGNED_CASES] = {0U, 1U, 0U, 2U, 0U, 0U, 3U, 0U};
    const uint32_t expected[SIGNED_CASES] = {
        TEE_SUCCESS,     SECURITY_ERROR,           TEE_SUCCESS, SECURITY_ERROR, TEE_SUCCESS,
        NOT_FOUND_ERROR, TEE_ERROR_BAD_PARAMETERS, TEE_SUCCESS};
    bool passed = true;
    for (unsigned int i = 0U; i < SIGNED_CASES; ++i)
    {
        struct tee_session session = {0U, false};
        volatile struct signed_case_record *record = &b2_signed_audit.cases[i];
        uint8_t uuid[16];
        for (unsigned int n = 0U; n < 16U; ++n)
        {
            uuid[n] = signed_ta_uuid[n];
        }
        if (i == 5U)
        {
            uuid[15] ^= 1U;
        }
        signed_rpc_select(variants[i]);
        b2_signed_audit.current_scenario = i;
        record->scenario = i;
        record->expected = expected[i];
        record->expected_origin = expected[i] == TEE_SUCCESS ? 4U : 3U;
        uint32_t before_rpc = b2_signed_audit.rpc_count, before_query = b2_signed_audit.queries,
                 before_copy = b2_signed_audit.copies;
        uint32_t before_alloc = b2_signed_audit.allocations, before_free = b2_signed_audit.frees;
        uint32_t before_unknown = b2_signed_audit.unsupported,
                 before_refused = b2_signed_audit.local_refusals;
        b2_stage(B2_OPERATE);
        publish_signed();
        struct tee_reply opened = tee_session_open(&session, uuid);
        record->opened = opened;
        b2_note_reply("runtime-TA-open", opened);
        bool case_ok = opened.transport == TEE_CLIENT_OK && opened.result == expected[i] &&
                       opened.origin == record->expected_origin;
        if (expected[i] == TEE_SUCCESS && success(opened))
        {
            struct tee_reply echoed = tee_invoke(&session, 0U, EXTERNAL_ECHO);
            record->echoed = echoed;
            record->invokes++;
            struct tee_reply queried = tee_invoke(&session, 1U, 0U);
            record->queried = queried;
            record->invokes++;
            b2_note_reply("runtime-TA-echo", echoed);
            b2_note_reply("fresh-instance-count", queried);
            case_ok = case_ok && success(echoed) && echoed.value == EXTERNAL_ECHO && success(queried) &&
                      queried.value == 1U && echoed.origin == 4U && queried.origin == 4U;
        }
        b2_stage(B2_CLEANUP);
        if (session.open)
        {
            struct tee_reply closed = tee_session_close(&session);
            record->closed = closed;
            case_ok = success(closed) && closed.origin == 3U && case_ok;
        }
        if (expected[i] != TEE_SUCCESS && record->invokes != 0U)
        {
            case_ok = false;
        }
        record->rpc_count = b2_signed_audit.rpc_count - before_rpc;
        record->queries = b2_signed_audit.queries - before_query;
        record->copies = b2_signed_audit.copies - before_copy;
        record->allocations = b2_signed_audit.allocations - before_alloc;
        record->frees = b2_signed_audit.frees - before_free;
        record->live = b2_signed_audit.live;
        record->unsupported = b2_signed_audit.unsupported - before_unknown;
        record->local_refusals = b2_signed_audit.local_refusals - before_refused;
        if (i != 5U && i != 6U)
        {
            case_ok = case_ok && record->queries >= 1U && record->copies >= 1U;
        }
        else
        {
            case_ok = case_ok && record->local_refusals >= 1U && record->copies == 0U;
        }
        case_ok = case_ok && record->live == 0U && record->allocations == record->frees &&
                  record->unsupported == 0U;
        b2_stage(B2_COMPARE);
        (void)log_printf("SIGNED_CASE %u expected=0x%08x actual=0x%08x origin=%u invokes=%u queries=%u "
                         "copies=%u live=%u\n",
                         i, expected[i], opened.result, opened.origin, record->invokes, record->queries,
                         record->copies, record->live);
        (void)b2_check_value("signed-loader-subcase", 1UL, case_ok);
        passed = case_ok && passed;
        b2_signed_audit.completed++;
        publish_signed();
        if (b2_signed_audit.poisoned)
        {
            break;
        }
    }
    const uint8_t audit_uuid[16] = {0x6d, 0xc1, 0x34, 0x1d, 0x53, 0x4f, 0x46, 0x42,
                                    0x91, 0x38, 0x21, 0x58, 0x76, 0x30, 0x49, 0x10};
    struct tee_session observer = {0U, false};
    bool audit_ok = success(tee_session_open(&observer, audit_uuid));
    if (audit_ok)
    {
        uint64_t address = SHARED_MEMORY_END - 8192UL;
        struct tee_reply copied =
            tee_invoke_output(&observer, 0U, address, sizeof(b2_signed_loader_snapshot));
        b2_signed_snapshot_status = copied;
        b2_note_reply("Core-loader-audit", copied);
        audit_ok = success(copied) && copied.origin == 4U;
        if (audit_ok)
        {
            for (unsigned int i = 0U; i < sizeof(b2_signed_loader_snapshot); ++i)
            {
                b2_signed_loader_snapshot[i] = ((volatile uint8_t *)address)[i];
            }
        }
    }
    if (observer.open)
    {
        audit_ok = success(tee_session_close(&observer)) && audit_ok;
    }
    (void)b2_check_value("Core-loader-audit-read", 1UL, audit_ok);
    bool cleaned = signed_rpc_end();
    publish_signed();
    status = passed && cleaned && b2_signed_audit.completed == SIGNED_CASES ? TEST_PASS : TEST_FAIL;
    if (!audit_ok || b2_signed_audit.poisoned || b2_signed_audit.unsupported)
    {
        status = TEST_BLOCKED;
    }
    reason = "EXT-TA-V03(T-16):runtime-signed-loader-controls;Core-audit-required-for-signature-attribution";
    if (b2_signed_audit.poisoned || b2_signed_audit.unsupported) {
        reason="required-RPC-unsupported;inspect-function-command;pending-slots-quarantined";
    } else if (!audit_ok) {
        reason="Core-loader-audit-unavailable;no-signature-attribution";
    }
    test_report("TEE-04", status, reason, 0UL);
}
