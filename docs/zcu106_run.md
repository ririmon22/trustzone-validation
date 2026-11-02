# 実機確認の入口：点検・ロード・停止・回収

[入口](../README.md) · [PC検証と残項目](pc_validation.md) · [署名／物理](profiles.md)

実XSDB・MMIO・実機試験は未実行。PCでの生成物検査はPASSだが、起動計画はBLOCKED、
`B2_RUNTIME_READY=0` / `B2_PHYS_REVIEWED=0`、生成Tclの実行許可0を維持する。
この文書の実機操作は、下記入力と証跡を確認して別途承認した後の段階別手順である。

## このビルドの起動方式と必要入力

TF-Aは `RESET_TO_BL31=1 SPD=opteed`。BL2/FIPやLinuxを経由せず、XSDBでELFを配置する方式を対象にする。
取得したTF-Aの `plat/xilinx/zynqmp/bl31_zynqmp_setup.c` を読むと、**JTAG boot modeではbuild時の既定入口**を使い、
それ以外ではFSBL(XBL) handoffを使って失敗時にpanicする。非JTAGでも既定入口へfallbackすると仮定しない。
このPCで確認した既定入口はBL32=0x60000000、BL33=0x10200000・Normal EL2hで、
`boot/bl33_start.S` がEL1hへ移る。実際のboot modeとSCR/SPSR/ELは実機で照合する。

| 入力／条件 | 必要性と確認方法 |
|---|---|
| 同じXSA/BSP由来のFSBL | DDR・MIO・clock等のPS初期化が必要。出所・hash・OCM/DDRの全runtime予約を照合する。旧成功版のLOAD断片だけで置換しない |
| 対応するPMUFW | TF-A/OP-TEEのZynqMP power managementとの整合が必要。firmware固有のready address/mask/期待値・根拠を確認する。推測値で起動しない |
| bitstream | CコードにPL peripheral/DMAの試験backendはない。これだけでbitstreamを必須とはしないが、対象XSA・FSBL・clock/reset設計がPLに依存するか不明なので、実構成で要否を決めて記録する |
| DTB／pageable image | 今回のOP-TEE実confはCFG_DT=n、CFG_WITH_PAGER=n。今回のELFロードで別DTB/pageable imageは要求しない。設定を変えた場合はこの手順の対応外 |
| JTAG/XSDB接続 | 実際のAPU/PMUのfilterが一意であること、debug権限、boot mode、reset/初期化、system PA/alias経路を確認する。固定target番号を使わない |
| 初期状態 | 他A53/masterの停止、EL3/S-EL1/NS-EL1、Stage2、MMU/cacheとdirty dataの扱いをcold bootから確認する。状態不明のままSCTLR等を書き換えない |
| 予約と保護 | FSBL/PMUFWのstack/heap/動的領域、DDR実効範囲、共有RAM・bundle・物理窓、XMPU/master/lock/poisonをレビューする |

XSA/FSBL/PMUFW/bitstreamの実物・VitisはこのPCにないため、要否・起動を成功扱いしていない。
Vitis 2025.1を用意し、対象XSAに合ったFSBL/PMUFWをソースから作り、元source/BSP/XSAのhashを記録する。
その作業にはボード設計入力が必要であり、汎用の空BSPや別ボードのELFで補わない。

## PC上で選ぶ生成物と点検

例は署名構成＋GCC。BL31/BL32/TA/bundleは同じprofileから選ぶ。
他compilerのBL33を選ぶ場合はELF・コンパイルログ・回収計画・hashを全て揃える。
現在のbuildはrun ID=1。実機runでは未使用の非ゼロIDでBL33を作り直し、下記検査とhashを更新する。

```sh
# 例: 次の実機run用。既存の検証記録とはhashが変わる。
make PROFILE=signed TOOLCHAIN=gcc DEBUG_RUN_ID=0x20261102 bl33 > results/signed-run-bl33.log 2>&1
build/python/bin/python scripts/check_components.py --profile signed \
  --bl31 build/components/signed/tfa/zynqmp/release/bl31/bl31.elf \
  --bl32 build/components/signed/optee/core/tee.elf --bl33 build/bl33/gcc/signed/optee_bl33.elf \
  --conf build/components/signed/optee/conf.mk \
  --ta-a build/components/signed/ta-a/6dc13418-534f-4642-9138-215876304910.stripped.elf \
  --ta-b build/components/signed/ta-b/6dc13419-534f-4642-9138-215876304910.stripped.elf \
  --bl31-log results/pc-preflight/run-a/signed-bl31.log --bl33-log results/signed-run-bl33.log \
  --output results/signed-run-components.json
build/python/bin/python scripts/check_signed_profile.py --platform zynqmp \
  --core build/components/signed/optee/core/tee.elf --conf build/components/signed/optee/conf.mk \
  --bundle build/components/signed/bundle --keys build/signed-test-keys \
  --optee third_party/optee --output results/signed-run-profile.json
```

このrun ID変更例は操作用の例で、今回のhash記録はID=1の両回の生成物に対するもの。
ID変更自体の実機動作は未実行。buildの既定選択は変えていない。

実機使用入力の採取には `config/b2_hardware_capture.example.json` を新しい `results/hardware.local.json` へコピーし、
実際のELF/XSA/source等のパスを記入する。ここでの相対パスは**入力JSONの親基点**。

```sh
build/python/bin/python scripts/inventory_b2_inputs.py \
  --input results/hardware.local.json --output results/hardware-inputs.json
```

これはPC上でファイルを読むだけで、実機使用版の認証ではない。
次に `platform/zcu106/jtag_b2_inputs.json` と `boot_plan.json` を `results/` 以下のlocalファイルへコピーし、
hash/entry、XSA結合、review証跡、停止位置を記入する。この2種のJSONの相対パスは**リポジトリ直下基点**。
source原本の固定commitとclean状態を確認したログを証跡にし、legacyのzip_reportを流用しない。

```sh
build/python/bin/python scripts/prepare_b2_jtag.py --inputs results/jtag.local.json \
  --boot-plan results/boot.local.json \
  --bl31 build/components/signed/tfa/zynqmp/release/bl31/bl31.elf \
  --bl32 build/components/signed/optee/core/tee.elf \
  --bl33 build/bl33/gcc/signed/optee_bl33.elf --bundle-dir build/components/signed/bundle \
  --output results/boot-inspection.json --emit-tcl results/boot-inspection.tcl
```

同じ明示パスで今回6組×2回の点検・定義生成を確認した。未確認入力があるため終了値2が正しい。
パス指定だけでhash/entryや証跡を確認済みにしない。入力/planの別ELF、不正bundleも拒否する。
署名構成では `--bundle-dir` が必要。bundleのslot・TA・digest・manifestを照合し、raw fileのパスをTclへ渡す。
`check_boot_plan.py` の `--inventory` はELFだけの情報採取、`--plan` は証跡込みの点検であり実行許可ではない。

## 停止位置を決める

| 段階 | 指定する位置と意味 |
|---|---|
| FSBL Exit | 実FSBLの終了・引渡し直前をmap/sourceで決める。今回のアドレスは未知。ここに達しただけでDDR・handoff全条件が成立したとしない |
| BL31入口 | 対象BL31 ELFのentry（今回0x1000）。全image配置後、最初のEL3命令を実行する前にEL/SCR/MPIDR/stack/cache状態を記録 |
| OP-TEE初期化 | 実tee.elfの `boot_init_primary_final` の入口等をsource/mapで選ぶ。関数入口ではfinalcallsはまだ未完了なので「初期化完了」と誤記しない |
| BL33 main | 選んだBL33 ELFの `standalone_main`。この時点でEL1、EL2引渡し記録、MMU/cache初期状態、run IDを確認し、試験開始前のゲートをレビュー |

```sh
# 実ELFに存在する候補symbolをPCで確認できる。
build/python/bin/python - <<'PY'
import sys
from pathlib import Path
sys.path.insert(0, 'scripts')
from elf_symbols import elf_symbols
for path, name in [('build/components/signed/optee/core/tee.elf', 'boot_init_primary_final'),
                   ('build/bl33/gcc/signed/optee_bl33.elf', 'standalone_main')]:
    print(path, name, hex(elf_symbols(Path(path))[2][name]['address']))
PY
```

アドレスが得られることと、そのPA/VA経路で実XSDB breakpointが使えることは別。後者は実機で確認する。

## 承認後の段階別ロード候補（実XSDB未実行）

`scripts/jtag/b2_boot.tcl` と生成inspection Tclはsourceだけでは操作しない。
現在のinspectionは実行許可0で、次の関数は全て拒否する。この記録だけを根拠に許可1へ変えない。

```tcl
source scripts/jtag/b2_boot.tcl
source results/boot-inspection.tcl
# 証跡と別途承認したreviewed planが揃った後にのみ:
# tz_b2_execute
#   PMU targetを一意確認 → PMUFW dow/con → firmware固有ready待ち
#   APU targetを一意確認 → FSBL dow/con → FSBL Exitで停止
#   BL32 ELF → BL33 ELF → 署名ならbundleを0x10600000へdow -data → BL31 ELF
#   BL31 entryを検査して停止。自動継続しない。
# tz_b2_to_optee
#   BL31から上表のCore停止位置まで。レジスタと初期化状態を再確認。
# tz_b2_to_bl33
#   BL33 main停止まで。まだ試験成功ではない。
```

CPU停止・download・breakpoint・実行は副作用がある。resetやbitstream書込みはこのTclに含めていない。
他のboot modeで手順を流用せず、XBL handoffを別途確認する。APU停止待ちのtimeoutではstopし、追加したbreakpointは不一致・timeout時にも除去する。
PMU readyのtimeoutはFSBLを開始せず打ち切るがPMUを自動停止しないため、停止・復旧はfirmwareに対応したレビュー済み手順で行う。
一意target、bundle download順序、timeoutの引数経路は [Tcl mock](../tests/host/jtag_mock.tcl) で検査した。
構文の根拠はAMD [UG1725 dow](https://docs.amd.com/r/2024.2-English/ug1725-xsdb-reference-guide/dow?contentId=Weac07A1d2_rm0_~QevHsw) と
[UG1208 targets](https://docs.amd.com/api/khub/documents/V9ibETBbaN2~qlUquLPsTA/content)。実際に使うVitis/XSDB版でhelpと動作を再確認する。

## 実機で根拠が揃った後に何を変更するか

1. **初回はgate=0の生成物で起動前状態・各停止位置・実メモリ経路を確認**する。
   FSBL/PMUFW全runtime予約、全master/alias、実効DDR、EL/Security State/MMU/cache、共有WB属性、debug/clock/GIC等を
   実レジスタ・同じhashのfirmware/sourceと結び付けた証跡でレビューする。
2. 実行を許可する段階では、別のレビュー済み変更で `platform/zcu106/b2/memory_map.h` のB2_RUNTIME_READYを変更する。
   対応する実機情報JSONの確認状態と証跡も同時に更新し、検査の「未確認版は0固定」という期待値を
   証跡付きreview済み版の方針へ合わせて再検査する。このbranchの未確認版用checkerは1を意図的に拒否する。
   ヘッダだけの変更、デバッガでのconst書換え、生成Tclの0だけの書換えで迂回しない。
3. 物理試験はさらにXMPU経路/master/lock/poison/priority・他master停止・専用1MiB予約・回復手順をレビューしてから
   B2_PHYS_REVIEWEDとその証跡を変更する。初回の通常試験許可と物理試験許可をまとめない。
   `prepare_xmpu_test.py` / `jtag/xmpu_test.tcl` は限定領域だけの操作候補で、CTRL全体や未知offsetを推測で書かない。
4. 変更後は固有run IDでBL33を再buildし、全hash・配置/署名/契約検査・停止/回収計画を作り直す。
   reviewed planは承認者・対象hash・証跡を結合して管理する。自動の許可昇格機能は設けていない。

## RAM回収と合否判定

署名構成は監査Coreも照合した計画を使う。他は `prepare_jtag_capture.py ELF --output ...`。

```sh
build/python/bin/python scripts/prepare_signed_capture.py \
  --elf build/bl33/gcc/signed/optee_bl33.elf --core build/components/signed/optee/core/tee.elf \
  --output results/capture-audit.tcl
```

PCで両compilerの定義生成とCore/TA input hashの記録まで確認した。
正常完了後の停止状態から、対象とPA/alias経路をレビューしたうえで次の読出しを行う候補とする。

```tcl
source scripts/jtag/capture.tcl
source results/capture-audit.tcl
# 一意のAPU filterを実環境から指定する。番号や名前を推測しない。
# tz_dump $tz_apu_filter $tz_capture_plan /実際の新しい保存先/raw
```

`tz_dump` は停止を自動実行せず、停止済みでなければ拒否する。`tz_stop_for_capture` は明示的にCPUを停止する別操作。
回収範囲はELF symbol/LOADから決め、任意のCore PAを直接読めると仮定せずNormal側snapshotを使う。
publishのPoC cleanはコードにあるが、JTAG APのcoherencyやPA経路は実機で確認する。

```sh
build/python/bin/python scripts/decode_b2_results.py results/run/raw/b2_results.bin \
  --ram-log results/run/raw/debug_ram_log.bin --elf build/bl33/gcc/signed/optee_bl33.elf \
  --core-elf build/components/signed/optee/core/tee.elf --capture-plan results/capture-audit.json \
  --run-id 0x20261102 --output results/run/report.txt
```

- collection=COMPLETEと試験PASSは別。古いID/hash不一致・欠落・途中停止・後始末不備は成功扱いしない。
- default基本selection14は従来ルートなので、新しいb2_results表を使わず `decode_ram_log.py --expected-run-id ...` でログを読む。
  新しい子試験表はselection0、署名review表は17、物理は15である。試験選択の意味を混ぜない。
- 署名8ケースは実ローダのstage監査・返答・継続・Close/live=0を含めて判定する。PC署名検証の結果を転記しない。
- 物理は期待VA→PA、限定fault PC/ESR/FAR、Secure pattern非改変、後始末、XMPU証跡を揃える。
  Stage1 permission fault、予約成功、poison、無応答をDDR保護成立のPASSにしない。
- 性能は測定区間・timer Hz・cache状態・overhead・raw sampleを保存する。測定中のbreakpointは値を無効にする。
  閾値なしはMEASUREDで、速度合格PASSとはしない。

正式範囲と各試験の期待値は [21項目](b2_scope_21.md) と [試験仕様](b2_test_specification.md) を参照。
