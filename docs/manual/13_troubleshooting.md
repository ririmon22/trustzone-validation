# 13 トラブルシューティング

## 目的

障害点をboot段階・EL・保護経路・ABIで絞り、根拠のないregister書込みで回避しない。

## 前提条件・事前確認事項

[全体手順](00_overview.md)で`TZ_ROOT`、`TZ_LOG_DIR`、必要時`TZ_CROSS`を設定し、各shellを`bash`で開く。前章の入力を確認し、出力directoryを作ってから進む。Ubuntu 24.04/Vitis 2025.1の実操作と全ZCU106実行はNOT_RUN。外部ネットワークを必要とする取得処理はbuildに含めない。実機のload/run/resetは[安全検査](14_hardware_safety_check.md)の対象であり、現在BLOCKED。

## 必要なファイル・入力ファイル

01章の環境log、build console、各ELF/map/hash、boot_plan、UART/例外snapshot。

## GUIのメニュー名

この章のcommandは端末で実行する。GUI操作なし。

## 操作手順・実行コマンド

1. 以下の該当行から調べる。最初の異常までのlogを保存し、以後の正常出力がないことを成功にしない。
2. 停止可能ならPC/SP/CurrentEL/SCR/ESR/FARを保存する。EL1とEL3では別registerで、Secure bank/non-secure bankを区別する。debug権限なしで取得できない場合はBLOCKED。
3. 入力/source/tool/configの変更を1つずつ記録し、新しいrun directoryで再試験する。既存PASSや原本UARTは上書きしない。
4. reset/電源断は07〜09章の復旧手順に従う。boot認証・debug・eFUSEを勝手に変更しない。

|障害|確認する設定/ログ/ファイル|確認register/判定|
|---|---|---|
|Vitisが起動しない|settings64.sh実path、vitis.log、OS版、DISPLAY/GUI session、install components|CPU register対象外。公式host要件と実installの不足を区別|
|Platform作成失敗|選択XSA/hash、SDTgen log、processor/domain、BSP生成console|対象外。ZCU106定義未同梱ならBLOCKED|
|コンパイル不可|tool-versions、実compile command、C/ASM file、target flags|対象外。GNU/Clangを混ぜず未対応optionを根拠なく抑制しない|
|header不足|common/include、platform/zcu106、client/include、生成BSP path、case|対象外。QEMU headerを代用品にしない|
|link失敗|map、undefined symbol、startup二重定義、linker MEMORY、libgcc/実runtime|ELF entry/PT_LOAD、stack/vector alignment。空library不可|
|JTAG未検出|電源、USB/cable、権限、hw_server log、server/cable選択、board boot mode|target一覧/MPIDR。未検出ならdownloadしない|
|ELF load失敗|実ELF PT_LOAD、DDR初期化、FSBL/PMUFW予約、gate log|PC、EL、SCTLR/TTBR/TCR、memory map、debug権限|
|UART無出力|UART instance/MIO/clock/baud、platform uart.c、TF-A console、OP-TEE conf|PC到達、UART status/baud/clock readback（catalog/公式定義を確認後）。Aがentry_state_blockedならCurrentEL/SCTLR_EL3条件不一致でUART前に停止する仕様|
|EL3遷移失敗|monitor/start.S、contexts、vector alignment、PLATFORM_PROTECTION_CONFIRMED|CurrentEL、SCR_EL3.NS/RW、SPSR/ELR_EL3、SP、VBAR、ESR/FAR_EL3|
|SMCが戻らない|FID、service state、context frame、TF-A runtime/opteed log|ESR_EL3.EC、ELR/SPSR、SCR、x0-x7、stack破壊。AのFIDをB1へ送らない|
|OP-TEE起動不可|SPD=opteed、BL32 ELF/bin parts、FSBL handoff、effective conf、予約重複|BL31/BL32 PC、EL、SCR、MMU、共有memory、Secure UART|
|TA呼出し不可|Early TA UUID/path/組込みlog、SDK target、message ABI、session id、RPC log|SMC x0 return、optee_msg_arg ret/ret_origin、SHM PA/size/cache属性|
|Data Abort|fault PC命令、ESR/FAR/ELR/SPSR、許可access対照、page tables|EC、DFSC、WnR、FAR validity、SCTLR.M、TTBR/TCR/MAIR、HCR.VM。Translation Faultと外部拒否を分ける|
|XMPUが期待と異なる|protection.c、board_profile、1MiB windowと64KiB buffer、全DDR bank/route|REGION START/END/MASTER/CONFIG、CTRL/STATUS、ERR捕捉（公式header名でoffset確認）。readback、新規違反、NS属性とmasterを照合|
|XPPUが期待と異なる|mmio_catalog、aperture、20 master profiles/parity、既存policy/lock|MASTER profile、permission、CTRL、violation capture。対象peripheral状態の非改変と正常access対照|
|DMA/GIC異常|zdma.c/gic.c、channel/SGIとroute、例外vector接続、cache整合|ZDMA control/status/ISR/error、GICD_CTLR/IGROUPR/ISENABLER、GICC_CTLR/PMR/IAR/EOIR、DAIF/SCR IRQ/FIQ|

MMIOはplatform/zcu106/includeの定義とmmio_catalog/公式UG1087・固定embeddedswを照合したread-only snapshotから始める。未確定なbase/offsetをこの表から推測しない。faultやviolationのclearはreadback記録後に承認済みbackendで行い、古いsticky違反と今回の違反を混同しない。

## 出力ファイル・実行ログの保存先

diagnosis.md、register snapshot、原因と再試験run参照。 `TZ_LOG_DIR`はGit登録しない社内の実行証跡directory。build helperの詳細logは`results/phase8/runs/<UTC>/`へ自動保存される。実際の絶対path、日時、構成、source commit、全ELF hashを同じrunへ記録する。

## 正常時の確認方法

本章の期待出力を保存し、終了コードと内容を両方確認する。

## エラー時の対応

原因を保存して処理を止め、[トラブルシューティング](13_troubleshooting.md)で切り分ける。

## PASS／FAIL／BLOCKED／NOT_RUN判定・実環境での確認状況

実行して期待値に一致した工程だけPASS、不一致はFAIL。入力・tool・初期化・安全条件不足はBLOCKED。未実行はNOT_RUN。試験対象外はSKIP。BUILD PASS、QEMU PASS、HARDWARE PASSを分ける。本章の環境操作・GUI・実機はNOT_RUNで、ロード準備はBLOCKED。開発hostで確認済みの範囲は[Phase 8報告](../phase8_progress.md)と[追加作業の確認記録](../../results/manual-validation-summary.json)を参照する。



## 公式資料の文書番号と章節

- [UG1400 2025.1 — Using the Vitis Unified IDE → Launching the Vitis Unified IDE](https://docs.amd.com/r/2025.1-English/ug1400-vitis-embedded/Launching-the-Vitis-Unified-IDE)。
- [UG1400 2025.1 — Configuring a Domain → Board Support Package Settings Page](https://docs.amd.com/r/2025.1-English/ug1400-vitis-embedded/Board-Support-Package-Settings-Page)。
- [UG1085 v2.5 (2025-03-21) — Chapter 16 System Protection Units → Introduction](https://docs.amd.com/api/khub/documents/xzMsp_c5sG9J6A3u7NkJYQ/content?Ft-Calling-App=ft)。
- [Arm TrustZone for Armv8-A Version 1.0 (2020-01-08) — 3.1/3.2/3.4/3.7、4.1/4.3/4.4/4.6/4.7/4.8、5.1.1/5.3（既存参照表で目次照合）](https://developer.arm.com/-/media/Arm%20Developer%20Community/PDF/Learn%20the%20Architecture/TrustZone%20for%20Armv8-A.pdf)。
- [OP-TEE documentation stable（閲覧2026-10-27） — Core → Normal World invokes OP-TEE OS using SMC / Shared memory / Remote Procedure Call](https://optee.readthedocs.io/en/stable/architecture/core.html)。

番号付き章節が公開Webにない場合は見出しの階層で示す。未確認の節番号は補わない。公式資料は一般仕様・一般操作の根拠であり、本repoのsource一覧、shell変数、検査コマンド、未確定handoffの停止条件は本プロジェクト固有の手順。資料閲覧は実機動作確認を意味しない。

[前へ](12_performance_measurement.md) · [目次](00_overview.md) · [次へ](14_hardware_safety_check.md)
