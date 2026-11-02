# 前回（672e4a0時点）の記録

このファイルは前回の確認記録。作業場所の経緯は過去の記録であり必須パスではない。
今回の移動・再検証は [検証記録](layout_validation.md) を参照。以下のソース参照は現行の移動先へ更新した。

# 整理後の確認記録（2026-11-02）

作業ブランチ: `refactor/readable-components-review-20261102`。
作業先: `~/dev/tz/trustzone-readable-review`。
受け取り仕上げ時に `/tmp/trustzone-readable-review` から常用場所へ移動。
`git worktree move` は別ファイルシステムのため失敗したので、
全ファイルのSHA-256・symlink一致を確認してコピーし、`git worktree repair` で登録先を更新した。
Gitの新しい作業先とブランチを確認後、旧/tmpコピーを削除した。
基準 `offline-transfer-zcu106-b2-20261031-r1` は9c06d33。
開始時には `refactor/readable-components-20261102` に多数の未コミット変更があった。
基準から別worktreeを作成し、その変更と未追跡ファイルをコピーして整理を引き継いだ。
元worktreeのブランチ・ファイルは変更していない。ファイル移動・JSON削除なし。

## 今回実行した確認

環境: Ubuntu Clang/LLD 21.1.8、GNU make、Python 3。
基準ソースはgit archiveで `/tmp/trustzone-readable-baseline` に展開した。

| 確認 | 結果 |
|---|---|
| 基準版 `make standalone-optee` / 整理版 `make bl33` | 両方成功 |
| 基準版・整理版の署名BL33（index=17、signed=1、canary=1） | 両方成功 |
| 基準版・整理版の物理BL33（index=15、dedicated=1、canary=1） | 両方成功 |
| `llvm-readelf-21 -l` 比較 | 基本・物理のProgram Header一致。署名は下記差分あり |
| 3構成のsymbol payload比較 | zcu106_b2_runtime_ready、b2_test_selection、b2_review_features、b2_physical_reservation一致 |
| `check_b2_physical_reservation.py`（物理BL33のみ） | STATIC PASS、HARDWARE BLOCKED |
| `check_signed_layout.py`（署名BL33） | PASS、HARDWARE BLOCKED |
| SHM契約ホスト試験10ケース | 基準・整理版とも成功 |
| ページテーブル比較 | 通常・署名構成とも8入力の戻り値・全バイト一致。NULL拒否も成功 |
| `python3 -m compileall -q scripts` | 成功 |
| 未対応TOOLCHAIN、PLATFORM、PROFILE、外部Clang、Secure SGI | 明示エラーで停止 |
| `git diff --check` | 成功 |

BL33の整理版は `make bl33`、`make PROFILE=signed bl33`、
`make PROFILE=physical bl33`。基準の追加構成はそれぞれ
`B2_TEST_INDEX=17 B2_SIGNED_TA_PROFILE=1 B2_VALIDATION_PTA=1 BUILD=build/signed`、
`B2_TEST_INDEX=15 B2_PHYSICAL_DEDICATED=1 B2_VALIDATION_PTA=1 BUILD=build/physical`
を `make standalone-optee` に渡した。

SHM試験の再実行:

```sh
clang -std=c11 -Wall -Wextra -Werror -DTEE_CLIENT_HOST_TEST \
  -Iruntime/include -Iplatform/zcu106/b2 -Inormal_world/client \
  tests/host/client_shm_contract.c normal_world/client/client.c \
  -o /tmp/tz-client-contract
for i in 0 1 2 3 4 5 6 7 8 9; do /tmp/tz-client-contract "$i" || break; done
```

正常最小領域・予約全域、能力なし、SMC失敗、基点の前後逸脱、サイズ不足、
overflow、非整列、cache契約不一致を調べる。SMC順序と回数も検査する。
実機cache/SMCを検証したものではない。

ページテーブル比較の再実行（基準ソースの展開先を合わせる）:

```sh
for profile in 0 1; do
  clang -std=c11 -Wall -Wextra -Werror -DTEE_CLIENT_HOST_TEST \
    -DB2_SIGNED_TA_PROFILE=$profile -Iplatform/zcu106/b2 \
    -Dzcu106_b2_build_page_tables=baseline_build_page_tables \
    -c /tmp/trustzone-readable-baseline/platform/zcu106/b2/client_mmu.c \
    -o /tmp/tz-baseline-mmu-$profile.o || break
  clang -std=c11 -Wall -Wextra -Werror -DTEE_CLIENT_HOST_TEST \
    -DB2_SIGNED_TA_PROFILE=$profile -Iplatform/zcu106/b2 \
    tests/host/page_tables_equivalence.c platform/zcu106/b2/client_mmu.c \
    /tmp/tz-baseline-mmu-$profile.o -o /tmp/tz-mmu-$profile || break
  /tmp/tz-mmu-$profile || break
done
```

## ELFの差と意味の保存

署名構成の先頭PT_LOADはbase=0x10200000、R E、alignment=0x10000を維持し、
FileSiz/MemSizが0x12884から0x129b4へ304バイト増えた。
その他のProgram Headerは一致。基本・物理構成は全Program Headerが一致。
リンカASSERTは3構成とも通過。全symbol・全命令の一致は主張しない。
主要ABI定数の一致とホスト試験は確認したが、全実機試験の同等性の証明ではない。

ページ写像の条件連鎖は、前段が成功した場合だけ次を実行する形へ展開した。
署名table descriptorは従来通り先行失敗時も設定し、falseを返す。
SMC・例外処理・メモリ配置の機能修正は意図していない。
既存変更の全数値項目をenum化したわけではなく、レビュー21項目は既存の仕様表も参照する。

## 未実行と残る制約

`make bl31` は `command -v aarch64-linux-gnu-gcc` で停止した。
third_partyには来歴JSONのみで外部ソースはない。
BL31・SDK・TA-A/B・外部署名TA・BL32の新Make経路による実ビルドは未実行。
追加PTAの実コンパイル、署名監査パッチの実ソースへの適用、署名bundle生成も未実行。
GCC BL33、Vitis、XSDB、ZCU106での機能・異常系・性能は未実行。
make -nを成功の根拠にはしていない。

旧integration/review/signed-profile検査は旧出力先と旧ビルド記録に結合する。
新MakeのBL31/BL32/BL33を統合した確認経路は未対応。
物理予約検査は今回BL33だけを入力し、BL32との重なりまでは未検査。
実機状態はruntime-ready/physical-reviewedを変更せず、ロードBLOCKEDを維持する。

外部ビルドの設定・版・compiler・鍵を変える際はREADME記載通り出力を退避して作り直す。
旧自動化は同じ場所に保全し、READMEで通常入口と区別した。
