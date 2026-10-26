# Phase 6 実機移植準備と性能検証

2026-10-27。実機/Vitisは利用できないため、実機のPlatform作成・FSBL/PMUFW生成・起動・性能評価はNOT_RUN。移植資料、runtimeを含む安全検査、性能adapterとQEMU測定を整備した。実機値の推測、ロードゲート無効化、不可逆設定変更は行っていない。

## 結果

|項目|実測結果|ログ|
|---|---|---|
|既存基盤の確認|開始時ホスト105件成功。Phase 5 source/log/overlayとQEMU実績を確認|results/phase6/baseline|
|自作build|12 ELFとZCU106 backend library成功|build-qemu.log、build-zcu106.log、build-clients.log|
|ホスト試験|116件成功、既存105件を含む|host-tests.log|
|QEMU A|TZ-SYS-01/02/03 PASS、FAIL 0、SKIP 1、BLOCKED 3|qemu/baremetal.log|
|QEMU B1|標準service補助1 PASS、FAIL 0、SKIP 1、BLOCKED 6|qemu/tfa.log|
|QEMU B2|8 PASS、FAIL 0、BLOCKED 7。TEE-01/03/07と5補助項目を維持|qemu/optee.log、optee-secure.log、security-evidence.json|
|性能|PERF-01/02/05/06/08/09/10/11をwarmup16/sample128で測定|performance.md、performance.json、qemu/*-benchmarks.csv|
|Core/TA/TF-A|固定commit＋明示的なcanary overlayでbuild成功|external-optee*.log、external-ta-*.log、external-tfa-*.log|
|署名ホスト検査|5ケース成功、runtime TAロードの結果ではない|signed-ta/results.json|
|実機安全検査|BLOCKED。未確定事項を記録|boot-plan-check.json、zcu106-load-gate.log、zcu106-profile-gate.log|
|ZCU106実行|全NOT_RUN|results/status.json、results/zcu106/status.json|

特記のないログはresults/phase6以下。BUILD、ホスト、QEMU、HARDWAREを区別する。実測値は性能閾値の合格判定ではなく収集結果であり、ZCU106性能の主張ではない。

## 最終成果物

|成果物|所在|
|---|---|
|ZCU106移植手順・起動構成図・保護設定|[実機手順](zcu106_phase6_porting.md)|
|Vitis2025.1操作手順|[IDE手順](vitis_2025_1_phase6.md)。公式2025.1 XD260の操作とFSBL用domainを照合|
|メモリ配置表・実ELF entry/LOAD|[配置表](zcu106_memory_phase6.md)、zcu106-elf-inventory.json|
|構成別runtime/boot計画|platform/zcu106/boot_plan.json（未確定template）|
|自動安全検査|scripts/check_boot_plan.py、既存check_memory_map/check_hardware_profile|
|性能コード|common/benchmark、optee_standalone/tests/main.c、platform/zcu106/performance.c|
|性能結果と測定条件|[性能説明](performance_phase6.md)、[実測統計](../results/phase6/performance.md)|
|特性・検証・source・証跡対応|[根拠表](security_evidence_phase6.md)|
|実機確認票|[確認項目](phase6_hardware_checklist.md)|
|再現script|scripts/run_phase6.sh|

## 安全検査の追加範囲

ELF32/64・両endiannessを読み、PMUFW MicroBlazeとAPU AArch64を区別する。entry、PT_LOAD、file sizeとruntime size（BSSを含む）、hash、system physical変換と予約領域の包含を照合する。stack/heap/dynamicは別の根拠付きreserved/unused申告を要求する。領域重複、alignment/overflow、SHM、FSBL/PMUFW/BL31/BL32/BL33、MMIO候補、master ID/security/route、debug/boot/handoff/UARTの証拠も検査する。

XSA、BSP map、live snapshot、debug/boot reviewのfileとSHA-256が必要。証拠の内容の正しさやlive状態は人間の確認も必要で、単なるhash照合をhardware状態の証明とはしない。既存map/protection/profileゲートを内包し、未統合DMA/GICとworld routingの停止条件も維持する。ゲートに通るためだけにconfirmedを変更しない。

現状は全未確定値をnull/falseにしてBLOCKED。生成BL31のOCM予約と従来DDR案は不一致、FSBL/PMUFW/実機BL32も未取得。正しい実機配置が完成したとはしていない。PMU firmwareのaddress viewも推測しない。

## 実行で見つかった問題と修正

- panic復帰を繰り返すとldelfが0xffff000c（out-of-memory）となった。clientがTARGET_DEAD後のCore sessionをcloseせず参照を残していたため、Core CLOSE成功を確認してからlocal sessionを失効させるよう修正した。失敗時はtransportを停止する。ホストで512回の再open/異常/closeとclose失敗を試験し、QEMUで各144回のpanic系列も成功した。
- 復帰系列増加で従来30秒のcapture上限に到達した。B2を90秒へ変更したが、FATAL/END欠落/FAIL/sample欠落を成功へ変換していない。
- stress後にTA間probeがFAILとなる実行も保存した。VA alias除外の高水準APIはheap割当のannotationも検査するため、ACCESS_DENIEDだけではmapping不在を判定できない。固定SDKのCore mapping syscallへ切替えて、実faultのFAR/ESR照合と非改変確認を再実行した。最終実行は全機能PASSを維持しているが、過去のFAILを消しておらず、全ASLR配置を網羅したとはしない。

失敗ログはoptee-recovery-attempt-01.log（FATAL）、同secure.log（memory不足）、attempt-02-timeout.log、attempt-03.log（TA probe FAIL）。最終結果はpipeline.logとqemu以下の成功run。以前のELF hashと変更後の一致は要求せず、今回のhashはartifacts.jsonへ保存する。

## 測定上の制約と次の作業

QEMUでSecure IRQ/DMA経路は実行できずPERF-03/04はBLOCKED。ZCU106 adapterは実ZDMA完了と内容照合、実SGI要求とSecure handler hookを使うが、実機経路への接続は未完了・全NOT_RUN。PERF-07もstorageなし。統一したwarmup/sample/counterと構成内条件で測定し、A/B2のMMU/cache差は明記した。構成を跨いだ直接性能比較はしない。CPU実クロックも不明。

次は実Vitisで内蔵ZCU106定義と生成物を取得し、FSBL/PMUFW/実機BL32・全runtime予約・master/route/XPPU profileを確定する。次にGIC/vectorと非同期world routing、DMA recoveryを接続して安全ゲートと許可/禁止機能試験を行い、その後に実機性能を測る。Normal要求者認証、鍵memory抽出、通常TA runtime署名検証、storage/Secure Bootは引き続き未完成。

再現：`bash scripts/run_phase6.sh`。このscriptはhost/QEMUのみで、実機loadもdebug/boot設定変更も実行しない。
