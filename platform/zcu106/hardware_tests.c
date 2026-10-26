#include "hardware_tests.h"
enum hardware_test_result run_access_denial_test(const struct access_test_ops *ops)
{
    if (ops == NULL || !ops->configuration_verified || ops->allowed_control == NULL ||
        ops->secure_seed == NULL || ops->secure_read == NULL || ops->nonsecure_read == NULL ||
        ops->nonsecure_write == NULL || ops->violation_matches == NULL) { return HARDWARE_TEST_BLOCKED; }
    uint64_t value;
    if (!ops->allowed_control(ops->context) || !ops->secure_seed(ops->context, HARDWARE_TEST_SENTINEL) ||
        !ops->secure_read(ops->context, &value) || value != HARDWARE_TEST_SENTINEL) { return HARDWARE_TEST_FAIL; }
    enum access_result read = ops->nonsecure_read(ops->context, &value);
    if (read == ACCESS_INCONCLUSIVE) { return HARDWARE_TEST_BLOCKED; }
    if (read != ACCESS_DENIED || !ops->violation_matches(ops->context, false)) { return HARDWARE_TEST_FAIL; }
    if (!ops->secure_read(ops->context, &value) || value != HARDWARE_TEST_SENTINEL) { return HARDWARE_TEST_FAIL; }
    enum access_result write = ops->nonsecure_write(ops->context, HARDWARE_TEST_WRITE_VALUE);
    if (write == ACCESS_INCONCLUSIVE) { return HARDWARE_TEST_BLOCKED; }
    if (write != ACCESS_DENIED || !ops->violation_matches(ops->context, true) ||
        !ops->secure_read(ops->context, &value) || value != HARDWARE_TEST_SENTINEL ||
        !ops->allowed_control(ops->context)) { return HARDWARE_TEST_FAIL; }
    return HARDWARE_TEST_PASS;
}
enum hardware_test_result run_dma_protection_test(const struct dma_test_ops *ops)
{
    if (ops == NULL || !ops->route_and_nonsecure_master_verified || ops->allowed_copy == NULL ||
        ops->secure_seed == NULL || ops->secure_unchanged == NULL || ops->forbidden_read == NULL ||
        ops->read_destination_unchanged == NULL || ops->forbidden_write == NULL ||
        ops->violation_matches == NULL) { return HARDWARE_TEST_BLOCKED; }
    if (!ops->allowed_copy(ops->context) || !ops->secure_seed(ops->context)) { return HARDWARE_TEST_FAIL; }
    enum access_result read = ops->forbidden_read(ops->context);
    if (read == ACCESS_INCONCLUSIVE) { return HARDWARE_TEST_BLOCKED; }
    if (read != ACCESS_DENIED || !ops->violation_matches(ops->context, false) ||
        !ops->read_destination_unchanged(ops->context) || !ops->secure_unchanged(ops->context)) { return HARDWARE_TEST_FAIL; }
    enum access_result write = ops->forbidden_write(ops->context);
    if (write == ACCESS_INCONCLUSIVE) { return HARDWARE_TEST_BLOCKED; }
    if (write != ACCESS_DENIED || !ops->violation_matches(ops->context, true) ||
        !ops->secure_unchanged(ops->context) || !ops->allowed_copy(ops->context)) { return HARDWARE_TEST_FAIL; }
    return HARDWARE_TEST_PASS;
}
enum hardware_test_result run_interrupt_isolation_test(const struct interrupt_test_ops *ops)
{
    if (ops == NULL || !ops->gic_and_routing_verified || ops->deliver == NULL) { return HARDWARE_TEST_BLOCKED; }
    for (unsigned int sample = 0U; sample < HARDWARE_IRQ_SAMPLES; ++sample) {
        for (unsigned int world = 0U; world < 2U; ++world) {
            for (unsigned int group = 0U; group < 2U; ++group) {
                bool secure_irq = group != 0U;
                struct interrupt_observation observation = { 0U, 0U, 0U, false, false };
                if (!ops->deliver(ops->context, secure_irq, world != 0U, &observation) ||
                    !observation.resumed || !observation.context_preserved || observation.unexpected_handlers != 0U ||
                    observation.secure_handlers != (secure_irq ? 1U : 0U) ||
                    observation.nonsecure_handlers != (secure_irq ? 0U : 1U)) { return HARDWARE_TEST_FAIL; }
            }
        }
    }
    return HARDWARE_TEST_PASS;
}
