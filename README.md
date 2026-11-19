# PC署名 → VxWorks TPM検証（ESAPI）

## 構成

```text
user.c（既存）
  └─ pcsignTpmVerify(esys, directory)
       ├─ opensslParseFiles() : ファイル読み込み・PEM/DER解析
       ├─ tpmHash()           : Esys_Hash
       ├─ tpmLoadExternal()   : Esys_LoadExternal
       ├─ tpmVerify()         : Esys_VerifySignature
       └─ tpmFlush()          : Esys_FlushContext （失敗時も実施）
```

| ファイル | 公開関数 | 担当 |
|---|---|---|
| pcsign_tpmverify.c | pcsignTpmVerify | 処理順の制御と後始末 |
| openssl_parse.c | opensslParseFiles | data.bin/public.pem/signature.der 読み込み・変換 |
| tpm_hash.c | tpmHash | TPMのSHA-256 |
| tpm_load_external.c | tpmLoadExternal | 公開鍵を一時ロード |
| tpm_verify.c | tpmVerify | ECDSA P-256署名をTPMで検証 |
| tpm_flush.c | tpmFlush | 一時鍵を破棄 |
| pcsign_tpmverify.h | - | API・データ構造の共有 |

## 前提
- VxWorks側でTPM/TCTI/ESAPI初期化済み。`ESYS_CONTEXT *`を代表関数に渡す。
- 使用するAPIはtpm2-tss ESAPI v2.2.0想定。OpenSSL 3.1.1の非推奨EC_KEY APIは使わない。
- 起動時のディレクトリに `data.bin`, `public.pem`, `signature.der` が存在する。
- 署名方式はECDSA/NIST P-256、ハッシュはSHA-256。
- `data.bin`は `TPM2B_MAX_BUFFER` 以下。超えた場合はエラーにする。
- 公開鍵の永続化、NV書き込み、TPMの初期化は行わない。

## PC側のテストデータ作成

```sh
printf 'Hello World\n' > data.bin
openssl ecparam -name prime256v1 -genkey -noout -out private.pem
openssl pkey -in private.pem -pubout -out public.pem
openssl dgst -sha256 -sign private.pem -out signature.der data.bin
openssl dgst -sha256 -verify public.pem -signature signature.der data.bin
```

`private.pem` はVxWorksへ持ち込まずPC側で保護すること。

## LinuxでTPM実機なしのビルド確認

```sh
gcc -std=c11 -Wall -Wextra -Werror=deprecated-declarations \
  $(pkg-config --cflags openssl tss2-esys) \
  -c pcsign_tpmverify.c openssl_parse.c tpm_hash.c \
     tpm_load_external.c tpm_verify.c tpm_flush.c
```

TPM実機がなくてもコンパイル可能。ただし実行試験はできない。
このビルドはホスト側のライブラリ版に対する検査であり、VxWorks環境そのもののビルド保証ではない。

## セキュリティ上の範囲

公開鍵もデータと同じディレクトリから受け取るため、確認できるのは署名とデータの整合性。
公開鍵の信頼性や送信者の正当性を保証する試験ではない。
