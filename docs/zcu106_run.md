# ZCU106/B2 起動前確認と結果回収

## 未実施と前提

実機操作・実XSDBはNOT_RUN。必要な証跡がない間はBLOCKED。配布sourceのgateを編集して実行許可を作らない。起動順序はPMUFW→FSBLの初期化→BL32/BL33/BL31配置→BL31開始という計画を、実生成物と実機状態でレビューする。実機初期EL・Security State・MMU/cache・Stage2を推測しない。

## 入力と計画生成

1. 既存成功版を別保存する。`config/b2_hardware_capture.example.json`へ使用ELF/XSA/source等の位置を記入したlocal入力を用意し、hash/entry/LOADを採取する。
   ```bash
   python3 scripts/inventory_b2_inputs.py --input config/hardware.local.json --output results/hardware-inputs.json
   ```
   これはPC上の入力読出しだけ。実機使用版との同一性や来歴を自動認証しない。
2. `platform/zcu106/jtag_b2_inputs.json`を元にlocal記入ファイルを作る。XSA/FSBL/PMUFW対応、各ELFのhash/entry、runtime stack/heap/予約、DDR実効範囲/PA alias、handoff、XMPUの経路/master/lock/poison、GIC、debug条件を確認する。
3. configのb2_inputsへ記入ファイルを指定し、計画を生成する。
   ```bash
   python3 scripts/field.py b2-plan config/review.local.json
   ```
   既定の結果はBLOCKED。`check_boot_plan.py`はB2専用の予約/headerとruntime範囲、MMIO catalog、ELF・証跡、APU/XMPU条件を読む。A用map/リンカへの依存を外し、B2の未確認予約・実行禁止は維持する。
4. `scripts/jtag/b2_boot.tcl`はsourceだけでは操作しない。`tz_b2_execute`にはdownload、run、breakpoint、timeout時停止の副作用がある。未許可計画では最初に拒否する。reset・boot mode・初期化は別レビューが必要。今回は実行しない。

## 読出し専用のXMPU採取

生成初期化関数return 1、A53 mrdでtranslation fault level2、PSUで読出し可能、DDR XMPU 6基CTRL=0xB、96 region CONFIG=0x8/Enable=0はUSER_REPORTED。全経路が無保護とは断定しない。未知のlock offsetやmaster IDを書き込まない。

```tcl
# 実XSDB未検証。review済みの読出し経路だけを選ぶ。
# targets -set -filter {name == "PSU"}
# source scripts/jtag/xmpu_readonly.tcl
# tz_ddr_xmpu_readonly /path/to/new-xmpu.txt REVIEWED_PSU_DDR_XMPU_READONLY
```

読出しerrorは部分記録を残して打切る。専用領域だけのXMPU設定・読戻しは[物理試験手順](b2_test_specification.md)の明示操作に限定する。未確認入力ではBLOCKED。専用候補1MiBは既存予約との静的衝突検査だけであり、物理保護成立ではない。

## 停止済みRAMをPCへ保存

1. 実際に使用するBL33/Coreで回収計画を作る。
   ```bash
   python3 scripts/prepare_signed_capture.py --elf /path/to/optee_bl33.elf --core /path/to/tee.elf --output results/capture.tcl
   ```
   ELFのsymbol/LOADから位置・サイズを決め、ELF/Core/TA入力のhashを対応付ける。Core内の詳細traceを任意PAから読めると仮定せず、既存PTAがNormalへ返す監査metadataだけを扱う。
2. 正常完了後、許可された停止状態から採取する。
   ```tcl
   source scripts/jtag/capture.tcl
   source /path/to/results/capture.tcl
   # mrdを行う読出し操作。停止/対象/PA経路のreview後だけ実行:
   # tz_dump {name =~ "*Cortex-A53*#0"} $tz_capture_plan /path/to/new-run/raw
   ```
   明示停止の`tz_stop_for_capture`はCPU停止を伴うため、読出しだけと区別する。対象0件/複数件を許可しない。最適化されたC引数表示に依存しない。
3. PCでtxt/JSON/CSVへ変換する。
   ```bash
   python3 scripts/decode_b2_results.py /path/to/new-run/raw/b2_results.bin --ram-log /path/to/new-run/raw/debug_ram_log.bin --elf /path/to/optee_bl33.elf --core-elf /path/to/tee.elf --capture-plan results/capture.json --run-id 0x20261040 --output results/new-run.txt
   ```
4. 例外記録単独は`decode_exception.py`、RAMログ単独は`decode_ram_log.py`で解析する。毎回固有run IDと新しい保存先を使う。欠落・古いrun・形式/hash不一致・未完了・後始末不備は正常完了にしない。

完了publishはRAM recordをCortex-A53のPoCへcleanする。live/途中停止は未完了のまま。JTAG APのPA/alias・read権限・coherencyは実機確認が必要で、保護を解除して読む運用にしない。BL33結果とTF-A/Core/TA内部traceは別物。性能はraw sampleとoverheadを保存し、閾値なしはMEASUREDで速度PASSを付けない。測定中のbreakpointは性能値を無効にする。

通常署名TAのbundle配置・専用UUID・RSA/digest拒否の条件は[署名TA設計](b2_test_specification.md)、物理probeの限定復帰と拒否層は[物理設計](b2_test_specification.md)、各小試験は[21項目](b2_scope_21.md)を参照する。
