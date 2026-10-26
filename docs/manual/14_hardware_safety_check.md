# 14 実機ロード前の安全チェック

## 目的

メモリ・保護・boot・debugの未確定条件で実機を停止する。

## 前提条件・事前確認事項

[全体手順](00_overview.md)で`TZ_ROOT`、`TZ_LOG_DIR`、必要時`TZ_CROSS`を設定し、各shellを`bash`で開く。前章の入力を確認し、出力directoryを作ってから進む。会社Ubuntu 24.04/Vitis 2025.1の実操作と全ZCU106実行はNOT_RUN。外部ネットワークを必要とする取得処理はbuildに含めない。実機のload/run/resetは[安全検査](14_hardware_safety_check.md)の対象であり、現在BLOCKED。

## 必要なファイル・入力ファイル

platform/zcu106/{memory_map.json,board_profile.json,boot_plan.json,mmio_catalog.json}、実XSA/SDT/BSP/linker/FSBL/PMUFW/BL31/BL32/BL33、実接続snapshot。

## GUIのメニュー名

この章のcommandは端末で実行する。GUI操作なし。

## 操作手順・実行コマンド

1. 次の自動検査を実行する。exit2は未確定によるBLOCKEDで、失敗を握りつぶさない。現状は各検査が止まることを確認する工程。

```bash
python3 scripts/check_hardware_profile.py > "$TZ_LOG_DIR/hardware-profile-gate.log" 2>&1
python3 scripts/check_memory_map.py --platform zcu106 --require-protection > "$TZ_LOG_DIR/memory-gate.log" 2>&1
python3 scripts/check_boot_plan.py --output "$TZ_LOG_DIR/boot-plan-gate.json" > "$TZ_LOG_DIR/boot-plan-gate.log" 2>&1
```

2. board/XSA/SDT/BSPの実path/hash、DDR/OCM範囲、FSBLのOCM/DDR scratch/stack/heapと継続使用、PMU RAMのprocessor view→system address変換を確認する。
3. 構成ごとにELFのentry/PT_LOAD p_fileszとp_memsz、BSS、stack、heap、TA RAM/dynamic pool、SHM、vectorを予約する。重複/終端/align/ELFとheader/linkerの一致を検査し、unusedにも根拠を付ける。
4. XMPUの1MiB保護窓と内部64KiB検査bufferを別管理し、code/stack/FSBL/BL31/BL32を窓へ巻き込まない。6 DDR bankの実経路、APU/DMAの10bit master ID/mask、NS属性、default policy/lockを確認する。
5. XPPUは対象周辺/apertureと20 profiles/parityの実snapshot、UART/JTAG/boot資源の除外を確認する。DMAはchannel/FPD-LPD/DDR経路/NS/cache、GICはIIDR/group/priority/routing/DAIF/SCR/vectorと他core停止条件を確認する。
6. debug認可/boot mode、初期EL/security、PSU/DDR初期化、PMUFW/FSBLの停止点とhandoff、BL31/32/33の実行順・引数・Secure属性をreviewする。自作AとB1/B2のplanを共用しない。
7. 確認flagは証拠file/hashと照合して更新する。現gateはDMA/GIC実機統合と例外/violation帰属未完了を**常にBLOCKED**にしており、profileの値だけ埋めてもREADYにはならない。backendをrunnerへ接続し、検査の必要条件を満たす正式な実装/review工程が別途必要。単に無条件停止行を削ることは許可しない。
8. boot plan、header/linker、外部firmware configが一致するよう再設計しsourceから再build/reviewするまでload禁止。現Phase8 buildのupstream OP-TEE TZDRAM 0x60000000/256MiBは暫定mapの自作EL3等を覆う。同時使用不可で、別構成planで整理する。B1 BL31 OCM/ B2 BL31 DDR0x1000も既存予約へ照合が必要。

## 出力ファイル・実行ログの保存先

hardware-profile-gate.log、memory-gate.log、boot-plan-gate.json、確定値と根拠を記したreview。 `TZ_LOG_DIR`はGit登録しない社内の実行証跡directory。build helperの詳細logは`results/phase8/runs/<UTC>/`へ自動保存される。実際の絶対path、日時、構成、source commit、全ELF hashを同じrunへ記録する。

## 正常時の確認方法

現時点はBLOCKED、全実機NOT_RUN。フラグだけでREADYになるようにゲートを書き換えていないことをsourceでも確認する。

## エラー時の対応

原因を保存して処理を止め、[トラブルシューティング](13_troubleshooting.md)で切り分ける。

## PASS／FAIL／BLOCKED／NOT_RUN判定・実環境での確認状況

実行して期待値に一致した工程だけPASS、不一致はFAIL。入力・tool・初期化・安全条件不足はBLOCKED。未実行はNOT_RUN。試験対象外はSKIP。BUILD PASS、QEMU PASS、HARDWARE PASSを分ける。本章の会社環境操作・GUI・実機はNOT_RUNで、ロード準備はBLOCKED。開発hostで確認済みの範囲は[Phase 8報告](../phase8_progress.md)と[追加作業の確認記録](../../results/manual-validation-summary.json)を参照する。

## 根拠の分類

|分類|扱い|
|---|---|
|CONFIRMED|公式仕様として確認できた制約（DDR XMPU 1MiB等）、実生成ELFのLOAD/entry、固定sourceの定義。実機での設定済みを意味しない|
|ASSUMED|暫定map、UART0、予定master/経路、初期化順。loadに使わない|
|REQUIRES_HARDWARE|実BSP/FSBL/PMUFW予約、clock/MIO、debug認可、初期EL、live master profiles/route、IRQ/DMA排他。現状NOT_RUN|

公式register定義の根拠は[backend資料](../zcu106_backends.md)。UG1085第16章はsystem protection概念の根拠で、個別offsetの照合を省略する理由にしない。

## 公式資料の文書番号と章節

- [UG1085 v2.5 (2025-03-21) — Chapter 16 System Protection Units → Introduction](https://docs.amd.com/api/khub/documents/xzMsp_c5sG9J6A3u7NkJYQ/content?Ft-Calling-App=ft)。
- [UG1137 2025.1 — 起動の概要 / FSBL build（同名節）](https://docs.amd.com/r/2025.1-English/ug1137-zynq-ultrascale-mpsoc-swdev/Boot-Process-Overview)。
- [TF-A 2.12.0 — §7.45（ZynqMP）、§7.45.1（build設定）、§7.45.5（FSBLからの引渡し）](https://trustedfirmware-a.readthedocs.io/en/v2.12/plat/xilinx-zynqmp.html)。
- [Arm TrustZone for Armv8-A Version 1.0 (2020-01-08) — 3.1/3.2/3.4/3.7、4.1/4.3/4.4/4.6/4.7/4.8、5.1.1/5.3（既存参照表で目次照合）](https://developer.arm.com/-/media/Arm%20Developer%20Community/PDF/Learn%20the%20Architecture/TrustZone%20for%20Armv8-A.pdf)。

番号付き章節が公開Webにない場合は見出しの階層で示す。未確認の節番号は補わない。公式資料は一般仕様・一般操作の根拠であり、本repoのsource一覧、shell変数、検査コマンド、未確定handoffの停止条件は本プロジェクト固有の手順。資料閲覧は実機動作確認を意味しない。

[前へ](13_troubleshooting.md) · [目次](00_overview.md) · [次へ](15_offline_source_transfer.md)
