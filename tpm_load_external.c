#include "pcsign_tpmverify.h"
#include <stdio.h>
#include <string.h>

/* TPM2_LoadExternal専用: P-256公開鍵を一時的にTPMに登録する。 */
int tpmLoadExternal(ESYS_CONTEXT *esys, const uint8_t xy[64],
                    ESYS_TR *object_handle)
{
    TPM2B_PUBLIC pub = {0};
    TSS2_RC rc;
    if (!esys || !xy || !object_handle) return -1;
    *object_handle = ESYS_TR_NONE;

    pub.publicArea.type = TPM2_ALG_ECC;
    pub.publicArea.nameAlg = TPM2_ALG_SHA256;
    pub.publicArea.objectAttributes = TPMA_OBJECT_SIGN_ENCRYPT |
                                       TPMA_OBJECT_USERWITHAUTH;
    pub.publicArea.parameters.eccDetail.symmetric.algorithm = TPM2_ALG_NULL;
    pub.publicArea.parameters.eccDetail.scheme.scheme = TPM2_ALG_NULL;
    pub.publicArea.parameters.eccDetail.curveID = TPM2_ECC_NIST_P256;
    pub.publicArea.parameters.eccDetail.kdf.scheme = TPM2_ALG_NULL;
    pub.publicArea.unique.ecc.x.size = 32;
    pub.publicArea.unique.ecc.y.size = 32;
    memcpy(pub.publicArea.unique.ecc.x.buffer, xy, 32);
    memcpy(pub.publicArea.unique.ecc.y.buffer, xy + 32, 32);

    rc = Esys_LoadExternal(esys, ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE,
                           NULL, &pub, TPM2_RH_NULL, object_handle);
    if (rc != TSS2_RC_SUCCESS) {
        printf("Esys_LoadExternal failed: 0x%08lx\n", (unsigned long)rc);
        return -1;
    }
    return 0;
}
