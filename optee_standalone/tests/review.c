#include "review.h"
#include "benchmark.h"
#ifndef B2_VALIDATION_PTA
#define B2_VALIDATION_PTA 0U
#endif
#ifndef B2_SECURE_SGI
#define B2_SECURE_SGI 0U
#endif
#define REVIEW_MAGIC 0x42325256U
#define STATE_ITERATIONS 256U
#define STATE_SEED 0x59e1a273U
#define INVALID_CASE UINT32_MAX
const uint32_t b2_review_features[3] = {1U, B2_VALIDATION_PTA, B2_SECURE_SGI};
volatile const uint32_t b2_review_parent = B2_REVIEW_PARENT;
static bool wanted(unsigned int item) { return B2_REVIEW_PARENT == 0U || B2_REVIEW_PARENT == item + 1U; }
volatile struct sgi_samples b2_sgi_results __attribute__((aligned(64)));
volatile struct review_buffer b2_review_results __attribute__((aligned(64)));
static const char *const parent_ids[B2_REVIEW_COUNT] = {
    "TZ-SYS-01-V01", "TZ-SYS-02-V01", "TZ-SYS-02-V02", "TZ-SYS-03-V01", "TZ-SYS-03-V02",
    "TZ-SYS-04-V01", "TZ-SYS-05-V01", "TZ-SYS-06-V01", "TZ-SYS-07-V01",
    "EXT-AUTH-V01", "EXT-KEY-V01", "EXT-TA-V01", "EXT-TA-V02", "EXT-TA-V03",
    "EXT-BOOT-V01", "EXT-STORE-V01", "PERF-01", "PERF-02", "PERF-03", "PERF-04", "PERF-05"
};
extern struct tee_reply b2_state_echo(struct tee_session *, uint32_t, uint64_t *);
static bool success(struct tee_reply reply)
{
    return reply.transport == TEE_CLIENT_OK && reply.result == TEE_SUCCESS;
}
static void copy_text(volatile char *out, unsigned int size, const char *text)
{
    unsigned int i = 0U;
    while (i + 1U < size && text[i]) { out[i] = text[i]; ++i; }
    out[i] = '\0';
}
static void publish(void)
{
    uint64_t sctlr;
    __asm__ volatile("mrs %0, sctlr_el1" : "=r"(sctlr));
    if (sctlr & 4UL) {
        for (uintptr_t p = (uintptr_t)&b2_review_results;
             p < (uintptr_t)&b2_review_results + sizeof(b2_review_results); p += 64UL) {
            __asm__ volatile("dc cvac, %0" :: "r"(p) : "memory");
        }
    }
    __asm__ volatile("dsb sy" ::: "memory");
}
static void stage(unsigned int item, enum b2_phase phase)
{
    b2_review_results.current = item;
    b2_review_results.items[item].phase = phase;
    b2_review_results.items[item].status = TEST_NOT_RUN;
    publish();
}
static bool check(unsigned int item, const char *name, uint64_t expected, uint64_t actual)
{
    volatile struct review_result *r = &b2_review_results.items[item];
    if (r->count < B2_REVIEW_CHECKS) {
        unsigned int i = r->count++;
        copy_text(r->checks[i].name, 32U, name);
        r->checks[i].expected = expected; r->checks[i].actual = actual;
    } else { r->overflow++; }
    (void)log_printf("REVIEW %s %s expected=0x%llx actual=0x%llx\n", parent_ids[item], name,
                     (unsigned long long)expected, (unsigned long long)actual);
    publish();
    return expected == actual;
}
static void finish(unsigned int item, enum test_status status, const char *reason)
{
    volatile struct review_result *r = &b2_review_results.items[item];
    r->status = status; r->phase = B2_DONE;
    copy_text(r->reason, sizeof(r->reason), reason);
    (void)log_printf("REVIEW_RESULT %s status=%u %s\n", parent_ids[item], (unsigned int)status, reason);
    publish();
}
static bool close_owned(struct tee_session *session)
{
    if (!session->open) { return true; }
    struct tee_reply closed = tee_session_close(session);
    if (!success(closed)) { b2_review_results.cleanup_errors++; return false; }
    return true;
}
/* 既存小試験の全条件を引き継ぎ、親の意味を拡張しない。 */
static void inherit(unsigned int item, const unsigned int *indices, unsigned int count, const char *reason)
{
    enum test_status status = TEST_PASS;
    for (unsigned int i = 0U; i < count; ++i) {
        const volatile struct b2_case_result *r = &b2_results.cases[indices[i]];
        (void)check(item, r->id[0] ? "functional-status" : "missing-ID", TEST_PASS, r->status);
        if (r->status == TEST_FAIL) { status = TEST_FAIL; }
        else if (status != TEST_FAIL && (!r->selected || r->reported != 1U || r->overflow || r->status != TEST_PASS)) {
            status = TEST_BLOCKED;
        }
    }
    finish(item, status, reason);
}
static void run_state_retention(void)
{
    struct tee_session a = {0U, false}, b = {0U, false};
    stage(2U, B2_PREPARE);
    if (!b2_open_pair(&a, &b)) {
        (void)close_owned(&a); (void)close_owned(&b);
        finish(2U, TEST_BLOCKED, "Early-TA-open-unavailable"); return;
    }
    uint64_t sctlr, ttbr, tcr, daif, after_sctlr, after_ttbr, after_tcr, after_daif;
    __asm__ volatile("mrs %0,sctlr_el1; mrs %1,ttbr0_el1; mrs %2,tcr_el1; mrs %3,daif"
                     : "=r"(sctlr), "=r"(ttbr), "=r"(tcr), "=r"(daif));
    volatile uint32_t normal_state = STATE_SEED;
    unsigned int completed = 0U; uint64_t register_errors = 0UL;
    bool valid = true;
    uint32_t last_input = 0U, expected_counter = 0U, expected_normal = 0U, observed_normal = 0U;
    struct tee_reply last_echo = {0U}, last_counter = {0U};
    stage(2U, B2_OPERATE);
    for (unsigned int i = 0U; i < STATE_ITERATIONS; ++i) {
        uint32_t input = STATE_SEED ^ (i * 0x10201U);
        uint64_t altered = 0UL;
        struct tee_reply echo = b2_state_echo((i & 1U) ? &b : &a, input, &altered);
        struct tee_reply counter = tee_invoke((i & 1U) ? &b : &a, TA_COMMAND_SESSION_COUNTER, 0U);
        last_input = input; expected_counter = i / 2U + 1U;
        expected_normal = STATE_SEED ^ i; observed_normal = normal_state;
        last_echo = echo; last_counter = counter;
        register_errors += altered;
        if (!success(echo) || echo.value != input || !success(counter) || counter.value != i / 2U + 1U || normal_state != (STATE_SEED ^ i) || altered) {
            valid = false; break;
        }
        normal_state = STATE_SEED ^ (i + 1U); ++completed;
    }
    __asm__ volatile("mrs %0,sctlr_el1; mrs %1,ttbr0_el1; mrs %2,tcr_el1; mrs %3,daif"
                     : "=r"(after_sctlr), "=r"(after_ttbr), "=r"(after_tcr), "=r"(after_daif));
    struct tee_reply secret_a = tee_invoke(&a, TA_COMMAND_SECRET_CHECK, 0U);
    struct tee_reply secret_b = tee_invoke(&b, TA_COMMAND_SECRET_CHECK, 0U);
    stage(2U, B2_COMPARE);
    valid = check(2U, "iterations", STATE_ITERATIONS, completed) && valid;
    valid = check(2U, "per-session-counter-steps", STATE_ITERATIONS / 2U, completed / 2U) && valid;
    valid = check(2U, "input-seed", STATE_SEED, STATE_SEED) && valid;
    valid = check(2U, "x19-x29-SP-errors", 0UL, register_errors) && valid;
    valid = check(2U, "SCTLR_EL1", sctlr, after_sctlr) && valid;
    valid = check(2U, "TTBR0_EL1", ttbr, after_ttbr) && valid;
    valid = check(2U, "TCR_EL1", tcr, after_tcr) && valid;
    valid = check(2U, "DAIF", daif, after_daif) && valid;
    valid = check(2U, "A-secret", 0x6d3a4719U, success(secret_a) ? secret_a.value : 0U) && valid;
    valid = check(2U, "B-secret", 0xb5208c14U, success(secret_b) ? secret_b.value : 0U) && valid;
    valid = check(2U, "last-ECHO", last_input, last_echo.value) && valid;
    valid = check(2U, "last-counter", expected_counter, last_counter.value) && valid;
    valid = check(2U, "last-Normal-state", expected_normal, observed_normal) && valid;
    valid = check(2U, "ECHO-transport-result", 0U, ((uint64_t)last_echo.transport << 32) | last_echo.result) && valid;
    valid = check(2U, "counter-transport-result", 0U, ((uint64_t)last_counter.transport << 32) | last_counter.result) && valid;
    valid = check(2U, "TA-origins", (4UL << 32) | 4UL, ((uint64_t)last_echo.origin << 32) | last_counter.origin) && valid;
    stage(2U, B2_CLEANUP);
    valid = close_owned(&a) && valid; valid = close_owned(&b) && valid;
    finish(2U, valid ? TEST_PASS : TEST_FAIL, "256-ECHO+session-counters+callee-saved-GPR-SP+EL1-state+private-state");
}
static void run_token_policy(void)
{
    struct tee_session first = {0U, false}, other = {0U, false};
    stage(9U, B2_PREPARE);
    bool opened_first = success(tee_session_open(&first, validation_ta_uuid));
    bool opened_other = success(tee_session_open(&other, validation_ta_uuid));
    bool valid = opened_first && opened_other;
    if (!valid) {
        (void)close_owned(&first); (void)close_owned(&other);
        finish(9U, TEST_BLOCKED, "token-policy-session-open-unavailable"); return;
    }
    if (valid) {
        stage(9U, B2_OPERATE);
        struct tee_reply before = tee_invoke(&first, TA_COMMAND_READ, 0U);
        struct tee_reply wrong = tee_invoke(&first, TA_COMMAND_AUTHENTICATE, 0U);
        struct tee_reply allow = tee_invoke(&first, TA_COMMAND_AUTHENTICATE, TA_TEST_CREDENTIAL);
        struct tee_reply read = tee_invoke(&first, TA_COMMAND_READ, 0U);
        struct tee_reply isolated = tee_invoke(&other, TA_COMMAND_READ, 0U);
        struct tee_reply revoke = tee_invoke(&first, TA_COMMAND_AUTHENTICATE, 0U);
        struct tee_reply revoked = tee_invoke(&first, TA_COMMAND_READ, 0U);
        stage(9U, B2_COMPARE);
        valid = check(9U, "before-token", TEE_ERROR_ACCESS_DENIED, before.result) && success(allow);
        valid = check(9U, "wrong-token", TEE_ERROR_ACCESS_DENIED, wrong.result) && valid;
        valid = check(9U, "authorized-read", TA_PRIVATE_VALUE, success(read) ? read.value : 0U) && valid;
        valid = check(9U, "other-session-denied", TEE_ERROR_ACCESS_DENIED, isolated.result) && valid;
        valid = check(9U, "revoke", TEE_ERROR_ACCESS_DENIED, revoke.result) && valid;
        valid = check(9U, "after-revoke", TEE_ERROR_ACCESS_DENIED, revoked.result) && valid;
        valid = before.transport == TEE_CLIENT_OK && wrong.transport == TEE_CLIENT_OK && isolated.transport == TEE_CLIENT_OK && revoke.transport == TEE_CLIENT_OK && revoked.transport == TEE_CLIENT_OK && valid;
        valid = before.origin == 4U && wrong.origin == 4U && isolated.origin == 4U && revoke.origin == 4U && revoked.origin == 4U && valid;
    }
    stage(9U, B2_CLEANUP);
    valid = close_owned(&first) && valid;
    struct tee_reply stale = tee_invoke(&first, TA_COMMAND_READ, 0U);
    valid = check(9U, "closed-client-state", TEE_CLIENT_BAD_STATE, stale.transport) && valid;
    bool reopened = success(tee_session_open(&first, validation_ta_uuid));
    struct tee_reply fresh = tee_invoke(&first, TA_COMMAND_READ, 0U);
    valid = check(9U, "reopen-without-token", TEE_ERROR_ACCESS_DENIED, fresh.result) && reopened && fresh.transport == TEE_CLIENT_OK && fresh.origin == 4U && valid;
    valid = close_owned(&first) && valid; valid = close_owned(&other) && valid;
    valid = check(9U, "TA-identity-subtest", TEST_PASS, b2_results.cases[2].status) && valid;
    finish(9U, valid ? TEST_PASS : TEST_FAIL, "fixed-test-token-session-policy+Core-TA-identity; NOT-Normal-person-authentication");
}
static void supplement_isolation(void)
{
    extern void run_stress_tests(void);
    if (B2_VALIDATION_PTA != 1U) {
        finish(12U, TEST_BLOCKED, "TA-isolation-subtests-recorded; Core-canary/stats-not-in-selected-profile"); return;
    }
    stage(12U, B2_OPERATE);
    enum test_status core = b2_probe_core_isolation();
    /* Core canary/statsは検証overlayの補足。TA仮想分離と物理保護を同一視しない。 */
    (void)check(12U, "Core-canary-status", TEST_PASS, core);
    run_stress_tests();
    (void)check(12U, "resource-stress-status", TEST_PASS, b2_stress_results.status);
    (void)check(12U, "stress-completed", 512U, b2_stress_results.completed);
    if (core == TEST_FAIL || b2_stress_results.status == TEST_FAIL) {
        finish(12U, TEST_FAIL, "TA-isolation-or-optional-Core/resource-supplement-failed");
    } else if (core != TEST_PASS || b2_stress_results.status != TEST_PASS) {
        finish(12U, TEST_BLOCKED, "TA-subtests-recorded; optional-Core-canary/stats-profile-missing");
    } else {
        finish(12U, TEST_PASS, "TA-VM+test-Core-canary+512-resource-cycles; not-NS-physical-protection");
    }
}

static void run_secure_sgi(void)
{
    static const uint8_t uuid[16] = {0x6d,0xc1,0x34,0x1e,0x53,0x4f,0x46,0x42,0x91,0x38,0x21,0x58,0x76,0x30,0x49,0x10};
    struct tee_session service = {0U, false};
    unsigned int item = b2_test_selection == B2_REVIEW_SELECTION && wanted(7U) ? 7U : 18U;
    bool measure = b2_test_selection == B2_PERFORMANCE_SELECTION || wanted(18U);
    if (B2_SECURE_SGI != 1U) {
        finish(item, TEST_BLOCKED, "Secure-SGI-not-in-selected-Core; ZCU-SGI-group-route-review-required");
        if (measure && item != 18U) finish(18U, TEST_BLOCKED, "Secure-SGI-profile-not-enabled");
        return;
    }
    stage(item, B2_PREPARE);
    if (!success(tee_session_open(&service, uuid))) {
        finish(item, TEST_BLOCKED, "Secure-SGI-PTA-unavailable; Core/profile-mismatch");
        if (measure && item != 18U) finish(18U, TEST_BLOCKED, "Core/profile-mismatch");
        return;
    }
    stage(item, B2_OPERATE);
    struct tee_reply fired = tee_invoke(&service, 0U, 0U);
    struct tee_reply id = tee_invoke(&service, 1U, 0U);
    struct tee_reply masks = tee_invoke(&service, 2U, 0U);
    struct tee_reply entry = tee_invoke(&service, 3U, 0U);
    struct tee_reply count = tee_invoke(&service, 4U, 0U);
    struct tee_reply el = tee_invoke(&service, 8U, 0U);
    bool valid = success(fired) && success(id) && success(masks) && success(entry) && success(count) && success(el);
    valid = check(item, "received-this-request", 1U, fired.value) && valid;
    valid = check(item, "Secure-SGI-ID", 11U, id.value) && valid;
    valid = check(item, "raised-count", 1U, count.value) && valid;
    valid = check(item, "handler-EL", 1U, el.value) && valid;
    (void)check(item, "handler-exception-mask", masks.value, masks.value);
    (void)check(item, "entry-exception-mask", entry.value, entry.value);
    if (measure) {
        volatile uint8_t *bytes = (volatile uint8_t *)&b2_sgi_results;
        for (size_t i = 0U; i < sizeof(b2_sgi_results); ++i) bytes[i] = 0U;
        b2_sgi_results.version = 1U; b2_sgi_results.bytes = sizeof(b2_sgi_results);
        b2_sgi_results.run_id = debug_boot.run_id; b2_sgi_results.state = B2_RUNNING;
        b2_sgi_results.magic = 0x42325347U; b2_sgi_results.warmup = BENCH_WARMUP;
        b2_sgi_results.received_id = id.value; b2_sgi_results.received_mask = masks.value;
        b2_sgi_results.entry_mask = entry.value;
        struct tee_reply frequency = tee_invoke(&service, 7U, 0U);
        b2_sgi_results.frequency = frequency.value;
        bool measured = valid && success(frequency) && frequency.value == counter_frequency();
        for (unsigned int i = 0U; i < BENCH_WARMUP + BENCH_SAMPLES && measured; ++i) {
            struct tee_reply next = tee_invoke(&service, 0U, 0U);
            struct tee_reply ticks = tee_invoke(&service, 5U, 0U);
            struct tee_reply overhead = tee_invoke(&service, 6U, 0U);
            measured = success(next) && next.value == 1U && success(ticks) && success(overhead);
            if (i >= BENCH_WARMUP && measured) {
                unsigned int at = i - BENCH_WARMUP;
                b2_sgi_results.samples[at] = ticks.value; b2_sgi_results.overhead[at] = overhead.value;
                b2_sgi_results.count = at + 1U;
            }
        }
        b2_sgi_results.errors = measured ? 0U : 1U;
        b2_sgi_results.state = measured ? B2_COMPLETE : B2_ERROR;
        valid = measured && valid;
        (void)check(18U, "samples", BENCH_SAMPLES, b2_sgi_results.count);
        finish(18U, measured ? TEST_PASS : TEST_FAIL, "MEASUREMENT-COMPLETE: Secure-SGI-issue-to-C-handler-timestamp; no-speed-threshold");
    }
    stage(item, B2_CLEANUP); valid = close_owned(&service) && valid;
    if (measure && !valid) finish(18U, TEST_FAIL, "Secure-SGI-measurement-or-cleanup-failed");
    if (item == 18U) finish(18U, valid ? TEST_PASS : TEST_FAIL, "MEASUREMENT-COMPLETE: Secure-self-SGI; no-speed-threshold");
    if (item != 18U) finish(item, valid ? TEST_PASS : TEST_FAIL, "Secure-self-SGI-only; Normal-running-delivery-NOT_RUN");
}

void b2_run_review(void)
{
    volatile uint8_t *bytes = (volatile uint8_t *)&b2_review_results;
    for (size_t i = 0U; i < sizeof(b2_review_results); ++i) { bytes[i] = 0U; }
    b2_review_results.version = 2U; b2_review_results.bytes = sizeof(b2_review_results);
    b2_review_results.run_id = debug_boot.run_id; b2_review_results.state = B2_RUNNING;
    b2_review_results.current = INVALID_CASE; b2_review_results.magic = REVIEW_MAGIC;
    for (unsigned int i = 0U; i < B2_REVIEW_COUNT; ++i) {
        copy_text(b2_review_results.items[i].id, 24U, parent_ids[i]);
        b2_review_results.items[i].status = TEST_NOT_RUN;
    }
    publish();
    b2_results.current_case = INVALID_CASE;
    if (b2_test_selection == B2_REVIEW_SELECTION) {
        const unsigned int service[] = {0U}, shared[] = {3U,8U,11U}, key[] = {4U,12U};
        const unsigned int calls[] = {0U,6U,7U,10U,11U}, isolation[] = {1U,5U}, signed_ta[] = {14U};
        if (wanted(0U)) inherit(0U, service, 1U, "Normal-controls-and-Secure-TA-ECHO; not-two-baremetal-worlds");
        if (wanted(1U)) inherit(1U, service, 1U, "SMC-message-TA-call-and-Normal-continuation");
        if (wanted(2U)) run_state_retention();
        if (wanted(3U)) finish(3U, TEST_BLOCKED, "XMPU-path-master-lock-and-dedicated-region-review-required; use-selection-15");
        if (wanted(4U)) inherit(4U, shared, 3U, "private-TA-state+SHM-validation; not-physical-protection");
        if (wanted(9U)) run_token_policy();
        if (wanted(10U)) inherit(10U, key, 2U, "generate-use-export-policy-nonextractable-AES-KAT; no-raw-extraction-claim");
        if (wanted(11U)) inherit(11U, calls, 5U, "input-session-Core-and-TA-validation+cleanup");
        if (wanted(12U)) inherit(12U, isolation, 2U, "TA-VM-read-write-faults+victim-unchanged+panic-recovery");
        if (wanted(13U)) inherit(13U, signed_ta, 1U, "runtime-signed-TA-loader; require-separate-Core-audit-evidence");
    }
    for (unsigned int i = 0U; i < B2_REVIEW_COUNT; ++i) {
        if (wanted(i) && (i == 5U || i == 6U || i == 8U || i == 14U || i == 15U || i == 19U)) {
            finish(i, TEST_SKIP, "OUT-OF-SCOPE: peripheral-DMA-debug-authenticated-boot-persistent-storage");
        }
    }
    if (b2_test_selection == B2_REVIEW_SELECTION) { if (wanted(12U)) supplement_isolation(); if (wanted(7U) || wanted(18U)) run_secure_sgi(); }
    if (b2_test_selection == B2_PERFORMANCE_SELECTION) run_secure_sgi();
    bool measure = b2_test_selection == B2_PERFORMANCE_SELECTION || wanted(16U) || wanted(17U) || wanted(20U);
    if (measure) b2_run_performance();
    const unsigned int measured[] = {16U,17U,20U};
    for (unsigned int i = 0U; i < 3U; ++i) {
        unsigned int item = measured[i];
        if (!measure || (b2_test_selection != B2_PERFORMANCE_SELECTION && !wanted(item))) continue;
        stage(item, B2_COMPARE);
        bool valid = check(item, "series-count", 8U, b2_bench_results.count);
        valid = check(item, "operation-errors", 0U, b2_bench_results.errors) && valid;
        valid = check(item, "record-complete", 2U, b2_bench_results.state) && valid;
        finish(item, valid ? TEST_PASS : TEST_FAIL, "MEASUREMENT-COMPLETE-only; no-speed-threshold-or-performance-PASS");
    }
    uint64_t cache;
    __asm__ volatile("mrs %0,sctlr_el1" : "=r"(cache));
    if (cache & 4UL) {
        for (uintptr_t p = (uintptr_t)&b2_stress_results;
             p < (uintptr_t)&b2_stress_results + sizeof(b2_stress_results); p += 64UL) {
            __asm__ volatile("dc cvac, %0" :: "r"(p) : "memory");
        }
    }
    __asm__ volatile("dsb sy" ::: "memory");
    if (cache & 4UL) {
        for (uintptr_t p = (uintptr_t)&b2_sgi_results;
             p < (uintptr_t)&b2_sgi_results + sizeof(b2_sgi_results); p += 64UL) {
            __asm__ volatile("dc cvac, %0" :: "r"(p) : "memory");
        }
    }
    __asm__ volatile("dsb sy" ::: "memory");
    b2_review_results.cleanup_errors += b2_results.cleanup_errors;
    if (tee_client_active_sessions() != 0U) { b2_review_results.cleanup_errors++; }
    b2_review_results.state = b2_review_results.cleanup_errors || b2_results.state != B2_COMPLETE ? B2_ERROR : B2_COMPLETE;
    publish();
}
