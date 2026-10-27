# Git sourceだけで受け渡す手順

外部networkが使える場所で取得し、の許可Gitへmirrorする工程と、でのbuildを分ける。build中にはclone from remote、submodule取得、APT、pip、wget/curlを実行しない。`build_vitis_offline.sh` は既存local sourceからfresh workspaceを作り、古いELF/BIN/SDK/コンテナimageを入力にしない。

|Git repository（原本URL）|固定commit|必要な構成|
|---|---|---|
|本trustzone-validation project|で採用したcommitを記録|全構成。docs/scripts/tests/platform/commonと各payloadを欠落させない|
|Xilinx/arm-trusted-firmware|dd3512287a17eea866d5c82fa7b86ad58de59163|BL31、QEMU BL1/2/31/FIP|
|OP-TEE/optee_os|0919de0f7c79ad35ad3c8ace5f823ad1344b4716|BL32、SDK、TA依存source|
|eliben/pyelftools|d4ea35fc3c34eb0e4d3f8cfafb31552f5b5c37d6|OP-TEE Core ELF処理、純Python|
|llvm/llvm-project|2078da43e25a4623cab2d0d60decddf709aaea28|Clang routeのみ。compiler-rt/lib/builtinsの除算と関連header/license|
|Xilinx/embeddedsw|c0aed2eff7a30f307238ec853fa8fbc45dcabdda|将来のFSBL/PMUFW/BSP。今回のraw payload/TF-A/OP-TEE buildとは別。未取得/未ビルド|

URL・release・branchはthird_party/dependencies.lock.json。取得URLはmirrorへ置き換えてよいが、upstream commitを勝手に変更しない。完全baremetalには外部firmware repoは不要。GCCを使えばLLVM repoも不要。

## Gitから取得する工程（build前）

```sh
# URLはの許可された実URLへ置換する
# この取得工程はbuild scriptとは別に実施する
 git clone <company-project-url> trustzone-validation
 cd trustzone-validation
 git clone <company-tfa-url> third_party/tfa-amd
 git -C third_party/tfa-amd checkout --detach dd3512287a17eea866d5c82fa7b86ad58de59163
 git clone <company-optee-url> third_party/optee
 git -C third_party/optee checkout --detach 0919de0f7c79ad35ad3c8ace5f823ad1344b4716
 git clone <company-pyelftools-url> third_party/pyelftools
 git -C third_party/pyelftools checkout --detach d4ea35fc3c34eb0e4d3f8cfafb31552f5b5c37d6
```

Clang routeならcompiler-rt用repoも固定commitで配置する。全依存のlicenseとGit metadataを保持する。partial clone/sparse checkoutを使う場合も必要objectを**取得工程で**揃える。build中はremote protocolとlazy fetchを禁止し、不足時は失敗させる。

submoduleを採用する場合はsuperprojectのgitlinkを固定し、全URLと入れ子のURLをGitへ向ける。取得工程でinit/updateを完了し、build scriptからsubmodule updateしない。今回選択した固定OP-TEE treeにはgitlinkがなく、配下library sourceは同repoに含まれる。将来の版や別sourceの追加で再確認する。

## 受領時の検査

```sh
python3 scripts/check_build_environment.py --configuration optee --compiler gcc --cross-compile /actual/vitis/tool/bin/aarch64-none-elf- --output results/phase8/company-environment.json
bash scripts/build_vitis_offline.sh --configuration optee --compiler gcc --cross-compile /actual/vitis/tool/bin/aarch64-none-elf-
```

完全checkoutが既定policy。HEAD不一致やtracked source変更を拒否する。sourceの所在を無視して別SDKを検索したり、ネットから補完しない。Vitisに付属するか不明なtoolは実検出する。事前にlibraryが不足していた場合はの許可された配布方法でhost tool/packageを準備し、build scriptへinstall処理を埋め込まない。

## 開発用subsetと実験の区別

この開発環境の外部sourceは、固定Git blobを検証したbuild subsetでありfull checkoutではない。開発試験だけ `--allow-snapshots` を明示する。metadataを削除したり、別commitのsubset repoを原本commitのcheckoutと偽装しない。

Phase 8ではこれらのsourceだけをlocal Git repoへ投入し、別directoryへGit cloneしてから再度fresh buildする試験も行う。receiptはsource-only-git-trial.jsonで、独自mirror commitとupstream commitを両方記録する。この試験をGitへの接続確認やfull upstream checkoutの検証としては報告しない。cache済みELF、BIN、SDK、container imageは使わない。compilerとhost OS libraryはbuild toolとして別管理する。

firmwareの固定sourceだけで生成できても、実機bootにはXSA/PSU初期化、生成BSP、FSBL/PMUFWと予約範囲が必要。本helperはそれらが確定したふりをせず、全実機NOT_RUNとload gateを維持する。

`make external`も取得処理を呼ばず、配置済みTF-A/OP-TEE sourceを検証するだけに変更した。従来の`fetch_dependencies.py`は明示的な取得工程用のlegacy utilityであり、のbuild手順では呼ばない。mirror URLの指定には上記の手動Git取得工程を使用する。
