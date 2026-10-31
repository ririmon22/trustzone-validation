#include "client_mmu.h"
#include "memory_map.h"
#include <stddef.h>
#ifndef B2_SIGNED_TA_PROFILE
#define B2_SIGNED_TA_PROFILE 0U
#endif
#define DESCRIPTOR_PAGE 3UL
#define DESCRIPTOR_TABLE 3UL
#define DESCRIPTOR_ACCESS_FLAG (1UL << 10)
#define DESCRIPTOR_READ_ONLY (1UL << 7)
#define DESCRIPTOR_NORMAL_INDEX (1UL << 2)
#define DESCRIPTOR_INNER_SHAREABLE (3UL << 8)
#define DESCRIPTOR_PRIVILEGED_XN (1UL << 53)
#define DESCRIPTOR_USER_XN (1UL << 54)
#define PAGE_INDEX_SHIFT 12U
#define BLOCK_INDEX_SHIFT 21U
#define ROOT_INDEX_SHIFT 30U
#define TABLE_INDEX_MASK (B2_TABLE_ENTRIES - 1U)
#define SCTLR_MMU (1UL << 0)
#define SCTLR_DATA_CACHE (1UL << 2)
#define SCTLR_INSTRUCTION_CACHE (1UL << 12)
#define SCTLR_TRANSLATION_CACHE (SCTLR_MMU | SCTLR_DATA_CACHE | SCTLR_INSTRUCTION_CACHE)
#define TCR_IDENTITY_39BIT (25UL | (1UL << 8) | (1UL << 10) | (3UL << 12) | (1UL << 23) | (2UL << 32))
#define MAIR_DEVICE_AND_NORMAL 0xff00UL
#define EL1_ENCODING (1UL << 2)
_Static_assert(sizeof(struct zcu106_b2_page_tables) == (6U + B2_SIGNED_TA_PROFILE) * B2_PAGE_SIZE, "table layout");

static bool map_region(struct zcu106_b2_page_tables *tables, uint64_t base, uint64_t end, uint64_t attributes)
{
    if (base >= end || base % B2_PAGE_SIZE != 0UL || end % B2_PAGE_SIZE != 0UL) { return false; }
    for (uint64_t address = base; address < end; address += B2_PAGE_SIZE) {
        uint64_t *pages;
        if ((address >> BLOCK_INDEX_SHIFT) == (NONSECURE_CODE_BASE >> BLOCK_INDEX_SHIFT)) { pages = tables->client_first; }
        else if ((address >> BLOCK_INDEX_SHIFT) == (NONSECURE_STACK_BASE >> BLOCK_INDEX_SHIFT)) { pages = tables->client_second; }
#if B2_SIGNED_TA_PROFILE
        else if ((address >> BLOCK_INDEX_SHIFT) == (SIGNED_TA_SOURCE_BASE >> BLOCK_INDEX_SHIFT)) { pages = tables->ta_images; }
#endif
        else if ((address >> BLOCK_INDEX_SHIFT) == (B2_UART_BASE >> BLOCK_INDEX_SHIFT)) { pages = tables->uart; }
        else { return false; }
        unsigned int index = (unsigned int)((address >> PAGE_INDEX_SHIFT) & TABLE_INDEX_MASK);
        if (pages[index] != 0UL) { return false; }
        pages[index] = address | attributes | DESCRIPTOR_PAGE;
    }
    return true;
}

bool zcu106_b2_build_page_tables(struct zcu106_b2_page_tables *tables, uint64_t vector_base, uint64_t vector_end)
{
    if (tables == NULL || ((uintptr_t)tables % B2_PAGE_SIZE) != 0U) { return false; }
    if (vector_base < NONSECURE_DATA_BASE || vector_end > NONSECURE_DATA_END || vector_base >= vector_end ||
        vector_base % B2_PAGE_SIZE != 0UL || vector_end % B2_PAGE_SIZE != 0UL) { return false; }
    unsigned char *bytes = (unsigned char *)tables;
    for (size_t index = 0U; index < sizeof(*tables); ++index) { bytes[index] = 0U; }
    tables->root[NONSECURE_CODE_BASE >> ROOT_INDEX_SHIFT] = (uint64_t)(uintptr_t)tables->low | DESCRIPTOR_TABLE;
    tables->root[B2_UART_BASE >> ROOT_INDEX_SHIFT] = (uint64_t)(uintptr_t)tables->high | DESCRIPTOR_TABLE;
    tables->low[(NONSECURE_CODE_BASE >> BLOCK_INDEX_SHIFT) & TABLE_INDEX_MASK] = (uint64_t)(uintptr_t)tables->client_first | DESCRIPTOR_TABLE;
    tables->low[(NONSECURE_STACK_BASE >> BLOCK_INDEX_SHIFT) & TABLE_INDEX_MASK] = (uint64_t)(uintptr_t)tables->client_second | DESCRIPTOR_TABLE;
    tables->high[(B2_UART_BASE >> BLOCK_INDEX_SHIFT) & TABLE_INDEX_MASK] = (uint64_t)(uintptr_t)tables->uart | DESCRIPTOR_TABLE;
    uint64_t normal = DESCRIPTOR_ACCESS_FLAG | DESCRIPTOR_NORMAL_INDEX | DESCRIPTOR_INNER_SHAREABLE | DESCRIPTOR_USER_XN;
    uint64_t writable = normal | DESCRIPTOR_PRIVILEGED_XN;
    /* コードだけをRO実行可にし、Secure RAMや未確認DDR1は写像しない。 */
    bool mapped = map_region(tables, NONSECURE_CODE_BASE, NONSECURE_CODE_END, normal | DESCRIPTOR_READ_ONLY) &&
        map_region(tables, NONSECURE_DATA_BASE, NONSECURE_DATA_END, writable) &&
        map_region(tables, NONSECURE_STACK_BASE, NONSECURE_STACK_END, writable) &&
        map_region(tables, SHARED_MEMORY_BASE, SHARED_MEMORY_END, writable) &&
        map_region(tables, B2_UART_BASE, B2_UART_END, DESCRIPTOR_ACCESS_FLAG | DESCRIPTOR_USER_XN | DESCRIPTOR_PRIVILEGED_XN);
#if B2_SIGNED_TA_PROFILE
    tables->low[(SIGNED_TA_SOURCE_BASE >> BLOCK_INDEX_SHIFT) & TABLE_INDEX_MASK] = (uint64_t)(uintptr_t)tables->ta_images | DESCRIPTOR_TABLE;
    mapped = mapped && map_region(tables,SIGNED_TA_SOURCE_BASE,SIGNED_TA_SOURCE_END,writable | DESCRIPTOR_READ_ONLY);
#endif
    if (!mapped) { return false; }
    /* ベクタはBSSと別ページに配置し、そのページだけRO実行可へ変更する。 */
    for (uint64_t address = vector_base; address < vector_end; address += B2_PAGE_SIZE) {
        tables->client_first[(address >> PAGE_INDEX_SHIFT) & TABLE_INDEX_MASK] = address | normal | DESCRIPTOR_READ_ONLY | DESCRIPTOR_PAGE;
    }
    return true;
}

#ifndef TEE_CLIENT_HOST_TEST
static struct zcu106_b2_page_tables client_tables;
extern char __b2_vectors_start[], __b2_vectors_end[];
/* 実機確認を終えるまでゼロのままにし、デバッガで上書きして迂回しない。 */
volatile const uint32_t zcu106_b2_runtime_ready = B2_RUNTIME_READY;
bool platform_prepare_tee_client(void)
{
    if (zcu106_b2_runtime_ready != 1U) { return false; }
    uint64_t level, state;
    __asm__ volatile("mrs %0, CurrentEL; mrs %1, sctlr_el1" : "=r"(level), "=r"(state));
    if (level != EL1_ENCODING || (state & SCTLR_TRANSLATION_CACHE) != 0UL) { return false; }
    uint64_t base = (uint64_t)(uintptr_t)&client_tables;
    if (base < NONSECURE_DATA_BASE || base > NONSECURE_DATA_END - sizeof(client_tables) ||
        !zcu106_b2_build_page_tables(&client_tables, (uint64_t)(uintptr_t)__b2_vectors_start, (uint64_t)(uintptr_t)__b2_vectors_end)) { return false; }
    /* 入力時M/C/I無効を要求し、dirty cacheを黙って破棄しない。 */
    __asm__ volatile("dsb sy; msr mair_el1, %0; msr tcr_el1, %1; msr ttbr0_el1, %2; isb; tlbi vmalle1; ic iallu; dsb sy; isb"
        :: "r"(MAIR_DEVICE_AND_NORMAL), "r"(TCR_IDENTITY_39BIT), "r"(base) : "memory");
    state |= SCTLR_TRANSLATION_CACHE;
    __asm__ volatile("msr sctlr_el1, %0; isb; mrs %0, sctlr_el1" : "+r"(state) :: "memory");
    return (state & SCTLR_TRANSLATION_CACHE) == SCTLR_TRANSLATION_CACHE;
}
#endif
