# 02 Platform Component作成

## 目的

内蔵ZCU106定義と生成BSPから実際のメモリ・周辺・startup情報を収集する。

## 前提条件・事前確認事項

[全体手順](00_overview.md)で`TZ_ROOT`、`TZ_LOG_DIR`、必要時`TZ_CROSS`を設定し、各shellを`bash`で開く。前章の入力を確認し、出力directoryを作ってから進む。Ubuntu 24.04/Vitis 2025.1の実操作と全ZCU106実行はNOT_RUN。外部ネットワークを必要とする取得処理はbuildに含めない。実機のload/run/resetは[安全検査](14_hardware_safety_check.md)の対象であり、現在BLOCKED。

## 必要なファイル・入力ファイル

Vitis 2025.1、内蔵ZCU106 hardware定義または承認済みXSA、01章のworkspace。

## GUIのメニュー名

`File > New Component > Platform`、Platform/Domain設定、Board Support Package settings、Flow Navigator `Build`。内蔵ZCU106の選択肢・詳細欄名は実GUI NOT_RUN。

## 操作手順・実行コマンド

1. Vitis Explorerで`File > New Component > Platform`を選び、名称を`tz_zcu106`とする。hardware選択にZCU106があれば選ぶ。ない場合はインストールcomponentsと提供XSAを確認しBLOCKED。内蔵定義を使うためだけに新規Vivado設計を作らない。
2. processorを`psu_cortexa53_0`、OSを`standalone`、architectureをAArch64にする。表示が異なる場合はXSA/SDTの対象を確認し、他CPUへ置き換えない。
3. DomainのBSP設定でstdin/stdout、UART instance、CPU input clock、timer、cache/MMU、例外/stack/heap設定を記録する。boot artifactsを生成する選択を使った場合はFSBL/PMUFWを含む生成先を記録する。
4. 生成linkerのDDR/OCMと、FSBL/PMUFW/TF-A/OP-TEE予約を14章で照合する。BSSやstack/heapをsectionのfile sizeだけで評価しない。
5. Flow NavigatorのPlatform `Build`を実行しconsole全体を保存する。生成物は固定pathと仮定せずworkspaceから確認する。

```bash
find "$TZ_WORKSPACE" -type f \( -name 'bspconfig.h' -o -name 'boot.S' -o -name 'lscript.ld' -o -name '*.xsa' -o -name '*fsbl*.elf' -o -name '*pmufw*.elf' \) > "$TZ_LOG_DIR/platform-files.txt"
cat "$TZ_LOG_DIR/platform-files.txt"
```

6. 生成された`bspconfig.h`、startup `boot.S`とcompile commandを読み、`EL3`/`EL1_NONSECURE`の実定義、`CurrentEL`の比較、`error`分岐、MMU有効化を記録する。BSPは常にEL3とは仮定しない。
7. 固定embeddedswのARMv8/64bit/gcc/boot.Sを調査したところ、入口EL3＋EL3 build、または入口EL1＋EL1_NONSECURE buildを受理し、それ以外はerror loopになる。NS属性自体はCurrentELだけで判定できない。の実生成物で同じ実装か再確認する。これらのstartupはMMU/cacheとSCR等も設定するため、自作monitorへ混ぜない。

## 出力ファイル・実行ログの保存先

生成Platform、XSA/SDT、BSP/生成linker、生成時のFSBL/PMUFW ELF/map、platform-build.log、startup-review.txt。 `TZ_LOG_DIR`はGit登録しない社内の実行証跡directory。build helperの詳細logは`results/phase8/runs/<UTC>/`へ自動保存される。実際の絶対path、日時、構成、source commit、全ELF hashを同じrunへ記録する。

## 正常時の確認方法

本章の期待出力を保存し、終了コードと内容を両方確認する。

## エラー時の対応

原因を保存して処理を止め、[トラブルシューティング](13_troubleshooting.md)で切り分ける。

## PASS／FAIL／BLOCKED／NOT_RUN判定・実環境での確認状況

実行して期待値に一致した工程だけPASS、不一致はFAIL。入力・tool・初期化・安全条件不足はBLOCKED。未実行はNOT_RUN。試験対象外はSKIP。BUILD PASS、QEMU PASS、HARDWARE PASSを分ける。本章の環境操作・GUI・実機はNOT_RUNで、ロード準備はBLOCKED。開発hostで確認済みの範囲はPhase 8報告（別途生成・用意: `../phase8_progress.md`）と追加作業の確認記録（別途生成・用意: `../../results/manual-validation-summary.json`）を参照する。

## startupのソース根拠

[Xilinx embeddedsw固定commit boot.S](https://github.com/Xilinx/embeddedsw/blob/c0aed2eff7a30f307238ec853fa8fbc45dcabdda/lib/bsp/standalone/src/arm/ARMv8/64bit/gcc/boot.S)、Git blob `19b4210ad81b0aec6bca3bc2afcd97ccaa2eb62d`の`OKToRun`、`InitEL3`、`InitEL1`を参照した。で生成したBSPの実測ではない。

## 公式資料の文書番号と章節

- [UG1400 2025.1 — Develop → Creating a Platform Component from XSA](https://docs.amd.com/r/2025.1-English/ug1400-vitis-embedded/Creating-a-Platform-Component-from-XSA)。
- [UG1400 2025.1 — Configuring a Domain → Board Support Package Settings Page](https://docs.amd.com/r/2025.1-English/ug1400-vitis-embedded/Board-Support-Package-Settings-Page)。
- [UG1209 2025.1 — 起動と構成 → システムソフトウェア（同名節）](https://docs.amd.com/r/2025.1-English/ug1209-embedded-design-tutorial/Boot-and-Configuration)。

番号付き章節が公開Webにない場合は見出しの階層で示す。未確認の節番号は補わない。公式資料は一般仕様・一般操作の根拠であり、本repoのsource一覧、shell変数、検査コマンド、未確定handoffの停止条件は本プロジェクト固有の手順。資料閲覧は実機動作確認を意味しない。

[前へ](01_environment_setup.md) · [目次](00_overview.md) · [次へ](03_vitis_application_creation.md)
