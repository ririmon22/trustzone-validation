#ifndef BENCHMARK_H
#define BENCHMARK_H
#include "runtime.h"
#define BENCH_WARMUP 16U
#define BENCH_SAMPLES 128U
void benchmark_run_scoped(const char *id, void (*prepare)(void), void (*operation)(void), void (*cleanup)(void), const char *interval);
void benchmark_run(const char *id, void (*operation)(void), const char *interval);
#ifdef B2_BENCH_RAM
#define B2_BENCH_SERIES 8U
struct b2_bench_series {
    char id[16], interval[112];
    uint32_t count, overhead_count, complete, reserved;
    uint64_t samples[BENCH_SAMPLES], overhead[BENCH_SAMPLES];
};
struct b2_bench_buffer {
    uint32_t magic, version, bytes, state;
    uint64_t run_id, frequency, sctlr, daif;
    uint32_t count, errors, warmup, samples;
    struct b2_bench_series series[B2_BENCH_SERIES];
};
extern volatile struct b2_bench_buffer b2_bench_results;
void b2_bench_begin(void);
void b2_bench_end(bool valid);
#endif
#endif
