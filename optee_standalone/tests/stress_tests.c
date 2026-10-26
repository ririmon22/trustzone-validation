#include "client.h"
#include "test_runner.h"
#define STRESS_ITERATIONS 512U
#define STRESS_WARMUP 32U
#define PANIC_INTERVAL 16U
#define PROGRESS_INTERVAL 128U
#define STRESS_TOKEN 0xa135U
static bool ok(struct tee_reply reply)
{
    return reply.transport == TEE_CLIENT_OK && reply.result == TEE_SUCCESS;
}
static bool cycle(unsigned int index)
{
    struct tee_session a = { 0U, false }, b = { 0U, false };
    if (!ok(tee_session_open(&a, validation_ta_uuid)) || !ok(tee_session_open(&b, validation_ta_b_uuid))) { return false; }
    struct tee_reply first = tee_invoke(&a, TA_COMMAND_ECHO, STRESS_TOKEN);
    struct tee_reply second = tee_invoke(&b, TA_COMMAND_ECHO, STRESS_TOKEN);
    bool passed = ok(first) && ok(second) && first.value == STRESS_TOKEN && second.value == STRESS_TOKEN;
    struct tee_reply invalid = tee_probe_buffer_unchecked(&a, OPTEE_RESERVED_BASE, 16UL);
    passed = passed && invalid.transport == TEE_CLIENT_OK && invalid.result == TEE_ERROR_BAD_PARAMETERS;
    passed = passed && ok(tee_invoke(&b, TA_COMMAND_ECHO, STRESS_TOKEN));
    passed = passed && ok(tee_invoke(&a, TA_COMMAND_KEY_CYCLE, 0U)) && ok(tee_invoke(&a, TA_COMMAND_CRYPTO, 0U));
    if (index % PANIC_INTERVAL == 0U) {
        struct tee_reply fault = tee_invoke(&a, TA_COMMAND_PANIC, 0U);
        passed = passed && fault.transport == TEE_CLIENT_OK && fault.result == TEE_ERROR_TARGET_DEAD && !a.open;
        struct tee_reply alive = tee_invoke(&b, TA_COMMAND_ECHO, STRESS_TOKEN);
        passed = passed && ok(alive) && alive.value == STRESS_TOKEN && ok(tee_session_open(&a, validation_ta_uuid));
        struct tee_reply recovered = tee_invoke(&a, TA_COMMAND_ECHO, STRESS_TOKEN);
        passed = passed && ok(recovered) && recovered.value == STRESS_TOKEN;
    }
    bool closed_a = a.open && ok(tee_session_close(&a));
    bool closed_b = b.open && ok(tee_session_close(&b));
    return passed && closed_a && closed_b && tee_client_active_sessions() == 1U;
}
static bool read_stats(struct tee_session *session, uint32_t values[3])
{
    const uint32_t commands[] = { CANARY_COMMAND_HEAP, CANARY_COMMAND_TA_RAM, CANARY_COMMAND_ALLOC_FAILURES };
    for (unsigned int i = 0U; i < 3U; ++i) {
        struct tee_reply reply = tee_invoke(session, commands[i], 0U);
        if (!ok(reply)) { return false; }
        values[i] = reply.value;
    }
    return true;
}
void run_stress_tests(void)
{
    console_write("STRESS_META iterations=512 warmup=32 panic_every=16 invalid_shm_per_iteration=1 key_cycle_per_iteration=1 alternating=A-B-A sessions_per_iteration=2\n");
    uint64_t started = counter_read();
    struct tee_session stats = { 0U, false };
    if (!ok(tee_session_open(&stats, validation_pta_uuid))) { test_report("TEE-STRESS-01", TEST_BLOCKED, "Core-stats-service-unavailable", 0U); return; }
    bool passed = true;
    for (unsigned int i = 0U; i < STRESS_WARMUP; ++i) { if (!cycle(i)) { passed = false; break; } }
    uint32_t before[3] = { 0U }, after[3] = { 0U };
    passed = passed && read_stats(&stats, before);
    unsigned int completed = 0U;
    while (passed && completed < STRESS_ITERATIONS) {
        passed = cycle(completed);
        if (!passed) { break; }
        completed++;
        if (completed % PROGRESS_INTERVAL == 0U) {
            console_write("STRESS_PROGRESS completed="); console_uint(completed); console_write("\n");
        }
    }
    bool observed = read_stats(&stats, after);
    console_write("STRESS_RESOURCE heap_before="); console_uint(before[0]); console_write(" heap_after="); console_uint(after[0]);
    console_write(" ta_before="); console_uint(before[1]); console_write(" ta_after="); console_uint(after[1]);
    console_write(" alloc_fail_before="); console_uint(before[2]); console_write(" alloc_fail_after="); console_uint(after[2]);
    console_write(" completed="); console_uint(completed); console_write("\n");
    passed = passed && observed && completed == STRESS_ITERATIONS && after[0] <= before[0] && after[1] <= before[1] && after[2] == before[2];
    passed = ok(tee_session_close(&stats)) && passed && tee_client_active_sessions() == 0U;
    test_report("TEE-STRESS-01", passed ? TEST_PASS : TEST_FAIL, "bounded-session-command-panic-shm-key-cycles+Core-resource-recheck", counter_read() - started);
}
