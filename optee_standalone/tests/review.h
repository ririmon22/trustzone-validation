#ifndef B2_REVIEW_H
#define B2_REVIEW_H
#include "functional.h"
#define B2_REVIEW_SELECTION 17U
#define B2_PERFORMANCE_SELECTION 18U
#define B2_REVIEW_COUNT 21U
#define B2_REVIEW_CHECKS 16U
struct review_result {
    char id[24];
    uint32_t status, phase, count, overflow;
    char reason[96];
    struct { char name[32]; uint64_t expected, actual; } checks[B2_REVIEW_CHECKS];
};
struct review_buffer {
    uint32_t magic, version, bytes, state;
    uint64_t run_id;
    uint32_t current, cleanup_errors;
    struct review_result items[B2_REVIEW_COUNT];
};
struct sgi_samples {
    uint32_t magic, version, bytes, state;
    uint64_t run_id, frequency;
    uint32_t warmup, count, errors, received_id;
    uint64_t received_mask, entry_mask;
    uint64_t samples[128], overhead[128];
};
extern volatile struct sgi_samples b2_sgi_results;
extern volatile struct review_buffer b2_review_results;
struct b2_stress_record {
    uint32_t version, state;
    uint64_t run_id;
    uint32_t warmup, iterations, completed, status;
    uint32_t before[3], after[3];
};
extern volatile struct b2_stress_record b2_stress_results;
enum test_status b2_probe_core_isolation(void);
void b2_run_review(void);
void b2_run_performance(void);
#endif
