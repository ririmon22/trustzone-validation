# ZCU106 B2 実機確認の準備・証跡取得手順

[前回の整合化](b2_zcu106_alignment.md)と実績（別途生成・用意: `../results/b2-alignment-summary.json`）を生成ELF/logと再照合した。今回firmwareは変更せず、実機load/run/JTAG接続も行わない。B2_RUNTIME_READY=0、load_authorized=false、全安全ゲートBLOCKEDを維持する。GPIO/DMAは保留。原本と実測がない項目はNOT_RUNで、取得不能なら理由を付けBLOCKEDとする。

記入する票は[Markdownチェックリスト](b2_hardware_checklist.md)／[JSON原本](b2_hardware_checklist.json)。23項目の「何を・どこで・どう確認」「期待条件・実測値・証跡・判定」を用意した。JSONのコピーに記入し、原本templateや過去runを上書きしない。

## 収集を3段階に分ける

1. **原本収集・静的review（今回準備可能）**：XSA/SDT/BSP、生成linker、FSBL/PMUFW ELF/map/source、launch.json/init script、固定build optionsを集める。boardへ接続せず実行できる。
2. **既存の確認済み停止状態からの観察（で別途条件確認）**：既存debug接続が本当に停止中で、自動reset/init/download/runがないことを確認してから必要registerだけを取得する。attach自体が状態を変えるtoolもあり、無条件に実施可能とはしない。
3. **実際のhandoff停止点（将来の別工程）**：BL31 entry、BL32 init/return、BL33 entry、EL1移行後、MMU準備前という動的停止点は、起動・実行なしには一般に取得できない。今回の禁止条件の下ではNOT_RUNのままにする。B2を先にload/runして証跡を埋める案内はしない。

## で最初に保存する原本

|分類|何を保存するか|何を確定するか|
|---|---|---|
|hardware/domain|XSA、SDT、Platform生成物、bspconfig.h、lscript.ld、BSP startup/source/version|CPU/64bit/standalone、DDR/OCM、UART/clock、入口EL設定|
|FSBL|元ELF/map/linker、source/config、compiler/link command、停止/引渡しsource|entry/全LOAD/BSS、stack/heap/DDR scratch、引渡し後も生存する領域|
|PMUFW|元ELF/map/linker、MicroBlaze tool/source/config、起動方法|processor viewとsystem physicalの対応、stack/heap/常駐、並行保護設定|
|B2|BL31/32/33 ELF/BIN/map、Core conf.mk、TA/SDKの出所、B2 JSON/header/linker|全LOADのp_memsz、runtime予約、entry、SHM/cache、Early TA接続|
|debug/boot|元launch.json、initialization Tcl、生成PSU init、board revision/boot switch/接続図|reset範囲、FSBL/PMUFW処理、CPU選択、実boot branch、停止点と復旧設計|

すべてfile/hash、source commitと未コミット差分、tool版、取得者、UTC日時を記録する。FSBL/PMUFWのLOAD報告7件だけでruntime予約が全て分かったとしない。heap未使用の場合もlinker/実装の根拠を付ける。PMUFWをA53のaddress viewで無変換に比較しない。aliasが非線形ならsegmentごとの対応表を作り、一律offsetを推測しない。

## ローカルで動作確認済みの取得コマンド

repo rootのbashで使う。readelfは実検出したGNUまたはLLVM版を選ぶ。以下のELF header/program/section/symbol取得は開発hostの生成BL31/32/33で確認済み。のVitis toolとFSBL/PMUFW ELFは未確認。

```bash
bash
export TZ_ROOT="$(pwd -P)"
export TZ_EVIDENCE="$TZ_ROOT/results/company/b2-$(date -u +%Y%m%dT%H%M%SZ)"
mkdir -p "$TZ_EVIDENCE/artifacts" "$TZ_EVIDENCE/snapshots"
read -r -p 'Actual readelf executable (absolute path): ' TZ_READELF
test -x "$TZ_READELF"
"$TZ_READELF" --version > "$TZ_EVIDENCE/readelf-version.txt"
read -r -p 'Original FSBL ELF path: ' TZ_FSBL
read -r -p 'Original PMUFW ELF path: ' TZ_PMUFW
"$TZ_READELF" -h -l -S -s "$TZ_FSBL" > "$TZ_EVIDENCE/artifacts/fsbl-readelf.txt"
"$TZ_READELF" -h -l -S -s "$TZ_PMUFW" > "$TZ_EVIDENCE/artifacts/pmufw-readelf.txt"
python3 scripts/check_boot_plan.py --inventory "$TZ_FSBL" "$TZ_PMUFW" --output "$TZ_EVIDENCE/artifacts/firmware-inventory.json"
```

このinventoryはELF解析だけで、system address alias/lifetime/実機安全を判定しない。元ELF/map/linker等もartifactsへルールに従って保存する。A53用objdumpでPMUFW逆アセンブルが可能とは仮定しない。debug symbolのstack/heap値とruntime未提供範囲を手で照合する。

```bash
python3 - <<'PY'
import hashlib, json, os
from pathlib import Path
root = Path(os.environ['TZ_EVIDENCE'])
records = {}
for path in sorted((root / 'artifacts').rglob('*')):
    if path.is_file():
        records[str(path.relative_to(root))] = hashlib.sha256(path.read_bytes()).hexdigest()
(root / 'artifact-hashes.json').write_text(json.dumps(records, indent=4) + '\n')
PY
```

## Vitis 2025.1での確認箇所（GUI実操作NOT_RUN）

1. 実インストールのsettings64.shを読み込み、版とworkspaceを記録する。Platform Componentのhardware/domain/BSP設定からpsu_cortexa53_0、standalone、64bit、XSA/SDT、stdin/stdoutと生成linkerを確認する。画面から生成fileへ辿り、実path/hashを保存する。
2. FSBL/PMUFW componentのBuild consoleと生成ELF/mapを保存する。FSBLのstack/heap/scratch、PMUFW常駐領域はgenerated linkerとsourceから確認し、実装上の生存期間を表へ記入する。
3. Launch ConfigurationのLocal target、FSBL指定、Reset Entire System、Reset APU/RPU、初期化/自動download/自動run/stop-at-mainを元launch.jsonと照合する。設定確認だけを行いLaunch/Debugを押さない。FSBLのチェックがPMUFW起動を保証するとはしない。
4. 既に承認済みの停止sessionを観察できる場合だけ、Debug viewで対象CPUを選び、`View > Register Inspector`またはRegister Inspectorボタンを開く（UG1400 2025.1）。編集可能な値は変更しない。APU groupとA53 coreでは表示されるsystem/IP/core register群が異なる。
5. register名、raw値、bit decode、CPU/core/MPIDR、PC、停止点、EL/security/banked viewを保存する。export機能の2025.1実操作は未確認のため、有無を現地確認し、無ければ手動保存・画面記録を使う。2024.2のexport手順を2025.1で実行確認済みとはしない。
6. memory viewやDAP経由のreadはdebugger master/security/address-spaceで行われる。JTAGからSecure RAMを読めた事実をNormal CPUからの許可アクセスとして扱わない。MMIOの副作用を確認するまでbulk readしない。

## XSDBコマンド候補（公式構文照合済み・実行NOT_RUN）

このhostにvitis/xsdb/hw_server/bootgenはない。次はの実toolでまずhelpを確認し、既存停止targetへの観察が承認済みの場合だけ使う。ここには接続・reset・load・runやregister書込みを行うscriptを含めない。

```tcl
help targets
help rrd
help mrd
targets
```

UG1725 2025.1のtargets/rrd/mrdを根拠とする。target IDを一覧から選び、A53 coreとAPU groupを混同しない。以下はXSDB対話画面で1行ずつ実行する候補。Tclのread promptsも含め実XSDBでの動作はNOT_RUN。

```tcl
puts "Verified halted target ID:"
set target_id [gets stdin]
targets $target_id
rrd -defs
puts "Exact register/group name from definitions:"
set register_name [gets stdin]
rrd {*}[split $register_name]
```

`CurrentEL`等の実際の綴り/groupはtargetごとのrrd -defs/Register Inspectorから取得し、架空の名前を固定しない。EL1からEL2/EL3のregisterを読めると仮定しない。読めなければ上位stageの別snapshotが必要で、今回取得不能ならBLOCKED/NOT_RUN。

Register InspectorのIP群の全展開・自動refresh・全register exportがIAR/FIFO等を含む場合も使用しない。確認済みの個別registerだけを対象にする。MMIO readはmodule addressとread副作用とdebug権限のreview後だけ。-force、raw bulk dump、FIFO読出し、W1C clear、GIC IAR読出し、保護書込みを使わない。

```tcl
puts "Reviewed aligned register address (not FIFO/IAR/read-clear):"
set register_address [gets stdin]
mrd -size w $register_address 1
```

この例は実アドレスを確定するものではない。mrdのaddress spaceはtargetで変わるため、APU/DAP/processor viewと実効PA対応を先に記録する。

## 停止点ごとのregisterと期待条件

|停止点/観察段階|記録するもの|期待・判定の注意|
|---|---|---|
|FSBL→BL31直前/BL31 entry|PC、CurrentEL、SCR_EL3、SCTLR_EL3、TTBR0_EL3/TCR/MAIR、SP/SPSel、DAIF、VBAR、x0-x3|AArch64 EL3 Secure。entry/args/cache初期化の契約を固定TF-A RESET_TO_BL31経路へ照合。未実行で値を作らない|
|BL31→BL32、opteed init復帰|entry_point_infoのPC/SPSR/args/security、ELR/SPSR_EL3、SCR、Secure UART|BL32 entry=0x60000000はsoftware案。初期化→EL3復帰→Normalの順を確認する将来停止点|
|BL33 _start|PC、CurrentEL、SCR/SPSR/ELR（上位EL側）、SP/SPSel、DAIF|PC=0x10200000。固定JTAG defaultはNS EL2H。直接EL1なら別handoff根拠が必要|
|BL33 from_el2/start_el1|HCR_EL2、VTTBR_EL2、VTCR_EL2、CNTHCTL_EL2、CNTVOFF_EL2、SCTLR_EL1、SP_EL1、SPSR/ELR_EL2|既存trampolineはRW=1/VM=0、timer accessを設定してEL1Hへ。hardwareで実際に通ったことはNOT_RUN|
|BL33 MMU準備直前|SCTLR_EL1 M/C/I、TTBR/TCR/MAIR、VBAR、SP、page-table symbol/範囲、cache引渡し記録|EL1、M/C/I=0、dirty D-cacheが残らない契約。ゼロbitだけでdirty無しとしない。runtime_ready=0なのでinstallしない|
|将来のMMU設定後|TTBR/TCR/MAIR、実table、NS/Secure SHM PA/cache属性|現在は取得しない。SHM Normal WB/inner-shareable、VA=PA、vector独立RX/data XNの実態は静的記述と別|

固定TF-A bl31_entrypoint.SはRESET_TO_BL31時にel3_entrypoint_common(_init_sctlr=1)を使い、el3_common_macros.SはSCTLR初期化やC runtimeを行う。この実装をJTAGで任意の稼働状態から呼んで安全とはしない。Aの自作EL3 entry guardの条件を、そのままTF-A BL31の確認済み契約として転記しない。cache dirty/初期化済みRAM/他coreを含むhandoff reviewが必要。

## XMPU・UART・GICの取得対象

- **XMPU**：0xfd000000というbaseの一致だけでは足りない。全関連DDR bankのCTRL/POISON/LOCK、16 regionsのSTART/END/MASTER/CONFIG、statusとdefault policy/priority、APU S/NS master ID/mask、保護経路を確認する。LOCK offsetはUG1087の対象moduleで再確認し、この資料の列挙から計算して書き込まない。initがreturn 1だけでも、別stageの設定やlockの有無は分からない。XPU違反のW1C clearは本確認では実施しない。
- **UART**：BSP stdin/stdout、TF-A console、Core CFG_UART、BL33 uart.c、clock/MIO/baud/routeを同じphysical channelに対応づける。候補0xff000000の1page mappingはclock/配線確認ではない。UART FIFOは受信データを消費するので観察目的で読まない。
- **GIC**：GICD_CTLR/TYPER/IIDR/IGROUPR/ISENABLER/IPRIORITY/ITARGET、GICC_CTLR/PMR/BPR、SCR_IRQ/FIQ/DAIF、他A53の状態をCPU/banked view付きで保存する。GICC_IARはreadで割込みをacknowledgeするため読まない。EOIR、SGI発生、pending clearも実行しない。
- **他master**：GPIO/DMA機能試験は保留でも、既存OS/PMUFW/PL/DMAが対象memoryへ並行accessしない条件はreviewする。保留を安全条件の削除理由にしない。

## 記録・検査コマンド

```bash
cp docs/b2_hardware_checklist.json "$TZ_EVIDENCE/checklist.json"
```

JSONの各itemにmeasured object、evidence[{path,sha256,kind}]、reviewer/UTC、contextを入力する。kindはartifact/live、mixed項目は両方の証跡が必要。pathはTZ_EVIDENCEからの相対path。コピーしたtemplateは全23項目NOT_RUN。

```bash
python3 -S scripts/check_b2_hardware_checklist.py "$TZ_EVIDENCE/checklist.json" --evidence-root "$TZ_EVIDENCE" --output "$TZ_EVIDENCE/checklist-validation.json"
python3 -S scripts/check_b2_hardware_checklist.py "$TZ_EVIDENCE/checklist.json" --evidence-root "$TZ_EVIDENCE" --require-record-complete --output "$TZ_EVIDENCE/checklist-completeness.json"
```

前者のPASSはJSON/必要欄/hashの検査だけ。後者は未記入があればexit2。全欄を埋めてもhardware_load_statusはBLOCKEDで、boot planやruntime flagを自動更新しない。self-declared liveラベルやhashは測定の真正性を認証しないため、人の内容・出所reviewを省略できない。

## loadを検討するために必要な条件

元firmware/生成hardware、system PA対応、全runtime予約、実protection policy/lock/master、入口EL/cache/stage2、clock/GIC、実boot branch/初期化/停止復旧順が同じimage hash/runで整合している必要がある。software静的PASSやNOT_RUN票だけでは足りない。動的停止点がまだ無ければ未確認として別途の段階的評価計画へ進める必要があり、証跡取得を理由に現在のB2を先にrunしない。

既存check_hardware_profile.pyにはDMA/GIC統合・例外/違反帰属の停止条件も残る。今回の票やcheckerはそれを代替・解除しない。証跡とreviewを揃えた後に、構成B2に適用するload条件を正式に実装・reviewする別工程が必要。現在はその検討資料までを準備し、0/false/BLOCKEDを維持する。

## 確認範囲と参照

- [UG1400 2025.1 Viewing Registers](https://docs.amd.com/r/2025.1-English/ug1400-vitis-embedded/Viewing-Registers)：Register Inspectorの一般操作。GUI実操作はNOT_RUN。
- [UG1725 2025.1 targets](https://docs.amd.com/r/2025.1-English/ug1725-xsdb-reference-guide/targets)、[rrd](https://docs.amd.com/r/2025.1-English/ug1725-xsdb-reference-guide/rrd)、[mrd](https://docs.amd.com/r/2025.1-English/ug1725-xsdb-reference-guide/mrd)：構文とtarget/address-space条件。実XSDBは未導入でNOT_RUN。
- [UG1087 LOCK XMPU_DDR](https://docs.amd.com/r/en-US/ug1087-zynq-ultrascale-registers/LOCK-XMPU_DDR-Register)、UG1085第10/16章、[backend根拠](zcu106_backends.md)：address/protection/read副作用の照合。実policyは未確認。
- [TF-A 2.12 §7.45.1/7.45.5](https://trustedfirmware-a.readthedocs.io/en/v2.12/plat/xilinx-zynqmp.html)と固定source：build options/handoff。実JTAG branchの選択を推測しない。

今回の再検査・tool有無・ELF hash・host試験・記録validator結果は準備結果（別途生成・用意: `../results/b2-hardware-prep-summary.json`）。firmware/source build条件は変更せず、QEMU再起動や実機実行は行わない。

## 今回のローカル確認結果

- 既存BL31/BL32/BL33の3 ELFは前回hashと一致。software契約の再監査PASS、実機load条件はBLOCKED。
- firmware関連13ファイルはbuild時workspaceのsourceとhash一致。firmwareを再buildする変更は行っていない。
- host157試験PASS（既存146件を含む）。新規試験はチェック票の欠落/重複、静的証跡によるlive代用、hash改変、review不足、path逸脱、空の実測値、runtime flag変更を検出する。
- Bash/Python snippet構文、Tcl括弧の完全性だけをローカル検査。XSDB自体やregister名/GUIの実操作はNOT_RUN。
- チェック票は23項目すべてNOT_RUNでPASSなし。schema検査のPASSは実機PASSではない。
- 実機load/run、JTAG接続、QEMU新規実行、commit/pushは実施せず、既存成果物/logを保持した。
