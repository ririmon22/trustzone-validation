#ifndef TEST_RUNNER_H
#define TEST_RUNNER_H
#include "runtime.h"
enum test_status { TEST_PASS, TEST_FAIL, TEST_SKIP, TEST_BLOCKED, TEST_NOT_RUN, TEST_STATUS_COUNT };
void test_begin(const char *configuration);
void test_report(const char *id, enum test_status status, const char *detail, uint64_t ticks);
void test_finish(void);
#endif
