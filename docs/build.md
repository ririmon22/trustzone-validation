# 現行Makeの設定と個別ビルド

[入口](../README.md) · [追加構成](profiles.md) · [実機点検・回収](zcu106_run.md)

以下のコマンドはリポジトリ直下で実行する。利用者固有の常用ディレクトリ名は前提にしない。

## 必要なものと設定

`cp -n local.mk.example local.mk` で利用者のパスだけ設定する。空白を含むパスは未対応。
GNU make、Python 3、BL33用Clang＋LLDまたはAArch64 bare-metal GCC、
外部プロジェクト用AArch64 GCC（既存手順の `aarch64-linux-gnu-`）が必要。
OP-TEEのPython依存はpyelftools 0.32、cryptography。署名試験にはopensslも用意する。
外部ソースのホスト依存は各版のビルド資料にも従う。

外部ソースは自動取得しない。次のリポジトリの指定版を手動取得・展開する。
ZIPの展開物を指定してもビルドはするが、commit検証済みとは扱わない。

| ソース | 版・参照commit | 標準配置 |
|---|---|---|
| Xilinx/arm-trusted-firmware | xlnx_rebase_v2.12_2025.1 / dd3512287a17eea866d5c82fa7b86ad58de59163 | third_party/tfa-amd |
| OP-TEE/optee_os | 4.5.0 / 0919de0f7c79ad35ad3c8ace5f823ad1344b4716 | third_party/optee |
| eliben/pyelftools | v0.32 / d4ea35fc3c34eb0e4d3f8cfafb31552f5b5c37d6 | third_party/pyelftools |
| llvm/llvm-project（旧Clang外部ルートのみ） | llvmorg-21.1.8 / 2078da43e25a4623cab2d0d60decddf709aaea28 | third_party/compiler-rt |
| Xilinx/embeddedsw（Vitis参考） | xilinx_v2025.1 / c0aed2eff7a30f307238ec853fa8fbc45dcabdda | third_party/embeddedsw |

取得元は [TF-A](https://github.com/Xilinx/arm-trusted-firmware.git)、
[OP-TEE](https://github.com/OP-TEE/optee_os.git)、
[pyelftools](https://github.com/eliben/pyelftools.git)。上表のcommitを指定して準備する。
`third_party/dependencies.lock.json` は来歴資料であり、新しいMakeルートの入力にはしない。
上表の配置はこのREADMEのあるディレクトリを基点とする。
今回、このworktreeの `third_party/` には来歴JSONだけがあり、外部ソースは不足している。
旧バックアップからの自動復元・混在は行っていない。

| ツール | 版と確認状態 | 配置・指定方法 |
|---|---|---|
| Clang / LLD | 21.1.8でBL33を確認済み | PATH上の `clang` / `ld.lld`。この環境では `/usr/bin/clang` |
| 外部用AArch64 GCC / binutils | 不在。既存手順の必須版は未固定。再検証用の準備候補はGCC 13.3.0 / binutils 2.42（互換性は未確認、必須下限とは断定しない） | 例えば `~/opt/aarch64-gcc-13.3.0/bin/aarch64-linux-gnu-gcc` と関連ツールを置き、`local.mk` の `EXTERNAL_CROSS_COMPILE` に実際の絶対パスと末尾 `aarch64-linux-gnu-` を記入 |
| BL33用bare-metal GCC | 未実行、対応する版は未確定 | `TOOLCHAIN=gcc`、`CROSS_COMPILE=/実際の場所/bin/aarch64-none-elf-`。まず確認済みClangを使用する |
| Python依存 | pyelftools 0.32、cryptography（既存手順に版固定なし） | pyelftoolsは `third_party/pyelftools/elftools/` が存在するよう展開。cryptographyはmakeで使うPythonに導入。外部ビルドでの動作は未確認 |

GCCはcompiler単体でなく、同じ配布物のas/ld/ar/objcopy等とターゲットlibgccも必要。
配置例は自動インストール先ではない。準備後に実際の `gcc --version`、
`ld --version`、`gcc -print-libgcc-file-name` を記録して外部ビルドを検証する。
実績のないGCC版を動作確認済みとは扱わない。Vitis/FSBL/PMUFWが必要な場合は
Vitis 2025.1と上表のembeddedswを別途準備する（今回未実行）。

## 基本のビルド順と生成物

```sh
make bl31
make optee-sdk
make ta-a
make ta-b
make bl32
make bl33
```

BL31とBL33は独立して作れる。BL31の確認にはOP-TEEや署名鍵を要求しない。BL32はTAを組み込むのでSDK→TA→BL32の順。
`make` の既定はBL33のみ。実行されるコンパイラと全引数は端末に表示される。

| 入口 | 入力と実際の処理 | 生成物 |
|---|---|---|
| bl31 | TF-A、GCC、PLAT=zynqmp、SPD=opteed、RESET_TO_BL31=1 | `build/components/basic/tfa/zynqmp/release/bl31/bl31.elf`<br>`build/components/basic/tfa/zynqmp/release/bl31.bin`（外部ビルド未確認） |
| optee-sdk | OP-TEE、GCC、ARM64 Core/TA、ta_dev_kit | `build/components/basic/optee/export-ta_arm64/`（未確認） |
| ta-a | secure_world/ta/、SDKのta_dev_kit.mk、TA_VARIANT=a | `build/components/basic/ta-a/6dc13418-534f-4642-9138-215876304910.elf`<br>`build/components/basic/ta-a/6dc13418-534f-4642-9138-215876304910.stripped.elf`<br>`build/components/basic/ta-a/6dc13418-534f-4642-9138-215876304910.ta`（未確認） |
| ta-b | 同じソース、TA_VARIANT=b | `build/components/basic/ta-b/6dc13419-534f-4642-9138-215876304910.elf`<br>`build/components/basic/ta-b/6dc13419-534f-4642-9138-215876304910.stripped.elf`<br>`build/components/basic/ta-b/6dc13419-534f-4642-9138-215876304910.ta`（未確認） |
| bl32 | OP-TEE、GCC、上記2つのstripped.elfをEarly TAとして組込み | `build/components/basic/optee/core/tee.elf`<br>`build/components/basic/optee/core/tee.bin`（外部ビルド未確認） |
| bl33 | Makefileのソース一覧をClang/LLDまたはGCCで直接コンパイル・リンク | `build/zcu106/optee_bl33.elf`<br>`build/zcu106/optee_bl33.elf.map`（Clangで確認済み） |

上表は `PROFILE=basic` の正確な相対パス。外部生成物は未実行のためMake設定・
既存手順から示した予定パスであり、ファイルの存在確認済みではない。

署名・物理構成は上表の `build/components/basic/` をそれぞれ
`build/components/signed/`、`build/components/physical/` に置き換える。
BL33は `build/zcu106/signed/optee_bl33.elf` と
`build/zcu106/physical/optee_bl33.elf`、mapは各ELFパスに `.map` を付けたもの。
外部署名TAの予定出力は次の3ファイル（未確認）。

- `build/components/signed/ta-external/6dc1341c-534f-4642-9138-215876304910.elf`
- `build/components/signed/ta-external/6dc1341c-534f-4642-9138-215876304910.stripped.elf`
- `build/components/signed/ta-external/6dc1341c-534f-4642-9138-215876304910.ta`

署名bundleの予定出力は `build/components/signed/bundle/bundle.bin` と
`build/components/signed/bundle/manifest.json`（未確認）。

外部設定とコマンドは [build_config/components.mk](../build_config/components.mk)、
BL33は [Makefile](../Makefile) に明示。BL33の `TOOLCHAIN=gcc CROSS_COMPILE=...` は選択可能。
外部MakeルートはGCCのみ。`EXTERNAL_COMPILER=clang`、未知PROFILE、別PLATFORMはエラーにする。
現行ではClangをBL33にのみ使用する。旧Clang外部ビルドは [旧手順・参照専用](../legacy/docs/zcu106_build.md) に保全したが、新Makeルートの対応済みとは扱わない。
Vitis 2025.1/GUIは今回未実行。FSBL、PMUFW、bitstream、XSDBによるロードはELF生成と別工程。
[起動・回収資料](zcu106_run.md) の停止条件に従う。

BL33は設定変更を取り逃がさないよう毎回再コンパイルする。
外部生成物はPROFILEごとに分離する。外部compiler、パス、版、鍵、設定を変える場合は、
その `build/components/<PROFILE>` を手動で退避してSDKから作り直す。上書き増分ビルドで混ぜない。
署名/物理PROFILEのBL33は `build/zcu106/<PROFILE>/` に分離する。
