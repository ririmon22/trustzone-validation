# 15 オフラインsource取得とGit登録

## 目的

取得工程とbuildを分離し、生成物や秘密情報を登録せずにGitで再構築する。

## 前提条件・事前確認事項

[全体手順](00_overview.md)で`TZ_ROOT`、`TZ_LOG_DIR`、必要時`TZ_CROSS`を設定し、各shellを`bash`で開く。前章の入力を確認し、出力directoryを作ってから進む。Ubuntu 24.04/Vitis 2025.1の実操作と全ZCU106実行はNOT_RUN。外部ネットワークを必要とする取得処理はbuildに含めない。実機のload/run/resetは[安全検査](14_hardware_safety_check.md)の対象であり、現在BLOCKED。

## 必要なファイル・入力ファイル

の許可Git URL、third_party/dependencies.lock.json、各repoの完全な固定commit/object、license。

## GUIのメニュー名

この章のcommandは端末で実行する。GUI操作なし。

## 操作手順・実行コマンド

1. Gitに必要なsourceがmirrorされていることを管理者と確認する。プロジェクト、AMD TF-A、OP-TEE、pyelftoolsが必要。Clangのみcompiler-rt、FSBL/PMUFW/BSPを追加sourceからbuildするときは固定embeddedswも必要。URL/commitの全一覧は[Phase8受け渡し表](../source_only_transfer.md)。対象構成の完全checkoutをbuild開始前に取得する。
2. projectを未取得の場合は、取得専用端末で次を実行する。が採用したproject commitを入力する。既に取得済みならこのblockは省き00章のTZ_ROOT/TZ_LOG_DIRを使う。

```bash
bash
read -r -p 'Company project Git URL: ' TZ_PROJECT_GIT
read -r -p 'New destination directory: ' TZ_PROJECT_DIRECTORY
read -r -p 'Approved project commit: ' TZ_PROJECT_COMMIT
git clone "$TZ_PROJECT_GIT" "$TZ_PROJECT_DIRECTORY"
cd "$TZ_PROJECT_DIRECTORY"
git checkout --detach "$TZ_PROJECT_COMMIT"
export TZ_ROOT="$(pwd -P)"
export TZ_LOG_DIR="$TZ_ROOT/results/company/$(date -u +%Y%m%dT%H%M%SZ)"
mkdir -p "$TZ_LOG_DIR"
set -o pipefail
```

3. 外部sourceのURLを`read`へ入力し、実際のGitを使う。baremetalだけなら外部firmware source不要。

```bash
cd "$TZ_ROOT"
read -r -p 'Company TF-A Git URL: ' TZ_TFA_GIT
read -r -p 'Company OP-TEE Git URL: ' TZ_OPTEE_GIT
read -r -p 'Company pyelftools Git URL: ' TZ_ELF_GIT
git clone "$TZ_TFA_GIT" third_party/tfa-amd
git -C third_party/tfa-amd checkout --detach dd3512287a17eea866d5c82fa7b86ad58de59163
git clone "$TZ_OPTEE_GIT" third_party/optee
git -C third_party/optee checkout --detach 0919de0f7c79ad35ad3c8ace5f823ad1344b4716
git clone "$TZ_ELF_GIT" third_party/pyelftools
git -C third_party/pyelftools checkout --detach d4ea35fc3c34eb0e4d3f8cfafb31552f5b5c37d6
```

4. 既存directoryがある場合はcloneを重ねず、HEAD/dirty/source manifestを確認する。branch名だけで版を指定しない。submoduleなら全gitlink/object/入れ子URLを取得工程で揃える。partial cloneのlazy-fetch残存をbuildで補完しない。今回の開発subsetは明示`--allow-snapshots`でのみ許可する実験用で、full upstream checkoutや接続の証明とは別。
5. 01章の環境検査後、04〜06章でbuildする。ネット取得/追加package installをbuildに埋め込まない。旧ELF/BIN/SDK/コンテナを入力にせずfresh workspaceを使う。
6. QEMUが用意できる場所で全構成を再現する場合は次を実行する。VitisGNU経路は未検証、Clang例は01章で実検出したtoolがある場合だけ使う。

```bash
bash scripts/build_vitis_offline.sh --configuration all --platform qemu --compiler clang --qemu --host-tests | tee "$TZ_LOG_DIR/qemu-reproduction.log"
```

7. Git登録前は`git status --short`、`git diff --cached --stat`、`git diff --cached --check`で内容を確認する。build/、大量log、ELF/BIN/SDK、ローカル設定、鍵/証明書/資格情報を除外する。sourceとlockは登録し、外部repo原本/ライセンスをmirror側で保持する。開発subset自体は本project Gitへvendorしていない。
8. 以下は登録対象の確認だけを行う。秘密内容をconsoleへ出さない。履歴を書き換えるforce/reset/rebaseは使わない。や公開Gitへのpushは別工程で、remote URLと公開範囲を確認する。今回の作業はlocal indexへの登録までで、remote公開を実施しない。

```bash
git status --short > "$TZ_LOG_DIR/git-status.txt"
git diff --cached --stat > "$TZ_LOG_DIR/git-staged-stat.txt"
git diff --cached --check > "$TZ_LOG_DIR/git-whitespace-check.txt"
git ls-files > "$TZ_LOG_DIR/git-files.txt"
```

9. 公式PDF等をで参照する場合はの承認済み保管手段で事前準備し、sourceのGitへ無断で転載しない。build中に資料を取りに行かない。大量logはlocal results/に残して、Gitにはコンパクトな集計/hash/確認記録だけを登録する。古い文書のlocal evidenceリンクはGit cloneだけでは実体がない場合がある。ではrun scriptで再生成し、必要なら別の承認済み証跡保管先で管理する。

## 出力ファイル・実行ログの保存先

側source checkout、source commit検証log、Git登録内容の確認、将来のfresh build receipt。 `TZ_LOG_DIR`はGit登録しない社内の実行証跡directory。build helperの詳細logは`results/phase8/runs/<UTC>/`へ自動保存される。実際の絶対path、日時、構成、source commit、全ELF hashを同じrunへ記録する。

## 正常時の確認方法

全必須sourceのcommit/hashとclean treeを確認。buildはlocal file transport/Git lazy-fetch禁止で不足時BLOCKED。Git登録対象に生成物/秘密鍵/大量logがないことを確認する。の取得/実buildはNOT_RUN。

## エラー時の対応

原因を保存して処理を止め、[トラブルシューティング](13_troubleshooting.md)で切り分ける。

## PASS／FAIL／BLOCKED／NOT_RUN判定・実環境での確認状況

実行して期待値に一致した工程だけPASS、不一致はFAIL。入力・tool・初期化・安全条件不足はBLOCKED。未実行はNOT_RUN。試験対象外はSKIP。BUILD PASS、QEMU PASS、HARDWARE PASSを分ける。本章の環境操作・GUI・実機はNOT_RUNで、ロード準備はBLOCKED。開発hostで確認済みの範囲はPhase 8報告（別途生成・用意: `../phase8_progress.md`）と追加作業の確認記録（別途生成・用意: `../../results/manual-validation-summary.json`）を参照する。



## 公式資料の文書番号と章節

- [UG1400 2025.1 — Using the Vitis Unified IDE → Launching the Vitis Unified IDE](https://docs.amd.com/r/2025.1-English/ug1400-vitis-embedded/Launching-the-Vitis-Unified-IDE)。
- [TF-A 2.12.0 — §7.45（ZynqMP）、§7.45.1（build設定）、§7.45.5（FSBLからの引渡し）](https://trustedfirmware-a.readthedocs.io/en/v2.12/plat/xilinx-zynqmp.html)。
- [OP-TEE documentation stable（閲覧2026-10-27） — Core → Normal World invokes OP-TEE OS using SMC / Shared memory / Remote Procedure Call](https://optee.readthedocs.io/en/stable/architecture/core.html)。

番号付き章節が公開Webにない場合は見出しの階層で示す。未確認の節番号は補わない。公式資料は一般仕様・一般操作の根拠であり、本repoのsource一覧、shell変数、検査コマンド、未確定handoffの停止条件は本プロジェクト固有の手順。資料閲覧は実機動作確認を意味しない。

[前へ](14_hardware_safety_check.md) · [目次](00_overview.md)
