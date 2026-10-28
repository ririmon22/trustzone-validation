# 次回オフライン実機作業：短い入口

現在はローカルbuild/検査まで。実機load/run/resetを行う条件は未確認で、全ゲートを維持する。ソース・設定・スクリプト・テスト・文書のみを移送し、外部ソース/過去結果/ELF/SDKは含めない。

## 1. 展開と設定

PCでtarとSHA確認fileを同じdirectoryへ置く。今回tarはtrustzone-validation/をtop directoryに持つ。

```bash
sha256sum -c trustzone-validation.tar.sha256
tar -xf trustzone-validation.tar
cd trustzone-validation
cp config/field.example.json config/field.local.json
```

local.jsonへ実在するZIP展開root、任意ZIP path、AArch64 tool prefix、run_id、新しいa_export directoryを入力する。元sourceは編集しない。[ZIP build](zip_offline_build.md)に判定と不足toolの意味を記載した。

## 2. A単一ELFを1 Applicationへ

```bash
python3 scripts/field.py a-export config/field.local.json
python3 scripts/field.py a-build config/field.local.json
python3 scripts/field.py a-check config/field.local.json
```

a-exportは新規folderを作り、既存folderは上書きしない。Vitis2025.1でZCU106 Platform、psu_cortexa53_0/AArch64/Standalone domainを選び、空のApplicationを1つ作る。exportのsrc/CMakeLists.txtをcustom target a_unifiedとして接続する。helperがWorldごとにcompile/symbol prefixして1 ELFへlinkするため、IDEが全sourceを通常targetへ再帰追加しない。BSP startup/crt0/crtbegin.o/libc/libxil/default linkerを組み込まない。

includeはcommon/includeとplatform/zcu106、ASMは前処理する.S、linkerはplatform/zcu106/linker_a_unified.ld。TZ_COMPILER、TZ_CROSS_COMPILE、TZ_RUN_IDはlocal.jsonからCMakeへ渡す。実commandはcommands.json/commands.logに保存する。ELFはa_export/src/build/zcu106/unified/a_unified.elf。

Vitis GUIでのcustom target連携とGNU routeはNOT_RUN。連携できない場合は上記外部CMake buildで作った同じELFをdebug対象に指定する。3 Application構成へ戻して単一ELF成功と扱わない。詳細な実行順は[A統合手順](a_unified_field_runbook.md)。

## 3. JTAG停止・例外・txt回収

a-checkが表示したresults/field/<UTC>/a-plan.tclに、実ELF hash・停止点・capture領域が入る。status2は静的検査が通ってもhardware BLOCKEDという意味。初期化/DDR/保護/EL3条件が未確認ならloadしない。

XSDBの既存の許可された接続で、次の定義読込みはそれ自体では副作用を持たない。

```tcl
source scripts/jtag/field.tcl
set plan /path/to/results/field/RUN/a-plan.tcl
set cpu {name =~ "*Cortex-A53*#0"}
tz_field_check $plan
```

一意filterを実target properties/cableで確認する。以下は副作用を伴うため現在実行しない。

```tcl
# CPU停止のみ（reset/load/runなし）:
# tz_field_stop $cpu
# 停止済みRAM読出し→PC側binary保存:
# tz_field_dump $cpu $plan /path/to/pc/raw
# loadとmonitorまでのrun。現planは0なので操作前にBLOCKED:
# tz_field_a_start $cpu $plan
```

rawを保存した後のPC側変換はローカル操作。

```bash
python3 scripts/field.py decode config/field.local.json --raw /path/to/pc/raw
```

EL3/Secure/Normal別txtとdecode-status.jsonをPCへ保存する。run ID不一致・未初期化・RUNNING/overflowは完了PASSにしない。fatalはdebug_exception、最後のSMC/期待abortはdebug_sync_exception。最適化引数ではなく固定ESR/FAR/ELR/SPSR/SP/GPR/vectorを確認する。未到達Worldの古いRAM値を今回の証跡にしない。Secure領域が読めない場合はBLOCKEDとし保護解除しない。

## 4. B2 build→起動準備

```bash
python3 scripts/field.py b2-check config/field.local.json
python3 scripts/field.py b2-build config/field.local.json
python3 scripts/field.py b2-plan config/field.local.json
```

SDK→TA-A/B→Core→B2 BL31→BL33を新規copyで生成する。B2-planはplatform/zcu106/jtag_b2_inputs.jsonを元にし、FSBL/PMUFW原本、同一XSA hash、ELF hash/entry/LOAD、stop/ready値、固定source来歴やZIP未確認状態を確認する。現在はBLOCKED。build出力はrun workspaceに隔離され、元boot planの未確定値を自動確定しない。直前のB2 build receiptがあればhashを再照合し、生成したBL31/BL32/BL33をローカル入力copyへ結び付ける（静的build出力だけの対応）。FSBL/PMUFWや実機確認flagは更新しない。別入力を使う場合はlocal.jsonのb2_inputsへ確認用JSONのpathを指定する。必要な入力copyの作成・artifact照合を先に行う。

将来のreview対象の順序は、確認済み初期化/reset方針→PMUFW load/run＋ready確認→FSBLをXFsbl_Exitで停止→BL32/BL33/BL31 load→BL31入口確認→OP-TEE初期化停止→BL33停止。BL32/BL33を単独runしない。PMU ready/stop addressはimage固有で、推測値を示さない。実行候補は別のgated Tcl。

```tcl
# 現在はBLOCKEDで操作しない:
# tz_field_b2_start /path/to/results/field/RUN/b2-plan.tcl
# resetも未確認policyのため拒否する:
# tz_field_reset
```

PMU→FSBL後のBL31/OP-TEE/BL33段階継続は既存b2_boot.tclの別procedureで停止点ごとにreviewする。詳細は[B2計画](b2_jtag_test_plan.md)。BL33 RAMログとCore/TA traceの回収可能範囲は[一覧](uartless_log_matrix.md)。

## 5. 実行前の不足条件

- FSBL/PMUFWとXSAの対応、runtime stack/heap/予約・寿命。
- DDR実効範囲、system PA/alias、BL31/BL32/BL33/SHMと動的領域の予約。
- XMPU route/master/security/lockと保護非改変。CONFIG0x8やAP read errorだけでは証明しない。
- EL3入口とBL33引渡しEL、MMU/cache/Stage2、UART/GIC、他core、boot modeとreset順。
- PMUFW readiness、IPI待ちの実原因。PC5870/関数名だけで確定しない。
- ZIP来歴未確認、Vitis/GNU/GUI/XSDB実機未検証、OP-TEE/TAのRAM trace未実装。

GPIO/DMAは保留。検査scriptは実機へ触れない。Tclはsourceだけでは操作せず、stop/read/load/runの副作用を名前で区別する。ゲートを通すためにflagを変更しない。QEMU PASSをZCU106 PASSへ転記しない。
