#include "backends.h"
extern void console_write(const char *text);
extern _Noreturn void platform_halt(void);
static const struct gic_sgi_policy *active_policy;
static volatile struct interrupt_observation *active_observation;
static bool handler_secure_world;
enum protection_result zcu106_bind_interrupt_handler(const struct gic_sgi_policy *policy, volatile struct interrupt_observation *observation, bool secure_world)
{
    if (policy == NULL || observation == NULL) { return PROTECTION_INVALID; }
    if (!policy->vectors_and_routing_verified || !policy->single_cpu_exclusive_verified ||
        !policy->secure_configuration_verified || policy->io == NULL) { return PROTECTION_BLOCKED; }
    active_policy = policy;
    active_observation = observation;
    handler_secure_world = secure_world;
    return PROTECTION_OK;
}
void zcu106_irq_dispatch(void)
{
    if (active_policy == NULL || active_observation == NULL) {
        console_write("FATAL unbound interrupt handler\n");
        platform_halt();
    }
    uint32_t token;
    if (gic_acknowledge(active_policy, &token) != PROTECTION_OK) {
        active_observation->unexpected_handlers++;
        return;
    }
    unsigned int expected = handler_secure_world ? active_policy->secure_sgi : active_policy->nonsecure_sgi;
    if ((token & GIC_ID_MASK) != expected) { active_observation->unexpected_handlers++; }
    else if (handler_secure_world) { active_observation->secure_handlers++; }
    else { active_observation->nonsecure_handlers++; }
    if (gic_end_interrupt(active_policy, token) != PROTECTION_OK) { active_observation->unexpected_handlers++; }
    /* resumed/context_preservedは復帰した呼出元が検査し、handlerでは成功にしない。 */
}
