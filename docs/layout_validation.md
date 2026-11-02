# 現行入口と旧手順の分離：確認記録

現在の実ビルド・PC検証は [最新確認記録](pc_validation.md) を参照。以下の過去の確認状態や111版の実績を、現行の実機成功へ転記しない。

対象ブランチ `refactor/readable-components-review-20261102`。
開始commitは `672e4a05a8ed7be600d22d7713c2d77a5c79afc8`、開始時のgit statusはclean。
元の別worktree `trustzone-validation` の未コミット変更には触れていない。
変更は役割による移動、include/参照更新、旧CLIの参照専用化、検査ツールの入力パス指定、資料分割。
C/ASMの処理・日本語コメント・リンカ・メモリ属性は維持した。

## 実行結果

Clang/LLD 21.1.8。外部ソース・GCC・実機は追加していない。

| 確認 | 結果 |
|---|---|
| `make bl33` | 成功 |
| `make PROFILE=signed bl33` | 成功 |
| `make PROFILE=physical bl33` | 成功 |
| 上記3構成と672e4a0版ELFの比較 | Program Headerと全SHF_ALLOCセクションのアドレス・size・flags・alignment・ファイル内容が一致。NOBITSは配置・sizeを比較 |
| SHM契約ホスト試験 | 既存10ケース成功 |
| ページテーブルホスト比較 | 672e4a0版と通常/署名の両方で8範囲の戻り値・全バイト一致、NULL拒否も成功 |
| 署名BL33の `check_signed_layout.py` | PASS、実機BLOCKED |
| 物理BL33の `check_b2_physical_reservation.py` | STATIC PASS、実機BLOCKED。Coreは入力していない |
| `check_boot_plan.py --inventory` | 基本BL33のinventory生成成功 |
| `prepare_jtag_capture.py` | 基本・署名・物理BL33でTclとmetadata JSON生成成功 |
| 起動点検の明示BL33引数 | 選択したELFをinventory/Tclへ反映し、実行許可0・終了値2を維持 |
| 起動入力のhash未確認・入力とplanのELF不一致 | 拒否を確認 |
| Core/profile検査の明示Core/conf引数 | 合成confで指定パスの参照と不一致拒否を確認。実Coreの検査成功ではない |
| XMPU入力例・Tclゲート | 未確認証跡・未許可操作の拒否を確認。XSDB/MMIOなし |
| 現行Python CLI/import | `--help`・構文検査に成功、legacy importなし |
| legacy CLI全10本 | `--help` を含めUNSUPPORTEDで明示停止 |
| ローカルMarkdownリンク | legacy資料を含む全リンクのファイル存在を確認 |
| 旧パス検索 | 現行コード・Make・通常手順に残存なし。移動対応表・参照専用legacyの記録は除く |
| 差分の空白検査 | `git diff --check` 成功 |

ELFのdebug情報・ソースファイル名が変わるのでELF全バイト一致は要求していない。
今回の比較はリンクして配置されるコード・定数まで同じことを確認したもの。
実行回数・順序・ABI・cache属性の変更は意図せず、ハードウェアでの同等性の証明とは区別する。

## 再実行するコマンド

リポジトリ直下で実行する。ソース基準の抽出やホスト出力は一時ファイルでよい。

```sh
make bl33
make PROFILE=signed bl33
make PROFILE=physical bl33
clang -std=c11 -Wall -Wextra -Werror -DTEE_CLIENT_HOST_TEST \
  -Iruntime/include -Iplatform/zcu106/b2 -Inormal_world/client \
  tests/host/client_shm_contract.c normal_world/client/client.c -o /tmp/tz-shm-contract
for i in 0 1 2 3 4 5 6 7 8 9; do /tmp/tz-shm-contract "$i" || break; done
```

ページテーブル比較は `git show 672e4a0:platform/zcu106/b2/client_mmu.c` を
`/tmp/tz-baseline-mmu.c` に保存して次の既存試験をリンクする。

```sh
for profile in 0 1; do
  clang -std=c11 -Wall -Wextra -Werror -DTEE_CLIENT_HOST_TEST \
    -DB2_SIGNED_TA_PROFILE=$profile -Iplatform/zcu106/b2 \
    -Dzcu106_b2_build_page_tables=baseline_build_page_tables \
    -c /tmp/tz-baseline-mmu.c -o /tmp/tz-baseline-mmu-$profile.o || break
  clang -std=c11 -Wall -Wextra -Werror -DTEE_CLIENT_HOST_TEST \
    -DB2_SIGNED_TA_PROFILE=$profile -Iplatform/zcu106/b2 \
    tests/host/page_tables_equivalence.c platform/zcu106/b2/client_mmu.c \
    /tmp/tz-baseline-mmu-$profile.o -o /tmp/tz-page-tables-$profile || break
  /tmp/tz-page-tables-$profile || break
done
```

配置検査・回収計画のコマンドは [実機点検と回収](zcu106_run.md) にまとめた。
追加の自動build・一括検査入口は設けていない。

## 未実行・未対応

- 外部ソースがないためBL31/SDK/TA-A/B/外部署名TA/BL32の実ビルドは未実行。
- 実Coreを使うprofile整合、署名bundle検査、署名Core付き回収計画は未実行。
- 新MakeのBL31/BL32/TA全体を旧command receiptと結合する統合検査は未対応。legacyの統合検査は使用不可。
- 実機のロード・起動・実XSDB・XMPU設定/採取・機能/異常系/性能試験は未実行。
- 実機実行許可を生成する経路は未対応。runtime-ready/physical-reviewedは0のまま。
- 新しい実dumpでのdecodeは未実行。CLI/import確認は実dump解析の成功ではない。
- GCC版の互換性は未確認。[外部ソースとツール](build.md) の版・配置を参照。

旧手順は単純な移動で使用可能とは案内せず、[legacy](../legacy/README.md) に参照専用として分離した。
