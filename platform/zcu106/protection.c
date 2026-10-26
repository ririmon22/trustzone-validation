#include "protection.h"
static bool authority_valid(const struct protection_authority *authority)
{
    return authority->hardware_map_verified && authority->master_route_verified &&
        authority->secure_el3_verified && authority->configuration_unlocked &&
        authority->other_masters_quiesced;
}
static bool io_valid(const struct protection_io *io)
{
    return io != NULL && io->read32 != NULL && io->write32 != NULL && io->barrier != NULL;
}
static enum protection_result check_register(const struct protection_io *io, uint64_t address, uint32_t expected)
{
    uint32_t actual;
    if (!io->read32(io->context, address, &actual)) { return PROTECTION_IO_ERROR; }
    return actual == expected ? PROTECTION_OK : PROTECTION_STATE_MISMATCH;
}
static enum protection_result write_checked(const struct protection_io *io, uint64_t address, uint32_t value)
{
    if (!io->write32(io->context, address, value)) { return PROTECTION_IO_ERROR; }
    io->barrier(io->context);
    return check_register(io, address, value);
}
enum protection_result xmpu_program_secure_region(const struct protection_io *io, const struct xmpu_region_policy *policy)
{
    if (!io_valid(io) || policy == NULL) { return PROTECTION_INVALID; }
    if (!authority_valid(&policy->authority)) { return PROTECTION_BLOCKED; }
    if (policy->bank_base < XMPU_DDR_FIRST_BASE ||
        policy->bank_base >= XMPU_DDR_FIRST_BASE + XMPU_DDR_BANK_COUNT * XMPU_DDR_BANK_STRIDE ||
        (policy->bank_base - XMPU_DDR_FIRST_BASE) % XMPU_DDR_BANK_STRIDE != 0U ||
        policy->region >= XMPU_REGION_COUNT || policy->start >= policy->end ||
        policy->end > XMPU_ADDRESS_LIMIT || policy->start % XMPU_DDR_ALIGNMENT != 0U ||
        policy->end % XMPU_DDR_ALIGNMENT != 0U || policy->master_id > XMPU_MASTER_MASK ||
        policy->master_mask > XMPU_MASTER_MASK ||
        (policy->expected_disabled_config & XMPU_REGION_ENABLE) != 0U) { return PROTECTION_INVALID; }
    /* DDRのpoison経路が既に検証済みであることを実値でも確認する。 */
    if ((policy->expected_ctrl & (XMPU_ALIGN_1MB | XMPU_POISON_ADDRESS)) != XMPU_ALIGN_1MB ||
        (policy->expected_poison & XMPU_POISON_ATTRIBUTE) == 0U) { return PROTECTION_BLOCKED; }
    uint64_t region_base = policy->bank_base + XMPU_REGION_FIRST + policy->region * XMPU_REGION_STRIDE;
    enum protection_result result = check_register(io, policy->bank_base + XMPU_CTRL_OFFSET, policy->expected_ctrl);
    if (result != PROTECTION_OK) { return result; }
    result = check_register(io, policy->bank_base + XMPU_POISON_OFFSET, policy->expected_poison);
    if (result != PROTECTION_OK) { return result; }
    result = check_register(io, region_base + XMPU_CONFIG_OFFSET, policy->expected_disabled_config);
    if (result != PROTECTION_OK) { return result; }
    const uint32_t values[] = {
        (uint32_t)(policy->start >> XMPU_PAGE_SHIFT),
        (uint32_t)((policy->end - XMPU_DDR_ALIGNMENT) >> XMPU_PAGE_SHIFT),
        (uint32_t)policy->master_id | ((uint32_t)policy->master_mask << XMPU_MASTER_MASK_SHIFT)
    };
    const unsigned int offsets[] = { XMPU_START_OFFSET, XMPU_END_OFFSET, XMPU_MASTER_OFFSET };
    for (unsigned int i = 0U; i < sizeof(values) / sizeof(values[0]); ++i) {
        result = write_checked(io, region_base + offsets[i], values[i]);
        if (result != PROTECTION_OK) { return result; }
    }
    /* region有効化は全フィールドの書込と読戻しの後に行う。 */
    return write_checked(io, region_base + XMPU_CONFIG_OFFSET,
        XMPU_REGION_ENABLE | XMPU_REGION_READ | XMPU_REGION_WRITE);
}
enum protection_result xppu_program_secure_aperture(const struct protection_io *io, const struct xppu_aperture_policy *policy)
{
    if (!io_valid(io) || policy == NULL) { return PROTECTION_INVALID; }
    if (!authority_valid(&policy->authority) || !policy->parity_encoding_verified) { return PROTECTION_BLOCKED; }
    if (policy->aperture >= XPPU_APERTURE_COUNT ||
        policy->peripheral_base != XPPU_IOP_BASE + policy->aperture * XPPU_APERTURE_ALIGNMENT ||
        (policy->expected_ctrl & XPPU_ENABLE) == 0U ||
        (policy->secure_permission & (XPPU_NONSECURE | XPPU_RESERVED_MASK)) != 0U ||
        (policy->secure_permission & XPPU_PERMISSION_MASK) == 0U ||
        ((policy->secure_permission & XPPU_PERMISSION_MASK) & ~policy->expected_permission) != 0U) { return PROTECTION_INVALID; }
    enum protection_result result = check_register(io, XPPU_BASE + XPPU_CTRL_OFFSET, policy->expected_ctrl);
    if (result != PROTECTION_OK) { return result; }
    for (unsigned int i = 0U; i < XPPU_MASTER_COUNT; ++i) {
        result = check_register(io, XPPU_BASE + XPPU_MASTER_FIRST + i * XPPU_MASTER_STRIDE,
            policy->expected_master_profiles[i]);
        if (result != PROTECTION_OK) { return result; }
    }
    uint64_t address = XPPU_BASE + XPPU_APERTURE_FIRST + policy->aperture * XPPU_APERTURE_STRIDE;
    result = check_register(io, address, policy->expected_permission);
    if (result != PROTECTION_OK) { return result; }
    /* 検証済みparityを含む設定で既存権限を縮小し、全体enableは変更しない。 */
    return write_checked(io, address, policy->secure_permission);
}

enum protection_result xmpu_verify_secure_region(const struct protection_io *io, const struct xmpu_region_policy *policy)
{
    if (!io_valid(io) || policy == NULL) { return PROTECTION_INVALID; }
    if (!authority_valid(&policy->authority)) { return PROTECTION_BLOCKED; }
    if (policy->bank_base < XMPU_DDR_FIRST_BASE ||
        policy->bank_base >= XMPU_DDR_FIRST_BASE + XMPU_DDR_BANK_COUNT * XMPU_DDR_BANK_STRIDE ||
        (policy->bank_base - XMPU_DDR_FIRST_BASE) % XMPU_DDR_BANK_STRIDE != 0U ||
        policy->region >= XMPU_REGION_COUNT || policy->start >= policy->end ||
        policy->start % XMPU_DDR_ALIGNMENT != 0U || policy->end % XMPU_DDR_ALIGNMENT != 0U ||
        policy->end > XMPU_ADDRESS_LIMIT || policy->master_id > XMPU_MASTER_MASK || policy->master_mask > XMPU_MASTER_MASK) { return PROTECTION_INVALID; }
    if ((policy->expected_ctrl & (XMPU_ALIGN_1MB | XMPU_POISON_ADDRESS)) != XMPU_ALIGN_1MB ||
        (policy->expected_poison & XMPU_POISON_ATTRIBUTE) == 0U) { return PROTECTION_BLOCKED; }
    uint64_t region = policy->bank_base + XMPU_REGION_FIRST + policy->region * XMPU_REGION_STRIDE;
    const uint64_t addresses[] = { policy->bank_base + XMPU_CTRL_OFFSET, policy->bank_base + XMPU_POISON_OFFSET,
        region + XMPU_START_OFFSET, region + XMPU_END_OFFSET, region + XMPU_MASTER_OFFSET, region + XMPU_CONFIG_OFFSET };
    const uint32_t values[] = { policy->expected_ctrl, policy->expected_poison,
        (uint32_t)(policy->start >> XMPU_PAGE_SHIFT),
        (uint32_t)((policy->end - XMPU_DDR_ALIGNMENT) >> XMPU_PAGE_SHIFT),
        (uint32_t)policy->master_id | ((uint32_t)policy->master_mask << XMPU_MASTER_MASK_SHIFT),
        XMPU_REGION_ENABLE | XMPU_REGION_READ | XMPU_REGION_WRITE };
    for (unsigned int i = 0U; i < sizeof(values) / sizeof(values[0]); ++i) {
        enum protection_result result = check_register(io, addresses[i], values[i]);
        if (result != PROTECTION_OK) { return result; }
    }
    return PROTECTION_OK;
}
enum protection_result xppu_verify_secure_aperture(const struct protection_io *io, const struct xppu_aperture_policy *policy)
{
    if (!io_valid(io) || policy == NULL) { return PROTECTION_INVALID; }
    if (!authority_valid(&policy->authority) || !policy->parity_encoding_verified) { return PROTECTION_BLOCKED; }
    if (policy->aperture >= XPPU_APERTURE_COUNT ||
        policy->peripheral_base != XPPU_IOP_BASE + policy->aperture * XPPU_APERTURE_ALIGNMENT ||
        (policy->secure_permission & (XPPU_NONSECURE | XPPU_RESERVED_MASK)) != 0U ||
        (policy->expected_ctrl & XPPU_ENABLE) == 0U ||
        (policy->secure_permission & XPPU_PERMISSION_MASK) == 0U ||
        ((policy->secure_permission & XPPU_PERMISSION_MASK) & ~policy->expected_permission) != 0U) { return PROTECTION_INVALID; }
    enum protection_result result = check_register(io, XPPU_BASE + XPPU_CTRL_OFFSET, policy->expected_ctrl);
    if (result != PROTECTION_OK) { return result; }
    for (unsigned int i = 0U; i < XPPU_MASTER_COUNT; ++i) {
        result = check_register(io, XPPU_BASE + XPPU_MASTER_FIRST + i * XPPU_MASTER_STRIDE, policy->expected_master_profiles[i]);
        if (result != PROTECTION_OK) { return result; }
    }
    return check_register(io, XPPU_BASE + XPPU_APERTURE_FIRST + policy->aperture * XPPU_APERTURE_STRIDE, policy->secure_permission);
}
