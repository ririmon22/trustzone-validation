#include "backends.h"
static bool valid_base(uint64_t base)
{
    return base == XPPU_BASE || (base >= XMPU_DDR_FIRST_BASE &&
        base < XMPU_DDR_FIRST_BASE + XMPU_DDR_BANK_COUNT * XMPU_DDR_BANK_STRIDE &&
        (base - XMPU_DDR_FIRST_BASE) % XMPU_DDR_BANK_STRIDE == 0U);
}
static bool valid_probe(const struct xpu_probe *probe)
{
    return probe != NULL && probe->io != NULL && probe->io->read32 != NULL &&
        probe->io->write32 != NULL && probe->io->barrier != NULL && valid_base(probe->base);
}
enum protection_result xpu_probe_begin(struct xpu_probe *probe)
{
    if (!valid_probe(probe)) { return PROTECTION_INVALID; }
    probe->armed = false;
    if (!probe->exclusive_verified) { return PROTECTION_BLOCKED; }
    uint32_t status;
    uint32_t mask = probe->base == XPPU_BASE ? XPPU_ISR_MASK : XMPU_ISR_MASK;
    if (!probe->io->read32(probe->io->context, probe->base + XPU_ISR, &status)) { return PROTECTION_IO_ERROR; }
    if ((status & ~mask) != 0U) { return PROTECTION_STATE_MISMATCH; }
    /* 前回の違反を消してから、新しい一回のアクセスだけを帰属させる。 */
    if (status != 0U && !probe->io->write32(probe->io->context, probe->base + XPU_ISR, status)) { return PROTECTION_IO_ERROR; }
    probe->io->barrier(probe->io->context);
    if (!probe->io->read32(probe->io->context, probe->base + XPU_ISR, &status)) { return PROTECTION_IO_ERROR; }
    if (status != 0U) { return PROTECTION_STATE_MISMATCH; }
    probe->armed = true;
    return PROTECTION_OK;
}
enum protection_result xpu_probe_capture(struct xpu_probe *probe, struct xpu_capture *capture)
{
    if (!valid_probe(probe) || capture == NULL) { return PROTECTION_INVALID; }
    if (!probe->exclusive_verified || !probe->armed) { return PROTECTION_BLOCKED; }
    probe->armed = false;
    uint32_t after;
    probe->io->barrier(probe->io->context);
    if (!probe->io->read32(probe->io->context, probe->base + XPU_ISR, &capture->status) ||
        !probe->io->read32(probe->io->context, probe->base + XPU_ERR_ADDRESS, &capture->address_page) ||
        !probe->io->read32(probe->io->context, probe->base + XPU_ERR_MASTER, &capture->master) ||
        !probe->io->read32(probe->io->context, probe->base + XPU_ISR, &after)) { return PROTECTION_IO_ERROR; }
    return capture->status == after ? PROTECTION_OK : PROTECTION_STATE_MISMATCH;
}
bool xpu_capture_matches(const struct xpu_capture *capture, uint64_t address, uint16_t master, uint32_t allowed_status)
{
    if (capture == NULL || master > XMPU_MASTER_MASK || allowed_status == 0U ||
        address >= XMPU_ADDRESS_LIMIT) { return false; }
    /* ERR_STATUS1はページ単位の証跡で、バイト単位のFARを代替しない。 */
    return capture->status != 0U && (capture->status & ~allowed_status) == 0U &&
        capture->address_page == (uint32_t)(address >> XMPU_PAGE_SHIFT) && capture->master == master;
}
