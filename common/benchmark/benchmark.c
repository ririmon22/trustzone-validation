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
        console_write("OVERHEAD "); console_write(id); console_write(" "); console_uint(i);
        console_write(" "); console_uint(ticks); console_write("\n");
    }
    for (unsigned int i = 0U; i < BENCH_WARMUP; ++i) {
        if (prepare != NULL) { prepare(); }
        operation();
        if (cleanup != NULL) { cleanup(); }
    }
    console_write("BENCH_META "); console_write(id);
    console_write(" counter=CNTPCT_EL0 timer_hz="); console_uint(counter_frequency());
    console_write(" cpu_hz=unknown " BENCH_MEMORY_STATE " irq=masked interval=");
    console_write(interval); console_write(" warmup="); console_uint(BENCH_WARMUP); console_write("\n");
    for (unsigned int i = 0U; i < BENCH_SAMPLES; ++i) {
        if (prepare != NULL) { prepare(); }
        uint64_t start = counter_read();
        operation();
        uint64_t ticks = counter_read() - start;
        if (cleanup != NULL) { cleanup(); }
        console_write("SAMPLE "); console_write(id); console_write(" ");
        console_uint(i); console_write(" "); console_uint(ticks); console_write("\n");
    }
}

void benchmark_run(const char *id, void (*operation)(void), const char *interval)
{
    benchmark_run_scoped(id, NULL, operation, NULL, interval);
}
