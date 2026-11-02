# 実機投入前のPC検証記録（2026-11-02）

対象: `refactor/readable-components-review-20261102`、開始commit `9326579554d266e9327da682638b546adc40be36`。
開始時の追跡ファイルはclean。既存local.mkは `build/local.mk.before-pc-validation` に保全して設定を追記した。
元の別worktreeの未コミット変更には触れず、旧バックアップからソース・生成物を混ぜていない。

## 判定一覧

PASSはPC上の確認範囲だけを表す。期待した拒否や実機ゲートのBLOCKEDはPC検査の失敗ではない。

| 分類 | 対象 | 結果／次の操作 |
|---|---|---|
| PCで実行してPASS | 固定commitのTF-A/OP-TEE/pyelftools取得 | 追跡ファイルclean、commit一致。Git原本は変更せず追加PTAはbuildコピーへ配置 |
| PCで実行してPASS | 公式GCC/binutils・Python依存 | 下表の実版を導入。GCC archiveは公式SHA-256と一致 |
| PCで実行してPASS | 基本・署名・物理のBL31/SDK/TA-A/B/BL32 | 全てソースから実ビルド。署名は外部TAとbundleも生成 |
| PCで実行してPASS | BL33 | 全3profileでClang/GCC両経路を実ビルド |
| PCで実行してPASS | 空出力からの再現 | run-a（build/）とrun-b（build/reproduce/）で同じ順番の全個別buildを完了 |
| PCで実行してPASS | ELFと引渡し・メモリ契約 | 両回の6組ずつでcheck_components成功。AArch64/entry/LOAD/section権限/整列/予約上限、dispatcher、Core conf、TA UUID/実組込み、PTA、共有RAM等を検査 |
| PCで実行してPASS | 署名ファイル・bundle | 両回で正常0・改変1・別鍵1・対応する別鍵公開鍵なら0。Core公開鍵とslot/digest/manifest整合も確認 |
| PCで実行してPASS | ホスト試験 | SHM契約10ケース、ページテーブル2構成×（8範囲＋NULL）、Python異常系9群、Tcl mock |
| PCで実行してPASS | 検査の異常系 | UUID/構成混在、SHM/引渡し不一致、ELF破損、Early TA差替え、bundle破損/slot重複、古いrun・tornログ、不正JTAG入力、入力ELF上書きを拒否 |
| PCで実行してPASS | JTAG準備・回収 | 両回・全6組のELF点検と回収定義生成。署名Core付き回収も両compilerで生成。Tcl mockでtarget一意性、ロード順、bundle配置、timeout、未許可拒否を確認 |
| PCで実行してFAIL | 修正後に残るPC側FAIL | なし。下記の発見した不具合は修正して再実行済み |
| 環境不足で未実行 | Vitis 2025.1 / 実XSDB | PATH、/opt、/tools等の候補で未発見。対象版を導入して実コマンドを確認する |
| 環境不足で未実行 | FSBL/PMUFW、XSA/BSP、必要ならbitstream | このPCには検証できる対象ボード入力がない。同じXSAからのfirmwareと初期化条件を用意する。未知の値を埋めない |
| 実機が必要 | DDR/予約/alias/XMPU/master/lock/poison、初期EL・Security State・cache/Stage2、PMU readiness、GIC等 | [実機手順](zcu106_run.md) の証跡を採取してレビューする |
| 実機が必要 | SMC、TA実行/分離/例外復帰、実OP-TEEローダ8ケース、物理アクセス拒否、後始末、性能 | 同じhashの生成物を使い、固有run IDで新しいRAM dumpを採取する。PC合成試験を実機PASSにしない |

外部Clang、別版GCC、QEMU、他ボードは今回の提供・確認範囲に含めない。
実機用の `B2_RUNTIME_READY=0` / `B2_PHYS_REVIEWED=0` と実行許可0を維持した。

## 実際の版と取得元

| 項目 | 実際に用いた版・取得元 |
|---|---|
| TF-A | https://github.com/Xilinx/arm-trusted-firmware.git / dd3512287a17eea866d5c82fa7b86ad58de59163 |
| OP-TEE | https://github.com/OP-TEE/optee_os.git / 0919de0f7c79ad35ad3c8ace5f823ad1344b4716 |
| pyelftools | https://github.com/eliben/pyelftools.git / d4ea35fc3c34eb0e4d3f8cfafb31552f5b5c37d6 / package 0.32 |
| GCC | Arm GNU Toolchain 13.3.Rel1 (Build arm-13.24)、13.3.1 20240614 |
| binutils | GNU ld 2.42.0.20240614（同じArm配布物のas/ar/objcopy等） |
| Clang/LLD | Ubuntu 21.1.8（既存/usr/binのもの） |
| Python | 既存 ~/.pyenv/versions/3.12.14/bin/python3 から専用venv。system Python 3.14.4は外部buildには用いない |
| Python依存 | cryptography 46.0.5、cffi 2.1.1、pycparser 3.12、setuptools 80.9.0、wheel 0.45.1。PyPIからvenvへ導入 |
| 時刻 | SOURCE_DATE_EPOCH=1737117274（参照OP-TEE commitの時刻） |

GCCはユーザー領域 `~/.local/opt/arm-gnu-toolchain-13.3.rel1-x86_64-aarch64-none-linux-gnu/` に展開した。
配布元: [Armの13.3.Rel1 archive](https://developer.arm.com/-/media/Files/downloads/gnu/13.3.rel1/binrel/arm-gnu-toolchain-13.3.rel1-x86_64-aarch64-none-linux-gnu.tar.xz)。
[公式SHA-256資料](https://developer.arm.com/-/media/Files/downloads/gnu/13.3.rel1/binrel/arm-gnu-toolchain-13.3.rel1-x86_64-aarch64-none-linux-gnu.tar.xz.sha256asc) と次の値が一致した。

```text
322f0b4482fc0d9fa0bb468134841f08d8c554c54ff5aa29a13a7a24bf7e1eb5
```

Git取得（各配置が存在しない新しい環境で実行。既存の原本を上書きしない）:

```sh
git init third_party/tfa-amd
git -C third_party/tfa-amd remote add origin https://github.com/Xilinx/arm-trusted-firmware.git
git -C third_party/tfa-amd fetch --depth=1 origin dd3512287a17eea866d5c82fa7b86ad58de59163
git -C third_party/tfa-amd checkout --detach FETCH_HEAD
git init third_party/optee
git -C third_party/optee remote add origin https://github.com/OP-TEE/optee_os.git
git -C third_party/optee fetch --depth=1 origin 0919de0f7c79ad35ad3c8ace5f823ad1344b4716
git -C third_party/optee checkout --detach FETCH_HEAD
git init third_party/pyelftools
git -C third_party/pyelftools remote add origin https://github.com/eliben/pyelftools.git
git -C third_party/pyelftools fetch --depth=1 origin d4ea35fc3c34eb0e4d3f8cfafb31552f5b5c37d6
git -C third_party/pyelftools checkout --detach FETCH_HEAD
# 各原本でgit rev-parse HEAD、git status --shortを確認する
python3.12 -m venv build/python
build/python/bin/python -m pip install -r build_config/python-requirements.txt
build/python/bin/python -m pip install --no-build-isolation ./third_party/pyelftools
cp -n local.mk.example local.mk
# local.mkで実際のGCC prefix、上記venv、試験鍵、SOURCE_DATE_EPOCHを指定
```

ここでpython3.12は実際の3.12.14 interpreterを指定する。このPCではpyenvのshimではなく上表の実ファイルを使った。
Arm archiveは上記URLからcurl -fL等で取得し、SHA照合後に `~/.local/opt/` へ展開する。
管理者権限のパッケージ導入や旧環境の自動復元は行っていない。

## 実行したbuild順（run-a）

次は空のcomponent/BL33出力で実行した順番。個別コマンドが実際の入口で、追加ラッパーはない。
ログはGit管理外の `results/pc-preflight/run-a/` に保存した。
再実行で既存ログを上書きせず、新しいログディレクトリ／BUILD_ROOTを選ぶ。

```sh
# 初回だけ。既存の試験鍵は再生成しない。
build/python/bin/python scripts/make_signed_bundle.py keys --keys build/signed-test-keys
mkdir -p results/pc-preflight/run-a
make -j4 PROFILE=basic bl31 > results/pc-preflight/run-a/basic-bl31.log 2>&1
make -j4 PROFILE=basic optee-sdk > results/pc-preflight/run-a/basic-optee-sdk.log 2>&1
make PROFILE=basic ta-a
make PROFILE=basic ta-b
make -j4 PROFILE=basic bl32
make PROFILE=basic TOOLCHAIN=clang bl33 > results/pc-preflight/run-a/basic-bl33-clang.log 2>&1
make PROFILE=basic TOOLCHAIN=gcc bl33 > results/pc-preflight/run-a/basic-bl33-gcc.log 2>&1

make -j4 PROFILE=signed bl31 > results/pc-preflight/run-a/signed-bl31.log 2>&1
make -j4 PROFILE=signed optee-sdk
make PROFILE=signed ta-a
make PROFILE=signed ta-b
make PROFILE=signed ta-external
make PROFILE=signed prepare-core
make -j4 PROFILE=signed bl32
make PROFILE=signed TOOLCHAIN=clang bl33 > results/pc-preflight/run-a/signed-bl33-clang.log 2>&1
make PROFILE=signed TOOLCHAIN=gcc bl33 > results/pc-preflight/run-a/signed-bl33-gcc.log 2>&1
build/python/bin/python scripts/make_signed_bundle.py bundle \
  --keys build/signed-test-keys --ta-dir build/components/signed/ta-external \
  --output build/components/signed/bundle --optee third_party/optee

make -j4 PROFILE=physical bl31 > results/pc-preflight/run-a/physical-bl31.log 2>&1
make -j4 PROFILE=physical optee-sdk
make PROFILE=physical ta-a
make PROFILE=physical ta-b
make PROFILE=physical prepare-core
make -j4 PROFILE=physical bl32
make PROFILE=physical TOOLCHAIN=clang bl33 > results/pc-preflight/run-a/physical-bl33-clang.log 2>&1
make PROFILE=physical TOOLCHAIN=gcc bl33 > results/pc-preflight/run-a/physical-bl33-gcc.log 2>&1
```

実際の作業では表示していないターゲットも同じ命名の個別.logへ保存した。失敗時はそのコマンドで止め、ログ末尾を確認する。
新しい `build/reproduce/` に対して同じ全コマンドに `BUILD_ROOT=build/reproduce` を付け、run-bのログへ保存した。
TAパス・Coreパス・bundle出力も同じ新しいrootへ揃えた。SDK、Coreコピー、各TAも全て新規に作った。
鍵・Python・取得済みソースは明示した同じ入力を使用し、旧生成物を入力にしていない。

## 生成物に対する検査

以下は署名＋Clangの例。他のprofile/compilerも同じ明示引数で両回全6組を実行した。
BL31ログは引渡し設定・出力先、BL33ログはcached SHMの実コンパイル引数と出力先を照合するために使う。
検査のログ照合はコマンドの整合確認で、実機ロード同一性や署名付きbuild attestationではない。

```sh
build/python/bin/python scripts/check_components.py --profile signed \
  --bl31 build/components/signed/tfa/zynqmp/release/bl31/bl31.elf \
  --bl32 build/components/signed/optee/core/tee.elf \
  --bl33 build/bl33/clang/signed/optee_bl33.elf \
  --conf build/components/signed/optee/conf.mk \
  --ta-a build/components/signed/ta-a/6dc13418-534f-4642-9138-215876304910.stripped.elf \
  --ta-b build/components/signed/ta-b/6dc13419-534f-4642-9138-215876304910.stripped.elf \
  --bl31-log results/pc-preflight/run-a/signed-bl31.log \
  --bl33-log results/pc-preflight/run-a/signed-bl33-clang.log \
  --output results/pc-preflight/run-a/components-signed-clang.json
build/python/bin/python scripts/check_signed_profile.py --platform zynqmp \
  --core build/components/signed/optee/core/tee.elf --conf build/components/signed/optee/conf.mk \
  --bundle build/components/signed/bundle --keys build/signed-test-keys \
  --optee third_party/optee --output results/pc-preflight/run-a/signed-profile.json
build/python/bin/python -m unittest discover -s tests/host -p 'test_*.py' -v
tclsh tests/host/jtag_mock.tcl build/components/signed/bundle/bundle.bin
```

run-bは `TZ_BUILD_ROOT="$PWD/build/reproduce" TZ_LOG_ROOT="$PWD/results/pc-preflight/run-b"` を
unittestコマンドに付けて、同じ拒否試験を新規生成物に対して実行した。
`check_components` には旧integrationの必要な検査だけを移した。legacy入口や旧command receipt形式は使わない。
OP-TEE ELFの単一RWX LOADは実形式として記録し、section側のW+X混在を拒否した。
実CPUのMMUページ権限をPCのELF属性で証明したとは扱わない。

既存Cホスト試験:

```sh
clang -std=c11 -Wall -Wextra -Werror -DTEE_CLIENT_HOST_TEST \
  -Iruntime/include -Iplatform/zcu106/b2 -Inormal_world/client \
  tests/host/client_shm_contract.c normal_world/client/client.c -o /tmp/tz-shm
for i in 0 1 2 3 4 5 6 7 8 9; do /tmp/tz-shm "$i" || break; done
git show 9326579:platform/zcu106/b2/client_mmu.c > /tmp/tz-baseline-mmu.c
for profile in 0 1; do
  clang -std=c11 -Wall -Wextra -Werror -DTEE_CLIENT_HOST_TEST \
    -DB2_SIGNED_TA_PROFILE=$profile -Iplatform/zcu106/b2 \
    -Dzcu106_b2_build_page_tables=baseline_build_page_tables \
    -c /tmp/tz-baseline-mmu.c -o /tmp/tz-baseline-mmu-$profile.o || break
  clang -std=c11 -Wall -Wextra -Werror -DTEE_CLIENT_HOST_TEST \
    -DB2_SIGNED_TA_PROFILE=$profile -Iplatform/zcu106/b2 \
    tests/host/page_tables_equivalence.c platform/zcu106/b2/client_mmu.c \
    /tmp/tz-baseline-mmu-$profile.o -o /tmp/tz-mmu-$profile || break
  /tmp/tz-mmu-$profile || break
done
```

SHM試験はSMC返答を合成し、範囲・cache契約値の拒否と呼出し順/回数を調べる。
ホストのこの経路はuncached分岐で、実buildのcached契約はCore設定と実コンパイルログ・ページ表から静的に照合した。
cache coherency、SMC命令、例外復帰や実TAの成功は実機で別途確認する。
Tcl mockは名前をmock関数へ差し替えたtclsh上だけで動かし、実XSDBへ接続していない。

## 修正した不具合と意味の保存

1. **TA-BヘッダのUUID不一致**：ファイル名とTA本文はBだが、SDK生成user_ta_header.cがAのUUIDを持っていた。
   `cppflags-y` を全体に渡る `CPPFLAGS_ta_arm64` に修正し、ELFの.ta_headとCoreに組み込まれた実bytesを検査した。
   読みやすさの変更ではなく、実ビルドで発見した機能上の設定修正。
2. **コピーしたOP-TEEの版表示**：.gitのないコピーが親のtrustzone-validationの版を拾っていた。
   Makeに参照OP-TEEの版とvalidation構成名を明示し、Coreのcore_v_strも検査した。
3. **署名bundleのロード漏れ**：JTAG手順はELFだけをロードしていた。
   検査済みの `--bundle-dir` からraw bundleを0x10600000へ配置する工程を追加し、mockで順序・引数を検査した。
4. **対象選択と異常入力**：Tclで0件/複数targetを明示拒否。停止位置不一致・timeout時も追加breakpointを除去する。不正JTAG JSONでも未許可Tcl定義だけを生成し、
   回収計画の出力で入力ELFを上書きしないようにした。
5. **出力混在**：BL33のcompiler/profileを別ディレクトリへ分離し、外部部品もBUILD_ROOTの新規出力を選べるようにした。

試験コード、SMC引数、共有構造体、メモリ予約、cache属性、既定の試験選択と低レイヤの日本語コメントは維持した。
JSONは結果・静的提案・実機入力だけに使い、通常build入力へ戻していない。

## 再現性と生成物ハッシュ

今回の主出力は [pc_artifacts.sha256](pc_artifacts.sha256)。リポジトリ直下で `sha256sum -c docs/pc_artifacts.sha256` を実行できる。
この表は生成物・秘密鍵そのものを含まず、今回の出力に対する記録である。
試験鍵を再生成するとCore公開鍵・署名TA・bundleが変わる。秘密鍵は記録・コミットしない。
同じ試験鍵でもRSA-PSS署名には乱数があるので、TAファイル／bundleは各回の公式署名検証とdigestで確認した。
ELFは出力先・デバッグ情報・一時object名等で全byteが変わり得るので、entry/PT_LOADとロード対象セクションも比較した。
増分Core buildでは.buildcountも増える。このSHA表は今回のクリーン出力のsnapshotで、再実行時の全byte一致を要求するgolden imageではない。

2回の比較ではBL31×3、BL32×3、TA-A/B stripped×6、BL33×6の計18 ELFでentry/PT_LOADと全SHF_ALLOCセクションの配置・内容が一致した。
NOBITS領域は配置・sizeを比較した。TF-A・stripped TA・Clang BL33はELF全byteも一致。
OP-TEEは.debug_info/.debug_line/.debug_line_strが異なり、GCC BL33は.strtab中のランダムな一時.o名が異なる。
ロード対象の内容は両者とも一致した。外部署名TAファイルとbundleは一致せず、両回の公式署名検証が同じ期待結果になった。

## 実機確認へ渡すもの

同じprofileのBL31/BL32と、選んだcompilerのBL33、署名なら同じ公開鍵で作ったbundleを使う。
[実機手順](zcu106_run.md) にELF停止位置、ロード順、RAM回収、合否判定、review後に変更する根拠を記載した。
XSA/firmware/runtime予約等の証跡が不足する現時点では、生成される起動点検はBLOCKED・終了値2・実行許可0が正しい。
「実機以外は全て完了」とは扱わず、上表の環境不足（Vitis/XSA/FSBL/PMUFW等）も残るものとして引き継ぐ。
