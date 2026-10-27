# 09 B2 TF-A＋OP-TEE＋StandaloneのJTAG起動準備

実機条件の証跡取得は[確認準備](../b2_hardware_readiness.md)と[チェックリスト](../b2_hardware_checklist.md)を使用する。現段階ではdownload/runせず、動的停止点はNOT_RUNのまま残す。

最新の統合構成・再build結果・接続監査は [B2統合手順](../b2_integration.md) を参照する。ZCU106 boot planはB2用BL31を指すが、共有memory/cache/予約の不一致によりloadはBLOCKEDのまま。

## 目的

構成ごとにboot所有者と初期化条件を分離する。現在のload順序は未確定で、実行はBLOCKED。

## 前提条件・事前確認事項

[全体手順](00_overview.md)で`TZ_ROOT`、`TZ_LOG_DIR`、必要時`TZ_CROSS`を設定し、各shellを`bash`で開く。前章の入力を確認し、出力directoryを作ってから進む。Ubuntu 24.04/Vitis 2025.1の実操作と全ZCU106実行はNOT_RUN。外部ネットワークを必要とする取得処理はbuildに含めない。実機のload/run/resetは[安全検査](14_hardware_safety_check.md)の対象であり、現在BLOCKED。

## 必要なファイル・入力ファイル

SPD=opteedのZynqMP BL31、ZCU106 BL32 ELF/bin parts、optee_bl33.elf、生成config、組込みTA一覧、FSBL/PMUFW。platform/zcu106/{boot_plan,board_profile,memory_map}.json、14章の全証跡。

## GUIのメニュー名

Target Connections、Launch Configuration、Debug view、register/memory view。公式UG1400に項目名を確認した範囲を使用する。実接続・各画面の詳細はNOT_RUN。

## 操作手順・実行コマンド

1. 電源OFFでboard revisionとUG1244を照合し、正規電源、JTAG cable、USB-UART cableを確認する。board固有boot modeを確認して記録する。switch番号/方向を別boardの例から転記しない。
2. のUSB権限とケーブルdriver、実在hw_server/XSDBを確認する。VitisのTarget Connectionを作り、対象server/cableを選ぶ。現在はread-onlyのtarget一覧収集までとし、自動download/resetを伴うlaunchは使わない。CPU名に加えMPIDR/core indexでA53 #0、PMUを識別する。
3. PSU clock/DDR/MIO/UART初期化と必要なPMUFW/FSBL、停止breakpointをXSA/生成scriptと照合する。固定sleepを初期化完了判定にしない。resetがdebug状態や保護設定を変えるのでreset前後を別runとして保存する。
4. 実際のPMUFW/FSBL初期化とhandoff停止点を確認する。BL32とBL33を各確定先へloadし、BL31が参照するSecure/NS entry/引数を準備する設計候補。EL3 BL31→OP-TEE初期化→BL31→NS BL33の順でhandoffさせ、BL32/BL33を独立conしない。ELF load方式とtee header/pager/pageable bin分割方式を混ぜない。
5. 上記は設計候補であり、現在のboot plan/保護予約/handoff不一致が解決するまではdownloadもconも禁止。load address/entryは04〜06章の実ELF inventoryから取り、未確定値を手入力で補わない。確認registerは`CurrentEL、SCR_EL3、SPSR_EL3、ELR_EL3、VBAR_EL3/EL1、SCTLR/TTBR/TCR/MAIR、opteed BL32 entry/context、SHM PA/cache属性、handoff PC/SPSR/security`。Secure状態でのみ意味のあるregisterの読み方と非banked状態保存をreviewする。
6. 14章のgateが通り、構成専用Tclがreviewされた将来にだけ次の起動入口を使う。Tclは本repoに完成済みとして提供していない。`dow`（ELFのPT_LOADをloadしentryを設定する操作）、`con`（実行）、`stop`（停止）、`rst`（reset）を通常アプリの既定順で並べない。XSDBの実helpで構文を確認し、review済みscriptに限定する。

```bash
(
    set -e
    cd "$TZ_ROOT"
    python3 scripts/check_hardware_profile.py > "$TZ_LOG_DIR/09_jtag_optee-profile-gate.log" 2>&1
    python3 scripts/check_memory_map.py --platform zcu106 --require-protection > "$TZ_LOG_DIR/09_jtag_optee-map-gate.log" 2>&1
    python3 scripts/check_boot_plan.py --output "$TZ_LOG_DIR/09_jtag_optee-boot-gate.json"
    read -r -p 'Reviewed configuration-specific JTAG Tcl path: ' TZ_JTAG_PLAN
    test -f "$TZ_JTAG_PLAN"
    command -v xsdb
    xsdb "$TZ_JTAG_PLAN" > "$TZ_LOG_DIR/09_jtag_optee-xsdb.log" 2>&1
)
```

7. 現状は最初のgateがexit2で停止するのが正常。将来runできた場合は10章のUARTを先に開始し、各entryでPC/EL/securityと上記registerを保存する。自作/result runnerの起動と全RESULT/SUMMARY/ENDを確認する。
8. 終了は使用したCPUを停止し、DMA等がidleか確認する。異常時はlogとregisterを先に保存し、承認済みreset/電源断復旧手順で戻す。busy DMAやlock済み保護を不用意に再設定しない。PCを飛ばして故障点を迂回しない。

## 出力ファイル・実行ログの保存先

09_jtag_opteeのtarget一覧、接続/boot mode記録、初期化/handoff breakpoint記録、register snapshot、ELF inventory/hash、UART。 `TZ_LOG_DIR`はGit登録しない社内の実行証跡directory。build helperの詳細logは`results/phase8/runs/<UTC>/`へ自動保存される。実際の絶対path、日時、構成、source commit、全ELF hashを同じrunへ記録する。

## 正常時の確認方法

現在はgate BLOCKEDを確認する。将来は各boot段階の到達と実EL/security、UARTの完了、拒否とデータ維持の証跡を確認した試験だけHARDWARE PASSとする。

## エラー時の対応

接続なしはcable/USB権限/hw_serverを確認。load失敗やabortならDDR/予約/EL/MMUを13章で確認し停止する。保護/boot registerを無根拠に書き換えない。

## PASS／FAIL／BLOCKED／NOT_RUN判定・実環境での確認状況

実行して期待値に一致した工程だけPASS、不一致はFAIL。入力・tool・初期化・安全条件不足はBLOCKED。未実行はNOT_RUN。試験対象外はSKIP。BUILD PASS、QEMU PASS、HARDWARE PASSを分ける。本章の環境操作・GUI・実機はNOT_RUNで、ロード準備はBLOCKED。開発hostで確認済みの範囲は[Phase 8報告](../phase8_progress.md)と[追加作業の確認記録](../../results/manual-validation-summary.json)を参照する。

## JTAGとBootgenを分ける

JTAGのELF downloadはBOOT.bin生成でもSecure Boot認証でもない。BootgenはBIFにpartition/CPU/EL/securityを記述してboot imageを生成する別工程。のFSBL/PMUFW/BL31/BL32/BL33と予約が確定し、BIFをreviewした後にだけ実施する。現時点ではBIF未確定でBLOCKED。書込み/flash programmingは本手順に含めない。

```bash
(
    set -e
    python3 scripts/check_hardware_profile.py > "$TZ_LOG_DIR/bootgen-profile-gate.log" 2>&1
    python3 scripts/check_boot_plan.py --output "$TZ_LOG_DIR/bootgen-plan-gate.json"
    read -r -p 'Reviewed BIF path: ' TZ_BIF
    test -f "$TZ_BIF"
    command -v bootgen
    bootgen -arch zynqmp -image "$TZ_BIF" -o "$TZ_LOG_DIR/BOOT.bin" > "$TZ_LOG_DIR/bootgen.log" 2>&1
)
```

FSBL/PMUFWとBIFの`destination_cpu`、`exception_level`、`trustzone`等はUG1283と実handoffで照合する。QEMU FIPをZynqMP BOOT.binとして使用しない。認証無効JTAGをTEE-08 PASSにせず、eFUSE/不可逆認証設定は自動変更しない。

## 公式資料の文書番号と章節

- [UG1244 v1.4 — Chapter 3 Board Component Descriptions → Programmable Logic JTAG Programming Options, Figure 3-6](https://docs.amd.com/v/u/en-US/ug1244-zcu106-eval-bd)。
- [UG1400 2025.1 — Using the Vitis Unified IDE → Launching the Vitis Unified IDE](https://docs.amd.com/r/2025.1-English/ug1400-vitis-embedded/Launching-the-Vitis-Unified-IDE)。
- [UG1137 2025.1 — 起動の概要 / FSBL build（同名節）](https://docs.amd.com/r/2025.1-English/ug1137-zynq-ultrascale-mpsoc-swdev/Boot-Process-Overview)。
- [TF-A 2.12.0 — §7.45（ZynqMP）、§7.45.1（build設定）、§7.45.5（FSBLからの引渡し）](https://trustedfirmware-a.readthedocs.io/en/v2.12/plat/xilinx-zynqmp.html)。
- [UG1283 2025.1 — コマンドラインでのBootgen利用（同名節）](https://docs.amd.com/r/2025.1-English/ug1283-bootgen-user-guide/Using-Bootgen-on-the-Command-Line)。

番号付き章節が公開Webにない場合は見出しの階層で示す。未確認の節番号は補わない。公式資料は一般仕様・一般操作の根拠であり、本repoのsource一覧、shell変数、検査コマンド、未確定handoffの停止条件は本プロジェクト固有の手順。資料閲覧は実機動作確認を意味しない。

[前へ](08_jtag_tfa.md) · [目次](00_overview.md) · [次へ](10_uart_logging.md)
