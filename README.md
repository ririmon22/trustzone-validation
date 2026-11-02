# TrustZone検証コードを読む・個別に作る

対象はZCU106 B2。基準は `offline-transfer-zcu106-b2-20261031-r1`。
通常ビルドはMakefileから直接コンパイル、または外部プロジェクトのmakeを呼ぶ。
JSONの編集・読込み・設定生成は不要。実機用の許可は別であり、
`B2_RUNTIME_READY=0`、`B2_PHYS_REVIEWED=0` を維持している。

## 受け取り後の最初の操作

完成版のブランチは `refactor/readable-components-review-20261102`。
常用する作業場所は `~/dev/tz/trustzone-readable-review`。
元の `~/dev/tz/trustzone-validation` は未コミット変更を保全した別worktreeである。

```sh
cd ~/dev/tz/trustzone-readable-review
cp -n local.mk.example local.mk
# 最初にlocal.mkをエディタで開き、コンパイラと外部ソースのパスを設定する
make bl33
```

`local.mk` はGit管理外。既存ファイルを上書きせず自分のパスを記入する。
ビルド共通値は `Makefile` と `build_config/components.mk`、Cの配置定数は
`platform/zcu106/b2/memory_map.h` にある。通常ビルドでJSONは編集しない。
現在実ビルド確認済みなのはClangによるBL33の3構成。BL31/SDK/TA/BL32は未確認。

## 必要なものと設定

`cp local.mk.example local.mk` で利用者のパスだけ設定する。空白を含むパスは未対応。
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
| ta-a | optee_standalone/ta/、SDKのta_dev_kit.mk、TA_VARIANT=a | `build/components/basic/ta-a/6dc13418-534f-4642-9138-215876304910.elf`<br>`build/components/basic/ta-a/6dc13418-534f-4642-9138-215876304910.stripped.elf`<br>`build/components/basic/ta-a/6dc13418-534f-4642-9138-215876304910.ta`（未確認） |
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

外部設定とコマンドは [build_config/components.mk](build_config/components.mk)、
BL33は [Makefile](Makefile) に明示。BL33の `TOOLCHAIN=gcc CROSS_COMPILE=...` は選択可能。
外部MakeルートはGCCのみ。`EXTERNAL_COMPILER=clang`、未知PROFILE、別PLATFORMはエラーにする。
旧Clang外部ビルドは [旧手順](docs/zcu106_build.md) に保全したが、新Makeルートの対応済みとは扱わない。
Vitis 2025.1/GUIは今回未実行。FSBL、PMUFW、bitstream、XSDBによるロードはELF生成と別工程。
[起動・回収資料](docs/zcu106_run.md) の停止条件に従う。

BL33は設定変更を取り逃がさないよう毎回再コンパイルする。
外部生成物はPROFILEごとに分離する。外部compiler、パス、版、鍵、設定を変える場合は、
その `build/components/<PROFILE>` を手動で退避してSDKから作り直す。上書き増分ビルドで混ぜない。
署名/物理PROFILEのBL33は `build/zcu106/<PROFILE>/` に分離する。

## 署名TA・物理試験

署名TAはEarly TAの検査と異なり、Normal側のRPCから供給した署名ファイルをCoreで検証する。
試験鍵はgitへ追加しない。次はコマンドの実行順。新環境では未実行。

```sh
python3 scripts/make_signed_bundle.py keys --keys build/signed-test-keys
# local.mkへTA_SIGN_KEY=.../trusted.pem、TA_PUBLIC_KEY=.../trusted.pub.pemを絶対パスで指定
make PROFILE=signed bl31
make PROFILE=signed optee-sdk
make PROFILE=signed ta-a
make PROFILE=signed ta-b
make PROFILE=signed ta-external
make PROFILE=signed prepare-core
make PROFILE=signed bl32
make PROFILE=signed B2_TEST_INDEX=17 B2_SIGNED_TA_PROFILE=1 B2_VALIDATION_PTA=1 bl33
python3 scripts/make_signed_bundle.py bundle --keys build/signed-test-keys --ta-dir build/components/signed/ta-external --output build/components/signed/bundle --optee third_party/optee
```

`ta-external` のUUIDは `6dc1341c-534f-4642-9138-215876304910`。
bundle.binには正常・改変・別鍵のTAを収める。manifest.jsonは結果出力であり設定ではない。
`prepare-core` は元OP-TEEを変更せずビルド用コピーにcanary PTAと署名監査パッチを置く。
コピーが既存なら停止するので、再準備時はコピーを退避する。bl32の再実行は準備済みコピーを使う。
不一致のOP-TEE版では監査パッチが停止する。

```sh
make PROFILE=physical bl31
make PROFILE=physical optee-sdk
make PROFILE=physical ta-a
make PROFILE=physical ta-b
make PROFILE=physical prepare-core
make PROFILE=physical bl32
make PROFILE=physical B2_TEST_INDEX=15 B2_PHYSICAL_DEDICATED=1 B2_VALIDATION_PTA=1 bl33
python3 scripts/check_b2_physical_reservation.py --elf build/zcu106/physical/optee_bl33.elf --elf build/components/physical/optee/core/tee.elf --output build/components/physical/reservation.json
```

物理PROFILEはcanary＋physical PTAと固定予約0x10800000を追加する。
XMPU設定、他マスタ停止、DDR予約の実機レビューはこのコマンドに含まれない。
静的検査成功でもロード許可にならない。非固定の従来物理試験手順は旧資料に保全している。

## フォルダと読む順番

| 場所（元→現在） | 役割 |
|---|---|
| tfa_standalone/integration → 同左 | BL33のアセンブリ起動。名前は歴史的だがTF-A本体ではない |
| common → 同左 | BL33で共有するCPU操作、例外処理、メモリ操作、RAMログ、計測、試験報告 |
| optee_standalone/client → 同左 | Normal Worldクライアントと署名TA用RPC |
| optee_standalone/ta、ta_signed → 同左 | Secure WorldユーザーTA |
| optee_standalone/pta、loader → 同左 | Core権限PTA、署名ローダ監査追加部分 |
| optee_standalone/tests → 同左 | 機能・異常系・回帰・性能・物理試験 |
| platform/zcu106/b2 → 同左 | B2の配置、リンカ、ページテーブル |
| config、scripts → 同左 | 保全した旧JSON自動化、実機準備・回収・検査ツール |
| docs → 同左 | 仕様・低レイヤの読み方・旧運用手順 |
| 新規 build_config/components.mk、local.mk.example | 個別ビルドと利用者パス設定 |
| 新規 scripts/prepare_core.py | JSONなしのCoreコピー・追加PTA配置 |
| 新規 tests/host | SMC合成応答によるSHM契約試験、基準版とのページテーブル比較 |

既存の役割分割を保ち、移動は行っていない。旧自動化には固定パスが多く、見た目の改名で壊さないため。
まず `tests/main.c` のstandalone_mainを読み、次に `functional.c`、`client/client.c`、
`common/arch.c`、`ta/ta.c` と進む（tests等はoptee_standalone配下）。
ECHOは `functional.c:b2_run_service → tee_invoke → transport_call → smc_call`
からEL3/opteed、OP-TEE Coreを経て `TA_InvokeCommandEntryPoint` へ進み、同じ経路で戻る。
開始は `tfa_standalone/integration/start.S`、例外復帰は `common/el1_vectors.S` と `common/el1_fault.c`。
[用語と読み方](docs/b2_reading_guide.md)、[試験仕様](docs/b2_test_specification.md)、
[21項目の範囲](docs/b2_scope_21.md) も参照する。

## JSONの分類と配置の確認

| JSON | 用途と扱い |
|---|---|
| config/b2.example.json | 旧一括ビルド設定。通常Makeでは未使用 |
| config/b2_hardware_capture.example.json、xmpu_test.example.json | 実機採取・試験準備の設定。ELF生成には不要 |
| platform/zcu106/b2/memory_map.json | 旧ビルド入力兼静的配置提案。旧検査では引き続き使用 |
| platform/zcu106/{board_profile,boot_plan,jtag_b2_inputs,mmio_catalog,vitis_observations}.json | 実機情報・起動提案・未確認状態の記録。Makeでは未使用 |
| third_party/dependencies.lock.json | 外部ソース来歴・旧出所検査 |
| results/以下やmanifest.json | 検査・生成結果。通常ビルドの入力ではない |

MakeのTF-A/OP-TEE配置、`platform/zcu106/b2/memory_map.h`、親のmemory_map.h、
`linker_bl33.ld` に同じ予約を記述している。BL31=0x1000..0x80000、
BL32=0x60000000..0x70000000、BL33 code/data/stack=0x10200000/0x10300000/0x10400000、
SHM=0x10500000..0x10600000。変更時は全箇所を同時に確認する。
`llvm-readelf -l -S ELF` と生成mapでPT_LOAD、入口、境界を確認し、Coreのconf.mkと
TF-Aのビルド表示を照合する。リンカのASSERTが領域超過・stack境界を検査する。
JSONの静的提案との比較は実機/旧検査を使う場合に必要であり、Makeへ設定を逆輸入しない。

旧 `field.py` / `build_zcu106.py` / `build_external.py` はJSON自動化として保全。
新しい出力先に対して、旧integration/review/signed検査やJTAG準備の固定パスは未対応。
新Make生成物をそのまま旧検査・ロード入口で扱えるとは案内しない。

## 今回の確認

[確認記録](docs/readable_build_validation.md) にコマンドと限界を記載する。
BL33の3構成を変更前後でClang実ビルドし、ELF配置と主要ABI定数を比較した。
基本・物理構成のProgram Headerは一致。署名構成はコード領域が304バイト増え、
配置先・権限・整列は維持した。ELFバイト列の完全一致ではない。
BL31/BL32/TAの外部実ビルド、GCC、Vitis、実機は未実行。make -nを成功の根拠にしない。
