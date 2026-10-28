現在のZIP展開物向け入口は[次回手順](next_offline_session.md)と[ZIP専用build](zip_offline_build.md)。以下のGit checkout向け手順は別モードであり、ZIPにGit検査を要求したり、build中にcloneするものではない。

# オフライン移送ブランチ

未コミット更新の移送にはmanifest exporterを使う。Git bundleはcommit済み履歴のみで、今回の未コミット更新は含まれない。

このブランチは自作ソース・設定・テスト・構築と確認の手順を移送する。外部ソース、ツールチェーン、ELF/BIN/SDK、過去のログ・結果ファイルを含めない。文書中の既存実績は参考情報であり、移送先での成功を示さない。`results/`への文字列は再生成先であり、配布ファイルではない。Git履歴には元ブランチの過去結果が含まれるため、履歴まで持ち込まない場合は後述のarchiveを使う。

## 1. 取得・配置

取得工程とビルド工程を分ける。ビルド中にネットワーク取得やinstallは行わない。

1. 許可されたGitへ別途登録済みなら、移送先でそのGitから`offline-transfer`を取得する。本作業ではpushしない。
2. ネットワークを使わず現在のツリーだけ移す場合、送出側repo rootで実行する。

```bash
python3 scripts/export_offline_transfer.py --output ../offline-transfer-export/new-run
```

3. 承認された媒体で2ファイルを移送し、受入側で実行する。

```bash
sha256sum -c trustzone-validation.tar.sha256
tar -xf trustzone-validation.tar
cd trustzone-validation
```

4. Git履歴とブランチが必要なら、代わりに送出側でbundleを作る。bundleには祖先の結果ファイルも含まれるため、ツリーのみの移送とは区別する。

```bash
git bundle create ../offline-transfer-export/trustzone-validation.bundle offline-transfer
```

受入側では次を実行する。bundle pathは実在する場所を入力する。

```bash
read -r -p 'Bundle file: ' TZ_BUNDLE
git clone -b offline-transfer "$TZ_BUNDLE" trustzone-validation
cd trustzone-validation
```

## 2. 外部ソース

既存ソースを上書きしない。対応する完全Git checkoutを次の位置へ用意する。別の場所にある場合は、固定commit・変更有無を確認した上で、この位置へコピーする。現在のhelperは`third_party/`の実体を前提とするためsymlink配置は避ける。

|配置先|固定commit|用途|
|---|---|---|
|third_party/tfa-amd|dd3512287a17eea866d5c82fa7b86ad58de59163|AMD TF-A 2025.1、B1/B2 BL31|
|third_party/optee|0919de0f7c79ad35ad3c8ace5f823ad1344b4716|OP-TEE 4.5.0、Core/SDK/Early TA|
|third_party/pyelftools|d4ea35fc3c34eb0e4d3f8cfafb31552f5b5c37d6|v0.32、Core ELF解析|
|third_party/compiler-rt|2078da43e25a4623cab2d0d60decddf709aaea28|LLVM21.1.8、Clang利用時のみ。llvm-projectのrootを配置|
|third_party/embeddedsw|c0aed2eff7a30f307238ec853fa8fbc45dcabdda|2025.1、BSP/FSBL/PMUFWを別途ソースから作る場合|

原本は[lockファイル](../third_party/dependencies.lock.json)。独自変更を固定commitの原本と扱わない。次は取得済みsourceだけを検証し、downloadしない。

```bash
export GIT_NO_LAZY_FETCH=1
export GIT_ALLOW_PROTOCOL=file
python3 scripts/source_snapshot.py tfa-amd optee pyelftools
# Clangを使う場合のみ追加する。
python3 scripts/source_snapshot.py compiler-rt
```

個々のsource directoryで`git rev-parse HEAD`、`git status --porcelain --untracked-files=all`を確認する。固定HEADかつ変更なしが必要。部分cloneでobjectが不足している場合は取得工程へ戻り、build中には補完しない。完全ベアメタルだけなら外部firmware source不要。

## 3. ツール確認と不足時の扱い

Ubuntu24.04/Vitis2025.1の実動作はNOT_RUN。Vitis同梱toolの存在・版を推測しない。既存toolを検出し、追加導入は行わない。[最小依存表](minimum_dependencies.md)に構成別のcommandと用途がある。

1. Vitisのインストール先を入力し、環境設定する。設定ファイルの存在を確認する。

```bash
read -r -p 'Vitis 2025.1 installation directory: ' TZ_VITIS
if test -f "$TZ_VITIS/settings64.sh"; then
    source "$TZ_VITIS/settings64.sh"
else
    printf 'BLOCKED: settings64.sh not found\n'
fi
make --version
python3 --version
git --version
```

2. 実在するAArch64 GNU prefixを入力する。以下のhelperはcompiler/assembler/linker等と実ELF生成能力を検査する。ARM32 prefixは使わない。

```bash
read -r -p 'AArch64 compiler prefix (path ending in -): ' TZ_CROSS
"${TZ_CROSS}gcc" --version
"${TZ_CROSS}as" --version
"${TZ_CROSS}ld" --version
python3 scripts/check_build_environment.py --configuration optee --compiler gcc --cross-compile "$TZ_CROSS" --output results/offline/environment.json
```

不足時はBLOCKEDと不足一覧を返す。自動download/installはしない。基本Pythonは>=3.9、Make、bash/coreutils、Git、AArch64 tool familyが必要。OP-TEE/Early TAは選択Pythonの`cryptography`と固定pyelftools sourceを追加で使用する。Clang経路の既存検証版はClang/LLD/LLVM21.1.8。GNU/Vitisの版は現地で確認し、未確認版を互換性確認済みにしない。

QEMUを実行する場合のみ`qemu-system-aarch64`が必要。既存の検証版は10.2.1、machineは`virt-9.2`。QEMU TF-AのFIP構築はnative C compiler・OpenSSL header/libcrypto・opensslも使用する。ホスト試験のC fixtureにはnative clangが必要。

## 4. ソースからビルド・検査

repo rootで実行する。毎回fresh workspaceを作り、過去のELF/SDKを入力にしない。外部sourceは完全checkoutを既定とし、移送先では開発用`--allow-snapshots`を使わない。

```bash
bash scripts/build_vitis_offline.sh --configuration baremetal --platform zcu106 --compiler gcc --cross-compile "$TZ_CROSS"
bash scripts/build_vitis_offline.sh --configuration tfa --platform zcu106 --compiler gcc --cross-compile "$TZ_CROSS"
bash scripts/build_vitis_offline.sh --configuration optee --platform zcu106 --compiler gcc --cross-compile "$TZ_CROSS"
python3 scripts/check_memory_map.py --platform zcu106
python3 -S -m unittest discover -s tests -v
python3 scripts/check_manual.py --output results/offline/manual-check.json
python3 scripts/check_offline_transfer.py
python3 scripts/check_boot_plan.py --output results/offline/boot-gate.json
```

最後のboot gateは現在BLOCKED（exit2）が期待結果。source build成功と実機load許可は別。ビルド出力は`build/offline-<UTC>/`、詳細logは`results/phase8/`。BL31、BL32、BL33、TA SDK、Early TAはすべてソースから生成する。

Clangを使う場合はGNU用引数を`--compiler clang --llvm-dir <実在LLVM bin directory>`へ置き換える。QEMU対応toolと依存が揃った場合だけ次を実行する。

```bash
bash scripts/build_vitis_offline.sh --configuration all --platform qemu --compiler clang --qemu --host-tests
```

QEMU未実施ならNOT_RUNとし、ホスト試験を代用しない。性能集計は実行時logを`analyze_benchmarks.py`で解析し、実機性能と混同しない。

## 5. Vitisと安全条件

[Platform作成](manual/02_vitis_platform_creation.md)→[Application作成](manual/03_vitis_application_creation.md)を参照する。通常Standalone BSP startupと自作EL3入口を混同しない。B2のBL31は`tfa-zynqmp-b2`出力を使う。メモリ・リンカ・MMUは[整合化設計](b2_zcu106_alignment.md)を参照。

`B2_RUNTIME_READY=0`、boot planの未検証flag、hardware load BLOCKEDを維持する。FSBL/PMUFWの実行時予約、DDR/alias、XMPU/lock/master経路、EL/MMU/cache/Stage2引渡し、UART/GIC、起動順序の証跡不足を静的PASSで置き換えない。[実機確認チェックリスト](b2_hardware_checklist.md)と[取得手順](b2_hardware_readiness.md)で記録する。現段階ではJTAG load/run/resetを行わない。GPIO/DMAは保留。

## 6. 旧Phase7比較スクリプト

通常は上記offline_build入口を使う。`run_phase7_local.sh`と`validate_phase7_built.sh`はClang21系の旧比較用入口であり、送出元のtoolchain実測JSONを移送しない。使用する場合だけ、現地toolを人が確認してローカルbaselineを生成する。生成物をGitへ追加しない。baseline未用意なら両スクリプトはBLOCKEDを返す。

```bash
python3 scripts/toolchain_manifest.py --output results/offline/toolchain-baseline.json
```

この旧manifest helperはLLVM21の既定path等を前提とする。GNU/Vitis routeの検査は`check_build_environment.py`を使う。旧入口を実行可能にするために安全ゲートを変更しない。
