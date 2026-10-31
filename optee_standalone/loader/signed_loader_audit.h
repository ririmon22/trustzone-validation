/* 検証profile専用。署名処理は変更せず、既存ローダの返却段階だけ保存する。 */
#include <kernel/cache_helpers.h>
#include "signed_loader_types.h"
volatile struct signed_loader_audit signed_loader_audit __attribute__((aligned(64))) = { .version = 1 };
static struct signed_loader_entry *signed_loader_begin(const TEE_UUID *uuid)
{
    struct signed_loader_entry *entry = NULL;
    if (signed_loader_audit.used >= SIGNED_LOADER_RECORDS) { signed_loader_audit.overflow++; return NULL; }
    entry = (struct signed_loader_entry *)&signed_loader_audit.entries[signed_loader_audit.used++];
    memset(entry, 0, sizeof(*entry));
    tee_uuid_to_octets(entry->uuid, uuid);
    entry->flags = 1; entry->signature = UINT32_MAX; entry->digest = UINT32_MAX;
    dcache_clean_range((void *)&signed_loader_audit, sizeof(signed_loader_audit));
    return entry;
}
static void signed_loader_note(struct signed_loader_entry *entry, uint32_t stage, TEE_Result result)
{
    if (!entry) { return; }
    entry->flags |= stage;
    if (stage == 4) { entry->signature = result; if (!result) { entry->flags |= 8; } }
    if (stage == 16) { entry->digest = result; if (!result) { entry->flags |= 32; } }
    dcache_clean_range((void *)&signed_loader_audit, sizeof(signed_loader_audit));
    DMSG("SIGNED_LOADER flags=%"PRIu32" signature=%"PRIx32" digest=%"PRIx32, entry->flags, entry->signature, entry->digest);
}
