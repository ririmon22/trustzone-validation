#include "pcsign_tpmverify.h"
#include <stdio.h>
#include <string.h>

/* TPM2_Hash専用: 小さいデータをTPM内でSHA-256にする。 */
int tpmHash(ESYS_CONTEXT *esys, const TPM2B_MAX_BUFFER *input,
            TPM2B_DIGEST *digest)
{
    TPM2B_DIGEST *generated = NULL;
    TPMT_TK_HASHCHECK *ticket = NULL;
    TSS2_RC rc;
    int result = -1;

    if (!esys || !input || !digest || input->size > sizeof(input->buffer))
        return -1;
    rc = Esys_Hash(esys, ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE,
                   input, TPM2_ALG_SHA256, TPM2_RH_NULL,
                   &generated, &ticket);
    if (rc != TSS2_RC_SUCCESS) {
        printf("Esys_Hash failed: 0x%08lx\n", (unsigned long)rc);
        goto cleanup;
    }
    if (!generated || generated->size != 32) goto cleanup;
    memcpy(digest, generated, sizeof(*digest));
    result = 0;
cleanup:
    if (generated) Esys_Free(generated);
    if (ticket) Esys_Free(ticket);
    return result;
}
