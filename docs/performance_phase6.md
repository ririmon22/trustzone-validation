# 性能測定コードと条件

実測統計は 結果（別途生成・用意: `../results/phase6/performance.md`）、raw metadataとticksは同directoryのperformance.jsonとqemu/*-benchmarks.csv。いずれもQEMU TCGでありZCU106性能ではない。

|ID|区間|状態|
|---|---|---|
|PERF-01|SMC要求→Secure処理→NS復帰、dispatch込み|QEMU測定|
|PERF-02|64個の64-bit store＋DSB|QEMU測定。read bandwidthやDDR全性能ではない|
|PERF-03|SGI書込要求直前→確認済みSecure handlerの記録hook|ZCU106 adapter BUILDのみ。world routing未接続でNOT_RUN|
|PERF-04|ZDMA register設定・転送開始→完了poll終了|ZCU106 adapter BUILDのみ。実master/経路なしでNOT_RUN|
|PERF-05|TA echo invoke、messageとSMC込み|QEMU測定|
|PERF-06|TA内AES生成鍵往復＋別既知鍵KAT、transport込み|QEMU測定。AES演算単体のthroughputではない|
|PERF-08|別sessionがあるTA instanceへのopenのみ|QEMU測定。TA初回load時間ではない|
|PERF-09|別sessionを保持したcloseのみ|QEMU測定。最終instance破棄とは別|
|PERF-10|panic要求→dead session解放→TA再open→echo成功|QEMU測定。診断出力と再生成も含む|
|PERF-11|TARGET_DEAD確認・dead-session解放後→再open→echo成功|QEMU測定。panic発生・診断時間を除く|
|PERF-07|storage|backendなし、BLOCKED|

`benchmark_run_scoped` により準備と後処理を測定区間から分ける。全系列warmup16/sample128、同じTCG/Cortex-A53/1CPU、counter62500000Hz。min/max/mean/median、nearest-rank p95/p99を保存する。CPU実クロックは不明。AはMMU/cache off、B2はonであり構成を跨ぐ同条件の比較はしない。IRQは現在masked。測定値を性能合否の閾値として使わず、収集成功と機能結果も区別する。

長いpanic復帰系列に対応してB2 capture上限を90秒とした（他は30秒、環境変数で1..300秒）。FATAL、END欠落、FAILやsample欠落は引き続き失敗にする。上限延長で未完了をPASSにしない。

ZCU106用 `platform/zcu106/performance.c` はCNTVCT/CNTFRQを使い、実zdma_copyのstarted/completeと転送内容照合後のみvalid timingにする。IRQは確認済みhandlerへ明示接続し、pending/activeなし・armed・新規hookを要求する。timestampだけでEOI/復帰やcontext保存を証明しないため、既存割込み機能試験を先に実行する。hookはvectorへ自動接続せず、未確認policyではMMIOを開始しない。実機用統計はこれらの有効sampleをwarmup16＋128回集めて同じ統計処理へ渡す。今回は未接続・全NOT_RUNで、架空のsampleを出していない。
