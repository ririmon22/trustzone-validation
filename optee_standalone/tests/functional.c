#include "functional.h"
#define B2_CACHE_LINE 64UL
#define B2_NO_CASE UINT32_MAX
#define B2_TEST_VALUE 0x45a7U
volatile struct b2_result_buffer b2_results __attribute__((aligned(B2_CACHE_LINE)));
volatile const uint32_t b2_test_selection = B2_TEST_INDEX;
struct functional_case
{
    const char *id;
    void (*run)(void);
};
static const struct functional_case cases[B2_CASE_COUNT] = {
    {"TEE-01", b2_run_service},
    {"TEE-07", b2_run_panic_recovery},
    {"TEE-AUTH-TA", b2_run_identity},
    {"TEE-SHM-01", b2_run_shared_memory},
    {"TEE-KEY-API", b2_run_key_api},
    {"TEE-ISO-TA", b2_run_ta_isolation},
    {"TEE-SESSION-01", b2_run_session_limits},
    {"TEE-ABI-01", b2_run_message_abi},
    {"TEE-BOUNDARY-01", b2_run_shared_boundaries},
    {"B2-SMCCC-01", b2_run_standard_versions},
    {"TEE-SESSION-CORE-01", b2_run_closed_core_session},
    {"TEE-PARAM-01", b2_run_parameter_validation},
    {"TEE-KEY-ATTR-01", b2_run_key_attributes}, {"TZ-SYS-03", b2_run_physical_memory}, {"TEE-04", b2_run_signed_ta}};
static void copy_text(volatile char *out, unsigned int capacity, const char *text)
{
    unsigned int i = 0U;
    while (i + 1U < capacity && text[i] != '\0')
    {
        out[i] = text[i];
        i++;
    }
    out[i] = '\0';
}
/* JTAGのAP経路が非coherentでも、完了した更新をPoCへ反映する。 */
static void publish_results(void)
{
#ifndef B2_RESULT_HOST_TEST
    uint64_t sctlr;
    __asm__ volatile("mrs %0, sctlr_el1" : "=r"(sctlr));
    if ((sctlr & 4UL) == 0UL)
    {
        __asm__ volatile("dsb sy" ::: "memory");
        return;
    }
    uintptr_t begin = (uintptr_t)&b2_results;
    uintptr_t end = begin + sizeof(b2_results);
    for (uintptr_t line = begin; line < end; line += B2_CACHE_LINE)
    {
        __asm__ volatile("dc cvac, %0" ::"r"(line) : "memory");
    }
    __asm__ volatile("dsb sy" ::: "memory");
#endif
}
static bool review_requires_case(unsigned int index)
{
    switch (B2_REVIEW_PARENT) {
    case 0U: return index < 13U || index == 14U;
    case 1U: case 2U: return index == 0U;
    case 5U: return index == 3U || index == 8U || index == 11U;
    case 10U: return index == 2U;
    case 11U: return index == 4U || index == 12U;
    case 12U: return index == 0U || index == 6U || index == 7U || index == 10U || index == 11U;
    case 13U: return index == 1U || index == 5U;
    case 14U: return index == 14U;
    default: return false;
    }
}
void b2_results_begin(void)
{
    volatile unsigned char *bytes = (volatile unsigned char *)&b2_results;
    for (unsigned int i = 0U; i < sizeof(b2_results); ++i)
    {
        bytes[i] = 0U;
    }
    b2_results.version = B2_RESULT_VERSION;
    b2_results.bytes = sizeof(b2_results);
    b2_results.state = B2_RUNNING;
    b2_results.run_id = debug_boot.run_id;
    b2_results.selection = b2_test_selection;
    b2_results.current_case = B2_NO_CASE;
    for (unsigned int i = 0U; i < B2_CASE_COUNT; ++i)
    {
        copy_text(b2_results.cases[i].id, B2_ID_BYTES, cases[i].id);
        b2_results.cases[i].selected = (b2_test_selection == 0U && i < 13U) || (b2_test_selection == 17U && review_requires_case(i)) || b2_test_selection == (i >= 13U ? i + 2U : i + 1U);
        b2_results.cases[i].status = TEST_NOT_RUN;
    }
    b2_results.magic = B2_RESULT_MAGIC;
    publish_results();
}
void b2_stage(enum b2_phase phase)
{
    if (b2_results.magic != B2_RESULT_MAGIC)
    {
        return;
    }
    b2_results.phase = (uint32_t)phase;
    if (b2_results.current_case < B2_CASE_COUNT)
    {
        b2_results.cases[b2_results.current_case].phase = (uint32_t)phase;
        (void)log_printf("PHASE %s stage=%u\n", cases[b2_results.current_case].id, (unsigned int)phase);
    }
    publish_results();
}
bool b2_check_value(const char *name, uint64_t expected, uint64_t actual)
{
    if (b2_results.magic == B2_RESULT_MAGIC && b2_results.current_case < B2_CASE_COUNT)
    {
        volatile struct b2_case_result *record = &b2_results.cases[b2_results.current_case];
        if (record->check_count < B2_CHECK_COUNT)
        {
            volatile struct b2_check *check = &record->checks[record->check_count];
            copy_text(check->name, B2_CHECK_NAME_BYTES, name);
            check->expected = expected;
            check->actual = actual;
            record->check_count++;
        }
        else
        {
            record->overflow++;
        }
    }
    (void)log_printf("CHECK %s expected=0x%llx actual=0x%llx\n", name, (unsigned long long)expected,
                     (unsigned long long)actual);
    publish_results();
    return expected == actual;
}
void b2_note_reply(const char *name, struct tee_reply reply)
{
    (void)log_printf("REPLY %s transport=%u result=0x%08x origin=%u value=0x%08x\n", name, reply.transport,
                     reply.result, reply.origin, reply.value);
}
/* 共通runnerは弱参照でこの保存先を呼ぶ。A/B1の依存を増やさない。 */
void b2_save_report(const char *id, enum test_status status, const char *reason)
{
    if (b2_results.magic != B2_RESULT_MAGIC || b2_results.current_case >= B2_CASE_COUNT)
    {
        return;
    }
    volatile struct b2_case_result *record = &b2_results.cases[b2_results.current_case];
    const char *expected = cases[b2_results.current_case].id;
    unsigned int i = 0U;
    while (id[i] != '\0' && expected[i] == id[i])
    {
        i++;
    }
    if (id[i] != expected[i])
    {
        b2_results.state = B2_ERROR;
        publish_results();
        return;
    }
    if (record->reported != 0U)
    {
        b2_results.state = B2_ERROR;
    }
    record->status = (uint32_t)status;
    record->reported++;
    copy_text(record->reason, B2_REASON_BYTES, reason);
    b2_stage(B2_SAVE);
}
void b2_close_session(struct tee_session *session)
{
    if (!session->open)
    {
        return;
    }
    struct tee_reply reply = tee_session_close(session);
    b2_note_reply("close", reply);
    if (reply.transport != TEE_CLIENT_OK || reply.result != TEE_SUCCESS)
    {
        b2_results.cleanup_errors++;
        (void)log_printf("FATAL session-close-failed transport=%u result=0x%08x\n", reply.transport,
                         reply.result);
    }
}
bool b2_open_pair(struct tee_session *a, struct tee_session *b)
{
    struct tee_reply opened_a = tee_session_open(a, validation_ta_uuid);
    struct tee_reply opened_b = tee_session_open(b, validation_ta_b_uuid);
    b2_note_reply("open-A", opened_a);
    b2_note_reply("open-B", opened_b);
    return opened_a.transport == TEE_CLIENT_OK && opened_a.result == TEE_SUCCESS &&
           opened_b.transport == TEE_CLIENT_OK && opened_b.result == TEE_SUCCESS;
}
void b2_results_end(void)
{
    bool complete = b2_results.state != B2_ERROR && b2_results.cleanup_errors == 0U;
    for (unsigned int i = 0U; i < B2_CASE_COUNT; ++i)
    {
        if (b2_results.cases[i].selected &&
            (b2_results.cases[i].reported != 1U || b2_results.cases[i].overflow != 0U ||
             (b2_results.cases[i].status == TEST_PASS && b2_results.cases[i].check_count == 0U)))
        {
            complete = false;
        }
    }
    b2_results.state = complete ? B2_COMPLETE : B2_ERROR;
    b2_stage(B2_DONE);
}
void b2_run_service(void)
{
    struct tee_session first = {0U, false}, second = {0U, false};
    struct tee_reply opened_first = tee_session_open(&first, validation_ta_uuid);
    struct tee_reply opened_second = tee_session_open(&second, validation_ta_uuid);
    if (opened_first.transport != TEE_CLIENT_OK || opened_first.result != TEE_SUCCESS ||
        opened_second.transport != TEE_CLIENT_OK || opened_second.result != TEE_SUCCESS)
    {
        test_report("TEE-01", TEST_BLOCKED, "TA-open-or-RPC-unavailable", 0UL);
    }
    else
    {
        b2_stage(B2_OPERATE);
        struct tee_reply echoed = tee_invoke(&first, TA_COMMAND_ECHO, B2_TEST_VALUE);
        struct tee_reply second_echo = tee_invoke(&second, TA_COMMAND_ECHO, B2_TEST_VALUE);
        struct tee_reply invalid = tee_invoke(&first, UINT32_MAX, 0U);
        b2_note_reply("echo-first", echoed);
        b2_note_reply("echo-second", second_echo);
        b2_note_reply("invalid-command", invalid);
        b2_stage(B2_COMPARE);
        bool first_ok = echoed.transport == TEE_CLIENT_OK && echoed.result == TEE_SUCCESS &&
                        echoed.value == B2_TEST_VALUE;
        bool second_ok = second_echo.transport == TEE_CLIENT_OK && second_echo.result == TEE_SUCCESS &&
                         second_echo.value == B2_TEST_VALUE;
        bool rejected = invalid.transport == TEE_CLIENT_OK && invalid.result == TEE_ERROR_BAD_PARAMETERS;
        bool service_ok = b2_check_value("first-echo", 1UL, first_ok);
        service_ok = b2_check_value("second-echo", 1UL, second_ok) && service_ok;
        service_ok = b2_check_value("unknown-command-rejected", 1UL, rejected) && service_ok;
        test_report("TEE-01", service_ok ? TEST_PASS : TEST_FAIL, "two-sessions+echo+invalid-command", 0UL);
    }
    b2_stage(B2_CLEANUP);
    b2_close_session(&first);
    b2_close_session(&second);
}
void b2_run_panic_recovery(void)
{
    struct tee_session first = {0U, false}, second = {0U, false};
    if (!b2_open_pair(&first, &second))
    {
        test_report("TEE-07", TEST_BLOCKED, "TA-B-or-A-not-available", 0UL);
    }
    else
    {
        /* この区間だけTA-A終了を意図する。Core/BL33の異常停止は期待しない。 */
        b2_stage(B2_EXPECTED_FAULT);
        struct tee_reply panic = tee_invoke(&first, TA_COMMAND_PANIC, 0U);
        struct tee_reply alive = tee_invoke(&second, TA_COMMAND_ECHO, B2_TEST_VALUE);
        struct tee_reply reopened = tee_session_open(&first, validation_ta_uuid);
        struct tee_reply recovered = tee_invoke(&first, TA_COMMAND_ECHO, B2_TEST_VALUE);
        b2_note_reply("expected-panic", panic);
        b2_note_reply("victim-alive", alive);
        b2_note_reply("reopen", reopened);
        b2_note_reply("recovered", recovered);
        b2_stage(B2_COMPARE);
        bool expected_panic = panic.transport == TEE_CLIENT_OK && panic.result == TEE_ERROR_TARGET_DEAD;
        bool victim_alive =
            alive.transport == TEE_CLIENT_OK && alive.result == TEE_SUCCESS && alive.value == B2_TEST_VALUE;
        bool reopened_ok = reopened.transport == TEE_CLIENT_OK && reopened.result == TEE_SUCCESS;
        bool continued = recovered.transport == TEE_CLIENT_OK && recovered.result == TEE_SUCCESS &&
                         recovered.value == B2_TEST_VALUE;
        bool isolated_ok = b2_check_value("expected-target-dead", 1UL, expected_panic);
        isolated_ok = b2_check_value("other-TA-continues", 1UL, victim_alive) && isolated_ok;
        isolated_ok = b2_check_value("reopen", 1UL, reopened_ok) && isolated_ok;
        isolated_ok = b2_check_value("recovered-echo", 1UL, continued) && isolated_ok;
        test_report("TEE-07", isolated_ok ? TEST_PASS : TEST_FAIL, "TA-A-panic+TA-B-continuity+TA-A-reopen",
                    0UL);
    }
    b2_stage(B2_CLEANUP);
    b2_close_session(&first);
    b2_close_session(&second);
}
void b2_run_functional_suite(void)
{
    for (unsigned int i = 0U; i < B2_CASE_COUNT; ++i)
    {
        if (!b2_results.cases[i].selected)
        {
            continue;
        }
        b2_results.current_case = i;
        b2_stage(B2_PREPARE);
        if (tee_client_active_sessions() != 0U)
        {
            test_report(cases[i].id, TEST_BLOCKED, "previous-session-cleanup-incomplete", 0UL);
            continue;
        }
        cases[i].run();
        if (tee_client_active_sessions() != 0U)
        {
            b2_results.cleanup_errors++;
        }
        b2_stage(B2_DONE);
    }
    b2_results_end();
}
