# 01 環境導入と最小依存

## 目的

存在を検出してからtoolを選び、追加packageなしで可能な範囲を確定する。

## 前提条件・事前確認事項

[全体手順](00_overview.md)で`TZ_ROOT`、`TZ_LOG_DIR`、必要時`TZ_CROSS`を設定し、各shellを`bash`で開く。前章の入力を確認し、出力directoryを作ってから進む。Ubuntu 24.04/Vitis 2025.1の実操作と全ZCU106実行はNOT_RUN。外部ネットワークを必要とする取得処理はbuildに含めない。実機のload/run/resetは[安全検査](14_hardware_safety_check.md)の対象であり、現在BLOCKED。

## 必要なファイル・入力ファイル

取得済みsource、実在するVitis settings64.sh。追加依存は[構成別最小表](../minimum_dependencies.md)。

## GUIのメニュー名

UG1400の起動commandは`vitis -w <workspace>`。この手順ではTZ_WORKSPACEを指定する。付属toolの候補と実検出を混同しない。

## 操作手順・実行コマンド

1. 00章のshellで実インストール先を入力する。settings scriptの存在を確認してから読み込む。

```bash
read -r -p 'Vitis installation directory: ' TZ_VITIS_ROOT
export TZ_VITIS_ROOT
test -f "$TZ_VITIS_ROOT/settings64.sh" && source "$TZ_VITIS_ROOT/settings64.sh"
export TZ_WORKSPACE="$TZ_ROOT/build/vitis-workspace"
mkdir -p "$TZ_WORKSPACE"
{ cat /etc/os-release; uname -a; command -v vitis make python3; } > "$TZ_LOG_DIR/environment.txt" 2>&1
vitis -w "$TZ_WORKSPACE" > "$TZ_LOG_DIR/vitis.log" 2>&1 &
```

2. GNU AArch64 compilerを探す。同梱path/prefixは仮定しない。候補を選び末尾`-`までのprefixを入力する。

```bash
find "$TZ_VITIS_ROOT" -type f -name 'aarch64*gcc' > "$TZ_LOG_DIR/compiler-candidates.txt"
cat "$TZ_LOG_DIR/compiler-candidates.txt"
read -r -p 'Actual AArch64 GNU cross prefix (including final -): ' TZ_CROSS
export TZ_CROSS
{ "${TZ_CROSS}gcc" --version; "${TZ_CROSS}as" --version; "${TZ_CROSS}ld" --version; make --version; python3 --version; } > "$TZ_LOG_DIR/tool-versions.txt" 2>&1
python3 -S scripts/check_build_environment.py --configuration baremetal --compiler gcc --cross-compile "$TZ_CROSS" --output "$TZ_LOG_DIR/preflight-A.json" | tee "$TZ_LOG_DIR/preflight-A.log"
```

3. B1は`--configuration tfa`、B2/TAは`optee`で同じcheckerを再実行する。不足する名前・用途をJSONから確認し、の許可された配布手段で事前準備する。build中のAPT/pip installは禁止。
4. baremetalはPython stdlib、make/bash/coreutils、target C/ASM/linkerで生成可能。B1に固定TF-A/Git、B2に固定OP-TEE/pyelftools GitとPython cryptographyが追加。pyelftoolsはPYTHONPATHで固定sourceを使いpip不要。GNUはlibgcc、Clangのときだけcompiler-rt sourceが必要。QEMUは実機ELF生成に不要で、QEMU用FIPに限りnative C/libcrypto開発fileが追加。
5. 選択したPythonで`python3 -c 'import cryptography; print(cryptography.__version__)'`を確認する。B2で不足ならBLOCKED。独自暗号や空runtimeで代用しない。BSP生成に使うCMake/Ninja/SDTgen等も実IDEで確認する。

## 出力ファイル・実行ログの保存先

environment.txt、tool-versions.txt、preflight-*.json、IDE起動log。 `TZ_LOG_DIR`はGit登録しない社内の実行証跡directory。build helperの詳細logは`results/phase8/runs/<UTC>/`へ自動保存される。実際の絶対path、日時、構成、source commit、全ELF hashを同じrunへ記録する。

## 正常時の確認方法

本章の期待出力を保存し、終了コードと内容を両方確認する。

## エラー時の対応

原因を保存して処理を止め、[トラブルシューティング](13_troubleshooting.md)で切り分ける。

## PASS／FAIL／BLOCKED／NOT_RUN判定・実環境での確認状況

実行して期待値に一致した工程だけPASS、不一致はFAIL。入力・tool・初期化・安全条件不足はBLOCKED。未実行はNOT_RUN。試験対象外はSKIP。BUILD PASS、QEMU PASS、HARDWARE PASSを分ける。本章の環境操作・GUI・実機はNOT_RUNで、ロード準備はBLOCKED。開発hostで確認済みの範囲は[Phase 8報告](../phase8_progress.md)と[追加作業の確認記録](../../results/manual-validation-summary.json)を参照する。



## 公式資料の文書番号と章節

- [UG1400 2025.1 — Using the Vitis Unified IDE → Launching the Vitis Unified IDE](https://docs.amd.com/r/2025.1-English/ug1400-vitis-embedded/Launching-the-Vitis-Unified-IDE)。
- [UG1701 2025.1 — Developing Vitis Kernels and Applications](https://docs.amd.com/r/2025.1-English/ug1701-vitis-accelerated-embedded/Developing-Vitis-Kernels-and-Applications)。

番号付き章節が公開Webにない場合は見出しの階層で示す。未確認の節番号は補わない。公式資料は一般仕様・一般操作の根拠であり、本repoのsource一覧、shell変数、検査コマンド、未確定handoffの停止条件は本プロジェクト固有の手順。資料閲覧は実機動作確認を意味しない。

[前へ](00_overview.md) · [目次](00_overview.md) · [次へ](02_vitis_platform_creation.md)
