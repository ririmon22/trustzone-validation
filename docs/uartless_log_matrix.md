# UARTなしで回収できるログの範囲

本表の「実装済み」は回収コード・格納領域があるという意味であり、ZCU106で取得済みという意味ではない。全実機回収はNOT_RUN。保護・debug権限・cache coherencyを変更せず取得できる条件が必要。RAMログはtarget filesystemを使わずPC側でtxtにする。

|構成・イメージ|現在の出力経路と格納先|UARTなしで回収できる内容|現時点の限界|
|---|---|---|---|
|A EL3|log_printf→el3_debug_ram_log。EL3領域に別実体|BOOT/World準備/停止理由、el3_debug_boot、el3_debug_exception、el3_debug_sync_exception|保護flag0で意図停止。Secure debug権限・EL3領域読出しが必要。RUNNINGは途中状態|
|A Secure EL1|log_printf→secure_debug_ram_log。Secure dataに別実体|Secure起動、メモリ再確認、secure_debug_boot/exception/sync_exception|現状flag0では入口へ到達しないため未初期化が正常。Secure logをNSへcopyしない|
|A Normal EL1|log_printf→nonsecure_debug_ram_log。Normal dataに別実体|試験RESULT/SUMMARY/END、性能metadata/sample、Normal例外record|QEMUでUART無効RAM→txtを確認済み。ZCU106実機NOT_RUN。完全回収はCOMPLETEかつdropped0が必要|
|B2 BL31|TF-A NOTICE/ERROR→console framework→setup_console。既定Cadence UART|現在の既定buildではRAMへ保持しない。PC/register/停止点はdebuggerで確認可能|RAMログ未実装。DCC選択肢はsource/公式仕様ありだが実機consumer未検証|
|B2 OP-TEE Core|trace_ext_puts→console_putc→serial_console。ZynqMPはplat_console_initでCadence UART登録|現在の既定buildではCoreのtrace/panic/abort全文をRAMから回収できない。CPU registerとELF symbolによる停止点調査は可能|本projectのCore RAM consoleは未実装。BL33の例外recordはCoreの例外recordではない|
|B2 BL33|log_printf→debug_ram_log（131072byte）。NS data、B2 MMUでWB/NX|自作試験結果、SMC transport/result/origin、起動段階、NS側例外record|B2_RUNTIME_READY0ではMMU準備前に停止。Core/TAに未到達でもBL33部分logは取得候補。dirty cache/途中snapshotは別途coherency確認|
|B2 TA-A/B|通常traceはlibutee trace_ext_puts→_utee_log→Core syscall_log→Core trace/console|TAコマンドの返り値・originはBL33が記録可能。TA自身のtrace全文は現状UARTなしで独立回収不可|BL33の結果はTA traceそのものではない。Secure EL0の動的VAをそのままJTAG PAとして読まない。TA instanceはpanic/closeで解放され得る|

TA-A/Bのta.c/security.cには現在独自のtrace呼出しを追加していない。panic等のCore診断と、TAコマンドの戻り値を分けて読む。TAごとの自作debug_ram_logは存在しない。

## 現在使う回収方法

実loadと同じELFからprepare_jtag_capture.pyでplanを作り、許可された停止状態でscripts/jtag/capture.tclのtz_dumpを使用する。mrd -bin -file -size bでPCへbinaryを保存し、decode_ram_log.py/decode_exception.pyでtxt/JSONにする。Aの実行順は[a_unified_field_runbook.md](a_unified_field_runbook.md)。header32byteとcapacity全体を取得し、version/run_id/used/dropped/state、ELF hashを保存する。

未到達WorldのRAMには前runのlogが残り得る。新しいrun_idをbuildで指定し、decode_ram_log.pyの--expected-run-idで照合できる。magic/COMPLETEだけでは今回の実行証跡としない。

Aは起動契約上MMU/cache off。B2 BL33は完了時にlogをPoCへcleanするが、JTAG APによるlive途中読出しがcoherentとは限らない。Secure/EL3とNormalのcapture先を分ける。権限不足、未初期化、読出しerrorはBLOCKED/NOT_CAPTUREDで、NS共有へ逃がさない。

## OP-TEE CoreのRAM console対応案（未実装）

固定sourceのcore/kernel/console.cはweak console_putc/console_flushとregister_serial_consoleを持つ。既存console APIを維持して、検証専用port/overlayに次を追加する案がある。

1. CoreのSecure reserved RAM内に独立したbounded bufferを配置し、Core BSS/TA pool/pager/stacksと重ならないlinker予約を検査する。早期bootとMMU有効後のVA/PA対応、__nex_bss等の初期化時期を決める。
2. serial_chipのputc/flushをRAM実装として登録し、ZynqMP plat_console_initのUART初期化を、明示的な検証用configで置換する。通常portを黙って改変しない。trace_ext_putsのplat_trace_ext_puts hookに追記するだけでは後続UART出力が残るため、UART不要化は完了しない。
3. IRQ/複数Core/early/panic時のlock再入性、overflow、record version、run_idとfreeze、cache cleanを設計する。panic前のlog採取と例外register採取は別機能として扱う。
4. Core ELF symbolとphysical mappingを証明してSecure debug経路から取得する。Normalから読めるMEM_AREA_RAM_NSECへ置かない。flush前途中dumpの扱いを定義する。
5. QEMUで通常boot・TA trace・panic・overflow・reset別runを再検証し、normal/validation buildを別名・hashで管理する。実機では権限とcoherencyを確認する。

これを実装すれば、Core syscall_logへ集約されるTA traceも同じSecure bufferで回収可能になる見込み。ただしCore初期化前やTA trace無効時に失われた文字列は復元できない。TAごとに別RAM bufferが必要なら、識別情報・instance lifetime・アクセス制御の追加設計が必要。

## 既存OP-TEE機能をそのまま使えない理由

- core/drivers/cbmem_console.c（CFG_CBMEM_CONSOLE）はCoreboot table/DTを探索し、MEM_AREA_RAM_NSECに既存consoleをmapする。現在のFSBL/JTAG構成にはそのtableがなく、Secure traceをNS RAMへ置く設計も要件に反する。configを立てるだけの解決にはしない。
- CFG_SEMIHOSTING_CONSOLE=yはCFG_SEMIHOSTINGを有効にし、既定でhostのoptee.logへ出力する実装がある。Arm64のsemihosting_a64.SはHLT #0xf000を使う。Secure EL1のdebugger対応、monitorでのtrap扱い、継続・安全性、host側ファイル権限を確認する必要がある。現在は無効・未検証で、自動enableしない。
- CFG_FFA_CONSOLEはFF-A/SPM経路の仕組みであり、現在のSPD=opteed構成の出力先変更ではない。
- CFG_TEE_CORE_LOG_LEVEL/CFG_TEE_TA_LOG_LEVEL/CFG_TEE_CORE_TA_TRACEは量や生成を制御する。RAM保持やdebugger対応を追加する設定ではない。

## TF-A BL31対応案（未実装）

ZynqMP platform.mkとdrivers/arm/dcc/dcc_console.cにDCCがあり、`build_external.py tfa --platform zynqmp --configuration b2 --tfa-console dcc`で明示選択できる。実際のdebug権限・DCC receiver・XSDBとの接続方法はNOT_RUNで、JTAGが接続済みというだけでは出力保証にならない。noneは出力抑止であり回収ではない。

RAM化する場合はTF-Aのconsole_register frameworkへ独立RAM driverを追加し、BL31予約内のstack/BSS/translation tableと競合しないbufferをlinkする。boot/runtime console scope、panic/reentrant時の保存、Secure RAMのPAとcache maintenanceを設計する。自作log_printfをBL31全体へ置換する必要はない。新しいconsole driverを追加したTF-Aは派生buildとして来歴・差分・hashを明記する。

## 根拠

固定ソース: TF-A plat/xilinx/zynqmp/platform.mk、drivers/arm/dcc/dcc_console.c。OP-TEE core/kernel/{console,trace_ext}.c、core/arch/arm/plat-zynqmp/main.c、lib/libutee/trace_ext.c、core/tee/tee_svc.c、core/drivers/{cbmem_console,semihosting_console}.c、mk/config.mk。外部コードは今回変更していない。

AMD UG1725 2025.1 Target Memory/mrd: https://docs.amd.com/r/2025.1-English/ug1725-xsdb-reference-guide/mrd 。TF-A 2.12 §7.45のDCC例: https://trustedfirmware-a.readthedocs.io/en/v2.12/plat/xilinx-zynqmp.html 。公式機能の存在と本環境での実動作を区別する。
