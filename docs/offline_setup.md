# TF-A / OP-TEEのオフライン導入とTA SDK

## 固定版と取得先

`third_party/dependencies.lock.json` が基準。GitHub読み取り接続でコミット存在を確認した証跡は
`results/phase3/source-verification.json`。この確認はローカルcheckoutやビルド成功を意味しない。

|名前|repository|固定commit|ローカル配置|
|---|---|---|---|
|TF-A AMD fork|Xilinx/arm-trusted-firmware|dd3512287a17eea866d5c82fa7b86ad58de59163|third_party/tfa-amd|
|OP-TEE upstream 4.5.0|OP-TEE/optee_os|0919de0f7c79ad35ad3c8ace5f823ad1344b4716|third_party/optee|
|embeddedsw xilinx_v2025.1|Xilinx/embeddedsw|c0aed2eff7a30f307238ec853fa8fbc45dcabdda|third_party/embeddedsw|

Vitis・FSBL・PMUFW・TF-A・OP-TEEの実機組合せの互換性は未検証。

## 接続可能な別ホストでGit bundleを用意する

full cloneを使用する。shallow cloneから作るとprerequisite不足を持ち込むことがある。
以下の例をOP-TEE、embeddedswにもrepository/commitを置き換えて実施する。

```sh
git clone https://github.com/Xilinx/arm-trusted-firmware.git tfa-source
git -C tfa-source checkout --detach dd3512287a17eea866d5c82fa7b86ad58de59163
git -C tfa-source status --porcelain
git -C tfa-source bundle create ../tfa-amd.bundle HEAD
sha256sum tfa-amd.bundle
```

bundleとSHA-256記録を承認された媒体で運ぶ。SHA-256は輸送整合性の確認であり単独では発行者認証ではない。
submoduleを使用する外部設定が必要なら、各submoduleの固定commitも別bundleにする。
本版の必要submodule一覧の完全性は未照合なので、core build時に不足があればBLOCKEDとして扱う。

オフライン側repo root：

```sh
python3 scripts/import_dependency_bundle.py tfa-amd /media/transfer/tfa-amd.bundle --sha256 'replace-with-recorded-sha256'
python3 scripts/import_dependency_bundle.py optee /media/transfer/optee.bundle --sha256 'replace-with-recorded-sha256'
python3 scripts/import_dependency_bundle.py embeddedsw /media/transfer/embeddedsw.bundle --sha256 'replace-with-recorded-sha256'
```

importerはbundleのdigest、advertised commit、空repositoryでのbundle verify、checkout HEAD、
既存作業treeの差分とoriginを検査する。既存のローカル変更を上書きしない。
fixture bundleでimport、digest不一致拒否、変更済みtree拒否をホスト試験した。
実TF-A/OP-TEE bundleのimportはまだ未実施。

## ツール・Python依存

ホスト側にGNU Make、host C compiler、Git、Python 3、dtc、OpenSSL、pkg-configと
AArch64 GNU GCC/binutilsが必要。版とpathを記録し、対象OSのpackage名で確認する。
OP-TEE固定版の署名scriptはPython cryptographyを使用する。ELF解析script用pyelftoolsも用意する。
さらに固定版各scriptのimportと実build結果から追加依存を確認する。

接続ホストでは、オフラインホストと同じOS release・architectureの清潔なVM等で
必要packageの依存を含むdeb群を取得する。既に導入済みのホストで単純にdownload-onlyを実行しても
offline側に必要な依存の全量を取得できるとは限らない。
deb版一覧・digestとオフライン側の既存package一覧を照合する。
オフラインで管理者が導入する例：

```sh
sudo apt-get --no-download install /media/transfer/debs/*.deb
```

不足依存・ABI違いがあればここで停止する。設定変更・package導入は当セッションでは実施していない。
rootなしでdebを展開するだけの場合は共有libraryとfirmware/dataファイルの依存も別途確認が必要。

Pythonは同じPython minor版・host architecture向けwheelを接続ホストで用意する。

```sh
python3 -m pip download --only-binary=:all: --dest wheels cryptography pyelftools
python3 -m venv .venv-build
.venv-build/bin/python -m pip install --no-index --find-links=/media/transfer/wheels cryptography pyelftools
```

異なるPython向けwheelは流用しない。必要に応じPATHの先頭をvenv/binにし、実buildで使用したPythonを記録する。

## 環境変数とビルド順

PATHにはhost toolと `aarch64-linux-gnu-gcc` 一式を置く。
`build_external.py --cross-compile /absolute/toolchain/bin/aarch64-none-elf-` も選べるが、
そのtoolchainとの全互換性は未確認。`CROSS_COMPILE64` と `CROSS_COMPILE_ta_arm64` はscriptが設定する。
外部sourceの出力は `build/external` に分離する。

```sh
python3 scripts/build_external.py tfa
python3 scripts/build_external.py optee-sdk
python3 scripts/build_external.py ta-a
python3 scripts/build_external.py ta-b
python3 scripts/build_external.py optee \
    --early-ta build/external/ta-a/6dc13418-534f-4642-9138-215876304910.stripped.elf \
    --early-ta build/external/ta-b/6dc13419-534f-4642-9138-215876304910.stripped.elf
```

SDK段階は固定版の `make ta_dev_kit CFG_EARLY_TA=y` を使い、
`build/external/optee/export-ta_arm64/mk/ta_dev_kit.mk` の生成を成功条件にも含める。
TA段階のTA_DEV_KIT_DIRはそのexport directory。
Early TAは `EARLY_TA_PATHS` にstripped ELFを渡してCoreへ埋め込む。
固定4.5.0のmk/config.mkとta/ta.mkでこの方式を照合した。
通常の署名TAファイルのload拒否試験を実施した意味にはしない。

現環境ではsource checkout、GNU cross tools、SDK等が不足し、全段階preflight BLOCKED。
実build済みとはしていない。TF-A/BL33 entry、OP-TEE共有属性/RPC、FIP/flash生成・実bootは別の統合作業として残る。

## Phase 4で追加した依存と検証

TF-A/OP-TEE以外に、lock固定のpyelftools v0.32とLLVM compiler-rt llvmorg-21.1.8を使用した。ネット接続可能なホストで各repositoryを固定commitにcheckoutし、同じbundle方式で移送する。LLVM repository全体は大きいため、使用するファイルの固定tree/blob/modeを検証したsubsetとmanifestをsourceと一緒に配布する方法もある。`results/phase4/provenance`に今回のmanifestを保存した。manifestだけではソースを復元できない。

配置先は `third_party/{tfa-amd,optee,pyelftools,compiler-rt}`。既存snapshotへcheckoutを重ねず別場所で検証する。必要なホストtoolsはmake、host GCC/cc、Clang/LLD21.1.8、LLVM ar/objcopy/objdump/readelf/nm、Python3とcryptography。OP-TEEのpyelftoolsはbuilderがPYTHONPATHへ加える。

```sh
python3 scripts/source_snapshot.py tfa-amd optee pyelftools compiler-rt
bash scripts/run_phase4.sh
```

`run_phase4.sh` はビルドログとQEMUログを保存する。初回ソース取得以外はネットワーク不要。通常TAロードや永続storageに必要なRPC/supplicantは用意していない。外部ソースは完全checkoutまたは検証済みsubsetのどちらかを明記し、未取得のboard/featureが使えると仮定しない。
