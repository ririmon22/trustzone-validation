#include "physical_probe.h"
#include <kernel/cache_helpers.h>
#include <kernel/pseudo_ta.h>
#include <malloc.h>
#include <mm/core_memprot.h>
#include <mm/core_mmu.h>
#include <mm/phys_mem.h>
#include <string.h>
#include <tee_api_defines.h>
struct probe_page
{
    tee_mm_entry_t *allocation;
    volatile uint64_t *words;
    paddr_t physical;
};
#ifdef CFG_TZ_PHYSICAL_FIXED_BASE
#define FIXED_REGION_SIZE 0x100000U
#define FIXED_TEST_BYTES 0x10000U
/* 実機の予約・XMPU成立は別ゲートで確認する。Core/TA allocator外の専用領域だけをmapする。 */
register_phys_mem(MEM_AREA_RAM_SEC, CFG_TZ_PHYSICAL_FIXED_BASE, FIXED_REGION_SIZE);
static bool fixed_page_in_use;
#endif
/* TA allocatorで排他的に保持し、動作中のTA/Coreのページを転用しない。 */
static void release_page(struct probe_page *page)
{
    if (page->words)
    {
        memset((void *)page->words, 0, PHYSICAL_PAGE_BYTES);
        dcache_cleaninv_range((void *)page->words, PHYSICAL_PAGE_BYTES);
    }
#ifdef CFG_TZ_PHYSICAL_FIXED_BASE
    if (page->words) { fixed_page_in_use = false; }
#endif
    if (page->allocation)
    {
        tee_mm_free(page->allocation);
    }
    memset(page, 0, sizeof(*page));
}
static TEE_Result verify_page(struct probe_page *page)
{
    if (!page->words)
    {
        return TEE_ERROR_BAD_STATE;
    }
    /* NS writeをSecure側dirty cacheで隠さない。保存対象はこのページだけ。 */
    dcache_inv_range((void *)page->words, PHYSICAL_PAGE_BYTES);
    for (size_t i = 0; i < PHYSICAL_PAGE_BYTES / sizeof(uint64_t); i++)
    {
        if (page->words[i] != (PHYSICAL_PATTERN ^ i))
        {
            return TEE_ERROR_SECURITY;
        }
    }
    return TEE_SUCCESS;
}
static TEE_Result prepare_page(struct probe_page *page)
{
#ifdef CFG_TZ_PHYSICAL_FIXED_BASE
    if (fixed_page_in_use || page->words) { return TEE_ERROR_BUSY; }
    page->physical = CFG_TZ_PHYSICAL_FIXED_BASE;
    page->words = phys_to_virt(page->physical, MEM_AREA_RAM_SEC, FIXED_TEST_BYTES);
    if (!page->words) { return TEE_ERROR_NOT_SUPPORTED; }
    fixed_page_in_use = true;
    for (size_t i = 0U; i < PHYSICAL_PAGE_BYTES / sizeof(uint64_t); ++i) {
        page->words[i] = PHYSICAL_PATTERN ^ i;
    }
    dcache_cleaninv_range((void *)page->words, PHYSICAL_PAGE_BYTES);
    return verify_page(page);
#else
    if (page->allocation)
    {
        return TEE_ERROR_BAD_STATE;
    }
    page->allocation = phys_mem_ta_alloc(PHYSICAL_PAGE_BYTES);
    if (!page->allocation)
    {
        return TEE_ERROR_OUT_OF_MEMORY;
    }
    page->physical = tee_mm_get_smem(page->allocation);
    /* 公式fobj_sec_mem_allocと同じ排他的allocatorとSecure全体mappingを使う。
       既存の割当ページを参照せず、32bit ABI外を拒否する。 */
    page->words = phys_to_virt(page->physical, MEM_AREA_SEC_RAM_OVERALL, PHYSICAL_PAGE_BYTES);
    if (!page->words || page->physical % PHYSICAL_PAGE_BYTES ||
        page->physical > UINT32_MAX - PHYSICAL_PAGE_BYTES ||
        tee_mm_get_bytes(page->allocation) < PHYSICAL_PAGE_BYTES)
    {
        page->words = NULL;
        release_page(page);
        return TEE_ERROR_NOT_SUPPORTED;
    }
    for (size_t i = 0; i < PHYSICAL_PAGE_BYTES / sizeof(uint64_t); i++)
    {
        page->words[i] = PHYSICAL_PATTERN ^ i;
    }
    dcache_cleaninv_range((void *)page->words, PHYSICAL_PAGE_BYTES);
    return verify_page(page);
#endif
}
static TEE_Result open_probe(uint32_t types, TEE_Param params[TEE_NUM_PARAMS], void **context)
{
#if !defined(CFG_WITH_PAGER) && !defined(CFG_NS_VIRTUALIZATION)
    struct probe_page *page = NULL;
#endif
    (void)params;
    if (types != 0)
    {
        return TEE_ERROR_BAD_PARAMETERS;
    }
#if defined(CFG_WITH_PAGER) || defined(CFG_NS_VIRTUALIZATION)
    (void)context;
    return TEE_ERROR_NOT_SUPPORTED;
#else
    page = calloc(1, sizeof(*page));
    if (!page)
    {
        return TEE_ERROR_OUT_OF_MEMORY;
    }
    *context = page;
    return TEE_SUCCESS;
#endif
}
static void close_probe(void *context)
{
    struct probe_page *page = context;
    if (page)
    {
        release_page(page);
        free(page);
    }
}
static TEE_Result invoke_probe(void *context, uint32_t command, uint32_t types,
                               TEE_Param params[TEE_NUM_PARAMS])
{
    struct probe_page *page = context;
    if (!page || types != TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_INOUT, 0, 0, 0))
    {
        return TEE_ERROR_BAD_PARAMETERS;
    }
    params[0].value.a = 0;
    params[0].value.b = 0;
    if (command == PHYSICAL_PREPARE)
    {
        TEE_Result result = prepare_page(page);
        if (result == TEE_SUCCESS)
        {
            params[0].value.a = (uint32_t)page->physical;
        }
        return result;
    }
    if (command == PHYSICAL_VERIFY)
    {
        TEE_Result result = verify_page(page);
        if (result == TEE_SUCCESS)
        {
            params[0].value.a = PHYSICAL_CHECK_TAG;
        }
        return result;
    }
    if (command == PHYSICAL_SIZE)
    {
        params[0].value.a = PHYSICAL_PAGE_BYTES;
        return TEE_SUCCESS;
    }
    if (command == PHYSICAL_RELEASE)
    {
        release_page(page);
        return TEE_SUCCESS;
    }
    return TEE_ERROR_BAD_PARAMETERS;
}
pseudo_ta_register(.uuid = PHYSICAL_PROBE_UUID, .name = "physical-probe.pta", .flags = PTA_DEFAULT_FLAGS,
                   .open_session_entry_point = open_probe, .close_session_entry_point = close_probe,
                   .invoke_command_entry_point = invoke_probe);
