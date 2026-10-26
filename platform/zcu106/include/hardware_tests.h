#ifndef ZCU106_HARDWARE_TESTS_H
#define ZCU106_HARDWARE_TESTS_H
#include "protection.h"
#define HARDWARE_TEST_SENTINEL UINT64_C(0x5ac39e17)
#define HARDWARE_TEST_WRITE_VALUE UINT64_C(0xc03541e9)
#define HARDWARE_DMA_BYTES 64U
#define HARDWARE_IRQ_SAMPLES 16U
enum hardware_test_result { HARDWARE_TEST_PASS, HARDWARE_TEST_FAIL, HARDWARE_TEST_BLOCKED };
enum access_result { ACCESS_SUCCEEDED, ACCESS_DENIED, ACCESS_INCONCLUSIVE };
struct access_test_ops {
    void *context;
    bool configuration_verified;
    bool (*allowed_control)(void *context);
    bool (*secure_seed)(void *context, uint64_t value);
    bool (*secure_read)(void *context, uint64_t *value);
    enum access_result (*nonsecure_read)(void *context, uint64_t *value);
    enum access_result (*nonsecure_write)(void *context, uint64_t value);
    bool (*violation_matches)(void *context, bool write);
};
struct dma_test_ops {
    void *context;
    bool route_and_nonsecure_master_verified;
    bool (*allowed_copy)(void *context);
    bool (*secure_seed)(void *context);
    bool (*secure_unchanged)(void *context);
    enum access_result (*forbidden_read)(void *context);
    bool (*read_destination_unchanged)(void *context);
    enum access_result (*forbidden_write)(void *context);
    bool (*violation_matches)(void *context, bool write);
};
struct interrupt_observation {
    unsigned int secure_handlers;
    unsigned int nonsecure_handlers;
    unsigned int unexpected_handlers;
    bool resumed;
    bool context_preserved;
};
struct interrupt_test_ops {
    void *context;
    bool gic_and_routing_verified;
    bool (*deliver)(void *context, bool secure_irq, bool secure_world, struct interrupt_observation *observation);
};
enum hardware_test_result run_access_denial_test(const struct access_test_ops *ops);
enum hardware_test_result run_dma_protection_test(const struct dma_test_ops *ops);
enum hardware_test_result run_interrupt_isolation_test(const struct interrupt_test_ops *ops);
#endif
