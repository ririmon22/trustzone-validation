# A単一ELF：次回実機作業の実行順

この文書は準備→Vitis取込み→build→gate→将来のJTAG起動・停止→固定record→PC側txt回収を1本にまとめたもの。**現在実施できるのはローカル準備まで。実機load/run/resetはBLOCKEDで、以下の実機操作は未実施の候補。安全flagを1へ書き換える手順はない。** B2のBL31/BL32をこの起動へ混ぜない。

## 0. 判定と停止線

|工程|現在の確認範囲|進める条件|
|---|---|---|
|A単一ELF、配置検査、export/CMake|ローカルClangでPASS|実際のsource/run_id/ELF hashを保存|
|Vitis2025.1 GUIとGNU route|NOT_RUN|現地tool・GUI項目・custom CMake連携を確認|
|初期化・FSBL/PMUFW→自作EL3|BLOCKED|同一XSAの原本、runtime予約、DDR/alias、reset/boot順、EL3状態の証跡|
|Secure/Normal実行・保護試験|BLOCKED/NOT_RUN|実XMPU/master経路/lock/NS拒否と安全設計のreview。CONFIG0x8だけでは不足|
|XSDB停止・RAM/record回収|実機NOT_RUN|許可されたdebug access、正しい同一ELF、停止済みCPU、coherencyの確認|

報告済みのunexpected_vector原因は未確定。最適化引数でESR/FAR/ELRを推定しない。PLATFORM_PROTECTION_CONFIRMED=0、B2_RUNTIME_READY=0と既存gateを維持する。GPIO/DMAは保留。

## 1. 使用するファイルと作業directoryを固定する（ローカル）

1. bashでrepo rootへ移動し、新しいrun directoryとrun IDを指定する。例のreadには実在path/一意の整数を入力する。既存runを上書きしない。

```bash
read -r -p 'Repository root: ' TZ_ROOT
cd "$TZ_ROOT"
read -r -p 'New run ID (integer or 0x...): ' TZ_RUN_ID
TZ_LOG_DIR="$TZ_ROOT/results/field-a-$(date -u +%Y%m%dT%H%M%SZ)"
mkdir "$TZ_LOG_DIR"
git status --short > "$TZ_LOG_DIR/git-status.txt"
git rev-parse HEAD > "$TZ_LOG_DIR/git-head.txt"
```

2. 対象ファイルを確認する。編集元はrepo、Vitisへ渡すcopyは後述export先。両方を独立編集しない。

|用途|ファイル・symbol|
|---|---|
|ELF入口|baremetal/el3_monitor/start.S、`_start`=el3__start|
|World切替|baremetal/el3_monitor/monitor.c、el3_monitor_main/monitor_dispatch|
|Secure/Normal入口|baremetal/secure_app/start.S、baremetal/nonsecure_app/start.S。ELF symbolはsecure_entry/nonsecure_entry|
|試験|baremetal/{secure_app,nonsecure_app}/main.c、common/test_runner/runner.c|
|共通処理の別実体|common/arch.c、common/memory.c、common/result_report/console.c、common/debug_state.c|
|例外|common/el1_vectors.S、common/include/exception_capture.inc、common/el1_fault.c|
|配置|platform/zcu106/linker_a_unified.ld、memory_map.{h,json}、vitis_observations.json|
|構築/検査|scripts/{build_a_unified,check_a_unified,export_a_vitis,prepare_a_unified_jtag}.py|
|回収/変換|scripts/jtag/{a_unified,capture}.tcl、scripts/{decode_ram_log,decode_exception}.py|

旧3 ELFは比較用として残すが、単一ELFのrunで追加loadしない。World別に関数・BSS・vector・stack・logを配置し、単一ELFが保護機構の代わりになるとは扱わない。

## 2. ツール・Platformを確認する（Vitis GUI部分はNOT_RUN）

3. Vitisの実インストール先とAArch64 GNU prefixを確認する。付属tool名を推測しない。

```bash
read -r -p 'Vitis installation directory: ' TZ_VITIS
if test -f "$TZ_VITIS/settings64.sh"; then source "$TZ_VITIS/settings64.sh"; else echo 'BLOCKED: settings64.sh missing'; fi
read -r -p 'AArch64 GNU compiler prefix: ' TZ_CROSS
"${TZ_CROSS}gcc" --version
"${TZ_CROSS}as" --version
"${TZ_CROSS}ld" --version
"${TZ_CROSS}objcopy" --version
python3 --version
make --version
cmake --version
```

一つでも必要toolがなければ停止して不足を記録する。自動download/installはしない。

4. Vitis Unified IDEでWorkspaceを指定する。Platformが既に正しいZCU106/A53/Standaloneなら再利用し、変更しない。新規作成の候補メニューは`File > New Component > Platform`。内蔵ZCU106 hardware定義を選び、processor=`psu_cortexa53_0`、OS=`standalone`、AArch64にする。定義が表示されなければ別boardを代用せずBLOCKED。
5. PlatformのBuild、生成XSA/SDT/BSP/linker/FSBL/PMUFWの場所を記録する。FSBL/PMUFWの原本ELF/map・stack/heap・寿命を採取し、今回のreported LOAD範囲との一致だけで完全予約としない。stdin/stdout用UART設定があっても、本AコードはBSP console/startupを使わない。

## 3. 1 Applicationへ取り込む（GUI未検証、CMake部分のみローカル確認済み）

6. 存在しないexport先を指定する。helperは上書きを拒否する。

```bash
read -r -p 'New Vitis export directory: ' TZ_EXPORT
python3 scripts/export_a_vitis.py --output "$TZ_EXPORT"
```

7. `File > New Component > Application`で名称を例えば`tz_a_unified`とする。上記PlatformとA53 Standalone domainを指定する。空アプリからexport先`src`を取り込み、**自動列挙C/ASM＋BSP startupの通常linkを使わず、提供`src/CMakeLists.txt`のcustom target a_unifiedへbuildを接続**する。この部分の具体的なGUI連携は実機Vitis未検証。利用中IDEがcustom CMake targetを扱えない場合は、下記外部buildを使用し、そのELFをdebug対象として指定する。3 Application構成へ戻して単一ELF成功と扱わない。
8. 設定内容を次と照合する。

|項目|値・確認内容|
|---|---|
|CMake source|TZ_EXPORT/src。提供CMakeはproject NONE＋custom target|
|Compiler|TZ_COMPILER=gcc、TZ_CROSS_COMPILE=実在prefix|
|run ID|TZ_RUN_ID=手順1の値|
|Include|common/include、platform/zcu106（helperが指定）|
|ASM|`.S`をcompiler driverで前処理・assembly|
|Startup|各World既存startを別object化しsymbol prefix。ELF entryはEL3 `_start`だけ|
|C flags|C11、Cortex-A53、freestanding、general registers only、O2/g、warnings as errors、fstack-usage|
|Linker|platform/zcu106/linker_a_unified.ld、-nostdlib -nostartfiles -nodefaultlibs|
|追加しないもの|BSP boot.S/crt0/crtbegin.o/libc/libxil、default linker、他ApplicationのELF|
|Output|TZ_EXPORT/src/build/zcu106/unified/a_unified.elf|

外部buildの同じ入口は以下。以後はこのELFを使い、repo側にある別buildのELFへ取り違えない。

```bash
cmake -S "$TZ_EXPORT/src" -B "$TZ_EXPORT/cmake-build" -DTZ_COMPILER=gcc -DTZ_CROSS_COMPILE="$TZ_CROSS" -DTZ_RUN_ID="$TZ_RUN_ID"
cmake --build "$TZ_EXPORT/cmake-build" --verbose > "$TZ_LOG_DIR/build.log" 2>&1
TZ_ELF="$TZ_EXPORT/src/build/zcu106/unified/a_unified.elf"
python3 scripts/check_a_unified.py --platform zcu106 --elf "$TZ_ELF" > "$TZ_LOG_DIR/a-static.json"
"${TZ_CROSS}readelf" -h -l -S "$TZ_ELF" > "$TZ_LOG_DIR/elf-layout.txt"
sha256sum "$TZ_ELF" > "$TZ_LOG_DIR/elf.sha256"
cp "$TZ_EXPORT/src/build/zcu106/unified/commands.json" "$TZ_LOG_DIR/link-commands.json"
```

`build.log`とcommands.jsonの実link commandにCRT/BSPが紛れ込んでいないことを確認する。discarded .eh_frameだけを直すためにCRTを足さない。prefix付きsymbolが3領域に別実体として存在すること、LOAD/BSS/stack/vectorの静的検査成功を確認する。

## 4. ロード前gateで停止する（現在の到達点）

9. 同じELFから停止点・回収planを生成する。出力2は「静的PASSだがload BLOCKED」。出力1は入力/配置エラーなので両者を区別する。

```bash
python3 scripts/prepare_a_unified_jtag.py --elf "$TZ_ELF" --output "$TZ_LOG_DIR/a-plan.json" --emit-tcl "$TZ_LOG_DIR/a-plan.tcl"
python3 scripts/check_hardware_profile.py
```

10. a-plan.jsonのLOAD/entry/hashとblockersを保存する。FSBL/PMUFW/XSA/runtime予約、DDRのsystem PA/alias、XMPU route/master/lock、EL3入口状態、reset/CPU解放順、debug access/coherencyの確認が必要。**現在はここで止める。tz_a_load_authorized=0を変更しない。** jtag_a_plan.jsonは旧3 ELF用であり、この単一ELF手順のload順として使わない。既存の共通gateにはDMA/GIC接続の未確認理由も残っており、今回は保留項目だからという理由で削除しない。将来、A専用の許可条件を変更する場合も別途安全設計のreviewが必要。

## 5. 将来の初期化・JTAG load（現時点NOT_RUN/BLOCKED）

11. 以下は必要証跡のreviewが済んだ将来の手順候補。board電源・JTAG・同一XSAの初期化・PMUFW/FSBLの役割、停止点、他core停止をrunに記録する。resetやinitは現設定で未確定なので汎用reset/mwrコマンドを示さない。FSBL Exit到達を自作EL3への正しい引渡しと同一視しない。
12. Vitisのdebug構成は対象ELFを上記TZ_ELFにし、通常BSPのrun-to-main、自動reset/download/runを使わない。Register InspectorでCurrentEL=EL3、SPSel=1、SCTLR_EL3.M/C/I=0、SP/PC/DDRアクセスが有効である条件を確認する。未確認状態でcacheを強制offにしない。一般のStandalone起動と混同しない。
13. XSDBは既存の許可された接続で`targets -target-properties`を保存する。複数board/cableがある場合はfilterに実propertiesを追加する。以下は定義とplan読込みだけなら副作用なし。pathは作業したrepo/runの実pathにする。

```tcl
source scripts/jtag/a_unified.tcl
source scripts/jtag/capture.tcl
# 実際のrun directoryへ置換する。
set run_dir /path/to/results/field-a-RUN
source [file join $run_dir a-plan.tcl]
set apu_filter {name =~ "*Cortex-A53*#0"}
```

14. 将来の候補は`tz_a_load $apu_filter`→entryで停止・PC/ELを再確認→`tz_a_run_to $apu_filter monitor 10000`。**現planでは最初の命令でBLOCKEDになり、targets/dow/conを実行しない。** ゲートを迂回するために変数を1にしない。この文書は許可の代わりにならない。

## 6. 停止点・例外調査（実機NOT_RUN）

15. monitor停止点のsymbolは`el3_monitor_main`。引数表示ではなく固定recordを読む。protection flag0の通常runはmonitor内でstage20/reason1の意図停止となり、Secure/Normalへは進まない。
16. 将来、保護を含む実行条件が満たされたrunだけ、`secure`→`normal`→`finish`の順にtz_a_run_toで止める。最後のfinishは`nonsecure_platform_halt`であり、halt到達だけでは合格ではない。NormalのSUMMARY/ENDを必ず確認する。timeout/別PCならCPUを停止したまま調査し、reset/reload前にrecordを回収する。
17. 調査する固定symbolは以下。offsetの手入力ではなくa-plan.tclから回収する。

|symbol suffix|意味|
|---|---|
|debug_boot|version/stage/stop_reason/run_id。stage10/20/30/80/90（25はB2専用）、reason1/2/3|
|debug_exception|最初のfatal：valid0なし、1採取中、2保存完了。EL/vector/ESR/FAR/ELR/SPSR/SP/x0..30|
|debug_sync_exception|最後のSMC/期待Data Abortを含む同期record。fatal記録と区別|
|debug_ram_log|各Worldの文字列。header32byte＋capacity|

entry_state_blockedではRAMへ触れず止まるため、未初期化recordを読むだけで原因を判断できない。x0のCurrentEL、EL3時のx1/SCTLR、PCと入口逆アセンブルを確認する。SP破壊やrecord自身のアクセス不能時も採取できない場合がある。

## 7. 停止・binary→txt回収（実機NOT_RUN）

18. 必要なら影響をreviewしたrunで`tz_stop_for_capture $apu_filter`を実行する。副作用はCPU停止。load/run/resetはしない。停止済みtargetのみtz_dumpが受理する。
19. 許可されたSecure debugでEL3/Secure、通常のNS領域でNormalを別ファイルに採取する。権限不足時に保護解除、mrd -force、SHMへのcopyはしない。

```tcl
tz_dump $apu_filter $tz_capture_plan [file join $run_dir raw]
```

20. PC側bashへ戻り各logを変換する。停止中のEL3/SecureはRUNNINGであることがあり、decoderのexit2は部分回収。未到達Worldはheader未初期化か前runの残存であり、今回のログと扱わない。新規run_idとELF hash・到達証跡を照合し、FAIL/NOT_CAPTUREDと記録する。存在しないCOMPLETEを作らない。

```bash
for TZ_WORLD in el3 secure nonsecure; do
    python3 scripts/decode_ram_log.py "$TZ_LOG_DIR/raw/${TZ_WORLD}_debug_ram_log.bin" --output "$TZ_LOG_DIR/${TZ_WORLD}.txt" --expected-run-id "$TZ_RUN_ID"
    python3 scripts/decode_exception.py "$TZ_LOG_DIR/raw/${TZ_WORLD}_debug_exception.bin" > "$TZ_LOG_DIR/${TZ_WORLD}-exception.json"
    python3 scripts/decode_exception.py "$TZ_LOG_DIR/raw/${TZ_WORLD}_debug_sync_exception.bin" > "$TZ_LOG_DIR/${TZ_WORLD}-last-sync.json"
done
```

raw/ELF/hash/run_id/decoded JSON/txtを同じrunに保存する。AのMMU/cache off契約に反した状態や、AP経路の読出しerrorはcoherency/アクセス条件を調査する。JTAGで読めたことをNSから読めた証拠にしない。

## 8. 判定と片付け

21. Normal logがCOMPLETE・dropped0・正しいrun_idで、RUN/RESULT/SUMMARY/ENDが揃った場合だけ解析する。

```bash
python3 scripts/parse_results.py "$TZ_LOG_DIR/nonsecure.txt" --configuration baremetal --output "$TZ_LOG_DIR/results.json"
python3 scripts/check_tz_audit.py "$TZ_LOG_DIR/nonsecure.txt" --output "$TZ_LOG_DIR/tz-audit.json"
```

TZ-SYS-03は例外だけでなくMMU状態、実機保護機構、許可アクセス、読み書き拒否、Secureからの非改変確認が必要。QEMU用監査成功を実機XMPU成立と同一視しない。実機根拠不足はBLOCKEDのままにする。
22. 想定外例外は固定ESR/FAR/ELR→ELR前後命令→FAR有効性→SCR/SCTLR/HCR/VBAR/SP→XMPU/master経路の順で確認する。`"${TZ_CROSS}addr2line" -e "$TZ_ELF" -f -C <実ELR>`は補助であり関数名だけでは原因を確定しない。既存unexpected_vectorの原因を単一ELF化で修正済みと扱わない。
23. CPUを停止したまま証跡を保存し、復旧/resetは別途reviewしたboard手順に従う。本書に無条件のreset/保護解除は含めない。

## 公式資料と実測の境界

- AMD UG1400 2025.1「User Managed Mode」: https://docs.amd.com/r/2025.1-English/ug1400-vitis-embedded/User-Managed-Mode 。Makefile/CMakeを自主管理する一般手順を参照。User Managed ModeとPlatform付きApplication Componentは同一UIではない。GUI custom target連携は本projectでNOT_RUN。
- Application一般menuはUG1702 2025.1「Creating an Application Component」: https://docs.amd.com/r/2025.1-English/ug1702-vitis-accelerated-reference/Creating-an-Application-Component 。対象domain・既定startupの扱いは本project独自設定。
- AMD UG1725 2025.1「targets」「mrd」「bpadd」「dow」: https://docs.amd.com/r/2025.1-English/ug1725-xsdb-reference-guide/targets 、https://docs.amd.com/r/2025.1-English/ug1725-xsdb-reference-guide/mrd 、https://docs.amd.com/r/2025.1-English/ug1725-xsdb-reference-guide/bpadd 、https://docs.amd.com/r/2025.1-English/ug1725-xsdb-reference-guide/dow 。構文・一意filter・binary保存の根拠。ここでのTclはhost syntax/mock検査のみ。
- UG1400の2025.1 Application作成pageは今回の閲覧では取得できなかった。別版の操作を2025.1で確認済みとは記載しない。実GUIにない項目は推測して変更せずBLOCKEDとして記録する。
