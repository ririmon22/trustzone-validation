# 現行Makeの設定と個別ビルド

[入口](../README.md) · [PCで実行した全手順と結果](pc_validation.md) · [追加構成](profiles.md) · [実機](zcu106_run.md)

最初に `cp -n local.mk.example local.mk` で自分の設定を作る。
通常ELF生成はMakeだけを読み、JSON・legacy入口・自動ダウンロードを使わない。
ここに示す全構成をソースから実ビルドした。FSBL/PMUFWはXSA/Vitisがないため別扱い。

## 入力・コンパイラ・設定

| 入力 | 参照版／commit | 標準配置 |
|---|---|---|
| TF-A (Xilinx/arm-trusted-firmware) | xlnx_rebase_v2.12_2025.1 / dd3512287a17eea866d5c82fa7b86ad58de59163 | third_party/tfa-amd |
| OP-TEE/optee_os | 4.5.0 / 0919de0f7c79ad35ad3c8ace5f823ad1344b4716 | third_party/optee |
| eliben/pyelftools | 0.32 / d4ea35fc3c34eb0e4d3f8cfafb31552f5b5c37d6 | third_party/pyelftools |
| GCC / binutils | Arm GNU Toolchain 13.3.Rel1、GCC 13.3.1 20240614、ld 2.42.0.20240614 | local.mk の EXTERNAL_CROSS_COMPILE / CROSS_COMPILE |
| Clang / LLD | Ubuntu 21.1.8 | PATH上のclang / ld.lld |
| Python | 既存CPython 3.12.14から専用venv | build/python/bin/python |
| Python依存 | cryptography 46.0.5等を [固定リスト](../build_config/python-requirements.txt) へ記載。pyelftoolsは上記checkoutから導入 | 同じvenv |

取得元・導入コマンド・配布物のSHA-256は [確認記録](pc_validation.md) を参照。
外部ソースはGitで固定commitを新規取得し、原本の追跡ファイルがcleanであることを確認した。
旧バックアップや旧生成物は使用していない。追加PTAは `build/components/<PROFILE>/source/optee/` のコピーへ置く。

BL31/SDK/TA/BL32は**このGCC経路**で確認した。BL33は同じGCC（libcをリンクしないfreestanding）とClangの両方を確認した。
別のbare-metal GCCや外部Clangは今回未検証／未対応であり、この実績へ含めない。
Vitis/XSDB、ボード固有XSA/FSBL/PMUFWはこのPCでは不足している。

## 基本構成

```sh
make -j4 bl31
make -j4 optee-sdk
make ta-a
make ta-b
make -j4 bl32
make TOOLCHAIN=clang bl33
make TOOLCHAIN=gcc bl33
```

BL31・BL33は独立。BL32はSDK→TA-A/B→BL32の順。署名・物理は [追加構成](profiles.md)。
コマンド・引数はMake出力から外部make／実際のコンパイルまで追える。
Makeの詳細は [Makefile](../Makefile) と [components.mk](../build_config/components.mk)。

| 部品 | ELF／関連生成物（リポジトリ直下基点） |
|---|---|
| BL31 | build/components/basic/tfa/zynqmp/release/bl31/bl31.elf、build/components/basic/tfa/zynqmp/release/bl31.bin |
| SDK | build/components/basic/optee/export-ta_arm64/ |
| TA-A | build/components/basic/ta-a/6dc13418-534f-4642-9138-215876304910.elf、同じbasenameの .stripped.elf / .ta |
| TA-B | build/components/basic/ta-b/6dc13419-534f-4642-9138-215876304910.elf、同じbasenameの .stripped.elf / .ta |
| BL32 | build/components/basic/optee/core/tee.elf、同じcore/のtee.bin、tee-header_v2.bin、tee-pager_v2.bin、tee-pageable_v2.bin |
| BL33 Clang | build/bl33/clang/basic/optee_bl33.elf、build/bl33/clang/basic/optee_bl33.elf.map |
| BL33 GCC | build/bl33/gcc/basic/optee_bl33.elf、build/bl33/gcc/basic/optee_bl33.elf.map |

`PROFILE=signed` / `PROFILE=physical` は表の `basic` 部分を対応する名前へ置き換える。
外部署名TAは `build/components/signed/ta-external/6dc1341c-534f-4642-9138-215876304910.{elf,stripped.elf,ta}`。
bundleは `build/components/signed/bundle/bundle.bin` と同じディレクトリのmanifest.json／3種のTA。
全主要生成物の今回のSHA-256は [ハッシュ記録](pc_artifacts.sha256)。

## 設定変更とクリーン再現

BL33は毎回再コンパイルし、compiler/profileで出力を分離する。
外部GCCの版・source・鍵・設定を変えるときは古い出力を流用せず、**存在しないBUILD_ROOT**を指定してSDKから作り直す。
今回は空の `build/components/` と `build/bl33/` からの一式に加え、空の `build/reproduce/` でも全構成を再ビルドした。

```sh
# 新しい空の出力先に対して、同じ個別コマンド順を繰り返す。
export BUILD_ROOT=build/reproduce
# make bl31 → make optee-sdk → ... （詳細は確認記録）
```

`BUILD_ROOT` は出力先だけで、依存ソース／Python／試験鍵はlocal.mkの明示パスを使う。
`local.mk.example` のSOURCE_DATE_EPOCHは参照OP-TEEのcommit時刻を指定し、CoreとTF-Aの時刻文字列を固定する。
TAのRSA-PSS署名・新規生成する鍵には乱数を使うので、再build時に .ta / bundleのバイト一致を要求しない。
公開鍵照合、公式署名検証、bundle slot／digestとELF配置を各回で検査する。
デバッグ情報や異なる出力先によるELF差は、ロード対象のセクションと配置を別に比較する。
