# 03 Application Component作成

## 目的

通常Standalone templateと本repoの自作startupを分離して取り込む。

## 前提条件・事前確認事項

[全体手順](00_overview.md)で`TZ_ROOT`、`TZ_LOG_DIR`、必要時`TZ_CROSS`を設定し、各shellを`bash`で開く。前章の入力を確認し、出力directoryを作ってから進む。Ubuntu 24.04/Vitis 2025.1の実操作と全ZCU106実行はNOT_RUN。外部ネットワークを必要とする取得処理はbuildに含めない。実機のload/run/resetは[安全検査](14_hardware_safety_check.md)の対象であり、現在BLOCKED。

## 必要なファイル・入力ファイル

02章のPlatform/Standalone domain、Makefile、[構成別source表](../vitis_2025_1_offline_build.md)。

## GUIのメニュー名

Application作成、選択componentのBuild/Run/Debug、Launch Configuration。include/compiler/linkerの実設定欄名は実IDEで確認し、旧ClassicのProperties画面名を流用しない。

## 操作手順・実行コマンド

1. `File > New Component > Application`からcomponentを作り、`tz_zcu106`と対象Standalone domainを選ぶ。AはEL3/Secure/NSを別component、B1/B2はBL33のみApplicationにする。TF-A/Core/TAは公式Makeで別buildする。
2. Cと大文字`.S`をsourceとして追加し、ASMのpreprocess/includeも有効なcompile commandを確認する。必要sourceは下表とMakefileに従う。共有ファイルを各componentで重複定義しない。
3. B2だけは`platform/zcu106/b2`を最優先includeにして`-DTEE_SHARED_CACHED=1`を指定する。[整合化手順](../b2_zcu106_alignment.md)のruntime gateを維持する。Includeに`common/include`、`platform/zcu106`、`optee_standalone/client`を設定する。driver libraryを組む場合のみ`platform/zcu106/include`も必要。
4. C/ASM target flagsとlinker設定をMakefileと一致させる。`-mcpu=cortex-a53 -mgeneral-regs-only -mstrict-align`、freestanding、`-nostdlib -static`、対応linker、map出力を指定する。全C flagsは[Phase8設定表](../vitis_2025_1_offline_build.md)に記載。`_start`とBSP `_prestart`/crt0の二重linkを避ける。
5. 自作EL3はBSP templateを通常のままbuildしない。User Managed Modeで本repo Makeを外部buildとして使う方式なら、04/05/06章と同じcommandを登録する。標準Applicationの自動startupを止められなければBLOCKED。
6. CMake生成componentの場合は生成source list、include/compile/link options、ASM language認識とlinkerを確認する。GUIの設定表示だけでなくBuild consoleの実commandを保存する。GUIの細部は実環境NOT_RUN。
7. Build後、`readelf -h -l -S`/mapと04章のELF inventoryでentry/PT_LOAD/BSS等を確認する。
8. Run/DebugのLaunch Configurationを構成ごとに別作成し、processor、target connection、初期化script、自動download/reset/stop-at-mainをreviewする。Aは`monitor_main`、B1/B2はBL31 entryからのhandoffが必要であり、BL33のmainから単独runしない。14章BLOCKEDの間はlaunchしない。

|component|固有source|linker|
|---|---|---|
|A EL3|baremetal/el3_monitor/{start.S,monitor.c}|linker_el3.ld|
|A S-EL1|baremetal/secure_app/{start.S,main.c}＋EL1共通|linker_secure.ld|
|A NS-EL1|baremetal/nonsecure_app/{start.S,main.c}＋baremetal/tests/register_context.S＋runner/benchmark＋EL1共通|linker_nonsecure.ld|
|B1 BL33|tfa_standalone/integration/{start.S,main.c}＋runner＋EL1共通|linker_nonsecure.ld|
|B2 BL33|tfa_standalone/integration/start.S、optee_standalone/tests/{main,security_tests,stress_tests}.c、client/client.c、platform/zcu106/b2/client_mmu.c＋runner/benchmark＋EL1共通|platform/zcu106/b2/linker_bl33.ld|

全componentの共通sourceはcommon/{memory,arch}.c、common/result_report/console.c、platform/zcu106/uart.c。EL1共通はcommon/{el1_vectors.S,el1_fault.c,probe.S}、runnerはcommon/test_runner/runner.c、benchmarkはcommon/benchmark/benchmark.c。全headerを保持する。linkerはplatform/zcu106以下。QEMU専用external linker/client_mmu.cを入れない。

## 出力ファイル・実行ログの保存先

Aの3 component、B1/B2各BL33 component、設定export、compile/link command、ELF/map。 `TZ_LOG_DIR`はGit登録しない社内の実行証跡directory。build helperの詳細logは`results/phase8/runs/<UTC>/`へ自動保存される。実際の絶対path、日時、構成、source commit、全ELF hashを同じrunへ記録する。

## 正常時の確認方法

本章の期待出力を保存し、終了コードと内容を両方確認する。

## エラー時の対応

原因を保存して処理を止め、[トラブルシューティング](13_troubleshooting.md)で切り分ける。

## PASS／FAIL／BLOCKED／NOT_RUN判定・実環境での確認状況

実行して期待値に一致した工程だけPASS、不一致はFAIL。入力・tool・初期化・安全条件不足はBLOCKED。未実行はNOT_RUN。試験対象外はSKIP。BUILD PASS、QEMU PASS、HARDWARE PASSを分ける。本章の環境操作・GUI・実機はNOT_RUNで、ロード準備はBLOCKED。開発hostで確認済みの範囲はPhase 8報告（別途生成・用意: `../phase8_progress.md`）と追加作業の確認記録（別途生成・用意: `../../results/manual-validation-summary.json`）を参照する。



## 公式資料の文書番号と章節

- [XD260 2025.1 — Getting Started in Vitis Unified Embedded IDE → Create Application](https://docs.amd.com/r/2025.1-English/Vitis-Tutorials-Embedded-Software/Create-Application)。
- [UG1400 2025.1 — User Managed Mode](https://docs.amd.com/r/2025.1-English/ug1400-vitis-embedded/User-Managed-Mode)。
- [UG1400 2025.1 — Configuring a Domain → Board Support Package Settings Page](https://docs.amd.com/r/2025.1-English/ug1400-vitis-embedded/Board-Support-Package-Settings-Page)。

番号付き章節が公開Webにない場合は見出しの階層で示す。未確認の節番号は補わない。公式資料は一般仕様・一般操作の根拠であり、本repoのsource一覧、shell変数、検査コマンド、未確定handoffの停止条件は本プロジェクト固有の手順。資料閲覧は実機動作確認を意味しない。

[前へ](02_vitis_platform_creation.md) · [目次](00_overview.md) · [次へ](04_baremetal_build.md)
