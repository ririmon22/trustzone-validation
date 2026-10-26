#include "backends.h"
static bool verified(const struct gic_sgi_policy *policy)
{
    return policy != NULL && policy->io != NULL && policy->io->read32 != NULL &&
        policy->io->write32 != NULL && policy->io->barrier != NULL &&
        policy->secure_configuration_verified && policy->vectors_and_routing_verified &&
        policy->single_cpu_exclusive_verified;
}
static bool read_equal(const struct protection_io *io, uint64_t address, uint32_t expected)
{
    uint32_t value;
    return io->read32(io->context, address, &value) && value == expected;
}
static bool write_equal(const struct protection_io *io, uint64_t address, uint32_t value)
{
    if (!io->write32(io->context, address, value)) { return false; }
    io->barrier(io->context);
    return read_equal(io, address, value);
}
enum protection_result gic_prepare_sgis(const struct gic_sgi_policy *policy)
{
    if (!verified(policy)) { return PROTECTION_BLOCKED; }
    if (policy->secure_sgi >= GIC_SGI_COUNT || policy->nonsecure_sgi >= GIC_SGI_COUNT ||
        policy->secure_sgi == policy->nonsecure_sgi ||
        policy->secure_priority >= policy->nonsecure_priority ||
        (policy->secure_priority & GIC_PRIORITY_RESERVED) != 0U || (policy->nonsecure_priority & GIC_PRIORITY_RESERVED) != 0U ||
        (policy->expected_cpu_control & (GIC_ACK_CONTROL | GIC_EOI_MODES)) != 0U) { return PROTECTION_INVALID; }
    const struct protection_io *io = policy->io;
    uint32_t type, pending, active;
    if (!io->read32(io->context, GICD_BASE + GICD_TYPER, &type) ||
        !io->read32(io->context, GICD_BASE + GICD_ISPENDR0, &pending) ||
        !io->read32(io->context, GICD_BASE + GICD_ISACTIVER0, &active)) { return PROTECTION_IO_ERROR; }
    uint32_t mask = (1U << policy->secure_sgi) | (1U << policy->nonsecure_sgi);
    if ((type & GIC_SECURITY_EXTENSIONS) == 0U || ((pending | active) & mask) != 0U ||
        !read_equal(io, GICD_BASE + GICD_IIDR, policy->expected_iidr) ||
        !read_equal(io, GICD_BASE + GICD_IGROUPR0, policy->expected_group) ||
        !read_equal(io, GICD_BASE + GICD_CTLR, policy->expected_distributor_control) ||
        !read_equal(io, GICC_BASE + GICC_CTLR, policy->expected_cpu_control)) { return PROTECTION_STATE_MISMATCH; }
    uint32_t group = (policy->expected_group & ~(1U << policy->secure_sgi)) | (1U << policy->nonsecure_sgi);
    if (!write_equal(io, GICD_BASE + GICD_IGROUPR0, group)) { return PROTECTION_IO_ERROR; }
    const unsigned int ids[] = { policy->secure_sgi, policy->nonsecure_sgi };
    const uint8_t priorities[] = { policy->secure_priority, policy->nonsecure_priority };
    for (unsigned int i = 0U; i < 2U; ++i) {
        uint64_t address = GICD_BASE + GICD_IPRIORITYR0 + (ids[i] / 4U) * 4U;
        unsigned int shift = (ids[i] % 4U) * 8U;
        uint32_t value;
        if (!io->read32(io->context, address, &value)) { return PROTECTION_IO_ERROR; }
        value = (value & ~(0xffU << shift)) | ((uint32_t)priorities[i] << shift);
        if (!write_equal(io, address, value)) { return PROTECTION_IO_ERROR; }
    }
    if (!write_equal(io, GICC_BASE + GICC_PMR, GIC_PRIORITY_MASK) ||
        !write_equal(io, GICD_BASE + GICD_CTLR, policy->expected_distributor_control | GIC_ENABLE_BOTH) ||
        !write_equal(io, GICC_BASE + GICC_CTLR, policy->expected_cpu_control | GIC_ENABLE_BOTH | GIC_FIQ_ENABLE)) { return PROTECTION_IO_ERROR; }
    return PROTECTION_OK;
}
enum protection_result gic_send_sgi(const struct gic_sgi_policy *policy, bool secure)
{
    if (!verified(policy)) { return PROTECTION_BLOCKED; }
    unsigned int id = secure ? policy->secure_sgi : policy->nonsecure_sgi;
    if (id >= GIC_SGI_COUNT) { return PROTECTION_INVALID; }
    uint32_t group;
    if (!policy->io->read32(policy->io->context, GICD_BASE + GICD_IGROUPR0, &group)) { return PROTECTION_IO_ERROR; }
    if (((group >> id) & 1U) != (secure ? 0U : 1U)) { return PROTECTION_STATE_MISMATCH; }
    uint32_t request = GIC_SGI_SELF | (secure ? 0U : GIC_SGI_NONSECURE) | id;
    policy->io->barrier(policy->io->context);
    if (!policy->io->write32(policy->io->context, GICD_BASE + GICD_SGIR, request)) { return PROTECTION_IO_ERROR; }
    policy->io->barrier(policy->io->context);
    return PROTECTION_OK;
}
enum protection_result gic_acknowledge(const struct gic_sgi_policy *policy, uint32_t *token)
{
    if (!verified(policy)) { return PROTECTION_BLOCKED; }
    if (token == NULL) { return PROTECTION_INVALID; }
    if (!policy->io->read32(policy->io->context, GICC_BASE + GICC_IAR, token)) { return PROTECTION_IO_ERROR; }
    return (*token & GIC_ID_MASK) < GIC_SPECIAL_FIRST ? PROTECTION_OK : PROTECTION_STATE_MISMATCH;
}
enum protection_result gic_end_interrupt(const struct gic_sgi_policy *policy, uint32_t token)
{
    if (!verified(policy)) { return PROTECTION_BLOCKED; }
    if ((token & GIC_ID_MASK) >= GIC_SPECIAL_FIRST) { return PROTECTION_INVALID; }
    /* SGI送信CPUの情報を含むIAR token全体をEOIRへ戻す。 */
    policy->io->barrier(policy->io->context);
    if (!policy->io->write32(policy->io->context, GICC_BASE + GICC_EOIR, token)) { return PROTECTION_IO_ERROR; }
    policy->io->barrier(policy->io->context);
    return PROTECTION_OK;
}
