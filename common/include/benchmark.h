#ifndef BENCHMARK_H
#define BENCHMARK_H
#include "runtime.h"
#define BENCH_WARMUP 16U
#define BENCH_SAMPLES 128U
void benchmark_run_scoped(const char *id, void (*prepare)(void), void (*operation)(void), void (*cleanup)(void), const char *interval);
void benchmark_run(const char *id, void (*operation)(void), const char *interval);
#endif
