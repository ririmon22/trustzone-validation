# 未完成・制約（Phase 5）

最新結果は Phase 5報告（別途生成・用意: `phase5_progress.md`）。ホスト105件成功。QEMU AはTZ-SYS-01/02/03、B1は標準API補助、B2はTEE-01/03/07と補助5項目がPASS。HARDWARE PASSはない。

|項目|状態／理由|
|---|---|
|ZCU106全実機試験|NOT_RUN。hardware/Vitis/XSA/BSP/FSBL/PMUFW使用範囲・master/route未確認|
|BL31実機配置|BLOCKED。実OCM予約と旧DDR案が異なる。実機ロードゲート維持|
|GNU toolchain build|NOT_RUN。今回はClang21.1.8を使用|
|外部ソース|固定commitの検証済みbuild subset。完全checkoutではない|
|Core検証build|原本を変更せず派生copyへ自作canary PTAを明示的に追加。overlay記録あり。製品Coreへ使用しない|
|B1 TZ機能群|BLOCKED。SMCCC/PSCIのみ。Secure EL1検証service未統合|
|TZ-SYS-04/05/06/07|QEMUで周辺保護/DMA/IRQ分離/debug権限の試験なし。Aは05 SKIP、他BLOCKED|
|ZCU106 XMPU/XPPU/DMA/GIC|backend BUILD/mockのみ。board情報とvector/world切替接続未完了|
|TEE-02 / TEE-AUTH-NS|BLOCKED。Core設定のTA identity検査はPASS、Normal要求者のcredentialsは未実装|
|TEE-03|QEMU fixture範囲でPASS。他TAのprivate値とCore canaryへread/writeを実行、各非改変確認済。全領域・全攻撃経路の網羅ではない|
|TEE-04|BLOCKED。Early TAのみ起動。正常署名・改ざん・別鍵・別UUIDのホスト検査5件をruntimeロード検証の代替にしない|
|TEE-05 / TEE-KEY-MEM|BLOCKED。AES/KAT、非抽出usage、export API拒否はPASS。生memory抽出、attribute export直接試験、削除/再生成検証は未実施|
|TEE-06/PERF-07|BLOCKED。永続storage backend/RPCなし|
|TEE-08|BLOCKED。認証boot未構成。JTAGはSecure Boot試験にしない|
|Standalone client|同期valueとTMEM inputのみ。output/inout、TOCTOU競合、dynamic SHM、RPC/foreign IRQ/SMPは未対応|
|検証TA/PTA|address公開とメモリ攻撃commandを含む。既知fixtureを製品用秘密にしない。現protocolは32-bit pointer表現を前提|
|PRNG|QEMUのsoftware PRNGは実機entropy源の品質を証明しない|
|PERF-01/02/05/06|QEMUのみ。CPU実クロック不明、TCG値は実機性能ではない|
|PERF-03/04|実測プログラムの接続・実行未完了|
|Vitis2025.1 GUI|手順案。実IDE操作未確認|

今回QEMUのsocket作成失敗は観測していない。AF_INET socketのEPERMをホストOS全体の禁止とは判断しない。ホスト保護設定と実機の不可逆設定は変更していない。

Phase 6ではホスト116件と12 ELF、QEMU A3/B1補助1/B2 8 PASSを再確認。runtime/boot検査とIRQ/DMA timing adapterを追加したが実機は全NOT_RUN。全BLOCKED理由と性能制約は Phase 6報告（別途生成・用意: `phase6_progress.md`）。

最新Phase 7ではhost fresh workspaceから2回の再構築を確認したがPodman内のclean OS検証はBLOCKED。TEE反復は512 iterationの範囲でのみPASS。未完了・機構の区別は Phase 7（別途生成・用意: `phase7_progress.md`）。
