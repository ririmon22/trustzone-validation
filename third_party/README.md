現在のZIP展開物向け入口は[次回手順](../docs/next_offline_session.md)と[ZIP専用build](../docs/zip_offline_build.md)。以下のGit checkout向け手順は別モードであり、ZIPにGit検査を要求したり、build中にcloneするものではない。

# 固定外部ソース

`dependencies.lock.json` にrepository/full commit/releaseを記録する。AMD TF-Aとupstream OP-TEEを区別し、Vitis/ZCU106互換性を未確認のまま確認済みとはしない。

Phase 4ではGitHub connectorから固定git tree/blobを取得し、ビルドに必要なsubsetを配置した。完全git checkoutではない。ソースbytes、git blob SHA-1、size、実行modeをmanifestで検証し、第三者ソースは改変していない。

|配置先|commit|内容|
|---|---|---|
|tfa-amd|dd3512287a17eea866d5c82fa7b86ad58de59163|1414 files、ZynqMP/QEMUのbuild subset|
|optee|0919de0f7c79ad35ad3c8ace5f823ad1344b4716|2629 files、QEMU ARM64 Core/SDK subset|
|pyelftools|d4ea35fc3c34eb0e4d3f8cfafb31552f5b5c37d6|v0.32 Python package、57 files|
|compiler-rt|2078da43e25a4623cab2d0d60decddf709aaea28|LLVM21.1.8の実128-bit unsigned division、header/license、7 files|

`.source_manifest.json` は各source root、提出用コピーは `results/phase4/provenance`。`source_snapshot.py` はcommit/blob/mode/追加sourceを毎回検査する。通常のgit checkoutも固定HEADとclean treeで検査する。manifestを自分の変更に合わせて書き換えてはならない。

```sh
python3 scripts/source_snapshot.py tfa-amd optee compiler-rt pyelftools
```

ソース本体とビルド生成物はgitignore対象。新規ホストでは完全な固定commit checkoutか [offline bundle](../docs/offline_setup.md)を使用する。subsetがある場所へgit取得を上書きせず、別directoryでcheckout検証後に置換する。compiler-rtはLLVM全体を `third_party/compiler-rt` にcheckoutすると同じ相対pathになる。pyelftoolsは別repo。全ソースを配布する場合は元LICENSEを必ず保持する。

TF-A/OP-TEE/Early TAはbuildとQEMU起動済み。embeddedswはcommit確認のみ、FSBL/PMUFW/BSPのbuildは未実施。ZCU106 runtime互換性はNOT_RUN。

Phase 5ではCore canary用の自作PTAを、`build/source/optee-validation` の派生copyへ明示的に追加する。原本のblob検証は維持し、派生copyの追加2 fileとsub.mk変更を `results/phase5/optee-validation-overlay.json`へ記録する。元のmanifestは派生copyで `.base_source_manifest.json` とし、原本無改変の検証結果とoverlay適用後を区別する。`--validation-pta` は検証専用であり製品構成ではない。
