#include "benchmark.h"
#ifndef BENCH_MEMORY_STATE
#define BENCH_MEMORY_STATE "cache=off mmu=off"
#endif
static void empty_operation(void)
{
    __asm__ volatile("" ::: "memory");
}
void benchmark_run_scoped(const char *id, void (*prepare)(void), void (*operation)(void), void (*cleanup)(void), const char *interval)
{
    void (*volatile baseline)(void) = empty_operation;
    for (unsigned int i = 0U; i < BENCH_SAMPLES; ++i) {
        uint64_t start = counter_read();
        baseline();
        uint64_t ticks = counter_read() - start;
        (void)log_printf("%s", "OVERHEAD "); (void)log_printf("%s", id); (void)log_printf("%s", " "); (void)log_printf("%llu", (unsigned long long)(i));
        (void)log_printf("%s", " "); (void)log_printf("%llu", (unsigned long long)(ticks)); (void)log_printf("%s", "\n");
    }
    for (unsigned int i = 0U; i < BENCH_WARMUP; ++i) {
        if (prepare != NULL) { prepare(); }
        operation();
        if (cleanup != NULL) { cleanup(); }
    }
    (void)log_printf("%s", "BENCH_META "); (void)log_printf("%s", id);
    (void)log_printf("%s", " counter=CNTPCT_EL0 timer_hz="); (void)log_printf("%llu", (unsigned long long)(counter_frequency()));
    (void)log_printf("%s", " cpu_hz=unknown " BENCH_MEMORY_STATE " irq=masked interval=");
    (void)log_printf("%s", interval); (void)log_printf("%s", " warmup="); (void)log_printf("%llu", (unsigned long long)(BENCH_WARMUP)); (void)log_printf("%s", "\n");
    for (unsigned int i = 0U; i < BENCH_SAMPLES; ++i) {
        if (prepare != NULL) { prepare(); }
        uint64_t start = counter_read();
        operation();
        uint64_t ticks = counter_read() - start;
        if (cleanup != NULL) { cleanup(); }
        (void)log_printf("%s", "SAMPLE "); (void)log_printf("%s", id); (void)log_printf("%s", " ");
        (void)log_printf("%llu", (unsigned long long)(i)); (void)log_printf("%s", " "); (void)log_printf("%llu", (unsigned long long)(ticks)); (void)log_printf("%s", "\n");
    }
}

void benchmark_run(const char *id, void (*operation)(void), const char *interval)
{
    benchmark_run_scoped(id, NULL, operation, NULL, interval);
}
