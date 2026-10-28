実行順の統合版は[a_unified_field_runbook.md](a_unified_field_runbook.md)、イメージ別の回収範囲は[uartless_log_matrix.md](uartless_log_matrix.md)を参照。

# A単一ELF・RAMログ・例外採取

## 状態と今回の実機報告

本変更で実機操作は行わない。Vitis2025.1 GUI/XSDBの実行はNOT_RUN。報告されたmonitor_main到達、保護flagを手元で1にした後のunexpected_vector、XMPU0/1全Region CONFIG=0x8、XMPU2一部AXI AP読出しerrorはUSER_REPORTEDであり、原本register dump/ESR/FAR/ELRは未提供。XMPUが有効・無効・保護成立のいずれとも確定しない。AXI APアクセス失敗をCPU NSアクセス拒否と扱わない。既存のPLATFORM_PROTECTION_CONFIRMED=0、B2_RUNTIME_READY=0とロードゲートを維持する。

## 1. 単一ELFの構造

`make PLATFORM=zcu106 a-unified`で`build/zcu106/unified/a_unified.elf`を作る。比較用の既存3 ELFは`make PLATFORM=zcu106 baremetal`で保持する。前者のELF入口は`_start`（EL3のalias）の1つで、Secure/Normal入口は`secure_entry`/`nonsecure_entry`。EL3 monitorのWorld contextは従来どおり各CODE_BASEを指す。入口とCODE_BASEの一致を静的検査する。

`build_a_unified.py`はWorldごとに既存start/main/common/vectorをcompile→relocatable link→objcopy symbol prefix→最終linkする。prefixはel3_/secure_/nonsecure_。共通関数、文字列、BSS、printf、debug record、RAMログも各領域に別実体を置く。Secure側関数をNormal側から直接共有呼出ししない。追加ライブラリ、libc、BSP startup、CRT、heapを使用しない。最終linkは`-nostdlib -nostartfiles -nodefaultlibs`で、実commandを`commands.json`とbuild logへ保存する。C/ASM入力も全て記録する。crtbegin.oや既定crt0を追加してはいけない。`.eh_frame`を除外するだけでCRT依存を解決したことにしない。

各platformの`linker_a_unified.ld`がtext/data/BSS/vector/stackを別PT_LOADに置く。World別BSSだけを各入口がclearする。stackは全量NOLOAD、heapなし。vectorは2048 alignment。`check_a_unified.py`はLOAD重複・領域外・BSS・stack・vector・diagnostic位置と報告済みFSBL/PMUFW範囲を検査する。現物の未報告runtime予約・alias・XMPUは未確認。単一ELFは物理メモリ保護の代わりにならない。

## 2. Vitisの1 Applicationへ取り込む

1. [Platform手順](manual/02_vitis_platform_creation.md)でZCU106/A53用Platformを作る。実XSA・BSP・FSBL/PMUFWのhashを保存する。
2. 新しいexport先を指定する。既存directoryへ上書きしない。

```bash
python3 scripts/export_a_vitis.py --output build/vitis-a-unified
```

3. Applicationは1つだけ作成し、exportした`src`ツリーと`src/CMakeLists.txt`を使用する。空のアプリから、生成BSPのstartup/crt0と既定link処理を取り除き、提供CMakeのcustom targetへ接続する。GUI項目は[Application手順](manual/03_vitis_application_creation.md)を参照。Vitisによるcustom target連携は未実行であり、既定templateをそのまま使えるとは断定しない。
4. CMake cacheで`TZ_COMPILER=gcc`、実在する`TZ_CROSS_COMPILE`、`TZ_RUN_ID`を設定する。includeはcommon/includeとplatform/zcu106。ASMはGNU assembler-with-cppの`.S`。Python helperがWorld別にcompileするため、IDEが全C/ASMを再帰的に1targetへ自動追加する設定にしない。
5. IDEのBuild出力と`src/build/zcu106/unified/commands.json`を保存し、crtbegin.o/crt0/libxil/libcがリンクされないことを確認する。ELFは`src/build/zcu106/unified/a_unified.elf`。リンカはplatform/zcu106/linker_a_unified.ld。Debugの自動download/runは使用しない。

IDEなしでexportを検査する例（実在toolの場所へ置換）：

```bash
cmake -S build/vitis-a-unified/src -B build/vitis-a-unified/cmake -DTZ_COMPILER=clang -DTZ_LLVM_DIR=/usr/lib/llvm-21/bin
cmake --build build/vitis-a-unified/cmake --verbose
```

GNU/Vitis経路は実機環境での確認が必要。Clang routeの成功をVitis成功と記載しない。

## 3. printf形式とRAMログ

通常出力は`log_printf(format, ...)`へ統一した。旧console_write/hex/uintは同じformatterへ委譲する互換wrapper。整数、文字列、文字、pointer、%%、u/d/x、l/ll、最大幅32を提供する。float、%n、precision、mallocなし。formatは信頼した静的文字列を使う。unsupported formatは-1であり、libc printf全仕様ではない。ローカル数値bufferは20byte、実際のstackはcompiler依存。単一ELF buildでは`-fstack-usage`を有効にして`.su`を保存する。今回Clang21.1.8ではlog_printf単体192byte、数値helper単体32byteだった。呼出し側・例外frameを含む最大深度やVitis/GNUの値は別途確認が必要。再帰・IRQ並行出力・SMPは非対応で、現構成の1CPU/masked IRQ/同期呼出しを前提にする。

`debug_ram_log`は各WorldのBSSに別配置。headerはLEのmagic/version/capacity/used/dropped/state（各u32）とrun_id（u64）の32byte、続いてdata。通常capacity=32768、B2 BL33は131072。先頭を保存し、満杯後は上書きせずdroppedを飽和加算。RUNNING/COMPLETE/FATALを区別する。途中・overflowは完全logと扱わない。run_idはbuild時DEBUG_RUN_ID（既定1）、単一ELFでは`build_a_unified.py --run-id 0x...`で指定する。再実行ごとに一意な値を付け、ELF hashと保存する。

ZCU106の自作logは既定UARTなし。QEMUではUARTとRAMへ二重出力する。Secure/EL3のlogをNS共有メモリへcopyしない。秘密鍵・秘密データをlogへ追加しない。JTAGでSecure logを読むには許可されたSecure debug権限が必要で、読めない場合はBLOCKEDとする。保護を解除しない。

AはMMU/cache off前提。B2は完了時log_set_stateでA53の64byte lineをPoCへcleanする。途中停止時、例外record、他coreやdebug APとのcoherencyは別途確認が必要。実機JTAG取得自体はNOT_RUN。生のAXI AP dumpがCPUのdirty cacheを含むとは仮定しない。

## 4. JTAGからPC側txtへ

1. 前提としてロード安全ゲートは現在BLOCKED。以下は将来の承認済み停止状態での読出し候補であり、今回接続・stop・runはしない。
2. 実際にloadしたELFからsymbol planを生成し、hash、address view、対象boardを照合する。

```bash
python3 scripts/prepare_jtag_capture.py build/zcu106/unified/a_unified.elf --output results/jtag/a-capture.tcl
```

3. XSDBの既存接続で`targets -target-properties`を保存する。sourceだけでは操作しない。

```tcl
source scripts/jtag/capture.tcl
source results/jtag/a-capture.tcl
# 実在target propertiesから一意になるfilterを設定する。番号を固定しない。
set apu_filter {name =~ "*Cortex-A53*#0"}
# 将来、停止の影響をreviewしたrunでのみ実行する（CPU停止の副作用あり）。
# tz_stop_for_capture $apu_filter
# 未停止・複数対象・アクセス失敗ではerror。forceや保護解除はしない。
tz_dump $apu_filter $tz_capture_plan results/jtag/raw
```

4. PC側Pythonでtxtへ変換する。実機filesystem、semihosting、JTAG UARTを必要としない。

```bash
python3 scripts/decode_ram_log.py results/jtag/raw/nonsecure_debug_ram_log.bin --output results/jtag/nonsecure.txt
python3 scripts/decode_exception.py results/jtag/raw/el3_debug_exception.bin
```

decoderは不正header/lengthをFAIL、未完了・overflow・fatalをPARTIAL（exit2）とする。txt生成成功を機能PASSとしない。RUN/RESULT/SUMMARY/ENDが完全なら既存parse_results.pyへ渡す。EL3/Secureはservice継続中に完了しないためRUNNINGが正常な途中状態の場合もある。全WorldをCOMPLETEと偽装しない。raw binaryも保持する。

## 5. 起動・例外記録

`debug_boot`: version/stage/stop_reason/run_id（各u64）。stage10=BSS初期化後、20=monitor、25=B2 MMU準備停止、30=Worldへ移行直前、80=試験完了、90=fatal。reason1=未確認保護、2=B2準備未完、3=fatal。entryguardでEL3/変換状態不正の場合はRAMへ触れずentry_state_blockedへ停止するため、この段階のrecordは未採取。CurrentELとSCTLRをregister viewで確認する。

`debug_exception`: valid/EL/vector/ESR/FAR/ELR/SPSR/SP/SP_EL0/SP_EL1/x0..x30の328byte。valid0=なし、1=保存中、2=保存完了。unexpected vectorはstackやCを使わず最初のrecordを保存し、以後上書きせずWFE停止。保存用にTPIDR_EL0/TPIDRRO_EL0を破壊するが、fatal経路は復帰しない。record領域自体がアクセス不能なら採取できない。EL1でSP_EL1欄はhandler現在SPであり、EL3だけ実SP_EL1を読む。

`debug_sync_exception`はSMC/期待Data Abortを含む最後の同期例外。整数frameとESR等をC呼出し前に保存する。expected faultは既存handle_el1_syncの厳密な条件でのみ復帰する。同期入口の最初の整数frame保存は正常なSPを必要とする。SP破壊の二重例外を完全に救済するものではない。

最適化されたfatal_exception引数や逆アセンブルだけで原因を確定しない。まず固定record→ELR前後命令→ESR EC/ISS→FAR有効性→SCR/SCTLR/HCR/VBAR/SP→XMPU/master経路の順で照合する。今回のunexpected_vectorは単一ELF化で修正済みとは扱わない。

## 6. ローカル再現

```bash
RESULT_DIR=results/a-unified bash scripts/run_a_unified_qemu.sh
python3 scripts/build_a_unified.py --platform qemu --fault-injection
python3 scripts/pack_qemu_rom.py --unified --stub build/qemu/bootstrap.bin --output build/qemu/unified-fault/rom.bin build/qemu/unified-fault/a_unified.elf
python3 scripts/test_exception_qemu.py --output-dir results/a-unified/fault
```

fault注入はQEMU専用。undefined instructionのEC、vector、x0/x1 sentinelを実dumpと照合し、保護成立の試験とは分離する。実機の安全flagを変えるためのoptionは提供しない。

## 公式資料・確認範囲

- AMD UG1725 2025.1「Target Connection Management / targets」「Target Memory / mrd」: https://docs.amd.com/r/2025.1-English/ug1725-xsdb-reference-guide/targets 、https://docs.amd.com/r/2025.1-English/ug1725-xsdb-reference-guide/mrd 。-set/-filterは0件・複数件をerrorにし、mrd -bin -file -size bを使用する。Tcl syntax/host stub検査までで実XSDBはNOT_RUN。
- AMD UG1400 2025.1のIDE一般操作は既存manualを参照。本projectのcustom CMake/startup/linkは独自設計でありAMD GUI実行済みではない。
