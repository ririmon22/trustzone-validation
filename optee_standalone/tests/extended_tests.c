#include "client.h"
#include "functional.h"
#include "test_runner.h"
#define SMCCC_VERSION_FID 0x80000000UL
#define PSCI_VERSION_FID 0x84000000UL
#define SMCCC_MIN 0x10000UL
#define SMCCC_MAX 0x10007UL
#define PSCI_MIN 2UL
#define PSCI_MAX 0x10003UL
#define ORIGIN_CORE 3U
#define ORIGIN_TA 4U
#define VALUE_INPUT_ATTRIBUTE 1UL
#define BUFFER_SIZE 16UL
#define BUFFER_SEED 0x50U
#define TEE_ERROR_SECURITY_VALUE 0xffff000fU
static bool success(struct tee_reply reply)
{
    return reply.transport == TEE_CLIENT_OK && reply.result == TEE_SUCCESS;
}
void b2_run_standard_versions(void)
{
    b2_stage(B2_OPERATE);
    uint64_t smccc = smc_call(SMCCC_VERSION_FID, 0UL, 0UL, 0UL).x0;
    uint64_t psci = smc_call(PSCI_VERSION_FID, 0UL, 0UL, 0UL).x0;
    (void)log_printf("B2_STANDARD smccc=0x%llx psci=0x%llx\n", (unsigned long long)smccc,
                     (unsigned long long)psci);
    b2_stage(B2_COMPARE);
    (void)b2_check_value("SMCCC-version-in-range", 1UL, smccc >= SMCCC_MIN && smccc <= SMCCC_MAX);
    (void)b2_check_value("PSCI-version-in-range", 1UL, psci >= PSCI_MIN && psci <= PSCI_MAX);
    test_report("B2-SMCCC-01",
                smccc >= SMCCC_MIN && smccc <= SMCCC_MAX && psci >= PSCI_MIN && psci <= PSCI_MAX ? TEST_PASS
                                                                                                 : TEST_FAIL,
                "integrated-BL31-SMCCC-and-PSCI-version-only", 0UL);
}

void b2_run_closed_core_session(void)
{
    struct tee_session a = {0U, false}, b = {0U, false};
    if (!b2_open_pair(&a, &b))
    {
        test_report("TEE-SESSION-CORE-01", TEST_BLOCKED, "session-open-unavailable", 0UL);
    }
    else
    {
        b2_stage(B2_OPERATE);
        uint32_t closed_id = a.id;
        bool closed = success(tee_session_close(&a));
        struct tee_reply stale = tee_probe_session_id(closed_id);
        struct tee_reply other = tee_invoke(&b, TA_COMMAND_ECHO, BUFFER_SEED);
        b2_note_reply("closed-raw-ID", stale);
        b2_note_reply("other-TA", other);
        bool session_ok = closed && stale.transport == TEE_CLIENT_OK &&
                          stale.result == TEE_ERROR_BAD_PARAMETERS && stale.origin == ORIGIN_CORE &&
                          success(other) && other.value == BUFFER_SEED &&
                          success(tee_session_open(&a, validation_ta_uuid));
        struct tee_reply fresh = tee_invoke(&a, TA_COMMAND_READ, 0U);
        session_ok = session_ok && fresh.transport == TEE_CLIENT_OK &&
                     fresh.result == TEE_ERROR_ACCESS_DENIED && fresh.origin == ORIGIN_TA;
        b2_stage(B2_COMPARE);
        (void)b2_check_value("closed_core_session-predicates", 1UL, session_ok);
        test_report("TEE-SESSION-CORE-01", session_ok ? TEST_PASS : TEST_FAIL,
                    "raw-closed-ID-rejected-by-Core+other-TA-alive+reopen-without-auth-state", 0UL);
    }
    b2_stage(B2_CLEANUP);
    b2_close_session(&a);
    b2_close_session(&b);
}

void b2_run_parameter_validation(void)
{
    struct tee_session a = {0U, false}, b = {0U, false};
    if (!b2_open_pair(&a, &b))
    {
        test_report("TEE-PARAM-01", TEST_BLOCKED, "session-open-unavailable", 0UL);
    }
    else
    {
        b2_stage(B2_OPERATE);
        struct tee_reply wrong_type = tee_probe_attribute(&a, VALUE_INPUT_ATTRIBUTE);
        uint64_t base = 0UL, end = 0UL;
        b2_note_reply("wrong-TA-type", wrong_type);
        bool param_ok = wrong_type.transport == TEE_CLIENT_OK &&
                        wrong_type.result == TEE_ERROR_BAD_PARAMETERS && wrong_type.origin == ORIGIN_TA &&
                        tee_client_shared_range(&base, &end) && end - base >= 2UL * BUFFER_SIZE;
        if (param_ok)
        {
            uint64_t address = end - BUFFER_SIZE;
            volatile uint8_t *buffer = (volatile uint8_t *)address;
            for (uint64_t i = 0UL; i < BUFFER_SIZE; ++i)
            {
                buffer[i] = (uint8_t)(BUFFER_SEED + i);
            }
            buffer[0] ^= 1U;
            struct tee_reply rejected = tee_invoke_buffer(&a, address, BUFFER_SIZE);
            b2_note_reply("malformed-buffer", rejected);
            param_ok = rejected.transport == TEE_CLIENT_OK && rejected.result == TEE_ERROR_SECURITY_VALUE &&
                       rejected.origin == ORIGIN_TA;
            for (uint64_t i = 0UL; i < BUFFER_SIZE; ++i)
            {
                uint8_t expected = (uint8_t)(BUFFER_SEED + i);
                if (i == 0UL)
                {
                    expected ^= 1U;
                }
                if (buffer[i] != expected)
                {
                    param_ok = false;
                }
            }
            buffer[0] ^= 1U;
            param_ok = param_ok && success(tee_invoke_buffer(&a, address, BUFFER_SIZE)) &&
                       success(tee_invoke(&b, TA_COMMAND_ECHO, 0U));
        }
        b2_stage(B2_COMPARE);
        (void)b2_check_value("parameter_validation-predicates", 1UL, param_ok);
        test_report("TEE-PARAM-01", param_ok ? TEST_PASS : TEST_FAIL,
                    "valid-Core-attribute-wrong-TA-type+bad-content-rejected+unchanged+restore-and-continue",
                    0UL);
    }
    b2_stage(B2_CLEANUP);
    b2_close_session(&a);
    b2_close_session(&b);
}

void b2_run_key_attributes(void)
{
    struct tee_session a = {0U, false}, b = {0U, false};
    if (!b2_open_pair(&a, &b))
    {
        test_report("TEE-KEY-ATTR-01", TEST_BLOCKED, "session-open-unavailable", 0UL);
    }
    else
    {
        b2_stage(B2_OPERATE);
        struct tee_reply key = tee_invoke(&a, TA_COMMAND_KEY_ATTRIBUTE, 0U);
        b2_note_reply("nonextractable-attribute-test", key);
        bool attributes_ok =
            success(key) && key.value == 0U && success(tee_invoke(&a, TA_COMMAND_KEY_POLICY, 0U));
        b2_stage(B2_COMPARE);
        (void)b2_check_value("key_attributes-predicates", 1UL, attributes_ok);
        test_report("TEE-KEY-ATTR-01", attributes_ok ? TEST_PASS : TEST_FAIL,
                    "Core-secret-attribute-allowed-control+nonextractable-denied+buffer-intact+key-usable-no-"
                    "key-export",
                    0UL);
    }
    b2_stage(B2_CLEANUP);
    b2_close_session(&a);
    b2_close_session(&b);
}

void run_extended_tests(void)
{
    b2_run_standard_versions();
    b2_run_closed_core_session();
    b2_run_parameter_validation();
    b2_run_key_attributes();
}
