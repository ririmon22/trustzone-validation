#include "runtime.h"
#define PAGE_TABLE_ENTRIES 512U
#define PAGE_TABLE_ALIGNMENT 4096U
#define DESC_BLOCK 1UL
#define DESC_NORMAL_INDEX (1UL << 2)
#define DESC_ACCESS_FLAG (1UL << 10)
#define DESC_INNER_SHAREABLE (3UL << 8)
#define DESC_UNPRIVILEGED_XN (1UL << 54)
#define DESC_PRIVILEGED_XN (1UL << 53)
#define RAM_BLOCK_BASE 0x40000000UL
#define TCR_IDENTITY_39BIT (25UL | (1UL << 8) | (1UL << 10) | (3UL << 12) | (1UL << 23) | (2UL << 32))
#define MAIR_DEVICE_AND_NORMAL 0xff00UL
#define SCTLR_MMU 1UL
#define SCTLR_DATA_CACHE (1UL << 2)
#define SCTLR_INSTRUCTION_CACHE (1UL << 12)
static uint64_t translation_table[PAGE_TABLE_ENTRIES] __attribute__((aligned(PAGE_TABLE_ALIGNMENT)));
bool platform_prepare_tee_client(void)
{
    uint64_t state;
    __asm__ volatile("mrs %0, sctlr_el1" : "=r"(state));
    if ((state & (SCTLR_MMU | SCTLR_DATA_CACHE)) != 0UL) { return false; }
    translation_table[0] = DESC_BLOCK | DESC_ACCESS_FLAG | DESC_UNPRIVILEGED_XN | DESC_PRIVILEGED_XN;
    translation_table[1] = RAM_BLOCK_BASE | DESC_BLOCK | DESC_NORMAL_INDEX | DESC_ACCESS_FLAG | DESC_INNER_SHAREABLE | DESC_UNPRIVILEGED_XN;
    __asm__ volatile("dsb sy; msr mair_el1, %0; msr tcr_el1, %1; msr ttbr0_el1, %2; isb; tlbi vmalle1; dsb sy; isb"
        :: "r"(MAIR_DEVICE_AND_NORMAL), "r"(TCR_IDENTITY_39BIT), "r"((uint64_t)translation_table) : "memory");
    state |= SCTLR_MMU | SCTLR_DATA_CACHE | SCTLR_INSTRUCTION_CACHE;
    __asm__ volatile("msr sctlr_el1, %0; isb" :: "r"(state) : "memory");
    __asm__ volatile("mrs %0, sctlr_el1" : "=r"(state));
    return (state & (SCTLR_MMU | SCTLR_DATA_CACHE | SCTLR_INSTRUCTION_CACHE)) ==
        (SCTLR_MMU | SCTLR_DATA_CACHE | SCTLR_INSTRUCTION_CACHE);
}
