#include "pcsign_tpmverify.h"
#include <stdio.h>
#include <string.h>
#include <openssl/pem.h>
#include <openssl/evp.h>
#include <openssl/core_names.h>
#include <openssl/ecdsa.h>
#include <openssl/bn.h>

/* 固定ファイル名をディレクトリから安全に組み立てる。 */
static int makePath(char *dst, size_t cap, const char *dir, const char *name)
{
    int n;
    if (!dst || !dir || !name) return -1;
    n = snprintf(dst, cap, "%s/%s", dir, name);
    return (n > 0 && (size_t)n < cap) ? 0 : -1;
}

/*
 * この1ファイルでOpenSSLを使用する部分を完結させる。
 * data.binはTPM2_Hashの上限以下で読み込む。
 * public.pemからP-256の公開鍵座標X/Yを取り出す。
 * signature.derからECDSA署名R/Sを取り出す。
 * TPMの操作やハッシュ計算は行わない。
 */
int opensslParseFiles(const char *directory, PcSignInput *out)
{
    char path[512];
    FILE *f = NULL;
    EVP_PKEY *pub = NULL;
    BIGNUM *x = NULL, *y = NULL;
    ECDSA_SIG *signature = NULL;
    const BIGNUM *r = NULL, *s = NULL;
    uint8_t der[128];
    const unsigned char *p;
    size_t n, group_len = 0;
    char group[80] = {0};
    int result = -1;

    if (!directory || !out) return -1;
    memset(out, 0, sizeof(*out));

    /* ① 元データ。TPMの最大入力サイズを超えた場合は拒否する。 */
    if (makePath(path, sizeof(path), directory, "data.bin") != 0) goto cleanup;
    f = fopen(path, "rb");
    if (!f) goto cleanup;
    n = fread(out->data.buffer, 1, sizeof(out->data.buffer), f);
    if (ferror(f) || (n == sizeof(out->data.buffer) && fgetc(f) != EOF) || ferror(f))
        goto cleanup;
    out->data.size = (UINT16)n;
    fclose(f); f = NULL;

    /* ② 公開鍵。OpenSSL 3.1.1推奨のEVPパラメータAPIを利用する。 */
    if (makePath(path, sizeof(path), directory, "public.pem") != 0) goto cleanup;
    f = fopen(path, "rb");
    if (!f) goto cleanup;
    pub = PEM_read_PUBKEY(f, NULL, NULL, NULL);
    fclose(f); f = NULL;
    if (!pub || EVP_PKEY_is_a(pub, "EC") != 1) goto cleanup;
    if (EVP_PKEY_get_utf8_string_param(pub, OSSL_PKEY_PARAM_GROUP_NAME,
                 group, sizeof(group), &group_len) != 1) goto cleanup;
    if (strcmp(group, "prime256v1") != 0 && strcmp(group, "secp256r1") != 0)
        goto cleanup;
    if (EVP_PKEY_get_bn_param(pub, OSSL_PKEY_PARAM_EC_PUB_X, &x) != 1 ||
        EVP_PKEY_get_bn_param(pub, OSSL_PKEY_PARAM_EC_PUB_Y, &y) != 1)
        goto cleanup;
    if (BN_bn2binpad(x, out->public_xy, 32) != 32 ||
        BN_bn2binpad(y, out->public_xy + 32, 32) != 32) goto cleanup;

    /* ③ DER署名。ECDSA_SIGは現在も利用可能なOpenSSL API。 */
    if (makePath(path, sizeof(path), directory, "signature.der") != 0) goto cleanup;
    f = fopen(path, "rb");
    if (!f) goto cleanup;
    n = fread(der, 1, sizeof(der), f);
    if (n == 0 || ferror(f) || (n == sizeof(der) && fgetc(f) != EOF) || ferror(f))
        goto cleanup;
    fclose(f); f = NULL;
    p = der;
    signature = d2i_ECDSA_SIG(NULL, &p, (long)n);
    if (!signature || p != der + n) goto cleanup;
    ECDSA_SIG_get0(signature, &r, &s);
    if (!r || !s || BN_is_zero(r) || BN_is_negative(r) ||
        BN_is_zero(s) || BN_is_negative(s)) goto cleanup;
    if (BN_bn2binpad(r, out->signature_rs, 32) != 32 ||
        BN_bn2binpad(s, out->signature_rs + 32, 32) != 32) goto cleanup;

    result = 0;
cleanup:
    if (f) fclose(f);
    EVP_PKEY_free(pub);
    BN_free(x);
    BN_free(y);
    ECDSA_SIG_free(signature);
    if (result != 0) {
        memset(out, 0, sizeof(*out));
        printf("opensslParseFiles failed: check data.bin/public.pem/signature.der\n");
    }
    return result;
}
