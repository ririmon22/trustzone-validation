#ifndef ZCU106_PERFORMANCE_H
#define ZCU106_PERFORMANCE_H
#include "backends.h"
struct hardware_timing {
    uint64_t ticks;
    uint64_t frequency;
    bool valid;
};
struct secure_irq_timing {
    volatile uint64_t entered;
    volatile bool observed;
    volatile bool armed;
    bool handler_binding_verified;
};
enum protection_result zcu106_measure_dma(const struct zdma_policy *policy, uint64_t source, uint64_t destination, uint32_t size,
    bool (*contents_match)(void *), void *context, struct zdma_completion *completion, struct hardware_timing *timing);
void zcu106_record_secure_irq(struct secure_irq_timing *state);
enum protection_result zcu106_measure_secure_irq(const struct gic_sgi_policy *policy, struct secure_irq_timing *state,
    unsigned int poll_limit, struct hardware_timing *timing);
#endif
