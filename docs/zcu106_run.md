# 現行Make生成物の点検・JTAG準備・結果回収

[入口](../README.md) · [基本build](build.md) · [追加構成](profiles.md)

全コマンドはリポジトリ直下で実行する。JSONは実機情報・検査結果用で、ELF生成の設定ではない。
実機・実XSDBは未実行。`B2_RUNTIME_READY=0` / `B2_PHYS_REVIEWED=0` を維持する。
静的検査PASSや回収計画の生成は、ロード・実行許可を意味しない。

## 何を実行できるか

| ツール | 新Makeとの接続 | 今回の確認 |
|---|---|---|
| check_signed_layout.py | `--elf` で署名BL33を指定 | PASS、実機BLOCKED |
| check_b2_physical_reservation.py | `--elf` を複数指定。BL33/Coreの予約衝突を点検 | BL33だけPASS。Coreは未実行 |
| check_b2_review_profile.py | `--elf` / `--core` / `--conf` を明示。旧Core固定パスを廃止 | 引数経路のみ確認。実Core検査は未実行 |
| check_signed_profile.py | `--core` / `--conf` / `--bundle` / `--keys` / `--optee` を明示 | 引数経路のみ確認。実署名TA/Core検査は未実行 |
| check_boot_plan.py | 既定はMake基本構成。`--bl31` / `--bl32` / `--bl33` で選択 | 入力点検はBLOCKED、BL33 inventory生成は成功 |
| prepare_b2_jtag.py | 同じELF引数と `--inputs` / `--boot-plan` を指定 | 点検・Tcl定義生成のみ。常に実行許可0、終了値2 |
| prepare_jtag_capture.py | 位置引数で実際のBL33を指定。symbol/LOADから採取範囲を決める | 3構成で生成成功。実採取は未実行 |
| prepare_signed_capture.py | `--elf` / `--core` を明示。監査CoreとBL33を対応付ける | Core不足のため未実行 |
| inventory_b2_inputs.py | `--input` の実機使用ELF/XSAを採取。Make生成物との同一性は自己申告しない | 合成入力でBL33の採取を確認 |
| prepare_xmpu_test.py、jtag/*.tcl | 実機証跡・限定操作用。旧build専用ではないため現行に残す | 未確認入力の拒否・Tclの実行ゲートのみ確認。実機未実行 |
| decode_*.py、analyze_benchmarks.py | dumpとELFを明示するPC解析。buildを呼ばない | import/CLI確認。今回新たな実dumpなし |
| legacy/scripts/check_b2_integration.py | 旧ログ・旧配置結合の統合検査 | **現行で使用不可・参照専用**。BL31/BL32/TAを含む代替統合確認も未対応 |

## まずPC上でBL33だけを点検する

```sh
python3 scripts/check_signed_layout.py --platform zcu106 \
  --elf build/zcu106/signed/optee_bl33.elf --output results/signed-layout.json
python3 scripts/check_b2_physical_reservation.py \
  --elf build/zcu106/physical/optee_bl33.elf --output results/physical-reservation.json
python3 scripts/check_boot_plan.py --inventory build/zcu106/optee_bl33.elf \
  --output results/bl33-inventory.json
python3 scripts/prepare_jtag_capture.py build/zcu106/optee_bl33.elf \
  --output results/capture-basic.tcl
python3 scripts/prepare_jtag_capture.py build/zcu106/signed/optee_bl33.elf \
  --output results/capture-signed.tcl
python3 scripts/prepare_jtag_capture.py build/zcu106/physical/optee_bl33.elf \
  --output results/capture-physical.tcl
```

Coreが作れたら、物理予約検査へ `--elf build/components/physical/optee/core/tee.elf` も加える。
profile整合・署名の検査は [追加構成](profiles.md) のコマンドを使う（今回未実行）。

## 起動入力の点検：現状は実行不可

1. `config/b2_hardware_capture.example.json` をGit管理外の `results/hardware.local.json` へコピーし、
   実際に使用するELF/XSA/ソースの位置を記入する。相対パスはこの入力ファイルの親を基点にする。
   `python3 scripts/inventory_b2_inputs.py --input results/hardware.local.json --output results/hardware-inputs.json`
   はPC上でhash/entry/LOADを読むだけで、実機の使用版を認証しない。
2. `platform/zcu106/jtag_b2_inputs.json` と `platform/zcu106/boot_plan.json` を
   `results/` 以下のlocalファイルへコピーし、実機証跡・hash・entryを記入する。
   この2つのJSON内の相対ELF/証跡パスは**リポジトリ直下基点**。上の採取入力と基点が異なる。
   初期例のELFは `build/components/basic/` と `build/zcu106/` に一致する。
3. 使用する構成のELFを明示して点検する。次は署名構成の例（外部ELF不足のため実検証は未実行）。

```sh
python3 scripts/prepare_b2_jtag.py --inputs results/jtag.local.json \
  --boot-plan results/boot.local.json \
  --bl31 build/components/signed/tfa/zynqmp/release/bl31/bl31.elf \
  --bl32 build/components/signed/optee/core/tee.elf \
  --bl33 build/zcu106/signed/optee_bl33.elf \
  --output results/boot-inspection.json --emit-tcl results/boot-inspection.tcl
```

ファイル引数は選択パスだけを置き換える。hash/entry/実機証跡は確認済みにせず、入力値と照合する。
入力とboot planが別のELFを指す場合も拒否する。生成Tclは `tz_boot_authorized 0` の定義のみ。
このツールは常に終了値2のBLOCKEDであり、実機起動許可を生成する経路は**未対応**。
`check_boot_plan.py --plan ... --bl31 ... --bl32 ... --bl33 ... --output ...` でも同じパス指定ができるが、
必要な実機証跡なしではBLOCKED。gateを編集して許可を作らない。

`scripts/jtag/b2_boot.tcl` はsourceだけでは操作しない。`tz_b2_execute` は許可時に
PMUFW ready待ち→FSBLの停止→BL32/BL33/BL31配置→BL31入口停止を行う候補で、
download・実行・breakpoint・timeout時停止の副作用がある。現行の未許可計画では使用不可。
reset・boot mode・初期EL/Security State・MMU/cache・Stage2も別レビューが必要。

## 停止済みRAMの回収（実操作は未実行）

通常は上記の `prepare_jtag_capture.py`、監査Coreも照合する署名構成は次を使う。

```sh
python3 scripts/prepare_signed_capture.py --elf build/zcu106/signed/optee_bl33.elf \
  --core build/components/signed/optee/core/tee.elf --output results/capture-audit.tcl
```

Coreの内部traceを任意PAから読めると仮定せず、監査PTAがNormalへ返すsnapshotを採取する。
Coreの隣の `build/components/signed/ta-a/` / `ta-b/` からEarly TA入力hashも記録する。
生成物が同居しているだけでは実機同一性の証明ではない。

```tcl
# 以下は実XSDB未検証。停止状態・一意の対象・PA経路のレビュー後だけ使う候補。
source scripts/jtag/capture.tcl
source results/capture-basic.tcl
# tz_dump {name =~ "*Cortex-A53*#0"} $tz_capture_plan /実際の新しい保存先/raw
```

`tz_stop_for_capture` はCPU停止を伴う。回収は読出しだけと区別し、対象0件/複数件を許可しない。
各runは新しい保存先と固有run IDで管理する。PCでのdecode例（dumpは今回未採取）:

```sh
python3 scripts/decode_b2_results.py results/run/raw/b2_results.bin \
  --ram-log results/run/raw/debug_ram_log.bin --elf build/zcu106/signed/optee_bl33.elf \
  --core-elf build/components/signed/optee/core/tee.elf --capture-plan results/capture-audit.json \
  --run-id 0x20261102 --output results/run/report.txt
```

単独の例外は `decode_exception.py`、RAMログは `decode_ram_log.py`。
欠落・古いrun・形式/hash不一致・未完了・後始末不備を成功扱いしない。
完了publishはPoCへcleanするが、JTAG APのPA/alias・権限・coherencyは実機確認が必要。
性能測定中のbreakpointは値を無効にし、閾値なしの時間はMEASUREDで速度PASSにしない。

## XMPU採取・物理試験

`config/xmpu_test.example.json`、`scripts/prepare_xmpu_test.py`、
`scripts/jtag/xmpu_readonly.tcl` と `xmpu_test.tcl` は現行の限定実機操作用として残す。
未知のmaster/lockやCTRL全体へ推測で書かない。
[物理試験の仕様と前提](b2_test_specification.md) に従う。実XSDB/MMIOは未実行。
過去のA53読出しfault、PSUでのXMPU読出し等のUSER_REPORTEDは
[21項目の旧版観測](b2_scope_21.md) と区別し、今回の確認済みへ転記しない。
