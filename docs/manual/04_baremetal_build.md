# 04 構成AのビルドとEL3動作設計

## 目的

自作EL3、Secure EL1、Non-secure EL1をBSP startupなしで生成する。

## 前提条件・事前確認事項

[全体手順](00_overview.md)で`TZ_ROOT`、`TZ_LOG_DIR`、必要時`TZ_CROSS`を設定し、各shellを`bash`で開く。前章の入力を確認し、出力directoryを作ってから進む。会社Ubuntu 24.04/Vitis 2025.1の実操作と全ZCU106実行はNOT_RUN。外部ネットワークを必要とする取得処理はbuildに含めない。実機のload/run/resetは[安全検査](14_hardware_safety_check.md)の対象であり、現在BLOCKED。

## 必要なファイル・入力ファイル

Makefile、baremetal/、common/、platform/zcu106/memory_map.{h,json}、linker_{el3,secure,nonsecure}.ld、01章のtool。

## GUIのメニュー名

この章のcommandは端末で実行する。GUI操作なし。

## 操作手順・実行コマンド

1. 01章のbaremetal preflightがREADYであることを確認し、source-only buildを実行する。preflight READYは実機ロード許可ではない。

```bash
bash scripts/build_vitis_offline.sh --configuration baremetal --platform zcu106 --compiler gcc --cross-compile "$TZ_CROSS" | tee "$TZ_LOG_DIR/build-A.log"
export TZ_BUILD_ROOT="$(python3 -c 'import json; print(json.load(open("results/phase8/latest-baremetal-zcu106.json"))["workspace"])')"
python3 scripts/check_boot_plan.py --inventory "$TZ_BUILD_ROOT/build/zcu106/el3.elf" "$TZ_BUILD_ROOT/build/zcu106/secure.elf" "$TZ_BUILD_ROOT/build/zcu106/nonsecure.elf" --output "$TZ_LOG_DIR/a-inventory.json"
"${TZ_CROSS}readelf" -h -l -S "$TZ_BUILD_ROOT/build/zcu106/el3.elf" > "$TZ_LOG_DIR/el3-readelf.txt"
```

2. entry `_start`、vector 2048-byte alignment、16-byte stack alignment、PT_LOAD p_memsz（BSS含む）とruntime予約を検査する。詳細logのEXIT_CODE=0と生成hashを確認する。
3. `baremetal/el3_monitor/start.S`のEL3 vectorsとframe保存を読む。`monitor.c`はCurrentEL=3を要求し、未確認platform保護なら停止する。`PLATFORM_PROTECTION_CONFIRMED`を確認なしに変更しない。
4. `select_world`はSCR_EL3.RWを設定し、NS bitでworldを選ぶ。contextsのPCにSecure/NS code base、SPに個別stack、pstateに`SPSR_EL1H_MASKED`を用意し、enter_world/restore_frameでELR_EL3/SPSR_EL3/SP_EL1を復元しERETする。FID/引数と帰還状態をコードに照合する。これは本プロジェクトの実装説明で、実機の入口状態を保証しない。
5. monitorは両EL1のMMU/cacheをoffにし、NS EL2のHCR.RW/VM、timer accessを構成する。EL3自身のSCTLR/TTBR/TCR/cacheと残留状態はJTAG/FSBL初期化との照合が必要。稼働中cacheを無確認でoffにする操作を追加しない。
6. 同じ検査buffer VA/PA、Secure/NS security attribute、NS fault ESR/FARとデータ非改変を11章で検査する。現ZCU106 buildは保護未確認でhaltする設計で、QEMUの成功を実機の期待出力として強制しない。起動は07章、現在BLOCKED。

## 出力ファイル・実行ログの保存先

fresh workspaceのbuild/zcu106/{el3,secure,nonsecure}.elfとmap、build-baremetal.log、a-inventory.json。 `TZ_LOG_DIR`はGit登録しない社内の実行証跡directory。build helperの詳細logは`results/phase8/runs/<UTC>/`へ自動保存される。実際の絶対path、日時、構成、source commit、全ELF hashを同じrunへ記録する。

## 正常時の確認方法

本章の期待出力を保存し、終了コードと内容を両方確認する。

## エラー時の対応

原因を保存して処理を止め、[トラブルシューティング](13_troubleshooting.md)で切り分ける。

## PASS／FAIL／BLOCKED／NOT_RUN判定・実環境での確認状況

実行して期待値に一致した工程だけPASS、不一致はFAIL。入力・tool・初期化・安全条件不足はBLOCKED。未実行はNOT_RUN。試験対象外はSKIP。BUILD PASS、QEMU PASS、HARDWARE PASSを分ける。本章の会社環境操作・GUI・実機はNOT_RUNで、ロード準備はBLOCKED。開発hostで確認済みの範囲は[Phase 8報告](../phase8_progress.md)と[追加作業の確認記録](../../results/manual-validation-summary.json)を参照する。



## 公式資料の文書番号と章節

- [Arm TrustZone for Armv8-A Version 1.0 (2020-01-08) — 3.1/3.2/3.4/3.7、4.1/4.3/4.4/4.6/4.7/4.8、5.1.1/5.3（既存参照表で目次照合）](https://developer.arm.com/-/media/Arm%20Developer%20Community/PDF/Learn%20the%20Architecture/TrustZone%20for%20Armv8-A.pdf)。
- [UG1085 v2.5 (2025-03-21) — Chapter 16 System Protection Units → Introduction](https://docs.amd.com/api/khub/documents/xzMsp_c5sG9J6A3u7NkJYQ/content?Ft-Calling-App=ft)。

番号付き章節が公開Webにない場合は見出しの階層で示す。未確認の節番号は補わない。公式資料は一般仕様・一般操作の根拠であり、本repoのsource一覧、shell変数、検査コマンド、未確定handoffの停止条件は本プロジェクト固有の手順。資料閲覧は実機動作確認を意味しない。

[前へ](03_vitis_application_creation.md) · [目次](00_overview.md) · [次へ](05_tfa_build.md)
