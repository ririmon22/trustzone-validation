#include "client.h"
#include "test_runner.h"
#include "benchmark.h"
#define ECHO_TEST_VALUE 0x45a7U
static bool reply_success(struct tee_reply reply)
{
    return reply.transport == TEE_CLIENT_OK && reply.result == TEE_SUCCESS;
}
static struct tee_session *benchmark_session;
static bool benchmark_failed;
static void benchmark_ta(void)
{
    if (!reply_success(tee_invoke(benchmark_session, TA_COMMAND_ECHO, ECHO_TEST_VALUE))) { benchmark_failed = true; }
}
static void benchmark_crypto(void)
{
    if (!reply_success(tee_invoke(benchmark_session, TA_COMMAND_CRYPTO, 0U))) { benchmark_failed = true; }
}
static struct tee_session measured_session;
static void prepare_session(void)
{
    if (!reply_success(tee_session_open(&measured_session, validation_ta_uuid))) { benchmark_failed = true; }
}
static void close_session(void)
{
    if (!reply_success(tee_session_close(&measured_session))) { benchmark_failed = true; }
}
static void reopen_and_check(void)
{
    prepare_session();
    struct tee_reply reply = tee_invoke(&measured_session, TA_COMMAND_ECHO, ECHO_TEST_VALUE);
    if (!reply_success(reply) || reply.value != ECHO_TEST_VALUE) { benchmark_failed = true; }
}
static void prepare_dead_session(void)
{
    prepare_session();
    struct tee_reply fault = tee_invoke(&measured_session, TA_COMMAND_PANIC, 0U);
    if (fault.transport != TEE_CLIENT_OK || fault.result != TEE_ERROR_TARGET_DEAD) { benchmark_failed = true; }
}
static void recover_session(void)
{
    struct tee_reply fault = tee_invoke(&measured_session, TA_COMMAND_PANIC, 0U);
    if (fault.transport != TEE_CLIENT_OK || fault.result != TEE_ERROR_TARGET_DEAD) { benchmark_failed = true; return; }
    reopen_and_check();
}
#ifdef TEE_SHARED_CACHED
extern bool platform_prepare_tee_client(void);
#endif
extern void run_security_tests(void);
extern void run_stress_tests(void);
void standalone_main(void)
{
    test_begin("optee");
    if (read_current_el() != EXPECTED_EL1) { console_write("FATAL expected EL1\n"); platform_halt(); }
#ifdef TEE_SHARED_CACHED
    if (!platform_prepare_tee_client()) { console_write("FATAL shared memory MMU setup failed\n"); platform_halt(); }
#endif
    uint32_t initialized = tee_client_initialize();
    if (initialized != TEE_CLIENT_OK) {
        console_write("OPTEE_INIT transport="); console_uint(initialized); console_write("\n");
        for (unsigned int i = 1U; i <= 8U; ++i) {
            char id[] = "TEE-00"; id[5] = (char)('0' + i);
            test_report(id, TEST_BLOCKED, "transport-or-memory-configuration-unavailable", 0UL);
        }
        test_finish(); platform_halt();
    }
    struct tee_session first = { 0U, false }, second = { 0U, false };
    struct tee_reply opened_first = tee_session_open(&first, validation_ta_uuid);
    struct tee_reply opened_second = tee_session_open(&second, validation_ta_uuid);
    if (!reply_success(opened_first) || !reply_success(opened_second)) {
        for (unsigned int i = 1U; i <= 8U; ++i) {
            char id[] = "TEE-00"; id[5] = (char)('0' + i);
            test_report(id, TEST_BLOCKED, "TA-open-or-RPC-unavailable", 0UL);
        }
        test_finish(); platform_halt();
    }
    struct tee_reply echoed = tee_invoke(&first, TA_COMMAND_ECHO, ECHO_TEST_VALUE);
    struct tee_reply second_echo = tee_invoke(&second, TA_COMMAND_ECHO, ECHO_TEST_VALUE);
    struct tee_reply invalid = tee_invoke(&first, UINT32_MAX, 0U);
    bool service_ok = reply_success(echoed) && echoed.value == ECHO_TEST_VALUE &&
        reply_success(second_echo) && second_echo.value == ECHO_TEST_VALUE &&
        invalid.transport == TEE_CLIENT_OK && invalid.result == TEE_ERROR_BAD_PARAMETERS;
    test_report("TEE-01", service_ok ? TEST_PASS : TEST_FAIL, "two-sessions+echo+invalid-command", 0UL);
    struct tee_reply denied = tee_invoke(&second, TA_COMMAND_READ, 0U);
    struct tee_reply wrong = tee_invoke(&first, TA_COMMAND_AUTHENTICATE, 0U);
    struct tee_reply authenticated = tee_invoke(&first, TA_COMMAND_AUTHENTICATE, TA_TEST_CREDENTIAL);
    struct tee_reply allowed = tee_invoke(&first, TA_COMMAND_READ, 0U);
    struct tee_reply closed = tee_session_close(&first);
    struct tee_reply stale = tee_invoke(&first, TA_COMMAND_READ, 0U);
    bool policy_ok = denied.transport == TEE_CLIENT_OK && denied.result == TEE_ERROR_ACCESS_DENIED &&
        wrong.transport == TEE_CLIENT_OK && wrong.result == TEE_ERROR_ACCESS_DENIED &&
        reply_success(authenticated) && reply_success(allowed) && reply_success(closed) && stale.transport == TEE_CLIENT_BAD_STATE;
    console_write(policy_ok ? "POLICY_SMOKE PASS\n" : "POLICY_SMOKE FAIL\n");
    /* 固定試験トークンは本物の要求者認証を証明しない。 */
    test_report("TEE-02", policy_ok ? TEST_BLOCKED : TEST_FAIL, "Normal-requester-authentication-missing-TA-identity-tested-separately", 0UL);
    test_report("TEE-04", TEST_BLOCKED, "Early-TA-is-not-signed-file-loader-test", 0UL);
    struct tee_reply crypto = tee_invoke(&second, TA_COMMAND_CRYPTO, 0U);
    test_report("TEE-05", reply_success(crypto) ? TEST_BLOCKED : TEST_FAIL, "AES-KAT-and-export-policy-tested-memory-extraction-missing", 0UL);
    test_report("TEE-06", TEST_BLOCKED, "no-storage-backend-or-RPC", 0UL);
    struct tee_session isolated = { 0U, false };
    struct tee_reply isolated_open = tee_session_open(&isolated, validation_ta_b_uuid);
    if (!reply_success(isolated_open)) {
        test_report("TEE-07", TEST_BLOCKED, "TA-B-not-available", 0UL);
    } else {
        struct tee_reply panic = tee_invoke(&second, TA_COMMAND_PANIC, 0U);
        struct tee_reply alive = tee_invoke(&isolated, TA_COMMAND_ECHO, ECHO_TEST_VALUE);
        struct tee_reply reopened = tee_session_open(&first, validation_ta_uuid);
        struct tee_reply recovered = tee_invoke(&first, TA_COMMAND_ECHO, ECHO_TEST_VALUE);
        bool isolated_ok = panic.transport == TEE_CLIENT_OK && panic.result == TEE_ERROR_TARGET_DEAD &&
            reply_success(alive) && alive.value == ECHO_TEST_VALUE && reply_success(reopened) &&
            reply_success(recovered) && recovered.value == ECHO_TEST_VALUE;
        test_report("TEE-07", isolated_ok ? TEST_PASS : TEST_FAIL, "TA-A-panic+TA-B-continuity+TA-A-reopen", 0UL);
        if (first.open && !reply_success(tee_session_close(&first))) { console_write("FATAL recovery-session-close\n"); }
        if (!reply_success(tee_session_close(&isolated))) { console_write("FATAL TA-B-close\n"); }
    }
    test_report("TEE-08", TEST_BLOCKED, "authenticated-boot-chain-not-configured", 0UL);
    if (!second.open) { (void)tee_session_open(&second, validation_ta_uuid); }
    if (second.open) {
        benchmark_session = &second;
        benchmark_run("PERF-05", benchmark_ta, "TA-echo-invoke-including-message-and-SMC");
        benchmark_run("PERF-06", benchmark_crypto, "TA-AES-generation-roundtrip-and-known-answer-including-transport");
        benchmark_run_scoped("PERF-08", NULL, prepare_session, close_session, "session-open-only-existing-TA-instance");
        benchmark_run_scoped("PERF-09", prepare_session, close_session, NULL, "session-close-only-other-session-remains-open");
        if (!reply_success(tee_session_close(&second))) { console_write("FATAL benchmark-session-close\n"); }
        /* panicの診断出力、再生成、echoまでを含む。終了後のcloseは除外する。 */
        benchmark_run_scoped("PERF-10", prepare_session, recover_session, close_session, "panic-diagnostic+TA-reopen+echo-excluding-final-close");
        benchmark_run_scoped("PERF-11", prepare_dead_session, reopen_and_check, close_session, "TARGET-DEAD-observed-to-TA-reopen-and-successful-echo");
        if (benchmark_failed) { console_write("FATAL benchmark-operation-failed\n"); }
    }
    if (second.open && !reply_success(tee_session_close(&second))) { console_write("FATAL session-close-failed\n"); }
    run_security_tests();
    run_stress_tests();
    test_finish(); platform_halt();
}
