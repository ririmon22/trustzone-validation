#include "pcsign_tpmverify.h"
#include <stdio.h>
#include <string.h>

/* TPM2_VerifySignature専用: SHA-256とECDSA P-256の署名R/Sを検証。 */
int tpmVerify(ESYS_CONTEXT *esys, ESYS_TR object_handle,
              const TPM2B_DIGEST *digest, const uint8_t rs[64])
{
    TPMT_SIGNATURE sig = {0};
    TPMT_TK_VERIFIED *ticket = NULL;
    TSS2_RC rc;

    if (!esys || !digest || !rs || digest->size != 32 ||
        object_handle == ESYS_TR_NONE) return -1;

    sig.sigAlg = TPM2_ALG_ECDSA;
    sig.signature.ecdsa.hash = TPM2_ALG_SHA256;
    sig.signature.ecdsa.signatureR.size = 32;
    sig.signature.ecdsa.signatureS.size = 32;
    memcpy(sig.signature.ecdsa.signatureR.buffer, rs, 32);
    memcpy(sig.signature.ecdsa.signatureS.buffer, rs + 32, 32);

    rc = Esys_VerifySignature(esys, object_handle,
                              ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE,
                              digest, &sig, &ticket);
    if (ticket) Esys_Free(ticket);
    if (rc != TSS2_RC_SUCCESS) {
        printf("Esys_VerifySignature failed: 0x%08lx\n", (unsigned long)rc);
        return -1;
    }
    return 0;
}
