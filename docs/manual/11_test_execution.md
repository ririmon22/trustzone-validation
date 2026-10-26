# 11 機能試験の実行と合否

## 目的

TrustZone特性、TEE特性、補助試験を独立して評価し、拒否原因と保護対象非改変を記録する。

## 前提条件・事前確認事項

[全体手順](00_overview.md)で`TZ_ROOT`、`TZ_LOG_DIR`、必要時`TZ_CROSS`を設定し、各shellを`bash`で開く。前章の入力を確認し、出力directoryを作ってから進む。会社Ubuntu 24.04/Vitis 2025.1の実操作と全ZCU106実行はNOT_RUN。外部ネットワークを必要とする取得処理はbuildに含めない。実機のload/run/resetは[安全検査](14_hardware_safety_check.md)の対象であり、現在BLOCKED。

## 必要なファイル・入力ファイル

tests/expectations.json、docs/test_matrix.md、各構成のbuild/hash、UART原本、保護設定/readback、例外/違反snapshot。

## GUIのメニュー名

この章のcommandは端末で実行する。GUI操作なし。

## 操作手順・実行コマンド

1. 14章のgateがBLOCKEDなら全実機試験はNOT_RUNとし、未実装/未統合の理由を別欄でBLOCKEDとする。backend libraryがbuildできても実機runnerへ接続されていなければ実行済みにしない。
2. gate通過後の将来、対象構成を07〜09章で起動し10章でUARTを取得する。下表の各IDについて、許可操作→禁止操作→例外/違反→Secure側再読出し→サービス再利用を同じrunで記録する。対象外の工程は理由を明記する。
3. 正常なaccessを先に確認し、禁止readで機密値が得られず、禁止write後もSecure側の元データが一致することを確認する。ESR.EC/DFSC、FAR、MMU/VA/PA、master/security/新規XMPU/XPPU違反を突き合わせる。Translation FaultやtimeoutだけをTrustZone保護PASSとしない。
4. UARTのplatform表示と全ID/完了markerを確認して解析する。以下は実機ログが取得できた将来の例で、現時点ではNOT_RUN。`TZ_CONFIGURATION`はbaremetal/tfa/opteeのいずれかを入力する。

```bash
read -r -p 'Configuration (baremetal/tfa/optee): ' TZ_CONFIGURATION
case "$TZ_CONFIGURATION" in baremetal|tfa|optee) ;; *) false ;; esac
python3 scripts/parse_results.py "$TZ_LOG_DIR/uart.log" --configuration "$TZ_CONFIGURATION" --output "$TZ_LOG_DIR/function-results.json"
```

5. B2補助IDも要求する場合は`--require-security`を加える。Secure UARTとの`check_security_evidence.py`は現QEMU canary用の前提を持つため、通常実機Coreへそのまま適用しない。機能parserのexit0はBLOCKED/SKIPの無い全PASSを意味しない。build_status UNKNOWNは同一runのbuild receiptと手動照合し、ハードウェア証跡を確認して確定する。
6. 各IDにTest ID/Target platform/Build status/Execution status/Expected result/Actual result/Evidence/Limitationを残す。実行されていない実機票はNOT_RUN。認証、認可、key export拒否、抽出耐性、Early TA、署名TA runtime、Secure Bootを別々に判定する。

## 出力ファイル・実行ログの保存先

構成別機能結果JSON、各IDの実機試験票、同一runのUART/ESR/FAR/保護データ証跡。 `TZ_LOG_DIR`はGit登録しない社内の実行証跡directory。build helperの詳細logは`results/phase8/runs/<UTC>/`へ自動保存される。実際の絶対path、日時、構成、source commit、全ELF hashを同じrunへ記録する。

## 正常時の確認方法

下表の対象操作・拒否機構・非改変が全て確認できた範囲だけPASS。全実機の現実行結果はNOT_RUN。QEMUの過去PASSを実機欄へ転記しない。

## エラー時の対応

原因を保存して処理を止め、[トラブルシューティング](13_troubleshooting.md)で切り分ける。

## PASS／FAIL／BLOCKED／NOT_RUN判定・実環境での確認状況

実行して期待値に一致した工程だけPASS、不一致はFAIL。入力・tool・初期化・安全条件不足はBLOCKED。未実行はNOT_RUN。試験対象外はSKIP。BUILD PASS、QEMU PASS、HARDWARE PASSを分ける。本章の会社環境操作・GUI・実機はNOT_RUNで、ロード準備はBLOCKED。開発hostで確認済みの範囲は[Phase 8報告](../phase8_progress.md)と[追加作業の確認記録](../../results/manual-validation-summary.json)を参照する。



## 公式資料の文書番号と章節

- [Arm TrustZone for Armv8-A Version 1.0 (2020-01-08) — 3.1/3.2/3.4/3.7、4.1/4.3/4.4/4.6/4.7/4.8、5.1.1/5.3（既存参照表で目次照合）](https://developer.arm.com/-/media/Arm%20Developer%20Community/PDF/Learn%20the%20Architecture/TrustZone%20for%20Armv8-A.pdf)。
- [UG1085 v2.5 (2025-03-21) — Chapter 16 System Protection Units → Introduction](https://docs.amd.com/api/khub/documents/xzMsp_c5sG9J6A3u7NkJYQ/content?Ft-Calling-App=ft)。
- [OP-TEE documentation stable（閲覧2026-10-27） — Core → Normal World invokes OP-TEE OS using SMC / Shared memory / Remote Procedure Call](https://optee.readthedocs.io/en/stable/architecture/core.html)。

番号付き章節が公開Webにない場合は見出しの階層で示す。未確認の節番号は補わない。公式資料は一般仕様・一般操作の根拠であり、本repoのsource一覧、shell変数、検査コマンド、未確定handoffの停止条件は本プロジェクト固有の手順。資料閲覧は実機動作確認を意味しない。

[前へ](10_uart_logging.md) · [目次](00_overview.md) · [次へ](12_performance_measurement.md)

## IDごとの操作差分・実行コード・現在の実機結果

下表の各行を上記手順1〜6に従って個別実施する。期待値の独立定義はtests/expectations.json。証跡は`TZ_LOG_DIR/<ID>/`の試験票と共通uart.logへの参照に保存する。各実行前に承認済み設定を再確認し、未統合なら実行しない。

|ID / 対象構成|実行コード|操作・異常系・非改変・判定範囲|実機結果|
|---|---|---|---|
|TZ-SYS-01 / A|baremetal/{secure_app,nonsecure_app}/main.c|Secure/NS counterを交互更新し、双方の値を保持。異常は相手counter破壊で判定。|NOT_RUN|
|TZ-SYS-02 / A|baremetal/el3_monitor/monitor.c; baremetal/tests/register_context.S|引数pattern/連続SMC、register/継続PCを照合。破壊なしで元処理へ復帰。|NOT_RUN|
|TZ-SYS-03 / A|baremetal/nonsecure_app/main.c; common/{probe.S,el1_fault.c}; platform/zcu106/protection.c|Secure初期値→NS正常対照→protected境界read/write→fault機構→Secure再読出し一致。実機XMPU未接続でNOT_RUN。|NOT_RUN|
|TZ-SYS-04 / A backend未接続|platform/zcu106/{protection,hardware_tests}.c|Secure操作の成功→NS同一周辺禁止操作→新規XPPU違反→保護周辺状態非改変。UART/JTAGを対象に選ばない。|NOT_RUN|
|TZ-SYS-05 / A backend未接続|platform/zcu106/{zdma,dma_validation,hardware_tests}.c|通常DMA転送成功→Secure read/write DMA禁止→error/violation→Secure内容一致。timeoutだけでPASS不可。|NOT_RUN|
|TZ-SYS-06 / A backend未接続|platform/zcu106/{gic,interrupt_dispatch}.c; test_vectors.S|Secure/NS IRQを両world中で発生、正しいhandler/復帰/状態保存。誤handlerならFAIL。|NOT_RUN|
|TZ-SYS-07 / 構成別debug手順未確定|07〜09章; 14章|許可/禁止状態の同一情報accessを比較。debug権限/保護を迂回しない。不可逆変更は自動化しない。|NOT_RUN|
|B1-SMCCC-01 / B1|tfa_standalone/integration/main.c|SMCCC/PSCI version応答を期待値照合。メモリ非改変試験の対象外。|NOT_RUN|
|TEE-01 / B2|optee_standalone/{client/client.c,tests/main.c,ta/ta.c}|複数OPEN/INVOKE/CLOSE、正常echoと不正command拒否。拒否後session再利用。|NOT_RUN|
|TEE-02 / B2 全体未完成|optee_standalone/ta/{ta,security}.c; tests/security_tests.c|TA identity認可とNS要求者認証を別判定。自己申告tokenは認証ではない。全体BLOCKED。|NOT_RUN|
|TEE-03 / B2 通常Coreとcanary overlay別|optee_standalone/{ta/security.c,tests/security_tests.c,pta/validation_canary.c}|TA-A/B自身のsecret操作→他TA read/write fault→survivor再読出し一致。Coreは検証用canaryに限る。通常実機Coreの同等試験は未整備。|NOT_RUN|
|TEE-04 / B2 runtime未実装|scripts/check_signed_ta.py はhost署名補助のみ|正常/改ざん通常TAのruntime受理比較は未実装。Early TAやhost verifyを代用PASSにしない。|NOT_RUN|
|TEE-05 / B2 全体未完成|optee_standalone/ta/{ta,security}.c; tests/security_tests.c|Secure key生成/AES KAT/key deleteとexport拒否。key memory抽出耐性は別で未完成。|NOT_RUN|
|TEE-06 / B2 未実装|storage backend/RPC未実装|保存/再取得/不正参照/改変/再起動後のintegrity比較にbackendが必要。現状BLOCKED。|NOT_RUN|
|TEE-07 / B2|optee_standalone/tests/{main,stress_tests}.c; client/client.c|TA-A panic→dead session CLOSE→TA-B継続→TA-A reopen。漏れ/停止を検出。|NOT_RUN|
|TEE-08 / 別boot認証構成未確定|UG1283と14章の将来手順|正常/改ざんboot imageを認証有効chainで比較。JTAG非認証起動は対象外。|NOT_RUN|
|TEE-AUTH-TA / B2|optee_standalone/ta/security.c; tests/security_tests.c|TA identityに基づく許可/不許可を比較しsecret維持。認可限定。|NOT_RUN|
|TEE-AUTH-NS / B2 未実装|同上|NS要求者の信頼できる認証方式なし。自己申告IDで成功にしない。|NOT_RUN|
|TEE-SHM-01 / B2|optee_standalone/client/client.c; tests/security_tests.c|正常/サイズ不整合/範囲外を反復、拒否後正常service継続。|NOT_RUN|
|TEE-KEY-API / B2|optee_standalone/ta/security.c; tests/security_tests.c|正常crypto/KAT後key export拒否とservice継続。memory抽出耐性は含まない。|NOT_RUN|
|TEE-KEY-MEM / B2 未実装|攻撃backend未実装|Secure keyのmemoryからの抽出耐性はAPI拒否だけで判定しない。|NOT_RUN|
|TEE-ISO-TA / B2|optee_standalone/ta/security.c; tests/security_tests.c|他TA read/write拒否のESR/FARと生存TA secret非改変。|NOT_RUN|
|TEE-ISO-CORE / B2 検証overlay限定|optee_standalone/pta/validation_canary.c|Core canary read/write拒否と元canary照合。通常構成/全Core秘密の耐性へ拡大しない。|NOT_RUN|
|TEE-STRESS-01 / B2|optee_standalone/tests/stress_tests.c|512回のOPEN/CLOSE、交互TA、不正SHM、panic回復、key cycle。heap/TA前後一致、alloc_fail増分0。確認範囲外の長時間保証はしない。|NOT_RUN|

性能IDは12章に分離。全IDの期待結果原文とQEMU実績はtests/expectations.json、docs/test_matrix.md、results/phase8/summary.jsonを照合する。
