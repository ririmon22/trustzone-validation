#include "pcsign_tpmverify.h"
#include <stdio.h>

/* TPM2_FlushContext専用: LoadExternalで作った一時オブジェクトを削除。 */
int tpmFlush(ESYS_CONTEXT *esys, ESYS_TR object_handle)
{
    TSS2_RC rc;
    if (!esys || object_handle == ESYS_TR_NONE) return -1;
    rc = Esys_FlushContext(esys, object_handle);
    if (rc != TSS2_RC_SUCCESS) {
        printf("Esys_FlushContext failed: 0x%08lx\n", (unsigned long)rc);
        return -1;
    }
    return 0;
}
