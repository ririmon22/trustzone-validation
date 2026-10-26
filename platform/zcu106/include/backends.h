#ifndef ZCU106_BACKENDS_H
#define ZCU106_BACKENDS_H
#include "hardware_tests.h"
#define XPU_ERR_ADDRESS 0x04U
#define XPU_ERR_MASTER 0x08U
#define XPU_ISR 0x10U
#define XMPU_ISR_MASK 0x0fU
#define XPPU_ISR_MASK 0xefU
#define XMPU_READ_VIOLATION 0x02U
#define XMPU_WRITE_VIOLATION 0x04U
#define XMPU_SECURITY_VIOLATION 0x08U
#define XPPU_PERMISSION_VIOLATION 0x20U
#define XPPU_SECURITY_VIOLATION 0x40U
#define ZDMA_ADMA_BASE UINT64_C(0xffa80000)
#define ZDMA_GDMA_BASE UINT64_C(0xfd500000)
#define ZDMA_CHANNEL_STRIDE UINT64_C(0x10000)
#define ZDMA_CHANNEL_COUNT 8U
#define ZDMA_ISR 0x100U
#define ZDMA_IMR 0x104U
#define ZDMA_IDS 0x10cU
#define ZDMA_CTRL0 0x110U
#define ZDMA_FLOW_CONTROL 0x118U
#define ZDMA_STATUS 0x11cU
#define ZDMA_DATA_ATTR 0x120U
#define ZDMA_SRC_WORD0 0x128U
#define ZDMA_DST_WORD0 0x138U
#define ZDMA_TOTAL_BYTE 0x188U
#define ZDMA_CTRL2 0x200U
#define ZDMA_ENABLE 1U
#define ZDMA_DONE 0x400U
#define ZDMA_ERRORS 0xbf9U
#define ZDMA_READ_ERROR 0x100U
#define ZDMA_WRITE_ERROR 0x200U
#define ZDMA_DATA_ERRORS (ZDMA_READ_ERROR | ZDMA_WRITE_ERROR)
#define ZDMA_ISR_MASK 0xfffU
#define ZDMA_BURST_MASK 0x0c003000U
#define ZDMA_BURST_INCREMENT 0x04001000U
#define ZDMA_WORD_STRIDE 4U
#define ZDMA_SIZE_MAX 0x3fffffffU
#define ZDMA_STATE_MASK 3U
#define ZDMA_STATE_IDLE 0U
#define ZDMA_STATE_ERROR 3U
#define ZDMA_ADDR_LIMIT (UINT64_C(1) << 49)
#define ZDMA_SIMPLE_NORMAL_NO_OVERFETCH 0U
#define ZDMA_DESCRIPTOR_STOP 0x10U
#define GICD_BASE UINT64_C(0xf9010000)
#define GICC_BASE UINT64_C(0xf9020000)
#define GICD_CTLR 0U
#define GICD_TYPER 4U
#define GICD_IIDR 8U
#define GICD_IGROUPR0 0x80U
#define GICD_ISPENDR0 0x200U
#define GICD_ISACTIVER0 0x300U
#define GICD_IPRIORITYR0 0x400U
#define GICD_SGIR 0xf00U
#define GICC_CTLR 0U
#define GICC_PMR 4U
#define GICC_IAR 0x0cU
#define GICC_EOIR 0x10U
#define GIC_SECURITY_EXTENSIONS (1U << 10)
#define GIC_ENABLE_BOTH 3U
#define GIC_FIQ_ENABLE (1U << 3)
#define GIC_ACK_CONTROL (1U << 2)
#define GIC_EOI_MODES ((1U << 9) | (1U << 10))
#define GIC_SGI_COUNT 16U
#define GIC_SGI_SELF (2U << 24)
#define GIC_SGI_NONSECURE (1U << 15)
#define GIC_PRIORITY_MASK 0xf8U
#define GIC_PRIORITY_RESERVED 7U
#define GIC_ID_MASK 0x3ffU
#define GIC_SPECIAL_FIRST 1020U
struct xpu_capture {
    uint32_t status;
    uint32_t address_page;
    uint32_t master;
};
struct xpu_probe {
    const struct protection_io *io;
    uint64_t base;
    bool exclusive_verified;
    bool armed;
};
enum protection_result xpu_probe_begin(struct xpu_probe *probe);
enum protection_result xpu_probe_capture(struct xpu_probe *probe, struct xpu_capture *capture);
bool xpu_capture_matches(const struct xpu_capture *capture, uint64_t address, uint16_t master, uint32_t allowed_status);
struct zdma_policy {
    const struct protection_io *io;
    uint64_t base;
    bool route_verified;
    bool nonsecure_attribute_verified;
    bool exclusive_channel_verified;
    bool buffers_and_cache_verified;
    uint32_t expected_data_attr;
    uint64_t source_start, source_end;
    uint64_t destination_start, destination_end;
    unsigned int poll_limit;
};
struct zdma_completion {
    uint32_t status, isr, bytes_observed;
    unsigned int polls;
    bool started;
};
enum zdma_result { ZDMA_COMPLETE, ZDMA_BLOCKED, ZDMA_INVALID, ZDMA_BUSY, ZDMA_IO_ERROR, ZDMA_TRANSFER_ERROR, ZDMA_TIMEOUT };
enum zdma_result zdma_copy(const struct zdma_policy *policy, uint64_t source, uint64_t destination, uint32_t size, struct zdma_completion *completion);
struct gic_sgi_policy {
    const struct protection_io *io;
    bool secure_configuration_verified;
    bool vectors_and_routing_verified;
    bool single_cpu_exclusive_verified;
    uint32_t expected_iidr;
    uint32_t expected_group;
    uint32_t expected_distributor_control;
    uint32_t expected_cpu_control;
    unsigned int secure_sgi, nonsecure_sgi;
    uint8_t secure_priority, nonsecure_priority;
};
enum protection_result gic_prepare_sgis(const struct gic_sgi_policy *policy);
enum protection_result gic_send_sgi(const struct gic_sgi_policy *policy, bool secure);
enum protection_result gic_acknowledge(const struct gic_sgi_policy *policy, uint32_t *token);
enum protection_result gic_end_interrupt(const struct gic_sgi_policy *policy, uint32_t token);
struct dma_validation_backend {
    struct zdma_policy channel;
    struct xpu_probe protection;
    uint16_t master_id;
    uint64_t normal_source, normal_destination, secure_buffer;
    uint32_t size;
    void *memory_context;
    bool (*prepare_normal)(void *context);
    bool (*normal_copy_matches)(void *context);
    bool (*seed_secure)(void *context);
    bool (*secure_unchanged)(void *context);
    bool (*read_destination_unchanged)(void *context);
    struct zdma_completion last_completion;
    struct xpu_capture last_violation;
    bool last_violation_valid;
};
enum protection_result zcu106_make_dma_test_ops(struct dma_validation_backend *backend, struct dma_test_ops *ops);
enum protection_result zcu106_bind_interrupt_handler(const struct gic_sgi_policy *policy, volatile struct interrupt_observation *observation, bool secure_world);
void zcu106_irq_dispatch(void);
extern void zcu106_test_vectors(void);
#endif
