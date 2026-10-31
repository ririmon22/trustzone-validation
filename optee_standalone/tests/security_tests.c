#include "client.h"
#include "functional.h"
#include "test_runner.h"
#define SHARED_BUFFER_OFFSET 0x1000UL
#define BUFFER_BYTES 16UL
#define BUFFER_SEED 0x50U
#define CLAIMED_CALLER_ID 0x6dc13418U
#define SECRET_TAG_B 0xb5208c14U
#define SECRET_TAG_A 0x6d3a4719U
#define ORIGIN_TEE 3U
#define ORIGIN_TA 4U
#define ACCESS_CONFLICT 0xffff0003U
static bool success(struct tee_reply reply)
{
    return reply.transport == TEE_CLIENT_OK && reply.result == TEE_SUCCESS;
}
static bool denied(struct tee_reply reply)
{
    return reply.transport == TEE_CLIENT_OK && reply.result == TEE_ERROR_ACCESS_DENIED;
}
static bool target_dead(struct tee_reply reply)
{
    return reply.transport == TEE_CLIENT_OK && reply.result == TEE_ERROR_TARGET_DEAD &&
           reply.origin == ORIGIN_TEE;
}
static bool check_secret(struct tee_session *session, uint32_t tag)
{
    struct tee_reply reply = tee_invoke(session, TA_COMMAND_SECRET_CHECK, 0U);
    bool ok = success(reply) && reply.value == tag;
    (void)b2_check_value("private-state-tag", tag, reply.value);
    if (!ok)
    {
        (void)log_printf("%s", "SECRET_RECHECK_FAIL result=");
        (void)log_printf("0x%016llx", (unsigned long long)(reply.result));
        (void)log_printf("%s", " value=");
        (void)log_printf("0x%016llx", (unsigned long long)(reply.value));
        (void)log_printf("%s", " expected=");
        (void)log_printf("0x%016llx", (unsigned long long)(tag));
        (void)log_printf("%s", "\n");
    }
    return ok;
}
static void test_identity(struct tee_session *first, struct tee_session *second)
{
    struct tee_reply forged_login = tee_probe_identity_open();
    struct tee_reply forged = tee_invoke(second, TA_COMMAND_TRUSTED_READ, CLAIMED_CALLER_ID);
    struct tee_reply trusted = tee_invoke(first, TA_COMMAND_TRUSTED_SELFTEST, 0U);
    struct tee_reply alive = tee_invoke(second, TA_COMMAND_ECHO, CLAIMED_CALLER_ID);
    b2_note_reply("forged-login", forged_login);
    b2_note_reply("PUBLIC-forged-ID", forged);
    b2_note_reply("trusted-TA", trusted);
    b2_note_reply("continued", alive);
    bool ok = forged_login.transport == TEE_CLIENT_OK && forged_login.result == TEE_ERROR_BAD_PARAMETERS &&
              forged_login.origin == ORIGIN_TEE && denied(forged) && forged.origin == ORIGIN_TA &&
              success(trusted) && trusted.value == 0U && success(alive);
    b2_stage(B2_COMPARE);
    (void)b2_check_value("TA-identity-policy", 1UL, ok);
    test_report("TEE-AUTH-TA", ok ? TEST_PASS : TEST_FAIL,
                "Core-client-identity+TA-A-allowlist+forged-login-and-PUBLIC-denied", 0UL);
    if (b2_test_selection == B2_SELECTION_LEGACY)
    {
        test_report("TEE-AUTH-NS", TEST_BLOCKED, "Normal-requester-authentication-not-provisioned", 0UL);
    }
}
static void log_reply(const char *name, struct tee_reply reply)
{
    (void)log_printf("SECURITY_REPLY %s transport=%u result=0x%016llx origin=%u\n", name, reply.transport,
                     (unsigned long long)reply.result, reply.origin);
}
static void test_shared_memory(struct tee_session *session)
{
    uint64_t address = SHARED_MEMORY_BASE + SHARED_BUFFER_OFFSET;
    volatile uint8_t *buffer = (volatile uint8_t *)address;
    for (uint64_t i = 0UL; i < BUFFER_BYTES; ++i)
    {
        buffer[i] = (uint8_t)(BUFFER_SEED + i);
    }
    struct tee_reply valid = tee_invoke_buffer(session, address, BUFFER_BYTES);
    log_reply("valid", valid);
    bool ok = success(valid);
    struct tee_reply short_buffer = tee_probe_buffer_unchecked(session, address, BUFFER_BYTES - 1UL);
    struct tee_reply overflow = tee_probe_buffer_unchecked(session, SHARED_MEMORY_END - 8UL, BUFFER_BYTES);
    struct tee_reply secure = tee_probe_buffer_unchecked(session, OPTEE_RESERVED_BASE, BUFFER_BYTES);
    struct tee_reply wrap = tee_probe_buffer_unchecked(session, UINT64_MAX - 7UL, BUFFER_BYTES);
    log_reply("short", short_buffer);
    log_reply("overflow", overflow);
    log_reply("secure", secure);
    log_reply("wrap", wrap);
    ok = ok && short_buffer.transport == TEE_CLIENT_OK && short_buffer.result == TEE_ERROR_BAD_PARAMETERS &&
         short_buffer.origin == ORIGIN_TA && overflow.transport == TEE_CLIENT_OK &&
         overflow.result == TEE_ERROR_BAD_PARAMETERS && overflow.origin == ORIGIN_TEE &&
         secure.transport == TEE_CLIENT_OK && secure.result == TEE_ERROR_BAD_PARAMETERS &&
         secure.origin == ORIGIN_TEE && wrap.transport == TEE_CLIENT_OK &&
         wrap.result == TEE_ERROR_BAD_PARAMETERS && wrap.origin == ORIGIN_TEE;
    ok = ok &&
         tee_invoke_buffer(session, OPTEE_RESERVED_BASE, BUFFER_BYTES).transport ==
             TEE_CLIENT_SHM_UNAVAILABLE &&
         success(tee_invoke_buffer(session, address, BUFFER_BYTES));
    for (uint64_t i = 0UL; i < BUFFER_BYTES; ++i)
    {
        if (buffer[i] != (uint8_t)(BUFFER_SEED + i))
        {
            ok = false;
        }
    }
    b2_stage(B2_COMPARE);
    (void)b2_check_value("SHM-reject-and-nonmodification", 1UL, ok);
    test_report("TEE-SHM-01", ok ? TEST_PASS : TEST_FAIL,
                "valid+wrong-size+outside+secure+overflow+continuity+buffer-unchanged", 0UL);
}
static void test_key_api(struct tee_session *session)
{
    struct tee_reply before = tee_invoke(session, TA_COMMAND_KEY_POLICY, 0U);
    struct tee_reply exported = tee_invoke(session, TA_COMMAND_KEY_EXPORT, CLAIMED_CALLER_ID);
    struct tee_reply crypto = tee_invoke(session, TA_COMMAND_CRYPTO, 0U);
    struct tee_reply after = tee_invoke(session, TA_COMMAND_KEY_POLICY, 0U);
    b2_note_reply("key-before", before);
    b2_note_reply("export-denied", exported);
    b2_note_reply("AES-KAT", crypto);
    b2_note_reply("key-after", after);
    b2_stage(B2_COMPARE);
    bool key_usable = success(before) && success(after);
    bool export_rejected = denied(exported) && exported.value == 0U;
    bool crypto_valid = success(crypto);
    bool key_ok = b2_check_value("key-usable-before-after", 1UL, key_usable);
    key_ok = b2_check_value("export-rejected-zero-output", 1UL, export_rejected) && key_ok;
    key_ok = b2_check_value("AES-roundtrip-and-KAT", 1UL, crypto_valid) && key_ok;
    test_report("TEE-KEY-API", key_ok ? TEST_PASS : TEST_FAIL,
                "Secure-generated-nonextractable-key+export-denied+AES-KAT+key-still-usable", 0UL);
    if (b2_test_selection == B2_SELECTION_LEGACY)
    {
        test_report("TEE-KEY-MEM", TEST_BLOCKED, "raw-key-memory-extraction-not-tested", 0UL);
    }
}
static void log_probe_fault(const char *domain, uint32_t address, bool write)
{
    (void)log_printf("PROBE_FAULT domain=%s address=0x%016llx write=%u\n", domain,
                     (unsigned long long)address, write ? 1U : 0U);
}
static enum test_status probe_other_ta(struct tee_session *attacker, struct tee_session *victim,
                                       uint32_t command)
{
    struct tee_reply address = tee_invoke(victim, TA_COMMAND_SECRET_ADDRESS, 0U);
    if (!success(address) || !check_secret(attacker, SECRET_TAG_A) || !check_secret(victim, SECRET_TAG_B))
    {
        return TEST_FAIL;
    }
    struct tee_reply probe = {0U, 0U, 0U, 0U};
    for (unsigned int attempt = 0U; attempt < 8U; ++attempt)
    {
        b2_stage(B2_EXPECTED_FAULT);
        probe = tee_invoke(attacker, command, address.value);
        b2_stage(B2_COMPARE);
        b2_note_reply("TA-VM-probe", probe);
        if (probe.transport != TEE_CLIENT_OK || probe.result != ACCESS_CONFLICT)
        {
            break;
        }
        (void)log_printf("%s", "PROBE_VA_ALIAS command=");
        (void)log_printf("%llu", (unsigned long long)(command));
        (void)log_printf("%s", " attempt=");
        (void)log_printf("%llu", (unsigned long long)(attempt));
        (void)log_printf("%s", "\n");
        if (!success(tee_session_close(attacker)) ||
            !success(tee_session_open(attacker, validation_ta_uuid)) || !check_secret(attacker, SECRET_TAG_A))
        {
            return TEST_FAIL;
        }
    }
    if (probe.transport == TEE_CLIENT_OK && probe.result == ACCESS_CONFLICT)
    {
        return TEST_BLOCKED;
    }
    if (target_dead(probe))
    {
        log_probe_fault("ta", address.value, command == TA_COMMAND_PROBE_WRITE);
    }
    if (!target_dead(probe) || attacker->open || !check_secret(victim, SECRET_TAG_B))
    {
        (void)log_printf("%s", "PROBE_FAIL victim-or-state\n");
        return TEST_FAIL;
    }
    struct tee_reply reopened = tee_session_open(attacker, validation_ta_uuid);
    if (!success(reopened) || !check_secret(attacker, SECRET_TAG_A))
    {
        log_reply("attacker-reopen", reopened);
        return TEST_FAIL;
    }
    (void)b2_check_value("victim-and-recovery", TEST_PASS, TEST_PASS);
    return TEST_PASS;
}
static enum test_status probe_core_isolation(struct tee_session *first, struct tee_session *second)
{
    struct tee_session canary = {0U, false};
    enum test_status core_status = TEST_BLOCKED;
    if (success(tee_session_open(&canary, validation_pta_uuid)))
    {
        struct tee_reply intact = tee_invoke(&canary, CANARY_COMMAND_CHECK, 0U);
        struct tee_reply address = tee_invoke(&canary, CANARY_COMMAND_ADDRESS, 0U);
        bool core_ok = success(intact) && intact.value == CANARY_CHECK_TAG && success(address);
        if (core_ok)
        {
            for (unsigned int write = 0U; write < 2U; ++write)
            {
                struct tee_reply fault = tee_invoke(
                    first, write == 0U ? TA_COMMAND_PROBE_READ : TA_COMMAND_PROBE_WRITE, address.value);
                if (target_dead(fault))
                {
                    log_probe_fault("core", address.value, write != 0U);
                }
                struct tee_reply rechecked = tee_invoke(&canary, CANARY_COMMAND_CHECK, 0U);
                bool recovered = target_dead(fault) && success(rechecked) &&
                                 rechecked.value == CANARY_CHECK_TAG &&
                                 success(tee_session_open(first, validation_ta_uuid)) &&
                                 check_secret(first, SECRET_TAG_A) && check_secret(second, SECRET_TAG_B);
                core_ok = core_ok && recovered;
                if (!recovered)
                {
                    break;
                }
            }
        }
        core_status = core_ok ? TEST_PASS : TEST_FAIL;
        if (!success(tee_session_close(&canary)))
        {
            core_status = TEST_FAIL;
            (void)log_printf("%s", "FATAL canary-session-close\n");
        }
    }
    return core_status;
}

enum test_status b2_probe_core_isolation(void)
{
    struct tee_session first = {0U, false}, second = {0U, false};
    enum test_status status = TEST_BLOCKED;
    if (b2_open_pair(&first, &second)) { status = probe_core_isolation(&first, &second); }
    b2_close_session(&first); b2_close_session(&second);
    return status;
}

void run_security_tests(void)
{
    struct tee_session first = {0U, false}, second = {0U, false};
    if (!success(tee_session_open(&first, validation_ta_uuid)) ||
        !success(tee_session_open(&second, validation_ta_b_uuid)))
    {
        b2_close_session(&first); b2_close_session(&second);
        (void)log_printf("%s", "FATAL security-test-session-open\n");
        return;
    }
    test_identity(&first, &second);
    test_shared_memory(&first);
    test_key_api(&first);
    enum test_status read_status = probe_other_ta(&first, &second, TA_COMMAND_PROBE_READ);
    enum test_status write_status = probe_other_ta(&first, &second, TA_COMMAND_PROBE_WRITE);
    enum test_status combined = read_status == TEST_FAIL || write_status == TEST_FAIL         ? TEST_FAIL
                                : read_status == TEST_BLOCKED || write_status == TEST_BLOCKED ? TEST_BLOCKED
                                                                                              : TEST_PASS;
    test_report("TEE-ISO-TA", combined, "B-secret-read-write-probes+A-fault+B-private-state-recheck+A-reopen",
                0UL);
    enum test_status core_status = probe_core_isolation(&first, &second);
    test_report("TEE-ISO-CORE", core_status,
                "Core-private-canary+read-write-faults+privileged-recheck+TA-continuity", 0UL);
    enum test_status isolation_status = combined == TEST_FAIL || core_status == TEST_FAIL ? TEST_FAIL
                                        : combined == TEST_BLOCKED || core_status == TEST_BLOCKED
                                            ? TEST_BLOCKED
                                            : TEST_PASS;
    test_report("TEE-03", isolation_status, "TA-private-data-and-Core-canary-access-denied-and-unchanged",
                0UL);
    if (!success(tee_session_close(&first)) || !success(tee_session_close(&second)))
    {
        (void)log_printf("%s", "FATAL security-test-close\n");
    }
}

/* 個々の試験でsessionを所有し、前の試験の認可状態に依存しない。 */
void b2_run_identity(void)
{
    struct tee_session first = {0U, false}, second = {0U, false};
    if (!b2_open_pair(&first, &second))
    {
        test_report("TEE-AUTH-TA", TEST_BLOCKED, "TA-session-open-unavailable", 0UL);
    }
    else
    {
        b2_stage(B2_OPERATE);
        test_identity(&first, &second);
    }
    b2_stage(B2_CLEANUP);
    b2_close_session(&first);
    b2_close_session(&second);
}

/* 個々の試験でsessionを所有し、前の試験の認可状態に依存しない。 */
void b2_run_shared_memory(void)
{
    struct tee_session first = {0U, false}, second = {0U, false};
    if (!b2_open_pair(&first, &second))
    {
        test_report("TEE-SHM-01", TEST_BLOCKED, "TA-session-open-unavailable", 0UL);
    }
    else
    {
        b2_stage(B2_OPERATE);
        test_shared_memory(&first);
    }
    b2_stage(B2_CLEANUP);
    b2_close_session(&first);
    b2_close_session(&second);
}

/* 個々の試験でsessionを所有し、前の試験の認可状態に依存しない。 */
void b2_run_key_api(void)
{
    struct tee_session first = {0U, false}, second = {0U, false};
    if (!b2_open_pair(&first, &second))
    {
        test_report("TEE-KEY-API", TEST_BLOCKED, "TA-session-open-unavailable", 0UL);
    }
    else
    {
        b2_stage(B2_OPERATE);
        test_key_api(&first);
    }
    b2_stage(B2_CLEANUP);
    b2_close_session(&first);
    b2_close_session(&second);
}

/* 個々の試験でsessionを所有し、前の試験の認可状態に依存しない。 */
void b2_run_ta_isolation(void)
{
    struct tee_session first = {0U, false}, second = {0U, false};
    if (!b2_open_pair(&first, &second))
    {
        test_report("TEE-ISO-TA", TEST_BLOCKED, "TA-session-open-unavailable", 0UL);
    }
    else
    {
        b2_stage(B2_OPERATE);
        enum test_status read_status = probe_other_ta(&first, &second, TA_COMMAND_PROBE_READ);
        enum test_status write_status = probe_other_ta(&first, &second, TA_COMMAND_PROBE_WRITE);
        b2_stage(B2_COMPARE);
        bool any_failed = read_status == TEST_FAIL || write_status == TEST_FAIL;
        bool any_blocked = read_status == TEST_BLOCKED || write_status == TEST_BLOCKED;
        enum test_status combined = any_failed ? TEST_FAIL : any_blocked ? TEST_BLOCKED : TEST_PASS;
        (void)b2_check_value("read-probe-status", TEST_PASS, read_status);
        (void)b2_check_value("write-probe-status", TEST_PASS, write_status);
        test_report("TEE-ISO-TA", combined,
                    "B-secret-read-write-probes+A-fault+B-private-state-recheck+A-reopen", 0UL);
    }
    b2_stage(B2_CLEANUP);
    b2_close_session(&first);
    b2_close_session(&second);
}
