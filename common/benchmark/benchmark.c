#include "benchmark.h"
#define BENCH_SCTLR_MMU (UINT64_C(1) << 0)
#define BENCH_SCTLR_DATA_CACHE (UINT64_C(1) << 2)
#define BENCH_SCTLR_INSTRUCTION_CACHE (UINT64_C(1) << 12)
#ifdef B2_BENCH_RAM
volatile struct b2_bench_buffer b2_bench_results __attribute__((aligned(64)));
static void publish_bench(void)
{
    uint64_t sctlr;
    __asm__ volatile("mrs %0, sctlr_el1" : "=r"(sctlr));
    if (sctlr & 4UL) {
        for (uintptr_t p = (uintptr_t)&b2_bench_results;
             p < (uintptr_t)&b2_bench_results + sizeof(b2_bench_results); p += 64UL) {
            __asm__ volatile("dc cvac, %0" :: "r"(p) : "memory");
        }
    }
    __asm__ volatile("dsb sy" ::: "memory");
}
static void bench_text(volatile char *out, unsigned int size, const char *text)
{
    unsigned int i = 0U;
    while (i + 1U < size && text[i]) { out[i] = text[i]; ++i; }
    out[i] = '\0';
}
void b2_bench_begin(void)
{
    volatile uint8_t *bytes = (volatile uint8_t *)&b2_bench_results;
    for (size_t i = 0U; i < sizeof(b2_bench_results); ++i) { bytes[i] = 0U; }
    b2_bench_results.version = 1U; b2_bench_results.bytes = sizeof(b2_bench_results);
    b2_bench_results.state = 1U; b2_bench_results.run_id = debug_boot.run_id;
    b2_bench_results.frequency = counter_frequency();
    uint64_t sctlr, daif;
    __asm__ volatile("mrs %0, sctlr_el1; mrs %1, daif" : "=r"(sctlr), "=r"(daif));
    b2_bench_results.sctlr = sctlr; b2_bench_results.daif = daif;
    b2_bench_results.warmup = BENCH_WARMUP; b2_bench_results.samples = BENCH_SAMPLES;
    b2_bench_results.magic = 0x4232424dU; publish_bench();
}
void b2_bench_end(bool valid)
{
    if (!valid || b2_bench_results.count != B2_BENCH_SERIES) { b2_bench_results.errors++; }
    b2_bench_results.state = b2_bench_results.errors ? 3U : 2U; publish_bench();
}
#endif
/* B2で採取済みの状態を表示する。状態変更や計測区間内の追加処理は行わない。 */
static void report_memory_state_for_benchmark(void)
{
#ifdef B2_BENCH_RAM
    if (b2_bench_results.magic == 0x4232424dU &&
        b2_bench_results.version == 1U && b2_bench_results.state == 1U &&
        b2_bench_results.run_id == debug_boot.run_id)
    {
        uint64_t sctlr = b2_bench_results.sctlr;
        (void)log_printf(" cache=%s mmu=%s icache=%s sctlr_el1=0x%016llx state_source=run-start-SCTLR_EL1",
                         (sctlr & BENCH_SCTLR_DATA_CACHE) != 0UL ? "on" : "off",
                         (sctlr & BENCH_SCTLR_MMU) != 0UL ? "on" : "off",
                         (sctlr & BENCH_SCTLR_INSTRUCTION_CACHE) != 0UL ? "on" : "off",
                         (unsigned long long)sctlr);
        return;
    }
#endif
    (void)log_printf(" cache=UNKNOWN mmu=UNKNOWN icache=UNKNOWN state_source=UNKNOWN");
}
static void empty_operation(void)
{
    __asm__ volatile("" ::: "memory");
}
void benchmark_run_scoped(const char *id, void (*prepare)(void), void (*operation)(void), void (*cleanup)(void), const char *interval)
{
#ifdef B2_BENCH_RAM
    volatile struct b2_bench_series *record = NULL;
    if (b2_bench_results.state == 1U && b2_bench_results.count < B2_BENCH_SERIES) {
        record = &b2_bench_results.series[b2_bench_results.count++];
        bench_text(record->id, sizeof(record->id), id);
        bench_text(record->interval, sizeof(record->interval), interval);
    }
#endif
    void (*volatile baseline)(void) = empty_operation;
    for (unsigned int i = 0U; i < BENCH_SAMPLES; ++i) {
        uint64_t start = counter_read();
        baseline();
        uint64_t ticks = counter_read() - start;
#ifdef B2_BENCH_RAM
        if (record) { record->overhead[i] = ticks; record->overhead_count = i + 1U; }
#endif
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
    (void)log_printf("%s", " cpu_hz=unknown");
    report_memory_state_for_benchmark();
    (void)log_printf("%s", " irq=masked interval=");
    (void)log_printf("%s", interval); (void)log_printf("%s", " warmup="); (void)log_printf("%llu", (unsigned long long)(BENCH_WARMUP)); (void)log_printf("%s", "\n");
    for (unsigned int i = 0U; i < BENCH_SAMPLES; ++i) {
        if (prepare != NULL) { prepare(); }
        uint64_t start = counter_read();
        operation();
        uint64_t ticks = counter_read() - start;
        if (cleanup != NULL) { cleanup(); }
#ifdef B2_BENCH_RAM
        if (record) { record->samples[i] = ticks; record->count = i + 1U; }
#endif
        (void)log_printf("%s", "SAMPLE "); (void)log_printf("%s", id); (void)log_printf("%s", " ");
        (void)log_printf("%llu", (unsigned long long)(i)); (void)log_printf("%s", " "); (void)log_printf("%llu", (unsigned long long)(ticks)); (void)log_printf("%s", "\n");
    }
#ifdef B2_BENCH_RAM
    if (record) { record->complete = 1U; publish_bench(); }
#endif
}

void benchmark_run(const char *id, void (*operation)(void), const char *interval)
{
    benchmark_run_scoped(id, NULL, operation, NULL, interval);
}
