#include "pcsign_tpmverify.h"
#include <stdio.h>
#include <string.h>

/*
 * 代表関数。user.c から初期化済み ESYS_CONTEXT* とディレクトリだけ受け取る。
 * TPMの起動・TCTI・ESAPI初期化や終了は、このモジュールでは行わない。
 *
 * ① ファイル読み込みとOpenSSL解析
 * ② TPMでSHA-256計算
 * ③ 公開鍵をTPMへ一時ロード
 * ④ TPMでECDSA署名検証
 * ⑤ 成功/失敗に関係なく、一時公開鍵を解放
 */
int pcsignTpmVerify(ESYS_CONTEXT *esys, const char *directory)
{
    PcSignInput input;
    TPM2B_DIGEST digest = {0};
    ESYS_TR key = ESYS_TR_NONE;
    int result = -1;

    if (esys == NULL || directory == NULL) return -1;
    memset(&input, 0, sizeof(input));

    if (opensslParseFiles(directory, &input) != 0) goto cleanup;
    if (tpmHash(esys, &input.data, &digest) != 0) goto cleanup;
    if (tpmLoadExternal(esys, input.public_xy, &key) != 0) goto cleanup;
    if (tpmVerify(esys, key, &digest, input.signature_rs) != 0) goto cleanup;
    result = 0;

cleanup:
    /* LoadExternalが成功した場合はVerifySignatureの結果に関係なく解放。 */
    if (key != ESYS_TR_NONE) {
        if (tpmFlush(esys, key) != 0) result = -1;
    }
    memset(&input, 0, sizeof(input));
    printf("PCSign -> TPM Verify: %s\n", result == 0 ? "PASS" : "FAIL");
    return result;
}
