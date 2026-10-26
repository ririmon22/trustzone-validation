# Phase 7 開発環境と再現方法

Ubuntu26.04.1 x86_64、Clang integrated assembler/LLD/LLVM21.1.8、QEMU10.2.1、Python3.14.4、Make4.4.1を使用。実ファイルSHA-256とversion、TF-A/OP-TEE等の固定commitは `container/toolchain.lock.json`。Debian package版は `container/packages.lock`。TA/Coreのソースpinと検証overlayを区別する。

## ホスト側クリーン再構築

依存は既存offline手順で `third_party/{tfa-amd,optee,compiler-rt,pyelftools}` へ固定sourceを配置する。通常のfixed Git checkout、またはmanifest検証済みsubsetを使用できる。ソース不足やtoolchain mismatchは停止する。

```sh
bash scripts/run_phase7.sh
```

新規build/phase7-clean-*へ自作sourceと固定版のsourceだけをコピーする。Gitの場合はtracked file、snapshotの場合はmanifest掲載fileのみであり、第三者directory内のfiptool executableや.o等も持ち込まない。元build/resultsはコピーせず、元workspaceのELFは変更しない。LLVM runtime、12 ELF、ZynqMP/QEMU TF-A、SDK/TA/Core/FIPを生成し、host/QEMU全試験、性能、署名host検査、安全ゲート、Markdown集計まで実行する。

`results/phase7/clean-workspace.json` がworkspace、source hash、exit code、今回の証跡directoryを示す。既存出力は保存し再実行時は別directoryにする。`latest-report.md` は最新のexport済みlogを参照する。DWARF絶対path、TA署名、日時等があるため全imageのbit一致を保証する試験ではない。SOURCE_DATE_EPOCH、locale、Python hash seedを固定するが、実際のsource/hashと機能結果を根拠とする。

## Podman

[Podman build公式資料](https://docs.podman.io/en/stable/markdown/podman-build.1.html) のContainerfile方式を使用する。通常のPodman対応Ubuntuで、Ubuntu26.04 amd64のbase imageを取得・検査し、その**実際のimmutable digest**を指定する。

```sh
# baseはnormal hostでpull/inspect済みの正しいdigestへ置換する
TZ_BASE_IMAGE='docker.io/library/ubuntu@sha256:<verified-64-hex-digest>' bash scripts/podman_phase7.sh build
bash scripts/podman_phase7.sh run
```

base digestは本セッションで取得・検証できていないため、架空の値やmutable defaultを埋めていない。helperはdigest必須、pull=neverで、base未配置なら失敗する。packageはlockのexact versionを要求し、利用不可なら別版へ自動変更しない。全installed packageをimage内 /opt/tz/installed-packages.txtへ保存する。top-level toolsと主要runtimeをpinしており、APT repository全体を永久保存したものではない。base/image ID・package closure・成功logを取得して初めて検証済みimageとする。

このCodex実行プロセスではPodman5.7.0を確認したが、通常runrootがread-only。workspace内runroot/rootへ変更しても `cannot clone: Operation not permitted` で起動不可だった。証跡はpodman-*.log。コンテナbuild/runはBLOCKEDであり、hostクリーンworkspaceの成功をcontainer成功にしない。ホストOS全体のPodman禁止とは判断していない。seccomp/LSMやprivileged等の回避設定は使っていない。

## QEMUをhostで実行する場合

```sh
bash scripts/podman_phase7.sh build-only
# clean-workspace.jsonのworkspaceを通常hostから開く
cd <generated-clean-workspace>
bash scripts/validate_phase7_built.sh
```

build-onlyはBUILD_PASS_EXECUTION_NOT_RUNと記録する。再開は入力sourceを変更していないbuilt workspaceを対象とし、変更した場合はfresh buildをやり直す。host側にもlockに一致するtoolchain/Python/LLVM tools/QEMUが必要。ELF/FIP検査のobjcopyやMakeも必要なので、QEMUだけあればよいとはしない。guestはTCG/1CPU/virt-9.2 secure=on/Cortex-A53、network backendなし。KVM、host機器、USB/JTAGをcontainerへ渡さない。container内/外の性能値はQEMUによる基盤検証であり実機値ではない。SELinux等の設定変更は自動化しない。

## 残る未完了

container namespace、base digest、実image clean buildはnormal hostでの確認待ち。VitisとZCU106は利用せず全実機試験NOT_RUN。移植用source packageは別途 `python3 scripts/package_vitis_phase7.py` で作る。安全ゲートは常に維持する。
