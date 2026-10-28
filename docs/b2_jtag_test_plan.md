全IDの最新棚卸しは[b2_test_inventory.md](b2_test_inventory.md)、UARTなしの回収範囲は[uartless_log_matrix.md](uartless_log_matrix.md)を参照。

# B2のJTAG起動準備・試験範囲

## 実機報告と確定していないこと

USER_REPORTED: ZIP由来TF-A/OP-TEEからSDK/TA-A/B/Early TA入りCore/opteed付きBL31/BL33を生成、entryは1000/60000000/10200000、FSBL XFsbl_Exit到達、手動停止PC5870をaddr2lineでipi_mb_notifyと表示。原本ELF・map・ZIP hash・固定commit照合・PMUFW再起動完了・ESR/FAR/ELRは未提供。BUILD成功の報告と、来歴・実機起動成功を分ける。

固定TF-Aのplat/xilinx/common/ipi.c `ipi_mb_notify()`はis_blocking!=0のときOBRのremote bitが消えるまでloopする。したがってPMU応答待ちは調査候補だが、PC/関数名1点で原因をPMUFW起動漏れと確定しない。ELF hash、実際のPC命令、local/remote/is_blocking、clock/reset、IPI/TRIG/OBR、PMU側PC・firmware ready状態を同じrunで採取する。offset/masterは固定sourceと実生成物を照合し、推測MMIOやW1C読出しをしない。最適化された引数値に依存せず、逆アセンブルと保存registerを照合する。

## 起動候補と停止点

1. 原本XSA、生成init、FSBL、PMUFW、BL31/32/33、各map/config/source hashを保存する。ZIPのbuildをGit検査PASSと記録しない。既存source_snapshot.pyによる完全固定Git/検証済みmanifestと照合できるまでは来歴BLOCKED。
2. `platform/zcu106/jtag_b2_inputs.json`のnullへ、取得済み原本の値だけを記入した別copyを用意する。XSA hashとFSBL/PMUFWの生成元対応、reset/init review、同一board/cableのAPU/PMU一意filter、PMUFW固有ready判定、各停止点を必要とする。entryは報告値との照合であり、予約領域と起動状態の証明ではない。
3. ローカル検査を実行する。MMIO/JTAGには触れない。

```bash
python3 scripts/prepare_b2_jtag.py --output results/jtag/b2-preflight.json --emit-tcl results/jtag/b2-inspection-plan.tcl
```

status=BLOCKED/exit2が現在の期待結果。ELF役割・machine・entry・LOAD/BSS・hash、XSA対応、既存boot gate、source provenanceの不足を列挙する。現入力ではFSBL/PMUFW原本・runtime予約・実機reviewが不足。生成Tclのtz_boot_authorizedは常に0であり、ここを手動で1へ変えて迂回しない。

4. 以下は将来reviewする実行候補の順序。`scripts/jtag/b2_boot.tcl`はsourceのみでは副作用なし。現状は全実行procedureがauthorizationで停止し、NOT_RUN。

|段階|候補操作と確認|失敗時|
|---|---|---|
|初期化前|board/XSA/cable、boot mode、reset範囲と状態を別途review|未確定ならBLOCKED。自動resetしない|
|PMUFW|PMU targetを一意選択、load/run、固有ready predicateを有限時間poll|timeoutでFSBL/BL31開始しない。PC/ready/IPI経路を採取|
|FSBL|APU0を一意選択、FSBL load、XFsbl_Exitの数値addressへHW breakpoint、run|timeoutでstopしてPC/register採取。別停止点をExit成功としない|
|payload load|承認済み停止状態からBL32→BL33→BL31をload|ELF/hash/予約不一致はload禁止。BL32/BL33単独conをしない|
|BL31 entry|PC/CurrentEL/SCR/SCTLR/TTBR/TCR/MAIR/SP、PMUFW継続を確認|XFsbl_Exit到達だけでBL31への引渡し条件を満たしたとしない|
|OP-TEE初期化|別承認のcontinuationで既知の初期化symbolのHW breakpointへ|timeoutで停止・PC解決。IPI待ちならPMU状態を再照合|
|BL33 main|さらに別段階でstandalone_mainへ。EL2→EL1、HCR/Stage2、cacheを確認|runtime gate0のためTEE試験開始はBLOCKED|

実行候補はtargets -set -filterによる0件/複数件検出、有限poll、HW breakpointを使用する。timeout時にstopする処理は副作用であり、検査モードでは呼ばない。PMU ready/MMIOと停止点はimage固有で、汎用固定値を用意しない。ロード後にSHAを取り直すような一貫性のない組合せを避け、直前に静的検査とELF hashを照合する。

## 自作BL33のUARTなし結果

Aと同じlog_printf/debug_ram_log/debug_boot/debug_exceptionを使い、ZCU106ではUARTを既定無効にする。実際のB2 BL33 ELFから`prepare_jtag_capture.py`でplanを生成する。[Aの回収手順](a_unified_jtag.md)に従い、許可された停止状態でNS logだけを取得してPC側txtへ変換する。Core logがない状態で、Normal側返り値だけから例外原因までPASSとしない。

TF-A本体のNOTICE/ERRORは共通console frameworkで、ZynqMP setup_consoleを使用する。固定sourceのplatform.mkはcadence/cadence0/cadence1/dcc/dtb/noneを認識する。公式資料にはDCC build例がある。自作APIへ置換しない。build_external.pyの`--tfa-console dcc`または`none`はZynqMP用の明示選択肢で、既定は変更しない。DCCはdebug権限・debugger consumer・captureの実機確認が必要で、JTAG接続があるだけで使用可能とは扱わない。

OP-TEEのZynqMP main.c `plat_console_init()`はCadence UARTを初期化・registerする。現在のportに本project用のRAM consoleは実装していない。CFG_TEE_CORE_LOG_LEVEL/CFG_TEE_TA_LOG_LEVELはログ量の設定であり、RAM出力先への切替ではない。level0にしてもUART初期化を不要にした証明にはならない。semihostingやJTAG UARTへの追加portは未実装・実機NOT_RUN。UARTなしではCoreの詳細abort logを回収できない部分があり、breakpoint/ELF symbol/CPU registerによる調査を使用し、原因を確認できなければBLOCKED。

## 現在の試験範囲

実測はrunごとのresults/に保存し、下表を実機PASSで埋めない。全ZCU106機能実行はNOT_RUN。

|ID|実装・前提|期待/PASS条件|分類・未対応|
|---|---|---|---|
|TEE-01|main.c、Early TA-A、同期SHM|2session echo、未知command拒否、値一致|実装済み、QEMU回帰対象|
|TEE-AUTH-TA|security.c trusted_read/trusted_service_test|Core管理identityのTA-AだけBへ許可、PUBLIC/偽login拒否|現在実施可能|
|TEE-02/TEE-AUTH-NS|固定token smokeとTA認可は別|信頼できるNormal要求者認証が必要|認証backend不足BLOCKED|
|TEE-SHM-01|正常/短い/overflow/Secure/wrap指定|Core/TAの拒否元、内容不変、継続|現在実施可能|
|TEE-SESSION-01|boundary_tests.c、同時最大4|4open/close、超過・重複・close後invoke拒否、全session回収|今回追加。client上限試験でありCoreの最大容量試験ではない|
|TEE-ABI-01|boundary_tests.c、同期VALUE通信|不正attrはCore、未知commandはTAが拒否、0/UINT32_MAX echo、継続|今回追加|
|TEE-BOUNDARY-01|GET_SHM_CONFIGで得た範囲|末尾16byte許可、1byte超過・0size・message重複拒否、内容不変|今回追加。予約範囲ではなく交渉済み範囲を使う|
|TEE-ISO-TA|security_tests.c/probe_other_ta|VA alias排除後に実read/write拒否、被害TA非改変、再open継続|現在実施可能。例外だけでPASSにしない|
|TEE-03/TEE-ISO-CORE|QEMU用Core canary PTA overlay|Core canary非改変＋一致するFAR/ESR＋TA継続|通常ZCU106 CoreはPTAなし、backend不足BLOCKED|
|TEE-05/TEE-KEY-API|生成鍵、usage制限、API export拒否、AES KAT|既知解一致・拒否後も鍵利用可|API確認は可能。raw memory抽出耐性TEE-KEY-MEMはBLOCKED|
|TEE-07|panic後dead-sessionをCore CLOSE|別TA継続、再open/echo成功|現在実施可能|
|TEE-STRESS-01|512回、32warmup、Core stats PTA|panic/invalidSHM/keycycle/close、使用量非増加、失敗数不変|QEMU検証用。ZCU106 stats未接続はBLOCKED|
|TEE-04|Early TAと署名ファイルruntime loadは別|正規file受理・改ざんfile拒否を実loadで確認|loader/RPC backend不足BLOCKED|
|TEE-06|永続storage/RPC|再起動・改ざん等|backend不足BLOCKED|
|TEE-08|認証付きboot chain|改ざんboot拒否|未構成BLOCKED、不可逆設定を変更しない|
|GPIO/DMA|既存source保持|今回は実行しない|保留、A結果をB2へ流用しない|

期待値はtests/expectations.json、実行はmain.c→run_security_tests→run_boundary_tests→run_stress_tests、判定はparse_results.py。Core fault logとの突合はcheck_security_evidence.py。source build/QEMU/実機を別々に記録する。

## 再現コマンド

固定来歴を確認できるソースを用いる。外部取得・APT/pip installはbuild中に行わない。

```bash
python3 scripts/offline_build.py --configuration optee --platform qemu --compiler clang --allow-snapshots --qemu --host-tests
python3 scripts/offline_build.py --configuration optee --platform zcu106 --compiler clang --allow-snapshots --host-tests
```

`--allow-snapshots`は本開発環境の検証済みsubset専用。通常の完全固定Git checkoutでは外す。ZIPを無条件でsnapshot扱いしない。TA buildはCROSS_COMPILEに加えCROSS_COMPILE64/CROSS_COMPILE_ta_arm64を明示するよう修正した。最終compile command、ELF machine/entry、SDK生成元を確認し、host gccの出力で成功としない。

## 公式資料と独自部分

- AMD UG1137 2025.1「System Boot and Configuration / Boot Process Overview」: https://docs.amd.com/r/2025.1-English/ug1137-zynq-ultrascale-mpsoc-swdev/Boot-Process-Overview 。本projectの停止点付き手順は独自候補で実機NOT_RUN。
- TF-A 2.12 §7.45/§7.45.1/§7.45.5: https://trustedfirmware-a.readthedocs.io/en/v2.12/plat/xilinx-zynqmp.html 。ZynqMPのBL31、DCC例、FSBL handoffを参照。固定AMD forkの実装分岐を優先して照合する。
- AMD UG1725 2025.1 targets/mrd/bpadd/dow: https://docs.amd.com/r/2025.1-English/ug1725-xsdb-reference-guide/targets 、https://docs.amd.com/r/2025.1-English/ug1725-xsdb-reference-guide/mrd 、https://docs.amd.com/r/2025.1-English/ug1725-xsdb-reference-guide/bpadd 、https://docs.amd.com/r/2025.1-English/ug1725-xsdb-reference-guide/dow 。構文を参照したが、実XSDB/hw_serverは本環境にない。
