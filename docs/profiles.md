# 署名TA・物理試験・試験選択

[入口](../README.md) · [基本ビルド](build.md) · [実機点検](zcu106_run.md)

## 署名TA・物理試験

署名TAはEarly TAの検査と異なり、Normal側のRPCから供給した署名ファイルをCoreで検証する。
試験鍵はgitへ追加しない。次はコマンドの実行順。新環境では未実行。

```sh
python3 scripts/make_signed_bundle.py keys --keys build/signed-test-keys
# local.mkへTA_SIGN_KEY=.../trusted.pem、TA_PUBLIC_KEY=.../trusted.pub.pemを絶対パスで指定
make PROFILE=signed bl31
make PROFILE=signed optee-sdk
make PROFILE=signed ta-a
make PROFILE=signed ta-b
make PROFILE=signed ta-external
make PROFILE=signed prepare-core
make PROFILE=signed bl32
make PROFILE=signed B2_TEST_INDEX=17 B2_SIGNED_TA_PROFILE=1 B2_VALIDATION_PTA=1 bl33
python3 scripts/make_signed_bundle.py bundle --keys build/signed-test-keys --ta-dir build/components/signed/ta-external --output build/components/signed/bundle --optee third_party/optee
```

`ta-external` のUUIDは `6dc1341c-534f-4642-9138-215876304910`。
bundle.binには正常・改変・別鍵のTAを収める。manifest.jsonは結果出力であり設定ではない。
`prepare-core` は元OP-TEEを変更せずビルド用コピーにcanary PTAと署名監査パッチを置く。
コピーが既存なら停止するので、再準備時はコピーを退避する。bl32の再実行は準備済みコピーを使う。
不一致のOP-TEE版では監査パッチが停止する。

```sh
make PROFILE=physical bl31
make PROFILE=physical optee-sdk
make PROFILE=physical ta-a
make PROFILE=physical ta-b
make PROFILE=physical prepare-core
make PROFILE=physical bl32
make PROFILE=physical B2_TEST_INDEX=15 B2_PHYSICAL_DEDICATED=1 B2_VALIDATION_PTA=1 bl33
python3 scripts/check_b2_physical_reservation.py --elf build/zcu106/physical/optee_bl33.elf --elf build/components/physical/optee/core/tee.elf --output build/components/physical/reservation.json
```

物理PROFILEはcanary＋physical PTAと固定予約0x10800000を追加する。
XMPU設定、他マスタ停止、DDR予約の実機レビューはこのコマンドに含まれない。
静的検査成功でもロード許可にならない。非固定の従来物理試験手順は旧資料に保全している。

## CoreとBL33の整合を検査する（Coreが必要、今回未実行）

```sh
python3 scripts/check_b2_review_profile.py --root . --platform zcu106 \
  --elf build/zcu106/physical/optee_bl33.elf \
  --core build/components/physical/optee/core/tee.elf \
  --conf build/components/physical/optee/conf.mk --output results/physical-profile.json
python3 scripts/check_signed_profile.py --platform zynqmp \
  --core build/components/signed/optee/core/tee.elf \
  --conf build/components/signed/optee/conf.mk \
  --bundle build/components/signed/bundle --keys build/signed-test-keys \
  --optee third_party/optee --output results/signed-profile.json
```

パスだけを新Makeに対応させた検査であり、Core・外部TAが不足する本環境での成功実績はない。
`--core` と `--conf` は同じOP-TEE出力から指定する。別々のビルド結果を混ぜない。

## 個別試験の選択（新しい選択スクリプトは設けない）

`tests/validation/functional.c` の子IDと、`tests/validation/review.c` の親ID表に対応する
既存Make変数を直接指定する。選択を変えてもCoreに必要な追加PTAを省けない。

| 指定 | 選択される範囲 |
|---|---|
| `make bl33` | selection=14の従来機能セット（元の既定値を維持） |
| `make B2_TEST_INDEX=0 bl33` | 機能13子試験。親21項目の一式ではない |
| `make PROFILE=signed bl33` | selection=17、署名＋canaryを含むreview。物理は含まず親はBLOCKED |
| `make PROFILE=signed B2_TEST_INDEX=16 bl33` | 署名TA単独（EXT-TA-V03/TEE-04） |
| `make PROFILE=physical bl33` | selection=15、専用物理probe |
| `make PROFILE=signed B2_REVIEW_PARENT=10 bl33` | review親10（EXT-AUTH-V01）を選ぶ例 |
| `make PROFILE=signed B2_TEST_INDEX=18 bl33` | 8系列の性能測定。Secure SGI/DMA性能は含まない |

親番号は [21項目表](b2_scope_21.md) の行順（1..21）、実装では `parent_ids` と照合する。
子単独は `functional.c` の `cases` 順（1..13）を `B2_TEST_INDEX` に指定する。
これら追加選択の今回の実ビルドは未実行。確認済みは基本・署名review・物理の3構成だけ。
旧JSONの `--test ID` 選択CLIは参照専用となり使用不可。

Secure SGI、DDR保護レビュー、runtime-readyは従来の無効状態を維持する。
