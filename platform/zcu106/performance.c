#include "include/performance.h"
/* Generic counterを用い、CPU周波数と混同しない。 */
static uint64_t read_counter(void)
{
    uint64_t value;
    __asm__ volatile("isb; mrs %0, cntvct_el0" : "=r"(value) :: "memory");
    return value;
}
static uint64_t read_frequency(void)
{
    uint64_t value;
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(value));
    return value;
}
enum protection_result zcu106_measure_dma(const struct zdma_policy *policy, uint64_t source, uint64_t destination, uint32_t size,
    bool (*contents_match)(void *), void *context, struct zdma_completion *completion, struct hardware_timing *timing)
{
    if (timing == NULL || completion == NULL || contents_match == NULL) { return PROTECTION_INVALID; }
    *timing = (struct hardware_timing){ 0U, 0U, false };
    uint64_t start = read_counter();
    enum zdma_result result = zdma_copy(policy, source, destination, size, completion);
    uint64_t end = read_counter();
    if (result == ZDMA_BLOCKED) { return PROTECTION_BLOCKED; }
    if (result != ZDMA_COMPLETE || !completion->started || !contents_match(context)) { return PROTECTION_IO_ERROR; }
    timing->frequency = read_frequency();
    if (timing->frequency == 0U || end < start) { return PROTECTION_INVALID; }
    timing->ticks = end - start;
    timing->valid = true;
    return PROTECTION_OK;
}
/* 確認済みSecure handlerの先頭へ明示的に接続する。自動でvectorを書き換えない。 */
void zcu106_record_secure_irq(struct secure_irq_timing *state)
{
    if (state == NULL || !state->handler_binding_verified || !state->armed || state->observed) { return; }
    state->entered = read_counter();
    __asm__ volatile("dmb ishst" ::: "memory");
    state->observed = true;
}
enum protection_result zcu106_measure_secure_irq(const struct gic_sgi_policy *policy, struct secure_irq_timing *state,
    unsigned int poll_limit, struct hardware_timing *timing)
{
    if (timing == NULL || state == NULL || poll_limit == 0U) { return PROTECTION_INVALID; }
    *timing = (struct hardware_timing){ 0U, 0U, false };
    if (!state->handler_binding_verified || policy == NULL || !policy->secure_configuration_verified ||
        !policy->vectors_and_routing_verified || !policy->single_cpu_exclusive_verified || policy->io == NULL ||
        policy->io->read32 == NULL || policy->secure_sgi >= GIC_SGI_COUNT) { return PROTECTION_BLOCKED; }
    state->armed = false;
    uint32_t pending, active;
    if (!policy->io->read32(policy->io->context, GICD_BASE + GICD_ISPENDR0, &pending) ||
        !policy->io->read32(policy->io->context, GICD_BASE + GICD_ISACTIVER0, &active)) { return PROTECTION_IO_ERROR; }
    if (((pending | active) & (1U << policy->secure_sgi)) != 0U) { return PROTECTION_STATE_MISMATCH; }
    state->observed = false;
    __asm__ volatile("dsb sy" ::: "memory");
    uint64_t start = read_counter();
    state->armed = true;
    enum protection_result result = gic_send_sgi(policy, true);
    if (result != PROTECTION_OK) { state->armed = false; return result; }
    for (unsigned int index = 0U; index < poll_limit; ++index) {
        if (!state->observed) { continue; }
        __asm__ volatile("dmb ishld" ::: "memory");
        state->armed = false;
        timing->frequency = read_frequency();
        if (state->entered < start || timing->frequency == 0U) { return PROTECTION_INVALID; }
        timing->ticks = state->entered - start;
        timing->valid = true;
        return PROTECTION_OK;
    }
    state->armed = false;
    return PROTECTION_IO_ERROR;
}
