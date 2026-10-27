# 12 性能測定

## 目的

測定区間と条件を揃え、QEMUの基盤確認と実機性能を分離する。

## 前提条件・事前確認事項

[全体手順](00_overview.md)で`TZ_ROOT`、`TZ_LOG_DIR`、必要時`TZ_CROSS`を設定し、各shellを`bash`で開く。前章の入力を確認し、出力directoryを作ってから進む。Ubuntu 24.04/Vitis 2025.1の実操作と全ZCU106実行はNOT_RUN。外部ネットワークを必要とする取得処理はbuildに含めない。実機のload/run/resetは[安全検査](14_hardware_safety_check.md)の対象であり、現在BLOCKED。

## 必要なファイル・入力ファイル

common/benchmark/benchmark.c、各caller、scripts/analyze_benchmarks.py、実UART、CPU/timer clock・cache/MMU/IRQ条件。

## GUIのメニュー名

この章のcommandは端末で実行する。GUI操作なし。

## 操作手順・実行コマンド

1. 11章の機能が成立した構成だけ測定する。CPU clock、CNTFRQ_EL0、CNTPCT_EL0（timer Hz）、cache/MMU、IRQ mask、共有属性を記録する。timer clockをCPU Hzとみなさない。
2. warmup=16、各系列128 sample、overhead128 sampleを現コードに照合する。測定外のUART出力やwarmupを含めない。実機条件を変更したら別runとする。
3. AはSMC roundtrip/64 u64 stores＋DSB。B2はTA invoke、AES処理（transport込み）、session open/close、panic後reopen/reuse等で、正確な区間はCSV metadata/各callerに従う。暗号単体のcore時間とtransport込み時間を混同しない。
4. 実行ログが得られた将来に解析する。

```bash
python3 scripts/analyze_benchmarks.py "$TZ_LOG_DIR/uart.log" --output "$TZ_LOG_DIR/benchmarks.csv"
```

5. count/min/max/mean/median/p95/p99を確認する。p95/p99はnearest-rank。overheadを保存し、自動差引きしたと解釈しない。counter ticksの統計をそのままZCU106 CPU cyclesとしない。CPU Hz unknownならunknownを維持する。
6. DMAとSecure IRQ応答は現在backend実機統合がなくNOT_RUN。DMAでは許可転送実完了/bytes/方向/cache条件、IRQでは発生時刻と実handler入口/EOI/復帰を接続してから測定する。storage性能はbackend/RPC不足でBLOCKED。
7. QEMU基盤の再現は15章の全source build commandと`--platform qemu --qemu`を使う。8系列各128が過去実績であり、実機性能値ではない。

## 出力ファイル・実行ログの保存先

benchmark CSV、raw BENCH samples/overhead、条件metadata。実機PERF-03/04含め全NOT_RUN。 `TZ_LOG_DIR`はGit登録しない社内の実行証跡directory。build helperの詳細logは`results/phase8/runs/<UTC>/`へ自動保存される。実際の絶対path、日時、構成、source commit、全ELF hashを同じrunへ記録する。

## 正常時の確認方法

本章の期待出力を保存し、終了コードと内容を両方確認する。

## エラー時の対応

原因を保存して処理を止め、[トラブルシューティング](13_troubleshooting.md)で切り分ける。

## PASS／FAIL／BLOCKED／NOT_RUN判定・実環境での確認状況

実行して期待値に一致した工程だけPASS、不一致はFAIL。入力・tool・初期化・安全条件不足はBLOCKED。未実行はNOT_RUN。試験対象外はSKIP。BUILD PASS、QEMU PASS、HARDWARE PASSを分ける。本章の環境操作・GUI・実機はNOT_RUNで、ロード準備はBLOCKED。開発hostで確認済みの範囲は[Phase 8報告](../phase8_progress.md)と[追加作業の確認記録](../../results/manual-validation-summary.json)を参照する。

## IDと測定区間

|ID|区間/状態|
|---|---|
|PERF-01|SMC Secure service往復、dispatch込み|
|PERF-02|64個のu64 store＋DSB|
|PERF-03|Secure IRQ入口応答、実機NOT_RUN|
|PERF-04|DMA転送要求〜完了、実機NOT_RUN|
|PERF-05|TA echo invoke、message/SMC込み|
|PERF-06|TA AES処理とKAT、transport込み|
|PERF-07|Secure Storage、backend不足BLOCKED|
|PERF-08|session OPENのみ、既存TA instance利用。実機NOT_RUN|
|PERF-09|session CLOSEのみ、他sessionはOPEN状態。実機NOT_RUN|
|PERF-10|panic診断＋TA再OPEN＋echo、最終CLOSEを除く。実機NOT_RUN|
|PERF-11|TARGET_DEAD観測からTA再OPEN＋echo成功まで。実機NOT_RUN|


## 公式資料の文書番号と章節

- [Arm TrustZone for Armv8-A Version 1.0 (2020-01-08) — 3.1/3.2/3.4/3.7、4.1/4.3/4.4/4.6/4.7/4.8、5.1.1/5.3（既存参照表で目次照合）](https://developer.arm.com/-/media/Arm%20Developer%20Community/PDF/Learn%20the%20Architecture/TrustZone%20for%20Armv8-A.pdf)。
- [OP-TEE documentation stable（閲覧2026-10-27） — Core → Normal World invokes OP-TEE OS using SMC / Shared memory / Remote Procedure Call](https://optee.readthedocs.io/en/stable/architecture/core.html)。

番号付き章節が公開Webにない場合は見出しの階層で示す。未確認の節番号は補わない。公式資料は一般仕様・一般操作の根拠であり、本repoのsource一覧、shell変数、検査コマンド、未確定handoffの停止条件は本プロジェクト固有の手順。資料閲覧は実機動作確認を意味しない。

[前へ](11_test_execution.md) · [目次](00_overview.md) · [次へ](13_troubleshooting.md)
