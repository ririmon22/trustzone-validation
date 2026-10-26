#include "test_runner.h"
static unsigned int counts[TEST_STATUS_COUNT];
static const char *const names[] = { "PASS", "FAIL", "SKIP", "BLOCKED", "NOT_RUN" };
void test_begin(const char *configuration)
{
    for (unsigned int i = 0U; i < TEST_STATUS_COUNT; ++i) { counts[i] = 0U; }
    console_write("RUN platform=" PLATFORM_NAME " configuration=");
    console_write(configuration); console_write("\n");
}
void test_report(const char *id, enum test_status status, const char *detail, uint64_t ticks)
{
    if ((unsigned int)status >= TEST_STATUS_COUNT) { console_write("FATAL invalid status\n"); platform_halt(); }
    counts[status]++;
    console_write("RESULT "); console_write(id); console_write(" "); console_write(names[status]);
    console_write(" ticks="); console_uint(ticks);
    console_write(" detail="); console_write(detail); console_write("\n");
}
void test_finish(void)
{
    console_write("SUMMARY");
    for (unsigned int i = 0U; i < TEST_STATUS_COUNT; ++i) {
        console_write(" "); console_write(names[i]); console_write("="); console_uint(counts[i]);
    }
    console_write("\nEND\n");
}
