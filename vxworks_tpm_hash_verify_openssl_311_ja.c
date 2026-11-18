/*
 * ファイル名：vxworks_tpm_hash_verify_openssl_311_ja.c
 *
 * 【目的】
 * PCのOpenSSLで作成した ECDSA（NIST P-256）/ SHA-256 の署名を、
 * VxWorksからTPM 2.0に依頼して検証する最小サンプル。
 *
 * 【起動時に読み込み済みのフォルダ】（例：/ram0/verify）
 *   data.bin       : 署名対象の元データ（バイナリでよい）
 *   public.pem     : PC側の公開鍵（PEM / SubjectPublicKeyInfo）
 *   signature.der  : PC側のECDSA署名（ASN.1 DER形式）
 *
 * 【前提】
 * - VxWorksに OpenSSL 3.1.1 の libcrypto と tpm2-tss 2.2系 SAPI がある。
 * - TPMドライバ/TCTI/SAPIの初期化は呼び出し元で完了済み。
 * - 初期化済みの TSS2_SYS_CONTEXT * を関数の第1引数で受け取る。
 * - TPMが NIST P-256、ECDSA、SHA-256、TPM2_Hash に対応している。
 * - data.bin は TPM2_Hash の入力上限（TPM2B_MAX_BUFFER）以下。
 *
 * - OpenSSL 3.xで非推奨のEC_KEY取得APIを使用しない。
 *
 * 【処理の全体像】
 *   (1) data.binを読み、TPM2_HashでSHA-256（32バイト）を計算
 *   (2) public.pemを読み、P-256公開鍵の座標 X/Y を各32バイトで取得
 *   (3) signature.derを読み、署名値 r/s を各32バイトで取得
 *   (4) TPM2_LoadExternalで公開鍵を一時的にロード
 *   (5) TPM2_VerifySignatureでハッシュと署名を照合
 *   (6) TPM2_FlushContextで一時的な公開鍵を解放
 *
 * 【重要】
 * TPMには公開鍵を永続保存しない（NV領域や永続ハンドルは使用しない）。
 * また、公開鍵も同じフォルダから受け取るので、この試験で確認できるのは
 * 「データ・署名・公開鍵の整合性」であり、公開鍵の発行元の信頼性ではない。
 *
 * 戻り値：0 = 検証成功かつ一時鍵の解放成功、-1 = 処理失敗
 * ※実機のTPM/ライブラリでの動作確認は別途必要。
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <tss2/tss2_sys.h>
#include <openssl/pem.h>
#include <openssl/evp.h>
#include <openssl/core_names.h>
#include <openssl/ecdsa.h>
#include <openssl/bn.h>

/* 最後にTPMへ検証を依頼する内部関数。定義はファイルの後半。 */
static int verify_digest_p256_tpm(TSS2_SYS_CONTEXT *sys,
    const uint8_t digest[32], const uint8_t pub_xy[64],
    const uint8_t sig_rs[64]);

/*
 * 【1】小さいdata.binをTPM内でSHA-256ハッシュ化する。
 *
 * 今回は「Hello World」程度の短いデータを想定するので、
 * TPM2_Hashを1回呼ぶだけでよい。OpenSSLのSHA-256関数は使わない。
 *
 * TPM2B_MAX_BUFFERには最大サイズがある。サイズを超えるファイルは
 * このサンプルでは拒否する（勝手に途中で切ってハッシュ化しない）。
 *
 * 引数:
 *   sys   : 初期化済みSAPIコンテキスト
 *   path  : 元データdata.binのパス
 *   out   : TPMで計算したハッシュ（32バイト）の出力先
 */
static int tpm_hash_small_file(TSS2_SYS_CONTEXT *sys,
                               const char *path,
                               uint8_t out[32])
{
    FILE *f = NULL;
    TPM2B_MAX_BUFFER data = {0};     /* TPMへ渡す元データ */
    TPM2B_DIGEST digest = {0};       /* TPMが返すSHA-256 */
    TPMT_TK_HASHCHECK ticket = {0};  /* TPMからの検証チケット（今回は未使用） */
    size_t n;
    TSS2_RC rc;
    int result = -1;

    if (sys == NULL || path == NULL || out == NULL)
        return -1;

    f = fopen(path, "rb");
    if (f == NULL) {
        printf("data.bin open failed\n");
        return -1;
    }

    /*
     * 最大入力サイズまで読み込む。
     * さらにfgetc()で1バイト確認し、上限を超えるファイルを検出する。
     */
    n = fread(data.buffer, 1, sizeof(data.buffer), f);
    if (ferror(f)) {
        printf("data.bin read failed\n");
        goto cleanup;
    }
    if (n == sizeof(data.buffer) && fgetc(f) != EOF) {
        printf("data.bin too large for TPM2_Hash\n");
        goto cleanup;
    }
    if (ferror(f)) {
        printf("data.bin size check failed\n");
        goto cleanup;
    }
    data.size = (UINT16)n;

    /*
     * TPM2_Hash:
     *   data             元データ（Hello Worldなど）
     *   TPM2_ALG_SHA256  ハッシュアルゴリズム
     *   TPM2_RH_NULL     階層によるチケット発行を要求しない
     *   digest           TPMが計算した32バイトの結果
     *
     * SAPIのTss2_Sys_Hash()がTCTIを経由してTPMに処理を依頼する。
     */
    rc = Tss2_Sys_Hash(sys,
                       NULL,
                       &data,
                       TPM2_ALG_SHA256,
                       TPM2_RH_NULL,
                       &digest,
                       &ticket,
                       NULL);
    if (rc != TSS2_RC_SUCCESS) {
        printf("TPM2_Hash failed RC=0x%08lx\n", (unsigned long)rc);
        goto cleanup;
    }

    /* SHA-256の結果は必ず32バイトであることを確認する。 */
    if (digest.size != 32) {
        printf("Unexpected TPM SHA-256 length: %u\n", (unsigned)digest.size);
        goto cleanup;
    }

    memcpy(out, digest.buffer, 32);
    printf("TPM SHA-256: PASS (input=%lu bytes)\n", (unsigned long)n);
    result = 0;

cleanup:
    fclose(f);
    return result;
}

/*
 * 【2】public.pemから楕円曲線の公開鍵座標X/Yを取り出す。
 *
 * PEMはテキスト形式の公開鍵ファイルだが、TPMの公開鍵構造体では
 * 楕円曲線上の点を X座標 / Y座標 として渡す必要がある。
 * P-256の座標はそれぞれ32バイトなので、出力は64バイト。
 *
 * xy[ 0..31] = X座標
 * xy[32..63] = Y座標
 *
 * 別の楕円曲線の公開鍵を誤って渡さないよう、P-256を確認する。
 * OpenSSL 3.1.1のEVP_PKEY_get_*_param()を使用する。
 */
static int load_p256_xy(const char *path, uint8_t xy[64])
{
    FILE *f = NULL;
    EVP_PKEY *pkey = NULL;
    BIGNUM *x = NULL;
    BIGNUM *y = NULL;
    char group_name[80] = {0};
    size_t group_len = 0;
    int result = -1;

    if (path == NULL || xy == NULL)
        return -1;

    /* ① PEM形式の公開鍵を読み込む。PEM_read_PUBKEYはEVP_PKEYを返す。 */
    f = fopen(path, "rb");
    if (f == NULL) {
        printf("public.pem open failed\n");
        return -1;
    }
    pkey = PEM_read_PUBKEY(f, NULL, NULL, NULL);
    fclose(f);
    f = NULL;
    if (pkey == NULL) {
        printf("public.pem parse failed\n");
        goto cleanup;
    }

    /* ② EC公開鍵かどうか調べる。低レベルEC_KEY APIは使用しない。 */
    if (EVP_PKEY_is_a(pkey, "EC") != 1) {
        printf("Expected an EC public key\n");
        goto cleanup;
    }

    /*
     * ③ 公開鍵の曲線名を取得し、NIST P-256か確認する。
     *    OpenSSLではP-256を prime256v1 と呼ぶことが多い。
     *    他の曲線の公開鍵をTPMのP-256として誤用しないための確認。
     */
    if (EVP_PKEY_get_utf8_string_param(
            pkey, OSSL_PKEY_PARAM_GROUP_NAME,
            group_name, sizeof(group_name), &group_len) != 1) {
        printf("Cannot read EC group name\n");
        goto cleanup;
    }
    if (strcmp(group_name, "prime256v1") != 0 &&
        strcmp(group_name, "secp256r1") != 0) {
        printf("Unsupported EC group: %s\n", group_name);
        goto cleanup;
    }

    /*
     * ④ 公開鍵のX座標とY座標をBIGNUMとして取り出す。
     *    これらの関数はOpenSSL 3.x推奨のパラメータ取得API。
     *    EVP_PKEY_get1_EC_KEY / EC_POINT_point2oct は不要。
     */
    if (EVP_PKEY_get_bn_param(pkey, OSSL_PKEY_PARAM_EC_PUB_X, &x) != 1 ||
        EVP_PKEY_get_bn_param(pkey, OSSL_PKEY_PARAM_EC_PUB_Y, &y) != 1) {
        printf("Cannot read EC public coordinates\n");
        goto cleanup;
    }

    /*
     * ⑤ P-256の各座標を32バイトのビッグエンディアンに変換する。
     *    値が短ければ先頭をゼロ埋めし、長すぎればエラーにする。
     *    xy[0..31]がX、xy[32..63]がY。
     */
    if (BN_bn2binpad(x, xy, 32) != 32 ||
        BN_bn2binpad(y, xy + 32, 32) != 32) {
        printf("Invalid EC coordinate length\n");
        goto cleanup;
    }

    result = 0;

cleanup:
    BN_free(x);
    BN_free(y);
    EVP_PKEY_free(pkey);
    if (f != NULL)
        fclose(f);
    return result;
}

/*
 * 【3】signature.derからECDSA署名の2つの整数 r / s を取り出す。
 *
 * OpenSSLのECDSA署名は通常ASN.1 DERで記録される。
 * TPMのTPMT_SIGNATUREには r と s を別々の整数列として格納するため、
 * DERを解析して固定32バイト（不足する上位桁はゼロ埋め）へ変換する。
 *
 * rs[ 0..31] = r
 * rs[32..63] = s
 */
static int load_ecdsa_der_rs(const char *path, uint8_t rs[64]) {
    uint8_t b[80];                    /* P-256 ECDSAのDER署名を格納する領域 */
    size_t n;
    const unsigned char *cur;
    const BIGNUM *r = NULL, *s = NULL;
    ECDSA_SIG *sig = NULL;
    FILE *f = fopen(path, "rb");
    int ok = -1;

    if (!f) return -1;
    n = fread(b, 1, sizeof(b), f);
    if (ferror(f)) goto done;
    if (n == sizeof(b) && fgetc(f) != EOF) goto done; /* 上限超過を拒否 */
    if (ferror(f)) goto done;

    /* DERを解析してOpenSSLのECDSA_SIG構造体に変換する。 */
    cur = b;
    sig = d2i_ECDSA_SIG(NULL, &cur, (long)n);
    if (!sig || cur != b + n) goto done; /* 末尾に余計なデータがあれば拒否 */

    ECDSA_SIG_get0(sig, &r, &s);
    /* rとsは正の整数である必要がある。 */
    if (BN_is_zero(r) || BN_is_negative(r) || BN_is_zero(s) || BN_is_negative(s))
        goto done;

    /* TPMで使いやすいよう、r/sを32バイトずつにそろえる。 */
    if (BN_bn2binpad(r, rs, 32) != 32 ||
        BN_bn2binpad(s, rs + 32, 32) != 32) goto done;
    ok = 0;
done:
    ECDSA_SIG_free(sig);
    fclose(f);
    return ok;
}

/*
 * 【外部から呼び出す関数】
 * 引数sys：すでに初期化済みのSAPIコンテキスト
 * 引数dir：3ファイルが入ったフォルダ（例："/ram0/verify"）
 *
 * 「TPMでハッシュを作る → 公開鍵を解析する → 署名を解析する → TPMで検証」
 * という処理を、この関数で順番に実行する。
 */
int verify_files_with_openssl(TSS2_SYS_CONTEXT *sys, const char *dir) {
    char path[512];
    uint8_t digest[32], xy[64], rs[64];

    if (!sys || !dir || strlen(dir) > 480) return -1;

    snprintf(path, sizeof(path), "%s/data.bin", dir);
    if (tpm_hash_small_file(sys, path, digest)) return -1;

    snprintf(path, sizeof(path), "%s/public.pem", dir);
    if (load_p256_xy(path, xy)) return -1;

    snprintf(path, sizeof(path), "%s/signature.der", dir);
    if (load_ecdsa_der_rs(path, rs)) return -1;

    return verify_digest_p256_tpm(sys, digest, xy, rs);
}

/*
 * 【4～6】TPMに公開鍵を一時ロードし、ECDSA署名を検証して解放する。
 *
 * digest : SHA-256値（32バイト）
 * pub_xy : 公開鍵のX/Y（それぞれ32バイト）
 * sig_rs : 署名のr/s（それぞれ32バイト）
 *
 * 注：ハッシュ計算とECDSA署名検証はTPMが担当する。
 *     OpenSSLは公開鍵PEMと署名DERの解析にだけ使う。
 */
static int verify_digest_p256_tpm(TSS2_SYS_CONTEXT *sys,
                           const uint8_t digest[32],
                           const uint8_t pub_xy[64],
                           const uint8_t sig_rs[64])
{
    TPM2B_PUBLIC public_key = {0};    /* TPMにロードする公開鍵の形式 */
    TPM2B_NAME name = {0};            /* ロードされた鍵の名前 */
    TPM2B_DIGEST hash = {0};          /* 検証対象のSHA-256ハッシュ */
    TPMT_SIGNATURE signature = {0};   /* ECDSA署名(r,s)を渡す構造体 */
    TPMT_TK_VERIFIED ticket = {0};    /* TPMが返す検証結果のチケット */
    TPM2_HANDLE handle = 0;           /* 一時ロードした公開鍵のハンドル */
    TSS2_RC rc, flush_rc;
    int ok = -1;

    if (!sys || !digest || !pub_xy || !sig_rs) return -1;

    /* (4-1) TPM2B_PUBLICに「ECC / P-256 / SHA-256」を指定する。 */
    public_key.publicArea.type = TPM2_ALG_ECC;
    public_key.publicArea.nameAlg = TPM2_ALG_SHA256;
    public_key.publicArea.objectAttributes =
        TPMA_OBJECT_SIGN_ENCRYPT | TPMA_OBJECT_USERWITHAUTH;
    public_key.publicArea.parameters.eccDetail.symmetric.algorithm = TPM2_ALG_NULL;
    public_key.publicArea.parameters.eccDetail.scheme.scheme = TPM2_ALG_ECDSA;
    public_key.publicArea.parameters.eccDetail.scheme.details.ecdsa.hashAlg = TPM2_ALG_SHA256;
    public_key.publicArea.parameters.eccDetail.curveID = TPM2_ECC_NIST_P256;
    public_key.publicArea.parameters.eccDetail.kdf.scheme = TPM2_ALG_NULL;

    /* (4-2) 公開鍵のX座標・Y座標を各32バイトで設定する。 */
    public_key.publicArea.unique.ecc.x.size = 32;
    public_key.publicArea.unique.ecc.y.size = 32;
    memcpy(public_key.publicArea.unique.ecc.x.buffer, pub_xy, 32);
    memcpy(public_key.publicArea.unique.ecc.y.buffer, pub_xy + 32, 32);

    /* (5-1) TPM2_Hashの32バイト結果をTPM用の構造体へコピーする。 */
    hash.size = 32;
    memcpy(hash.buffer, digest, 32);

    /* (5-2) 署名アルゴリズムをECDSA、ハッシュをSHA-256として設定する。 */
    signature.sigAlg = TPM2_ALG_ECDSA;
    signature.signature.ecdsa.hash = TPM2_ALG_SHA256;
    signature.signature.ecdsa.signatureR.size = 32;
    signature.signature.ecdsa.signatureS.size = 32;
    memcpy(signature.signature.ecdsa.signatureR.buffer, sig_rs, 32);
    memcpy(signature.signature.ecdsa.signatureS.buffer, sig_rs + 32, 32);

    /*
     * (4-3) LoadExternal：PC由来の公開鍵をTPMへ一時ロードする。
     * TPM2_RH_NULLを指定する。永続化する処理ではない。
     * 成功すると後続コマンドで使うhandleが返ってくる。
     */
    rc = Tss2_Sys_LoadExternal(sys, NULL, NULL, &public_key,
                              TPM2_RH_NULL, &handle, &name, NULL);
    if (rc != TSS2_RC_SUCCESS) {
        printf("TPM LoadExternal failed RC=0x%08lx\n", (unsigned long)rc);
        return -1;
    }

    /*
     * (5-3) VerifySignature：TPMにECDSA署名検証を実行させる。
     * TPMが署名とハッシュと公開鍵の組み合わせを検証し、
     * TSS2_RC_SUCCESSなら署名検証成功と判定する。
     */
    rc = Tss2_Sys_VerifySignature(sys, handle, NULL,
                                 &hash, &signature, &ticket, NULL);
    if (rc == TSS2_RC_SUCCESS) {
        puts("TPM ECDSA P-256 SHA-256: PASS");
        ok = 0;
    } else {
        printf("TPM VerifySignature: FAIL RC=0x%08lx\n", (unsigned long)rc);
    }

    /*
     * (6) FlushContext：検証の成否にかかわらず一時公開鍵を解放する。
     * ロードのたびにTPM内の一時オブジェクトが増えないようにする。
     */
    flush_rc = Tss2_Sys_FlushContext(sys, handle);
    if (flush_rc != TSS2_RC_SUCCESS) {
        printf("WARNING: FlushContext RC=0x%08lx\n", (unsigned long)flush_rc);
        ok = -1;
    }
    return ok;
}

/*
 * 【呼び出し例】（このファイルだけではTPM初期化は行わない）
 *
 *   // sys は既存アプリで初期化した TSS2_SYS_CONTEXT *
 *   int result = verify_files_with_openssl(sys, "/ram0/verify");
 *   printf("Result: %s\n", result == 0 ? "PASS" : "FAIL");
 *
 * PC側で生成する場合：
 *   openssl ecparam -name prime256v1 -genkey -noout -out private.pem
 *   openssl pkey -in private.pem -pubout -out public.pem
 *   openssl dgst -sha256 -sign private.pem -out signature.der data.bin
 *   openssl dgst -sha256 -verify public.pem -signature signature.der data.bin
 *
 * ハッシュだけを確認する場合のPC側比較例：
 *   openssl dgst -sha256 data.bin
 *
 * VxWorksへ渡すのは data.bin / signature.der / public.pem の3つだけ。
 * private.pemは秘密鍵なので渡さない。
 */
