/*
 * ファイル名：vxworks_tpm_verify_openssl_ja.c
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
 * - VxWorksに OpenSSL の libcrypto と tpm2-tss 2.2系 SAPI がある。
 * - TPMドライバ/TCTI/SAPIの初期化は呼び出し元で完了済み。
 * - 初期化済みの TSS2_SYS_CONTEXT * を関数の第1引数で受け取る。
 * - TPMが NIST P-256、ECDSA、SHA-256 に対応している。
 *
 * 【処理の全体像】
 *   (1) data.binを読み、SHA-256ハッシュ（32バイト）を計算
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
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/ec.h>
#include <openssl/ecdsa.h>
#include <openssl/bn.h>

/* 最後にTPMへ検証を依頼する内部関数。定義はファイルの後半。 */
static int verify_digest_p256_tpm(TSS2_SYS_CONTEXT *sys,
    const uint8_t digest[32], const uint8_t pub_xy[64],
    const uint8_t sig_rs[64]);

/*
 * 【1】data.binからSHA-256のハッシュ値を計算する。
 *
 * 署名検証では、元データそのものではなく、元データのSHA-256ハッシュを
 * TPMに渡す。ファイルが大きくても4096バイトずつ読めるようにしている。
 *
 * path : data.binへのファイルパス
 * out  : 計算したハッシュ（32バイト）の書き込み先
 */
static int digest_file(const char *path, uint8_t out[32]) {
    uint8_t buf[4096];                 /* ファイルを分割して読み込む作業領域 */
    size_t n;                          /* 今回読み込めたバイト数 */
    unsigned int len = 0;              /* ハッシュ計算結果のバイト数 */
    int ok = -1;                       /* 失敗を初期値にする */
    FILE *f = fopen(path, "rb");       /* バイナリモードでファイルを開く */
    EVP_MD_CTX *md = EVP_MD_CTX_new(); /* OpenSSLのハッシュ計算用状態 */

    if (!f || !md) goto done;

    /* SHA-256でハッシュ計算を開始する。 */
    if (EVP_DigestInit_ex(md, EVP_sha256(), NULL) != 1) goto done;

    /* EOFまでファイルを読み、その内容を順番にハッシュへ反映する。 */
    while ((n = fread(buf, 1, sizeof(buf), f)) != 0)
        if (EVP_DigestUpdate(md, buf, n) != 1) goto done;
    if (ferror(f)) goto done;          /* 読み込みエラーと正常EOFを区別 */

    /* 完成したハッシュがSHA-256の32バイトであることを確認する。 */
    if (EVP_DigestFinal_ex(md, out, &len) == 1 && len == 32) ok = 0;
done:
    if (f) fclose(f);
    EVP_MD_CTX_free(md);
    return ok;
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
 * 別の楕円曲線の公開鍵を誤って渡さないよう、prime256v1を確認する。
 */
static int load_p256_xy(const char *path, uint8_t xy[64]) {
    FILE *f = fopen(path, "rb");
    EVP_PKEY *p = NULL;               /* PEMから読み込む公開鍵 */
    EC_KEY *ec = NULL;                /* 楕円曲線公開鍵として参照する */
    const EC_GROUP *group;            /* 使われている楕円曲線 */
    const EC_POINT *point;            /* 公開鍵の楕円曲線上の点 */
    unsigned char oct[65];           /* 非圧縮点：0x04 + X(32) + Y(32) */
    int ok = -1;

    if (!f) goto done;
    p = PEM_read_PUBKEY(f, NULL, NULL, NULL);
    if (!p || EVP_PKEY_base_id(p) != EVP_PKEY_EC) goto done;

    ec = EVP_PKEY_get1_EC_KEY(p);
    if (!ec) goto done;
    group = EC_KEY_get0_group(ec);
    point = EC_KEY_get0_public_key(ec);

    /* 楕円曲線がNIST P-256（OpenSSL名：prime256v1）か確認する。 */
    if (!group || !point || EC_GROUP_get_curve_name(group) != NID_X9_62_prime256v1)
        goto done;
    if (EC_POINT_is_on_curve(group, point, NULL) != 1) goto done;

    /* 公開鍵の点を「04 | X座標 | Y座標」の65バイト形式で取得する。 */
    if (EC_POINT_point2oct(group, point, POINT_CONVERSION_UNCOMPRESSED,
                           oct, sizeof(oct), NULL) != 65 || oct[0] != 0x04)
        goto done;

    /* 先頭の形式識別子0x04を除き、XとYだけをコピーする。 */
    memcpy(xy, oct + 1, 64);
    ok = 0;
done:
    if (f) fclose(f);
    EC_KEY_free(ec);
    EVP_PKEY_free(p);
    return ok;
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
    if (ferror(f) || !feof(f)) goto done; /* 入力が長すぎる場合も失敗 */

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
 * 「ハッシュを作る → 公開鍵を解析する → 署名を解析する → TPMで検証」
 * という処理を、この関数で順番に実行する。
 */
int verify_files_with_openssl(TSS2_SYS_CONTEXT *sys, const char *dir) {
    char path[512];
    uint8_t digest[32], xy[64], rs[64];

    if (!sys || !dir || strlen(dir) > 480) return -1;

    snprintf(path, sizeof(path), "%s/data.bin", dir);
    if (digest_file(path, digest)) return -1;

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
 * 注：ハッシュ計算とファイル解析はVxWorks上のOpenSSLが担当し、
 *     ECDSA署名の数学的な検証をTPMが担当する。
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

    /* (5-1) SHA-256の32バイト値をTPM用の構造体へコピーする。 */
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
 * VxWorksへ渡すのは data.bin / signature.der / public.pem の3つだけ。
 * private.pemは秘密鍵なので渡さない。
 */
