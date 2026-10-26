#include "client.h"
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
    return reply.transport == TEE_CLIENT_OK && reply.result == TEE_ERROR_TARGET_DEAD && reply.origin == ORIGIN_TEE;
}
static bool check_secret(struct tee_session *session, uint32_t tag)
{
    struct tee_reply reply = tee_invoke(session, TA_COMMAND_SECRET_CHECK, 0U);
    bool ok = success(reply) && reply.value == tag;
    if (!ok) {
        console_write("SECRET_RECHECK_FAIL result="); console_hex(reply.result);
        console_write(" value="); console_hex(reply.value); console_write(" expected="); console_hex(tag); console_write("\n");
    }
    return ok;
}
static void test_identity(struct tee_session *first, struct tee_session *second)
{
    struct tee_reply forged_login = tee_probe_identity_open();
    struct tee_reply forged = tee_invoke(second, TA_COMMAND_TRUSTED_READ, CLAIMED_CALLER_ID);
    struct tee_reply trusted = tee_invoke(first, TA_COMMAND_TRUSTED_SELFTEST, 0U);
    struct tee_reply alive = tee_invoke(second, TA_COMMAND_ECHO, CLAIMED_CALLER_ID);
    bool ok = forged_login.transport == TEE_CLIENT_OK && forged_login.result == TEE_ERROR_BAD_PARAMETERS &&
        forged_login.origin == ORIGIN_TEE && denied(forged) && forged.origin == ORIGIN_TA && success(trusted) && trusted.value == 0U && success(alive);
    test_report("TEE-AUTH-TA", ok ? TEST_PASS : TEST_FAIL, "Core-client-identity+TA-A-allowlist+forged-login-and-PUBLIC-denied", 0UL);
    test_report("TEE-AUTH-NS", TEST_BLOCKED, "Normal-requester-authentication-not-provisioned", 0UL);
}
static void log_reply(const char *name, struct tee_reply reply)
{
    console_write("SECURITY_REPLY "); console_write(name);
    console_write(" transport="); console_uint(reply.transport);
    console_write(" result="); console_hex(reply.result);
    console_write(" origin="); console_uint(reply.origin); console_write("\n");
}
static void test_shared_memory(struct tee_session *session)
{
    uint64_t address = SHARED_MEMORY_BASE + SHARED_BUFFER_OFFSET;
    volatile uint8_t *buffer = (volatile uint8_t *)address;
    for (uint64_t i = 0UL; i < BUFFER_BYTES; ++i) { buffer[i] = (uint8_t)(BUFFER_SEED + i); }
    struct tee_reply valid = tee_invoke_buffer(session, address, BUFFER_BYTES);
    log_reply("valid", valid);
    bool ok = success(valid);
    struct tee_reply short_buffer = tee_probe_buffer_unchecked(session, address, BUFFER_BYTES - 1UL);
    struct tee_reply overflow = tee_probe_buffer_unchecked(session, SHARED_MEMORY_END - 8UL, BUFFER_BYTES);
    struct tee_reply secure = tee_probe_buffer_unchecked(session, OPTEE_RESERVED_BASE, BUFFER_BYTES);
    struct tee_reply wrap = tee_probe_buffer_unchecked(session, UINT64_MAX - 7UL, BUFFER_BYTES);
    log_reply("short", short_buffer); log_reply("overflow", overflow);
    log_reply("secure", secure); log_reply("wrap", wrap);
    ok = ok && short_buffer.transport == TEE_CLIENT_OK && short_buffer.result == TEE_ERROR_BAD_PARAMETERS && short_buffer.origin == ORIGIN_TA &&
        overflow.transport == TEE_CLIENT_OK && overflow.result == TEE_ERROR_BAD_PARAMETERS && overflow.origin == ORIGIN_TEE &&
        secure.transport == TEE_CLIENT_OK && secure.result == TEE_ERROR_BAD_PARAMETERS && secure.origin == ORIGIN_TEE &&
        wrap.transport == TEE_CLIENT_OK && wrap.result == TEE_ERROR_BAD_PARAMETERS && wrap.origin == ORIGIN_TEE;
    ok = ok && tee_invoke_buffer(session, OPTEE_RESERVED_BASE, BUFFER_BYTES).transport == TEE_CLIENT_SHM_UNAVAILABLE &&
        success(tee_invoke_buffer(session, address, BUFFER_BYTES));
    for (uint64_t i = 0UL; i < BUFFER_BYTES; ++i) { if (buffer[i] != (uint8_t)(BUFFER_SEED + i)) { ok = false; } }
    test_report("TEE-SHM-01", ok ? TEST_PASS : TEST_FAIL, "valid+wrong-size+outside+secure+overflow+continuity+buffer-unchanged", 0UL);
}
static void test_key_api(struct tee_session *session)
{
    struct tee_reply before = tee_invoke(session, TA_COMMAND_KEY_POLICY, 0U);
    struct tee_reply exported = tee_invoke(session, TA_COMMAND_KEY_EXPORT, CLAIMED_CALLER_ID);
    struct tee_reply crypto = tee_invoke(session, TA_COMMAND_CRYPTO, 0U);
    struct tee_reply after = tee_invoke(session, TA_COMMAND_KEY_POLICY, 0U);
    test_report("TEE-KEY-API", success(before) && denied(exported) && exported.value == 0U && success(crypto) && success(after) ? TEST_PASS : TEST_FAIL,
        "Secure-generated-nonextractable-key+export-denied+AES-KAT+key-still-usable", 0UL);
    test_report("TEE-KEY-MEM", TEST_BLOCKED, "raw-key-memory-extraction-not-tested", 0UL);
}
static void log_probe_fault(const char *domain, uint32_t address, bool write)
{
    console_write("PROBE_FAULT domain="); console_write(domain);
    console_write(" address="); console_hex(address);
    console_write(" write="); console_uint(write ? 1U : 0U); console_write("\n");
}
static enum test_status probe_other_ta(struct tee_session *attacker, struct tee_session *victim, uint32_t command)
{
    struct tee_reply address = tee_invoke(victim, TA_COMMAND_SECRET_ADDRESS, 0U);
    if (!success(address) || !check_secret(attacker, SECRET_TAG_A) || !check_secret(victim, SECRET_TAG_B)) { return TEST_FAIL; }
    struct tee_reply probe = { 0U, 0U, 0U, 0U };
    for (unsigned int attempt = 0U; attempt < 8U; ++attempt) {
        probe = tee_invoke(attacker, command, address.value);
        if (probe.transport != TEE_CLIENT_OK || probe.result != ACCESS_CONFLICT) { break; }
        console_write("PROBE_VA_ALIAS command="); console_uint(command);
        console_write(" attempt="); console_uint(attempt); console_write("\n");
        if (!success(tee_session_close(attacker)) || !success(tee_session_open(attacker, validation_ta_uuid)) || !check_secret(attacker, SECRET_TAG_A)) { return TEST_FAIL; }
    }
    if (probe.transport == TEE_CLIENT_OK && probe.result == ACCESS_CONFLICT) { return TEST_BLOCKED; }
    if (target_dead(probe)) { log_probe_fault("ta", address.value, command == TA_COMMAND_PROBE_WRITE); }
    if (!target_dead(probe) || attacker->open || !check_secret(victim, SECRET_TAG_B)) { console_write("PROBE_FAIL victim-or-state\n"); return TEST_FAIL; }
    struct tee_reply reopened = tee_session_open(attacker, validation_ta_uuid);
    if (!success(reopened) || !check_secret(attacker, SECRET_TAG_A)) { log_reply("attacker-reopen", reopened); return TEST_FAIL; }
    return TEST_PASS;
}
void run_security_tests(void)
{
    struct tee_session first = { 0U, false }, second = { 0U, false };
    if (!success(tee_session_open(&first, validation_ta_uuid)) || !success(tee_session_open(&second, validation_ta_b_uuid))) {
        console_write("FATAL security-test-session-open\n"); return;
    }
    test_identity(&first, &second);
    test_shared_memory(&first);
    test_key_api(&first);
    enum test_status read_status = probe_other_ta(&first, &second, TA_COMMAND_PROBE_READ);
    enum test_status write_status = probe_other_ta(&first, &second, TA_COMMAND_PROBE_WRITE);
    enum test_status combined = read_status == TEST_FAIL || write_status == TEST_FAIL ? TEST_FAIL :
        read_status == TEST_BLOCKED || write_status == TEST_BLOCKED ? TEST_BLOCKED : TEST_PASS;
    test_report("TEE-ISO-TA", combined, "B-secret-read-write-probes+A-fault+B-private-state-recheck+A-reopen", 0UL);
    struct tee_session canary = { 0U, false };
    enum test_status core_status = TEST_BLOCKED;
    if (success(tee_session_open(&canary, validation_pta_uuid))) {
        struct tee_reply intact = tee_invoke(&canary, CANARY_COMMAND_CHECK, 0U);
        struct tee_reply address = tee_invoke(&canary, CANARY_COMMAND_ADDRESS, 0U);
        bool core_ok = success(intact) && intact.value == CANARY_CHECK_TAG && success(address);
        if (core_ok) {
            for (unsigned int write = 0U; write < 2U; ++write) {
                struct tee_reply fault = tee_invoke(&first, write == 0U ? TA_COMMAND_PROBE_READ : TA_COMMAND_PROBE_WRITE, address.value);
                if (target_dead(fault)) { log_probe_fault("core", address.value, write != 0U); }
                struct tee_reply rechecked = tee_invoke(&canary, CANARY_COMMAND_CHECK, 0U);
                bool recovered = target_dead(fault) && success(rechecked) && rechecked.value == CANARY_CHECK_TAG &&
                    success(tee_session_open(&first, validation_ta_uuid)) && check_secret(&first, SECRET_TAG_A) && check_secret(&second, SECRET_TAG_B);
                core_ok = core_ok && recovered;
                if (!recovered) { break; }
            }
        }
        core_status = core_ok ? TEST_PASS : TEST_FAIL;
        if (!success(tee_session_close(&canary))) { console_write("FATAL canary-session-close\n"); }
    }
    test_report("TEE-ISO-CORE", core_status, "Core-private-canary+read-write-faults+privileged-recheck+TA-continuity", 0UL);
    enum test_status isolation_status = combined == TEST_FAIL || core_status == TEST_FAIL ? TEST_FAIL :
        combined == TEST_BLOCKED || core_status == TEST_BLOCKED ? TEST_BLOCKED : TEST_PASS;
    test_report("TEE-03", isolation_status, "TA-private-data-and-Core-canary-access-denied-and-unchanged", 0UL);
    if (!success(tee_session_close(&first)) || !success(tee_session_close(&second))) { console_write("FATAL security-test-close\n"); }
}
