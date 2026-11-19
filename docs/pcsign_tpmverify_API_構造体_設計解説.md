# PC署名 → VxWorks TPM検証：API・構造体・設定値 解説書

> 対象ソース：`pcsign_tpmverify/`（前回作成したESAPI分割版）  
> 対象環境：VxWorks / TPM 2.0 / tpm2-tss ESAPI **2.2.0想定** / OpenSSL **3.1.1想定**  
> 設計目的：PCでOpenSSLによって作成した **ECDSA（NIST P-256）＋SHA-256** 署名を、VxWorksの**TPMでハッシュ計算・署名検証**する。  
> 対象範囲：コードの静的読解に基づく説明。**VxWorks実機・対象バージョンでのビルドやTPM動作の成功を保証するものではない。**

## 1. 全体の処理構成

```text
user.c（既存：TPM/TCTI/ESAPI初期化済み）
  └── pcsignTpmVerify(esys, directory)           [pcsign_tpmverify.c]
        ├── opensslParseFiles(directory, &input) [openssl_parse.c]
        │     ├── data.bin           → バイト列
        │     ├── public.pem         → 公開鍵のX/Y各32バイト
        │     └── signature.der      → 署名のR/S各32バイト
        ├── tpmHash(esys, &input.data, &digest)   [tpm_hash.c]
        │     └── Esys_Hash()          → SHA-256（32バイト）
        ├── tpmLoadExternal(esys, X/Y, &key)      [tpm_load_external.c]
        │     └── Esys_LoadExternal()  → 一時公開鍵ハンドル
        ├── tpmVerify(esys, key, digest, R/S)     [tpm_verify.c]
        │     └── Esys_VerifySignature() → 検証成功 / 失敗
        └── tpmFlush(esys, key)                   [tpm_flush.c]
              └── Esys_FlushContext() → 一時公開鍵を解放（検証失敗時も実行）
```

### 入力ファイル

| ファイル | 形式 | 内容 | VxWorks側での処理 |
|---|---|---|---|
| `data.bin` | 生バイナリ | 署名対象そのもの。例：`Hello World\n` | TPMのSHA-256入力 |
| `public.pem` | PEM / SubjectPublicKeyInfo | PCのP-256公開鍵 | OpenSSLでX/Yを抽出しTPMへ一時ロード |
| `signature.der` | ASN.1 DER | ECDSA署名 `(r, s)` | OpenSSLでR/Sを抽出しTPM検証 |
| `private.pem` | PEM | PCの署名秘密鍵 | **VxWorksには渡さない** |

**署名対象のバイト列は完全一致が必要。** `Hello World` と `Hello World\n` は異なるハッシュになる。

## 2. 共通データ型と値の読み方

### 2.1 独自構造体 `PcSignInput`

定義元：`pcsign_tpmverify.h`

```c
typedef struct {
    TPM2B_MAX_BUFFER data;
    uint8_t public_xy[64];
    uint8_t signature_rs[64];
} PcSignInput;
```

| メンバー | 型・大きさ | 今回の値 | 用途 |
|---|---|---|---|
| `data` | `TPM2B_MAX_BUFFER` | `data.bin`の全内容 | TPMへのハッシュ入力。`size`が有効長 |
| `public_xy[0..31]` | 32バイト | P-256公開鍵のX座標 | 固定長・ビッグエンディアン |
| `public_xy[32..63]` | 32バイト | P-256公開鍵のY座標 | 固定長・ビッグエンディアン |
| `signature_rs[0..31]` | 32バイト | ECDSA署名のr値 | 固定長・ビッグエンディアン |
| `signature_rs[32..63]` | 32バイト | ECDSA署名のs値 | 固定長・ビッグエンディアン |

### 2.2 TPM/ESAPI共通の型・定数

| 名前 | 意味 | 今回の使用方法 |
|---|---|---|
| `ESYS_CONTEXT *` | 初期化済みESAPIコンテキスト | `user.c`から渡す。ここでは初期化/終了しない |
| `ESYS_TR` | ESAPIが管理するオブジェクト参照 | 一時公開鍵の参照を保持 |
| `ESYS_TR_NONE` | ESAPIの参照なし / 認可セッションなし | 最初のハンドル値、および認可不要のコマンドで使用 |
| `TSS2_RC` | TPM/TSSコマンドの戻りコード | `TSS2_RC_SUCCESS`か確認 |
| `TSS2_RC_SUCCESS` | API成功コード | `0`相当の成功判定 |
| `TPM2_ALG_SHA256` | SHA-256アルゴリズムID | ハッシュ計算、鍵Nameのハッシュ、ECDSA署名のハッシュ指定 |
| `TPM2_ALG_ECC` | ECC公開鍵の型 | 外部公開鍵をECCとしてロード |
| `TPM2_ALG_ECDSA` | ECDSA署名方式 | 検証署名の方式 |
| `TPM2_ECC_NIST_P256` | NIST P-256曲線 | 公開鍵の曲線を固定 |
| `TPM2_ALG_NULL` | 指定の方式を使わない | 対称暗号・公開鍵スキーム・KDFを無指定 |
| `TPM2_RH_NULL` | NULL階層 | ハッシュのhierarchy、および外部公開鍵のロード先に指定 |
| `TPMA_OBJECT_SIGN_ENCRYPT` | オブジェクトが署名/検証用途である属性 | 外部鍵の属性に設定 |
| `TPMA_OBJECT_USERWITHAUTH` | ユーザー認可を利用できる属性 | 外部鍵の属性に設定（公開鍵検証で秘密の認可値を設定する意味ではない） |

**注意：** `TPM2_RH_NULL` と `ESYS_TR_NONE` は用途が異なる。前者はTPM階層の値、後者はESAPIのハンドル/セッションの不在を表す。

---

## 3. 代表関数：`pcsignTpmVerify()`

**配置**：`pcsign_tpmverify.c`  
**役割**：処理順序の管理・成功/失敗判定・一時鍵の後片付け。TPM初期化はしない。

```c
int pcsignTpmVerify(ESYS_CONTEXT *esys, const char *directory);
```

| 引数 | 今回の値・例 | 説明 |
|---|---|---|
| `esys` | `user.c`で初期化済みのポインタ | TPM操作を実行するESAPIの状態 |
| `directory` | `"/ram0/pcsign_tpmverify"`（例） | 3ファイルを置いたディレクトリ。パスは環境に合わせる |
| 戻り値 | `0` / `-1` | すべて成功 / 読み込み・解析・TPM処理・署名不一致・解放等の失敗 |

内部の主な変数：`PcSignInput input`、`TPM2B_DIGEST digest = {0}`、`ESYS_TR key = ESYS_TR_NONE`、`int result = -1`。

**実行順序**：`opensslParseFiles` → `tpmHash` → `tpmLoadExternal` → `tpmVerify` → `tpmFlush`。途中エラーでも、鍵のロード後なら`cleanup`で`FlushContext`を試す。公開鍵の永続化やNV保存は行わない。

## 4. OpenSSLラッパー：`opensslParseFiles()`

**配置**：`openssl_parse.c`  
**役割**：3ファイルの読み込み、PEM公開鍵のX/Y抽出、DER署名のr/s抽出。TPMコマンドやSHA-256計算はしない。

```c
int opensslParseFiles(const char *directory, PcSignInput *out);
```

| 引数 | 設定値 | 意味 |
|---|---|---|
| `directory` | 代表関数から受け取るフォルダ | 入力ファイルの所在 |
| `out` | `&input` | 読み込み結果をまとめる出力構造体 |
| 戻り値 | `0` / `-1` | 形式・読み込み・曲線・値の妥当性確認の成否 |

### 4.1 ファイル操作・標準C API

| API | 主な引数・今回の値 | 戻り値/説明 |
|---|---|---|
| `snprintf(dst, cap, "%s/%s", dir, name)` | `dst=path[512]`、`name` は `data.bin`等 | ディレクトリとファイル名を結合。長さ超過は拒否 |
| `fopen(path, "rb")` | 3ファイルをバイナリモードで開く | 成功時`FILE *`、失敗時NULL |
| `fread(buffer, 1, capacity, f)` | ファイル全体の読み込み | 読み込んだバイト数。大きさ超過・I/Oエラーを確認 |
| `fgetc(f)` | バッファを満たしたときに追加1バイトを確認 | 上限超過していないか判断 |
| `ferror(f)` | 読み込みストリーム | ファイル読み込み失敗の検出 |
| `fclose(f)` | 開いたファイル | ファイルを閉じる |
| `memset(out, 0, sizeof(*out))` | 入出力構造体 | 初期化、失敗時の内容クリア |

**データサイズ**：`TPM2B_MAX_BUFFER.data`ではなく `TPM2B_MAX_BUFFER.buffer` に格納し、`data.size = 読込バイト数`を設定。`TPM2B_MAX_BUFFER`のサイズ上限以下だけに対応（今回のHello World程度を想定）。

### 4.2 OpenSSL公開鍵解析API

| API | 引数・今回の値 | 得られる結果 / 使う理由 |
|---|---|---|
| `PEM_read_PUBKEY(f, NULL, NULL, NULL)` | `f=public.pem` | `EVP_PKEY *pub`。PEM公開鍵を解析 |
| `EVP_PKEY_is_a(pub, "EC")` | `"EC"` | 楕円曲線公開鍵か確認。`1`を期待 |
| `EVP_PKEY_get_utf8_string_param(pub, OSSL_PKEY_PARAM_GROUP_NAME, group, sizeof(group), &group_len)` | `group[80]` | 曲線名を取得 |
| `strcmp(group, "prime256v1")` / `strcmp(group, "secp256r1")` | いずれかに一致 | NIST P-256だけ許可。別曲線は拒否 |
| `EVP_PKEY_get_bn_param(pub, OSSL_PKEY_PARAM_EC_PUB_X, &x)` | X座標 | `BIGNUM *x`を取得 |
| `EVP_PKEY_get_bn_param(pub, OSSL_PKEY_PARAM_EC_PUB_Y, &y)` | Y座標 | `BIGNUM *y`を取得 |
| `BN_bn2binpad(x, out->public_xy, 32)` | 32バイト固定 | Xを左ゼロ埋めのビッグエンディアンに変換 |
| `BN_bn2binpad(y, out->public_xy + 32, 32)` | 32バイト固定 | Yを同じ形式に変換 |
| `EVP_PKEY_free(pub)` / `BN_free(x/y)` | 使用後 | OpenSSLの動的オブジェクトを解放 |

**構造体とメンバーの関係**：`EVP_PKEY`はOpenSSL内部構造を直接触らずAPI経由でアクセスする抽象型。`OSSL_PKEY_PARAM_GROUP_NAME`、`OSSL_PKEY_PARAM_EC_PUB_X/Y`は**構造体メンバーではなく名前付きパラメータのキー**。OpenSSL 3系の非推奨`EC_KEY_*`系に依存しない。公式OpenSSL 3.1の汎用鍵パラメータAPIに沿う方式。

### 4.3 OpenSSL署名解析API

| API | 引数・今回の値 | 得られる結果 / 使う理由 |
|---|---|---|
| `d2i_ECDSA_SIG(NULL, &p, (long)n)` | `p=DERバイト列`、`n=読み込み長` | DERから`ECDSA_SIG *`を生成 |
| `ECDSA_SIG_get0(signature, &r, &s)` | 署名オブジェクト | ECDSAの整数`r`, `s`を参照取得（所有権は移らない） |
| `BN_is_zero(r/s)` | r/s | 0を拒否 |
| `BN_is_negative(r/s)` | r/s | 負数を拒否 |
| `BN_bn2binpad(r, out->signature_rs, 32)` | 32バイト | 署名rをTPM向けバイト列へ |
| `BN_bn2binpad(s, out->signature_rs + 32, 32)` | 32バイト | 署名sをTPM向けバイト列へ |
| `ECDSA_SIG_free(signature)` | 解析完了後 | DER解析で確保したリソースを解放 |

その他の値：`uint8_t der[128]`は今回のDER署名読込用の固定バッファ。`p == der + n`の確認でDER全体が消費されたことを確認する。`r/s`が楕円曲線の位数未満かどうかはこの関数で明示的には調べず、署名検証の結果にも依存する。

---

## 5. TPMハッシュラッパー：`tpmHash()`

**配置**：`tpm_hash.c`  
**役割**：短い`data.bin`全体をTPMへ渡し、TPM内部でSHA-256を計算する。

```c
int tpmHash(ESYS_CONTEXT *esys,
            const TPM2B_MAX_BUFFER *input,
            TPM2B_DIGEST *digest);
```

### 5.1 ラッパー引数

| 引数 | 今回の値 | 説明 |
|---|---|---|
| `esys` | 初期化済み`ESYS_CONTEXT *` | TPMへのコマンド経路 |
| `input` | `&input.data` | ファイルバイト列とそのサイズ |
| `digest` | `&digest` | 計算結果を返す出力バッファ |
| 戻り値 | `0` / `-1` | SHA-256計算成功 / 異常 |

### 5.2 TPM API `Esys_Hash()`

```c
Esys_Hash(esys,
          ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE,
          input, TPM2_ALG_SHA256, TPM2_RH_NULL,
          &generated, &ticket);
```

| API引数 | 今回の設定値 | 意味 |
|---|---|---|
| `esysContext` | `esys` | ESAPIコンテキスト |
| `shandle1` | `ESYS_TR_NONE` | 認可セッション1を使わない |
| `shandle2` | `ESYS_TR_NONE` | 認可セッション2を使わない |
| `shandle3` | `ESYS_TR_NONE` | 認可セッション3を使わない |
| `data` | `input` | ハッシュ対象の`TPM2B_MAX_BUFFER` |
| `hashAlg` | `TPM2_ALG_SHA256` | ハッシュ計算にSHA-256を指定 |
| `hierarchy` | `TPM2_RH_NULL` | NULL階層のチケット扱いを指定。署名検証のための通常のダイジェストが主目的 |
| `outHash` | `&generated` | TPMが確保する`TPM2B_DIGEST *`を受け取る |
| `validation` | `&ticket` | `TPMT_TK_HASHCHECK *`を受け取る。今回は利用せず解放 |
| 戻り値 | `TSS2_RC` | 成功時`TSS2_RC_SUCCESS` |

### 5.3 使用構造体とメンバー

| 構造体 | メンバー | 今回の値 | 説明 |
|---|---|---|---|
| `TPM2B_MAX_BUFFER` | `size` | `data.bin`のバイト数 | 有効な入力長 |
| `TPM2B_MAX_BUFFER` | `buffer[]` | `data.bin`の内容 | TPMへ送るバイト列 |
| `TPM2B_DIGEST` | `size` | **32**を期待 | SHA-256の出力長 |
| `TPM2B_DIGEST` | `buffer[]` | TPMが計算した32バイト | 次の署名検証に使用 |
| `TPMT_TK_HASHCHECK` | `tag`, `hierarchy`, `digest` | TPMが返す（今回参照しない） | ハッシュ検証関連チケット |

返された`generated`の`size == 32`を検査し、`memcpy(digest, generated, sizeof(*digest))`でコピー。`generated`、`ticket`は`Esys_Free()`で解放。

---

## 6. TPM外部鍵ロードラッパー：`tpmLoadExternal()`

**配置**：`tpm_load_external.c`  
**役割**：OpenSSLが抽出したP-256公開鍵をTPMの**一時オブジェクト**として読み込む。

```c
int tpmLoadExternal(ESYS_CONTEXT *esys,
                    const uint8_t xy[64],
                    ESYS_TR *object_handle);
```

| 引数 | 今回の値 | 説明 |
|---|---|---|
| `esys` | 初期化済みコンテキスト | TPMコマンド用 |
| `xy` | 公開鍵X/Y各32バイト | ファイルから取り出した公開鍵。秘密鍵ではない |
| `object_handle` | `&key` | ロード成功時にTPM公開鍵を参照するESAPIハンドルを返す |
| 戻り値 | `0` / `-1` | ロード成功 / 異常 |

### 6.1 `TPM2B_PUBLIC pub` の全設定メンバー

コードでは`TPM2B_PUBLIC pub = {0}`で全体をゼロ初期化し、下記を設定している。

| メンバー階層 | 設定値 | 詳細 |
|---|---|---|
| `pub.size` | **0（ゼロ初期化のまま）** | `TPM2B_PUBLIC`のサイズフィールド。シリアライズはESAPI実装に依存するため、対象v2.2.0で受理されるか要確認 |
| `pub.publicArea.type` | `TPM2_ALG_ECC` | ECC公開鍵オブジェクト |
| `pub.publicArea.nameAlg` | `TPM2_ALG_SHA256` | TPMオブジェクトのName生成に使用するハッシュ。**署名対象データのハッシュ指定とは別** |
| `pub.publicArea.objectAttributes` | `TPMA_OBJECT_SIGN_ENCRYPT \| TPMA_OBJECT_USERWITHAUTH` | 署名/検証用途、ユーザー認可可能な属性 |
| `pub.publicArea.authPolicy.size` | 0（暗黙） | 認可ポリシーを設定しない |
| `pub.publicArea.parameters.eccDetail.symmetric.algorithm` | `TPM2_ALG_NULL` | 対称暗号設定なし |
| `pub.publicArea.parameters.eccDetail.scheme.scheme` | `TPM2_ALG_NULL` | 鍵側の固有署名スキーム指定なし。検証時に`TPMT_SIGNATURE`側でECDSAを指定 |
| `pub.publicArea.parameters.eccDetail.curveID` | `TPM2_ECC_NIST_P256` | 楕円曲線NIST P-256 |
| `pub.publicArea.parameters.eccDetail.kdf.scheme` | `TPM2_ALG_NULL` | 鍵導出を使用しない |
| `pub.publicArea.unique.ecc.x.size` | `32` | X座標の有効長 |
| `pub.publicArea.unique.ecc.x.buffer[0..31]` | 公開鍵X | 32バイト固定ビッグエンディアン |
| `pub.publicArea.unique.ecc.y.size` | `32` | Y座標の有効長 |
| `pub.publicArea.unique.ecc.y.buffer[0..31]` | 公開鍵Y | 32バイト固定ビッグエンディアン |

ゼロ初期化で残るその他のメンバーは、利用する`type=ECC`、`scheme=NULL`の条件では直接設定していない。

### 6.2 TPM API `Esys_LoadExternal()`

```c
Esys_LoadExternal(esys,
                  ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE,
                  NULL, &pub, TPM2_RH_NULL, object_handle);
```

| API引数 | 今回の値 | 説明 |
|---|---|---|
| `esysContext` | `esys` | ESAPIコンテキスト |
| `shandle1/2/3` | 全て`ESYS_TR_NONE` | 認可セッションなし |
| `inPrivate` | `NULL` | 秘密鍵をTPMへロードしない |
| `inPublic` | `&pub` | TPM形式の公開鍵情報 |
| `hierarchy` | `TPM2_RH_NULL` | NULL階層の外部公開鍵としてロード |
| `objectHandle` | `object_handle` | ロード結果のESAPI一時オブジェクト参照 |
| 戻り値 | `TSS2_RC` | 成功時`TSS2_RC_SUCCESS` |

**永続化されない理由**：`Esys_LoadExternal`で一時オブジェクトを作るだけで、`Esys_EvictControl`やNV書き込みは呼んでいない。使用後は`Esys_FlushContext`する。

---

## 7. TPM署名検証ラッパー：`tpmVerify()`

**配置**：`tpm_verify.c`  
**役割**：一時ロードしたP-256公開鍵を使い、TPM内部でECDSA署名がダイジェストに適合するか確認する。

```c
int tpmVerify(ESYS_CONTEXT *esys,
              ESYS_TR object_handle,
              const TPM2B_DIGEST *digest,
              const uint8_t rs[64]);
```

| 引数 | 今回の値 | 意味 |
|---|---|---|
| `esys` | 初期化済みESAPI | TPM操作 |
| `object_handle` | `LoadExternal`で取得した`key` | 使用する公開鍵 |
| `digest` | SHA-256結果32バイト | 署名対象のハッシュ値 |
| `rs` | r(32)＋s(32) | OpenSSLで抽出したECDSA署名 |
| 戻り値 | `0` / `-1` | 検証成功 / 不一致も含む異常 |

### 7.1 `TPMT_SIGNATURE sig` のメンバー

| メンバー | 今回の値 | 説明 |
|---|---|---|
| `sig.sigAlg` | `TPM2_ALG_ECDSA` | 署名形式がECDSAであることを指定 |
| `sig.signature.ecdsa.hash` | `TPM2_ALG_SHA256` | ECDSAの対象ハッシュはSHA-256 |
| `sig.signature.ecdsa.signatureR.size` | `32` | rの長さ |
| `sig.signature.ecdsa.signatureR.buffer[]` | `rs[0..31]` | ECDSA署名r値 |
| `sig.signature.ecdsa.signatureS.size` | `32` | sの長さ |
| `sig.signature.ecdsa.signatureS.buffer[]` | `rs[32..63]` | ECDSA署名s値 |

### 7.2 TPM API `Esys_VerifySignature()`

```c
Esys_VerifySignature(esys, object_handle,
                     ESYS_TR_NONE, ESYS_TR_NONE, ESYS_TR_NONE,
                     digest, &sig, &ticket);
```

| API引数 | 今回の値 | 意味 |
|---|---|---|
| `esysContext` | `esys` | TPM操作のコンテキスト |
| `keyHandle` | `object_handle` | 一時ロードした公開鍵 |
| `shandle1/2/3` | 全て`ESYS_TR_NONE` | 追加認可セッションなし |
| `digest` | `TPM2B_DIGEST *` | 署名対象データのSHA-256 |
| `signature` | `&sig` | ECDSA・SHA-256・R/Sを格納した構造体 |
| `validation` | `&ticket` | `TPMT_TK_VERIFIED *`。今回は内容を使わず解放 |
| 戻り値 | `TSS2_RC` | `TSS2_RC_SUCCESS`なら署名検証成功、それ以外は失敗 |

`TPMT_TK_VERIFIED`は通常`tag`、`hierarchy`、`digest`等を含む検証チケット。今回のコードでは`Esys_Free(ticket)`だけ実行し、別のTPMコマンドには渡さない。

---

## 8. TPM解放ラッパー：`tpmFlush()`

**配置**：`tpm_flush.c`  
**役割**：`LoadExternal`で作った一時公開鍵オブジェクトをTPMから削除する。

```c
int tpmFlush(ESYS_CONTEXT *esys, ESYS_TR object_handle);
```

| 引数 | 今回の値 | 説明 |
|---|---|---|
| `esys` | 初期化済みESAPI | TPM操作 |
| `object_handle` | `LoadExternal`で取得した`key` | 解放対象の一時オブジェクト |
| 戻り値 | `0` / `-1` | 解放成功 / 失敗 |

```c
Esys_FlushContext(esys, object_handle);
```

| API引数 | 今回の値 | 説明 |
|---|---|---|
| `esysContext` | `esys` | TPM操作用コンテキスト |
| `flushHandle` | `object_handle` | TPMから削除する一時オブジェクトの参照 |

**呼び出し条件**：ロード成功後は署名成功でも署名失敗でも`cleanup`で実行。`Esys_FlushContext()`の失敗も代表関数の失敗として返す。

### 8.1 `Esys_Free()`との違い

| API | 解放対象 |
|---|---|
| `Esys_Free(generated/ticket)` | ESAPIがホスト側メモリに割り当てた出力構造体 |
| `Esys_FlushContext(esys, key)` | TPM内部の一時公開鍵オブジェクト（とそのESAPI参照） |
| `Esys_Finalize()` | ESAPIコンテキスト全体。**今回のモジュールでは呼ばない** |

---

## 9. 全API・構造体の対応早見表

| ファイル | ラッパー | メインAPI | 主な構造体 | 今回の固定設定 |
|---|---|---|---|---|
| `pcsign_tpmverify.c` | `pcsignTpmVerify` | 各ラッパーを順次呼出 | `PcSignInput`, `TPM2B_DIGEST`, `ESYS_TR` | 最終成功`0`、失敗`-1` |
| `openssl_parse.c` | `opensslParseFiles` | `PEM_read_PUBKEY`, `EVP_PKEY_get_*_param`, `d2i_ECDSA_SIG`, `ECDSA_SIG_get0` | `EVP_PKEY`, `BIGNUM`, `ECDSA_SIG` | `prime256v1` / `secp256r1`、X/Y/R/S 各32B |
| `tpm_hash.c` | `tpmHash` | `Esys_Hash` | `TPM2B_MAX_BUFFER`, `TPM2B_DIGEST`, `TPMT_TK_HASHCHECK` | `TPM2_ALG_SHA256`, `TPM2_RH_NULL` |
| `tpm_load_external.c` | `tpmLoadExternal` | `Esys_LoadExternal` | `TPM2B_PUBLIC`, `ESYS_TR` | `ECC`, `NIST_P256`, `SHA256`, `TPM2_ALG_NULL` |
| `tpm_verify.c` | `tpmVerify` | `Esys_VerifySignature` | `TPMT_SIGNATURE`, `TPMT_TK_VERIFIED` | `ECDSA`, `SHA256`, 32BのR/S |
| `tpm_flush.c` | `tpmFlush` | `Esys_FlushContext` | `ESYS_TR` | 一時鍵解放 |

## 10. PC側の生成方法と試験結果の期待値

```sh
printf 'Hello World\n' > data.bin
openssl ecparam -name prime256v1 -genkey -noout -out private.pem
openssl pkey -in private.pem -pubout -out public.pem
openssl dgst -sha256 -sign private.pem -out signature.der data.bin
openssl dgst -sha256 -verify public.pem -signature signature.der data.bin
```

期待：PC上で`Verified OK`。VxWorksへは`data.bin`、`public.pem`、`signature.der`のみを配置する。

| 試験 | 条件 | TPM側で期待する結果 |
|---|---|---|
| 正常系 | データと署名と公開鍵が対応 | PASS |
| データ改ざん | `data.bin`を1文字変更 | FAIL |
| 署名改ざん | `signature.der`を変更 | FAIL（DER解析段階で失敗する場合もある） |
| 公開鍵不一致 | 別のP-256公開鍵を指定 | FAIL |
| サイズ超過 | ハッシュ入力上限を超えるファイル | 読み込み時にFAIL |
| 非P-256鍵 | 別曲線の公開鍵 | 解析時にFAIL |

## 11. 実機・ビルドに関する確認事項

1. **ESAPI 2.2.0のヘッダーで確認する**：`Esys_Hash`、`Esys_LoadExternal`、`Esys_VerifySignature`、`Esys_FlushContext`の宣言、`ESYS_TR`と`TPM2_RH_NULL`の互換性を確認する。バージョン間のAPI差異に注意。
2. **OpenSSL 3.1.1でビルドする**：パラメータ取得APIはOpenSSL 3系の書き方を使用しているが、実際の3.1.1・VxWorksでの動作は未検証。
3. **`TPM2B_PUBLIC`のサイズ欄**：ソースでは`pub.size`を明示設定していない。対象ESAPIでのマーシャリング・受理状況を確認する。
4. **TPM側の機能サポート**：対象TPMがSHA-256、NIST P-256、ECDSA、外部公開鍵のロード・検証をサポートする必要がある。
5. **TPM初期化は呼び出し元の責任**：VxWorksのドライバ、TCTI、ESAPIが利用可能な`ESYS_CONTEXT *`を作ってから呼ぶ。
6. **ファイルパス**：`/ram0/pcsign_tpmverify`は例。起動時の配置先に合わせる。
7. **公開鍵の信頼性は試験対象外**：攻撃者が`data.bin`、`signature.der`、`public.pem`を全部置き換えられるなら、署名が正しくても正規の発行者を保証できない。
8. **結果の意味**：現行ラッパーは署名不一致とAPI/ファイルエラーを同じ`-1`で返す。必要なら将来エラー種別を分ける。

### Linuxでのコンパイル確認（TPM実機不要）

```sh
gcc -std=c11 -Wall -Wextra -Werror=deprecated-declarations \
  $(pkg-config --cflags openssl tss2-esys) \
  -c pcsign_tpmverify.c openssl_parse.c tpm_hash.c \
     tpm_load_external.c tpm_verify.c tpm_flush.c
```

このコマンドで確認できるのは、**Linux側に入れたヘッダー/ライブラリ版でのコンパイル可否**。VxWorks対象ツールチェーンでのビルド・TPM実動作は別途確認する。

## 12. 参考仕様

- [tpm2-tss公式リポジトリ](https://github.com/tpm2-software/tpm2-tss)
- [ESAPI `Esys_VerifySignature` API資料](https://tpm2-tss.readthedocs.io/en/stable/group___esys___verify_signature.html)
- [OpenSSL 3.1：EVP_PKEYパラメータ取得](https://docs.openssl.org/3.1/man3/EVP_PKEY_gettable_params/)
- [OpenSSL 3.1：移行ガイド](https://docs.openssl.org/3.1/man7/migration_guide/)

---

**本資料の記述基準**：実コードで使用している設定値と、TPM/暗号アルゴリズムの役割を分離して説明した。未実施の実機試験・ライブラリ版の確認を「検証済み」とは扱わない。
