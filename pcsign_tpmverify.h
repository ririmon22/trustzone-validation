#ifndef PCSIGN_TPMVERIFY_H
#define PCSIGN_TPMVERIFY_H

#include <stdint.h>
#include <stddef.h>
#include <tss2/tss2_esys.h>

/*
 * 初期化は呼び出し元の責任。
 * 署名対象は TPM2_Hash の単発入力上限まで。
 * 戻り値: 0=署名検証成功、-1=エラー/署名不一致。
 */
int pcsignTpmVerify(ESYS_CONTEXT *esys, const char *directory);

/* フォルダから読み込んだ入力データ。公開鍵X/Y、署名R/Sは32バイトずつ。 */
typedef struct {
    TPM2B_MAX_BUFFER data;
    uint8_t public_xy[64];
    uint8_t signature_rs[64];
} PcSignInput;

/* OpenSSLを使ったファイル入出力・PEM/DER解析（TPMにはアクセスしない）。 */
int opensslParseFiles(const char *directory, PcSignInput *out);

/* TPMコマンド1つにつき、1ファイル・1ラッパー。 */
int tpmHash(ESYS_CONTEXT *esys, const TPM2B_MAX_BUFFER *input,
            TPM2B_DIGEST *digest);
int tpmLoadExternal(ESYS_CONTEXT *esys, const uint8_t xy[64],
                    ESYS_TR *object_handle);
int tpmVerify(ESYS_CONTEXT *esys, ESYS_TR object_handle,
              const TPM2B_DIGEST *digest, const uint8_t rs[64]);
int tpmFlush(ESYS_CONTEXT *esys, ESYS_TR object_handle);

#endif
