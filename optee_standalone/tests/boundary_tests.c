#include "client.h"
#include "functional.h"
#include "test_runner.h"
#define BUFFER_BYTES 16UL
#define BUFFER_SEED 0x50U
#define INVALID_ATTRIBUTE 0xffUL
static bool success(struct tee_reply reply)
{
    return reply.transport == TEE_CLIENT_OK && reply.result == TEE_SUCCESS;
}
/* 容量制限はclient側の四枠であり、Core全体の上限とは区別する。 */
void b2_run_session_limits(void)
{
    b2_stage(B2_OPERATE);
    struct tee_session sessions[TEE_CLIENT_MAX_SESSIONS + 1U] = {0};
    bool opened = tee_client_active_sessions() == 0U;
    for (unsigned int i = 0U; i < TEE_CLIENT_MAX_SESSIONS; ++i)
    {
        if (!success(tee_session_open(&sessions[i], validation_ta_uuid)))
        {
            opened = false;
        }
    }
    struct tee_reply excess = tee_session_open(&sessions[TEE_CLIENT_MAX_SESSIONS], validation_ta_uuid);
    struct tee_reply duplicate = tee_session_open(&sessions[0], validation_ta_uuid);
    bool session_ok =
        opened && excess.transport == TEE_CLIENT_BAD_STATE && duplicate.transport == TEE_CLIENT_BAD_STATE;
    for (unsigned int i = 0U; i < TEE_CLIENT_MAX_SESSIONS; ++i)
    {
        if (sessions[i].open && !success(tee_session_close(&sessions[i])))
        {
            session_ok = false;
        }
    }
    session_ok = session_ok && tee_session_close(&sessions[0]).transport == TEE_CLIENT_BAD_STATE &&
                 tee_invoke(&sessions[0], TA_COMMAND_READ, 0U).transport == TEE_CLIENT_BAD_STATE &&
                 tee_client_active_sessions() == 0U;
    b2_stage(B2_COMPARE);
    (void)b2_check_value("client-limit-and-cleanup", 1UL, session_ok);
    test_report("TEE-SESSION-01", session_ok ? TEST_PASS : TEST_FAIL,
                "client-limit-four+duplicate-open+closed-handle+all-Core-closes", 0U);
}

void b2_run_message_abi(void)
{
    struct tee_session session = {0U, false};
    if (!success(tee_session_open(&session, validation_ta_uuid)))
    {
        test_report("TEE-ABI-01", TEST_BLOCKED, "session-open-unavailable", 0UL);
    }
    else
    {
        b2_stage(B2_OPERATE);
        struct tee_reply malformed = tee_probe_attribute(&session, INVALID_ATTRIBUTE);
        struct tee_reply invalid = tee_invoke(&session, UINT32_MAX, 0U);
        struct tee_reply zero = tee_invoke(&session, TA_COMMAND_ECHO, 0U);
        struct tee_reply max = tee_invoke(&session, TA_COMMAND_ECHO, UINT32_MAX);
        b2_note_reply("bad-attribute", malformed);
        b2_note_reply("unknown-command", invalid);
        b2_note_reply("zero-echo", zero);
        b2_note_reply("max-echo", max);
        bool abi_ok = malformed.transport == TEE_CLIENT_OK && malformed.result == TEE_ERROR_BAD_PARAMETERS &&
                      malformed.origin == 3U && invalid.transport == TEE_CLIENT_OK &&
                      invalid.result == TEE_ERROR_BAD_PARAMETERS && invalid.origin == 4U && success(zero) &&
                      zero.value == 0U && success(max) && max.value == UINT32_MAX;
        b2_stage(B2_COMPARE);
        (void)b2_check_value("message_abi-predicates", 1UL, abi_ok);
        test_report("TEE-ABI-01", abi_ok ? TEST_PASS : TEST_FAIL,
                    "Core-rejects-attribute+TA-rejects-command+u32-extremes+continuity", 0U);
    }
    b2_stage(B2_CLEANUP);
    b2_close_session(&session);
}

void b2_run_shared_boundaries(void)
{
    struct tee_session session = {0U, false};
    if (!success(tee_session_open(&session, validation_ta_uuid)))
    {
        test_report("TEE-BOUNDARY-01", TEST_BLOCKED, "session-open-unavailable", 0UL);
    }
    else
    {
        b2_stage(B2_OPERATE);
        uint64_t base = 0UL, end = 0UL;
        bool bounds_ok = tee_client_shared_range(&base, &end) && end - base >= 2UL * BUFFER_BYTES;
        if (bounds_ok)
        {
            uint64_t address = end - BUFFER_BYTES;
            volatile uint8_t *buffer = (volatile uint8_t *)address;
            for (uint64_t i = 0U; i < BUFFER_BYTES; ++i)
            {
                buffer[i] = (uint8_t)(BUFFER_SEED + i);
            }
            struct tee_reply last = tee_invoke_buffer(&session, address, BUFFER_BYTES);
            struct tee_reply overflow = tee_probe_buffer_unchecked(&session, address, BUFFER_BYTES + 1UL);
            struct tee_reply empty = tee_probe_buffer_unchecked(&session, address, 0UL);
            struct tee_reply recheck = tee_invoke_buffer(&session, address, BUFFER_BYTES);
            b2_note_reply("last-valid", last);
            b2_note_reply("one-byte-outside", overflow);
            b2_note_reply("zero-size", empty);
            b2_note_reply("rechecked", recheck);
            bounds_ok =
                success(last) && overflow.transport == TEE_CLIENT_OK &&
                overflow.result == TEE_ERROR_BAD_PARAMETERS && empty.transport == TEE_CLIENT_OK &&
                empty.result == TEE_ERROR_BAD_PARAMETERS && success(recheck) &&
                tee_invoke_buffer(&session, base, BUFFER_BYTES).transport == TEE_CLIENT_SHM_UNAVAILABLE;
            for (uint64_t i = 0U; i < BUFFER_BYTES; ++i)
            {
                if (buffer[i] != (uint8_t)(BUFFER_SEED + i))
                {
                    bounds_ok = false;
                }
            }
        }
        b2_stage(B2_COMPARE);
        (void)b2_check_value("shared_boundaries-predicates", 1UL, bounds_ok);
        test_report("TEE-BOUNDARY-01", bounds_ok ? TEST_PASS : TEST_FAIL,
                    "negotiated-SHM-last-byte+one-byte-outside+zero-length+message-overlap+unchanged", 0U);
    }
    b2_stage(B2_CLEANUP);
    b2_close_session(&session);
}

void run_boundary_tests(void)
{
    b2_run_session_limits();
    b2_run_message_abi();
    b2_run_shared_boundaries();
}
