#include "test_runner.h"
static unsigned int counts[TEST_STATUS_COUNT];
static const char *const names[] = { "PASS", "FAIL", "SKIP", "BLOCKED", "NOT_RUN" };
void test_begin(const char *configuration)
{
    for (unsigned int i = 0U; i < TEST_STATUS_COUNT; ++i) { counts[i] = 0U; }
    (void)log_printf("%s", "RUN platform=" PLATFORM_NAME " configuration=");
    (void)log_printf("%s", configuration); (void)log_printf("%s", "\n");
}
void test_report(const char *id, enum test_status status, const char *detail, uint64_t ticks)
{
    if ((unsigned int)status >= TEST_STATUS_COUNT) { (void)log_printf("%s", "FATAL invalid status\n"); platform_halt(); }
    counts[status]++;
    (void)log_printf("%s", "RESULT "); (void)log_printf("%s", id); (void)log_printf("%s", " "); (void)log_printf("%s", names[status]);
    (void)log_printf("%s", " ticks="); (void)log_printf("%llu", (unsigned long long)(ticks));
    (void)log_printf("%s", " detail="); (void)log_printf("%s", detail); (void)log_printf("%s", "\n");
}
void test_finish(void)
{
    (void)log_printf("%s", "SUMMARY");
    for (unsigned int i = 0U; i < TEST_STATUS_COUNT; ++i) {
        (void)log_printf("%s", " "); (void)log_printf("%s", names[i]); (void)log_printf("%s", "="); (void)log_printf("%llu", (unsigned long long)(counts[i]));
    }
    (void)log_printf("%s", "\nEND\n");
    log_set_state(RAM_LOG_COMPLETE);
    debug_stage(80U, 0U);
}
