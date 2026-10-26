#include "backends.h"
static bool valid_channel(uint64_t base)
{
    const uint64_t groups[] = { ZDMA_ADMA_BASE, ZDMA_GDMA_BASE };
    for (unsigned int i = 0U; i < sizeof(groups) / sizeof(groups[0]); ++i) {
        if (base >= groups[i] && base < groups[i] + ZDMA_CHANNEL_COUNT * ZDMA_CHANNEL_STRIDE &&
            (base - groups[i]) % ZDMA_CHANNEL_STRIDE == 0U) { return true; }
    }
    return false;
}
static bool in_range(uint64_t address, uint32_t size, uint64_t start, uint64_t end)
{
    return start < end && end <= ZDMA_ADDR_LIMIT && address >= start && address < end && size <= end - address;
}
enum zdma_result zdma_copy(const struct zdma_policy *policy, uint64_t source, uint64_t destination, uint32_t size, struct zdma_completion *completion)
{
    if (completion == NULL) { return ZDMA_INVALID; }
    *completion = (struct zdma_completion){ 0U, 0U, 0U, 0U, false };
    if (policy == NULL || policy->io == NULL || policy->io->read32 == NULL ||
        policy->io->write32 == NULL || policy->io->barrier == NULL) { return ZDMA_INVALID; }
    if (!policy->route_verified || !policy->nonsecure_attribute_verified ||
        !policy->exclusive_channel_verified || !policy->buffers_and_cache_verified) { return ZDMA_BLOCKED; }
    if (!valid_channel(policy->base) || policy->poll_limit == 0U || size == 0U || size > ZDMA_SIZE_MAX ||
        (policy->expected_data_attr & ZDMA_BURST_MASK) != ZDMA_BURST_INCREMENT ||
        !in_range(source, size, policy->source_start, policy->source_end) ||
        !in_range(destination, size, policy->destination_start, policy->destination_end) ||
        (source < destination + size && destination < source + size)) { return ZDMA_INVALID; }
    const struct protection_io *io = policy->io;
    uint32_t ctrl, attributes, flow;
    if (!io->read32(io->context, policy->base + ZDMA_CTRL2, &ctrl) ||
        !io->read32(io->context, policy->base + ZDMA_STATUS, &completion->status) ||
        !io->read32(io->context, policy->base + ZDMA_DATA_ATTR, &attributes) ||
        !io->read32(io->context, policy->base + ZDMA_FLOW_CONTROL, &flow)) { return ZDMA_IO_ERROR; }
    uint32_t state = completion->status & ZDMA_STATE_MASK;
    if ((ctrl & ZDMA_ENABLE) != 0U || (state != ZDMA_STATE_IDLE && state != ZDMA_STATE_ERROR)) { return ZDMA_BUSY; }
    if (flow != 0U || attributes != policy->expected_data_attr) { return ZDMA_BLOCKED; }
    const unsigned int offsets[] = { ZDMA_IDS, ZDMA_ISR, ZDMA_CTRL0,
        ZDMA_SRC_WORD0, ZDMA_SRC_WORD0 + 4U, ZDMA_SRC_WORD0 + 8U, ZDMA_SRC_WORD0 + 12U,
        ZDMA_DST_WORD0, ZDMA_DST_WORD0 + 4U, ZDMA_DST_WORD0 + 8U, ZDMA_DST_WORD0 + 12U };
    const uint32_t values[] = { ZDMA_ISR_MASK, ZDMA_ISR_MASK, ZDMA_SIMPLE_NORMAL_NO_OVERFETCH,
        (uint32_t)source, (uint32_t)(source >> 32U), size, ZDMA_DESCRIPTOR_STOP,
        (uint32_t)destination, (uint32_t)(destination >> 32U), size, ZDMA_DESCRIPTOR_STOP };
    for (unsigned int i = 0U; i < sizeof(values) / sizeof(values[0]); ++i) {
        if (!io->write32(io->context, policy->base + offsets[i], values[i])) { return ZDMA_IO_ERROR; }
        if (i >= 2U) {
            uint32_t readback;
            if (!io->read32(io->context, policy->base + offsets[i], &readback) || readback != values[i]) { return ZDMA_IO_ERROR; }
        }
    }
    io->barrier(io->context);
    /* stale DONEを消せたことを確認してから、実転送を開始する。 */
    if (!io->read32(io->context, policy->base + ZDMA_ISR, &completion->isr)) { return ZDMA_IO_ERROR; }
    if (completion->isr != 0U) { return ZDMA_TRANSFER_ERROR; }
    if (!io->write32(io->context, policy->base + ZDMA_CTRL2, ZDMA_ENABLE)) { return ZDMA_IO_ERROR; }
    completion->started = true;
    io->barrier(io->context);
    for (unsigned int i = 0U; i < policy->poll_limit; ++i) {
        completion->polls = i + 1U;
        if (!io->read32(io->context, policy->base + ZDMA_ISR, &completion->isr) ||
            !io->read32(io->context, policy->base + ZDMA_STATUS, &completion->status)) { return ZDMA_IO_ERROR; }
        if ((completion->isr & ZDMA_ERRORS) != 0U || (completion->status & ZDMA_STATE_MASK) == ZDMA_STATE_ERROR) { return ZDMA_TRANSFER_ERROR; }
        if ((completion->isr & ZDMA_DONE) != 0U && (completion->status & ZDMA_STATE_MASK) == ZDMA_STATE_IDLE) {
            if (!io->read32(io->context, policy->base + ZDMA_TOTAL_BYTE, &completion->bytes_observed)) { return ZDMA_IO_ERROR; }
            io->barrier(io->context);
            return ZDMA_COMPLETE;
        }
    }
    /* timeout後のchannel状態を推測してreset/retryしない。 */
    return ZDMA_TIMEOUT;
}
