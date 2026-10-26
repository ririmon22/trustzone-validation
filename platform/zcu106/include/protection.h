#ifndef ZCU106_PROTECTION_H
#define ZCU106_PROTECTION_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define XMPU_DDR_FIRST_BASE UINT64_C(0xfd000000)
#define XMPU_DDR_BANK_STRIDE UINT64_C(0x10000)
#define XMPU_DDR_BANK_COUNT 6U
#define XMPU_REGION_COUNT 16U
#define XMPU_REGION_FIRST 0x100U
#define XMPU_REGION_STRIDE 0x10U
#define XMPU_START_OFFSET 0U
#define XMPU_END_OFFSET 4U
#define XMPU_MASTER_OFFSET 8U
#define XMPU_CONFIG_OFFSET 12U
#define XMPU_CTRL_OFFSET 0U
#define XMPU_POISON_OFFSET 0x0cU
#define XMPU_ALIGN_1MB (1U << 3)
#define XMPU_POISON_ADDRESS (1U << 2)
#define XMPU_POISON_ATTRIBUTE (1U << 20)
#define XMPU_REGION_ENABLE 1U
#define XMPU_REGION_READ (1U << 1)
#define XMPU_REGION_WRITE (1U << 2)
#define XMPU_REGION_NONSECURE (1U << 3)
#define XMPU_MASTER_MASK 0x3ffU
#define XMPU_MASTER_MASK_SHIFT 16U
#define XMPU_PAGE_SHIFT 12U
#define XMPU_DDR_ALIGNMENT UINT64_C(0x100000)
#define XMPU_ADDRESS_LIMIT (UINT64_C(1) << 40)
#define XPPU_BASE UINT64_C(0xff980000)
#define XPPU_CTRL_OFFSET 0U
#define XPPU_ENABLE 1U
#define XPPU_APERTURE_FIRST 0x1000U
#define XPPU_APERTURE_COUNT 256U
#define XPPU_APERTURE_STRIDE 4U
#define XPPU_APERTURE_ALIGNMENT UINT64_C(0x10000)
#define XPPU_IOP_BASE UINT64_C(0xff000000)
#define XPPU_PERMISSION_MASK 0xfffffU
#define XPPU_NONSECURE (1U << 27)
#define XPPU_RESERVED_MASK 0x07f00000U
#define XPPU_MASTER_FIRST 0x100U
#define XPPU_MASTER_COUNT 20U
#define XPPU_MASTER_STRIDE 4U
/* 未確認のボード設定ではレジスタ読出しも行わない。 */
enum protection_result {
    PROTECTION_OK, PROTECTION_BLOCKED, PROTECTION_INVALID,
    PROTECTION_IO_ERROR, PROTECTION_STATE_MISMATCH
};
struct protection_io {
    void *context;
    bool (*read32)(void *context, uint64_t address, uint32_t *value);
    bool (*write32)(void *context, uint64_t address, uint32_t value);
    void (*barrier)(void *context);
};
struct protection_authority {
    bool hardware_map_verified;
    bool master_route_verified;
    bool secure_el3_verified;
    bool configuration_unlocked;
    bool other_masters_quiesced;
};
struct xmpu_region_policy {
    struct protection_authority authority;
    uint64_t bank_base;
    unsigned int region;
    uint64_t start;
    uint64_t end;
    uint16_t master_id;
    uint16_t master_mask;
    uint32_t expected_ctrl;
    uint32_t expected_poison;
    uint32_t expected_disabled_config;
};
struct xppu_aperture_policy {
    struct protection_authority authority;
    uint64_t peripheral_base;
    unsigned int aperture;
    uint32_t expected_ctrl;
    uint32_t expected_permission;
    uint32_t secure_permission;
    uint32_t expected_master_profiles[XPPU_MASTER_COUNT];
    bool parity_encoding_verified;
};
extern const struct protection_io zcu106_native_protection_io;
enum protection_result xmpu_program_secure_region(const struct protection_io *io, const struct xmpu_region_policy *policy);
enum protection_result xmpu_verify_secure_region(const struct protection_io *io, const struct xmpu_region_policy *policy);
enum protection_result xppu_verify_secure_aperture(const struct protection_io *io, const struct xppu_aperture_policy *policy);
enum protection_result xppu_program_secure_aperture(const struct protection_io *io, const struct xppu_aperture_policy *policy);
#endif
